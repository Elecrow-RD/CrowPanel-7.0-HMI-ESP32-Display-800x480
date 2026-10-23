// CrowPanel 7" ESP32-S3 - Stream Deck (4x4 tile grid over WiFi/HTTP)
// Display: 800x480 RGB LCD, LVGL partial refresh (dirty-rect blit into the live framebuffer)
// Touch:   GT911 via direct I2C (0x5D or 0x14)
// Network: WiFi STA, POSTs {"key": "<id>"} to SERVER_HOST:SERVER_PORT/tap

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <lvgl.h>
#include "secrets.h"

#ifndef ENABLE_NOTES_LAYER
#define ENABLE_NOTES_LAYER 1
#endif

// Set to 1 in platformio.ini build_flags if touch stays dead or reads garbage
// on a V3.0 board: forces a GT911 reset through the PCA9557 on every boot.
#ifndef FORCE_TOUCH_RESET
#define FORCE_TOUCH_RESET 0
#endif

// ---------- Tunables ----------
static constexpr uint32_t HTTP_CONNECT_TIMEOUT_MS = 1500;
static constexpr uint32_t HTTP_READ_TIMEOUT_MS = 2500;
static constexpr uint32_t WIFI_RETRY_MS = 5000;
static constexpr uint32_t NOTE_POLL_MS = 3000;
static constexpr size_t TAP_KEY_MAX = 24;
static constexpr size_t TAP_QUEUE_LEN = 8;

#if ENABLE_NOTES_LAYER
#include <ArduinoJson.h>
#endif

// ---------- Pins ----------
#define TFT_BL 2
#define TFT_VSYNC 40
#define TOUCH_SDA 19
#define TOUCH_SCL 20

// ---------- Display ----------
Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    41, 40, 39, 0,
    14, 21, 47, 48, 45,
    9,  46, 3,  8,  16, 1,
    15, 7,  6,  5,  4,
    0, TFT_VSYNC, 48, 40,
    0, 1,  31, 13,
    1, 15000000);

 Arduino_RGB_Display *gfx = new Arduino_RGB_Display(800, 480, rgbpanel, 0, true);

// ---------- Touch (GT911 direct I2C) ----------
// CrowPanel V3.0 routes GT911 RST/INT through a PCA9557 I2C expander at 0x18
// (bit0 = RST, bit1 = INT). V1/V2 wire them straight to the ESP32, so the
// expander is only touched as a fallback when the GT911 doesn't answer.
static constexpr uint8_t PCA9557_ADDR = 0x18;
static constexpr uint8_t PCA9557_OUTPUT = 0x01;
static constexpr uint8_t PCA9557_CONFIG = 0x03;

static uint8_t gt911_addr = 0x5D;
static constexpr uint16_t GT911_REG_STATUS = 0x814E;
static constexpr uint16_t GT911_REG_POINT1 = 0x8150;

static bool gt911_probe(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

static bool gt911_read(uint16_t reg, uint8_t *buf, size_t n) {
    Wire.beginTransmission(gt911_addr);
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)(reg & 0xFF));
    if (Wire.endTransmission(false) != 0) return false;
    size_t got = Wire.requestFrom((int)gt911_addr, (int)n);
    if (got != n) return false;
    for (size_t i = 0; i < n; i++) buf[i] = Wire.read();
    return true;
}

static void gt911_write(uint16_t reg, uint8_t val) {
    Wire.beginTransmission(gt911_addr);
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)(reg & 0xFF));
    Wire.write(val);
    Wire.endTransmission();
}

static bool gt911_read_point(uint16_t &x, uint16_t &y) {
    uint8_t status;
    if (!gt911_read(GT911_REG_STATUS, &status, 1)) return false;
    if ((status & 0x80) == 0) return false;
    uint8_t touches = status & 0x0F;
    bool has = false;
    if (touches > 0) {
        uint8_t p[4];
        if (gt911_read(GT911_REG_POINT1, p, 4)) {
            x = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
            y = (uint16_t)p[2] | ((uint16_t)p[3] << 8);
            has = true;
        }
    }
    gt911_write(GT911_REG_STATUS, 0);
    return has;
}

static void pca9557_write(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(PCA9557_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

// Pulse GT911 reset with INT low so the controller comes up on 0x5D.
static void gt911_reset_via_expander() {
    pca9557_write(PCA9557_CONFIG, 0x00);  // all pins as outputs
    pca9557_write(PCA9557_OUTPUT, 0x00);  // RST low, INT low
    delay(20);
    pca9557_write(PCA9557_OUTPUT, 0x01);  // RST high, INT low -> addr 0x5D
    delay(10);
    pca9557_write(PCA9557_OUTPUT, 0x03);  // release INT
    delay(150);
}

// ---------- LVGL ----------
static const uint32_t SCREEN_W = 800;
static const uint32_t SCREEN_H = 480;
static const uint32_t DRAW_BUF_LINES = 48;  // ~1/10 screen per render chunk
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *lvgl_buf;
static SemaphoreHandle_t lvgl_mux;

// Idle dimmer state (used by the LVGL task and the touch callback).
static uint32_t last_input_ms = 0;
static bool dimmed = false;
static constexpr uint32_t DIM_AFTER_MS = 60000;  // dim after 1 min untouched

// The panel dims by lowering the backlight PWM, not by painting a translucent
// sheet. A full-screen sheet forces a 10-chunk repaint of the whole framebuffer
// the moment it is removed, which starves the RGB scanout and leaves the panel
// black. PWM costs nothing and needs no repaint at all.
static constexpr uint8_t BL_CHANNEL = 0;
static constexpr uint32_t BL_FREQ_HZ = 20000;
static constexpr uint8_t BL_RES_BITS = 8;
static constexpr uint8_t BL_ON_DUTY = 255;
static constexpr uint8_t BL_DIM_DUTY = 40;  // ~16 %

static void backlight_init() {
    ledcSetup(BL_CHANNEL, BL_FREQ_HZ, BL_RES_BITS);
    ledcAttachPin(TFT_BL, BL_CHANNEL);
    ledcWrite(BL_CHANNEL, BL_ON_DUTY);
}

static void backlight_set(bool on) {
    ledcWrite(BL_CHANNEL, on ? BL_ON_DUTY : BL_DIM_DUTY);
}

// LVGL is not thread-safe. Every widget touch must hold lvgl_mux, except when
// the caller already owns it (LVGL runs its own timer task).
static bool ui_lock() {
    if (!lvgl_mux) return false;
    if (xSemaphoreGetMutexHolder(lvgl_mux) == xTaskGetCurrentTaskHandle()) return false;
    return xSemaphoreTake(lvgl_mux, portMAX_DELAY) == pdTRUE;
}

static void ui_unlock(bool locked) {
    if (locked) xSemaphoreGive(lvgl_mux);
}

// Partial refresh: blit only the invalidated rectangle into the live framebuffer.
// A full-screen 768 KB copy per tap used to tear visibly while the panel scanned
// it out; a button-sized blit keeps the rest of the screen untouched.
//
// The panel scans the framebuffer continuously, so a blit that lands while the
// beam crosses the same rows shows up as a flicker. Each refresh therefore starts
// right after VSYNC: LVGL then paints top-to-bottom faster than the beam moves,
// so the writes stay ahead of it.
static volatile uint32_t vsync_seq = 0;
static bool vsync_sync = false;
static bool refresh_pending = true;

static void IRAM_ATTR on_rgb_vsync() {
    vsync_seq++;
}

static bool vblank() {
    const uint32_t seq = vsync_seq;
    const uint32_t started = millis();
    while (vsync_seq == seq) {
        if (millis() - started >= 50) return false;
        vTaskDelay(1);
    }
    return true;
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    if (refresh_pending && vsync_sync) {
        if (!vblank()) vsync_sync = false;  // panel stopped signalling: never stall again
    }
    const int16_t w = (int16_t)(area->x2 - area->x1 + 1);
    const int16_t h = (int16_t)(area->y2 - area->y1 + 1);
    gfx->draw16bitRGBBitmap((int16_t)area->x1, (int16_t)area->y1, (uint16_t *)color_p, w, h);
    refresh_pending = lv_disp_flush_is_last(drv);
    lv_disp_flush_ready(drv);
}

static void lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    static uint16_t last_x = 0, last_y = 0;
    static bool pressed = false;
    uint16_t x, y;
    if (gt911_read_point(x, y)) {
        last_x = x; last_y = y; pressed = true;
        last_input_ms = millis();
    } else {
        pressed = false;
    }
    data->point.x = last_x;
    data->point.y = last_y;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void lvgl_task(void *arg) {
    while (true) {
        if (xSemaphoreTake(lvgl_mux, portMAX_DELAY) == pdTRUE) {
            lv_timer_handler();
            const bool idle = (uint32_t)(millis() - last_input_ms) > DIM_AFTER_MS;
            if (idle != dimmed) {
                dimmed = idle;
                backlight_set(!idle);
                Serial.println(idle ? "backlight: dim" : "backlight: full");
            }
            xSemaphoreGive(lvgl_mux);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ---------- Actions ----------
struct Tile {
    const char *key;        // id sent to server
    const char *label;      // shown on tile
    const char *icon;       // LVGL symbol
    uint32_t    accent;     // accent color
};

// Palette from shop.fusionautomate.in: near-black green background, deep green
// cards, lime accent, teal secondary, sage muted text, red for destructive keys.
static constexpr uint32_t C_BG      = 0x030906;
static constexpr uint32_t C_CARD    = 0x0F3D26;
static constexpr uint32_t C_CARD_ON = 0x1A4A30;
static constexpr uint32_t C_LIME    = 0x4ADE4A;
static constexpr uint32_t C_TEAL    = 0x3393E0;
static constexpr uint32_t C_SAGE    = 0x84A18C;
static constexpr uint32_t C_MINT    = 0xD3ECD8;
static constexpr uint32_t C_RED     = 0xFB2C36;
static constexpr uint32_t C_AMBER   = 0xFCBB00;

static const Tile TILES[] = {
    { "mute",          "MUTE",       LV_SYMBOL_MUTE,      C_LIME  },
    { "play_pause",    "PLAY/PAUSE", LV_SYMBOL_PLAY,      C_LIME  },
    { "next_track",    "NEXT",       LV_SYMBOL_NEXT,      C_LIME  },
    { "vol_down",      "VOL DOWN",   LV_SYMBOL_VOLUME_MID,C_TEAL  },
    { "vol_up",        "VOL UP",     LV_SYMBOL_VOLUME_MAX,C_TEAL  },
    { "mic_mute",      "MIC",        LV_SYMBOL_AUDIO,     C_RED   },
    { "esc",           "ESC",        LV_SYMBOL_CLOSE,     C_SAGE  },
    { "snip",          "SNIP",       LV_SYMBOL_IMAGE,     C_AMBER },
    { "copy",          "COPY",       LV_SYMBOL_COPY,      C_TEAL  },
    { "paste",         "PASTE",      LV_SYMBOL_PASTE,     C_TEAL  },
    { "show_desktop",  "DESKTOP",    LV_SYMBOL_HOME,      C_SAGE  },
    { "open_terminal", "TERMINAL",   LV_SYMBOL_KEYBOARD,  C_AMBER },
    { "lock_screen",   "LOCK",       LV_SYMBOL_CLOSE,     C_RED   },
    { "start",         "START",      LV_SYMBOL_BARS,      C_LIME  },
    { "alt_tab",       "ALT+TAB",    LV_SYMBOL_LIST,      C_SAGE  },
    { "ctrl_tab",      "CTRL+TAB",   LV_SYMBOL_RIGHT,     C_SAGE  },
};

static constexpr int TILE_COUNT = (int)(sizeof(TILES) / sizeof(TILES[0]));

static lv_obj_t *status_label = nullptr;
static lv_obj_t *clock_label = nullptr;
static lv_obj_t *ip_label = nullptr;

// Layers
static lv_obj_t *layer_deck = nullptr;
#if ENABLE_NOTES_LAYER
static lv_obj_t *layer_notes = nullptr;
static lv_obj_t *notes_scroll = nullptr;
static lv_obj_t *tab_deck_btn = nullptr;
static lv_obj_t *tab_notes_btn = nullptr;

static volatile bool notes_layer_active = false;
static volatile bool notes_available = true;
static volatile double last_diary_mtime = -1;  // -1 forces the first fetch
static uint32_t notes_poll_at = 0;
static char notes_text_buf[8192];
#endif

static void set_status(const char *msg, uint32_t color = 0x84A18C) {
    if (!status_label) return;
    bool locked = ui_lock();
    lv_label_set_text(status_label, msg);
    lv_obj_set_style_text_color(status_label, lv_color_hex(color), 0);
    ui_unlock(locked);
}

// Human-readable reason for the negative HTTPClient error codes.
static const char *http_error_text(int code) {
    switch (code) {
        case HTTPC_ERROR_CONNECTION_REFUSED: return "server down";
        case HTTPC_ERROR_NOT_CONNECTED:      return "no network";
        case HTTPC_ERROR_CONNECTION_LOST:    return "conn lost";
        case HTTPC_ERROR_READ_TIMEOUT:       return "timeout";
        case HTTPC_ERROR_SEND_HEADER_FAILED: return "send failed";
        case HTTPC_ERROR_TOO_LESS_RAM:       return "low ram";
        default:                             return nullptr;
    }
}

// POST {"key":"<id>"}. Runs on net_task only, never on the LVGL task, so a slow
// or dead server can no longer freeze the UI for 2.5 s per tap.
static void post_tap(const char *key) {
    if (WiFi.status() != WL_CONNECTED) {
        set_status("WiFi offline", 0xFB2C36);
        return;
    }

    static WiFiClient client;  // reused so taps skip the TCP handshake
    HTTPClient http;
    char url[96];
    snprintf(url, sizeof(url), "http://%s:%u/tap", SERVER_HOST, SERVER_PORT);

    http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
    http.setTimeout(HTTP_READ_TIMEOUT_MS);
    http.setReuse(true);
    if (!http.begin(client, url)) {
        set_status("bad server url", 0xFB2C36);
        return;
    }
    http.addHeader("Content-Type", "application/json");
#ifdef SERVER_TOKEN
    if (SERVER_TOKEN[0] != '\0') http.addHeader("X-Deck-Token", SERVER_TOKEN);
#endif

    char body[96];
    snprintf(body, sizeof(body), "{\"key\":\"%s\"}", key);
    const uint32_t started = millis();
    int code = http.POST((uint8_t *)body, strlen(body));
    String payload;
    if (code > 0 && code < 300) payload = http.getString();
    http.end();
    Serial.printf("tap %s -> %d (%lu ms) %s\n", key, code, (unsigned long)(millis() - started),
                  payload.c_str());

    char msg[64];
    if (code > 0 && code < 300) {
        // Surface the result the action reported ("90%", "muted", ...) so a tap
        // can be confirmed by looking at the panel.
        const int probe = payload.indexOf("\"out\":\"");
        const int start = probe + 7;
        const int end = probe >= 0 ? payload.indexOf('"', start) : -1;
        const String result = end > start ? payload.substring(start, end) : String();
        if (result.length() > 0) {
            snprintf(msg, sizeof(msg), "%s: %s", key, result.c_str());
        } else {
            snprintf(msg, sizeof(msg), "OK: %s", key);
        }
        set_status(msg, 0x4ADE4A);
    } else if (code < 0 && http_error_text(code)) {
        snprintf(msg, sizeof(msg), "%s: %s", http_error_text(code), key);
        set_status(msg, 0xFB2C36);
    } else {
        snprintf(msg, sizeof(msg), "HTTP %d: %s", code, key);
        set_status(msg, 0xFB2C36);
    }
}

struct TapRequest {
    char key[TAP_KEY_MAX];
};

static QueueHandle_t tap_queue = nullptr;

// Tap callback: LVGL task context. Must not block, so it only enqueues.
static void tile_clicked_cb(lv_event_t *e) {
    const Tile *t = (const Tile *)lv_event_get_user_data(e);
    if (!t) return;
    Serial.printf("tap: %s\n", t->key);

    TapRequest req = {};
    strncpy(req.key, t->key, sizeof(req.key) - 1);
    if (xQueueSend(tap_queue, &req, 0) != pdTRUE) {
        Serial.printf("tap queue full, dropped %s\n", t->key);
        set_status("busy", 0xFCBB00);
    }
}

static void net_task(void *arg) {
    (void)arg;
    TapRequest req;
    for (;;) {
        if (xQueueReceive(tap_queue, &req, portMAX_DELAY) == pdTRUE) {
            post_tap(req.key);
        }
    }
}

// Header info: the PC's local clock (so the panel never disagrees with it) and
// the device's own WiFi address. Polls /health on its own client, off the tap
// path, so a slow probe cannot delay a key press.
static void clock_task(void *arg) {
    (void)arg;
    static WiFiClient client;
    char shown_ip[24] = "";
    vTaskDelay(pdMS_TO_TICKS(3000));  // let the WiFi association settle first
    for (;;) {
        if (WiFi.status() != WL_CONNECTED) {
            if (strcmp(shown_ip, "no wifi") != 0) {
                snprintf(shown_ip, sizeof(shown_ip), "no wifi");
                bool locked = ui_lock();
                if (ip_label) lv_label_set_text(ip_label, shown_ip);
                ui_unlock(locked);
            }
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        char ip[24];
        snprintf(ip, sizeof(ip), "%s", WiFi.localIP().toString().c_str());
        if (strcmp(ip, shown_ip) != 0) {
            strncpy(shown_ip, ip, sizeof(shown_ip) - 1);
            shown_ip[sizeof(shown_ip) - 1] = '\0';
            bool locked = ui_lock();
            if (ip_label) lv_label_set_text(ip_label, shown_ip);
            ui_unlock(locked);
        }

        HTTPClient http;
        char url[96];
        snprintf(url, sizeof(url), "http://%s:%u/health", SERVER_HOST, SERVER_PORT);
        http.setConnectTimeout(1500);
        http.setTimeout(1500);
        http.setReuse(true);
        if (http.begin(client, url)) {
            if (http.GET() == 200) {
                String body = http.getString();
                const int key = body.indexOf("\"hhmm\":\"");
                const int start = key + 8;
                const int end = key >= 0 ? body.indexOf('"', start) : -1;
                if (end > start) {
                    const String hhmm = body.substring(start, end);
                    if (clock_label) {
                        bool locked = ui_lock();
                        lv_label_set_text(clock_label, hhmm.c_str());
                        ui_unlock(locked);
                    }
                }
            }
            http.end();
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

#if ENABLE_NOTES_LAYER
// ---------- Diary fetch ----------
// GET a small text body. `status` receives the HTTP code (or negative
// HTTPClient error), which lets callers distinguish "diary disabled" (404)
// from a transient network failure.
static bool http_get_text(const char *path, char *out, size_t out_sz, int *status) {
    if (status) *status = 0;
    if (WiFi.status() != WL_CONNECTED) {
        if (status) *status = HTTPC_ERROR_NOT_CONNECTED;
        return false;
    }

    static WiFiClient client;  // diary polls run on loop(), taps on net_task
    HTTPClient http;
    char url[128];
    snprintf(url, sizeof(url), "http://%s:%u%s", SERVER_HOST, SERVER_PORT, path);

    http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
    http.setTimeout(HTTP_READ_TIMEOUT_MS + 500);
    http.setReuse(true);
    if (!http.begin(client, url)) return false;
#ifdef SERVER_TOKEN
    if (SERVER_TOKEN[0] != '\0') http.addHeader("X-Deck-Token", SERVER_TOKEN);
#endif

    int code = http.GET();
    if (status) *status = code;
    if (code < 200 || code >= 300) {
        http.end();
        return false;
    }

    String body = http.getString();
    http.end();
    if (body.length() == 0) return false;
    strncpy(out, body.c_str(), out_sz - 1);
    out[out_sz - 1] = '\0';
    return true;
}

// Replace the notes pane with a single status line (thread-safe).
static void notes_show_message(const char *msg, uint32_t color = 0x84A18C) {
    bool locked = ui_lock();
    lv_obj_clean(notes_scroll);
    lv_obj_set_flex_flow(notes_scroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *lab = lv_label_create(notes_scroll);
    lv_label_set_long_mode(lab, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lab, lv_obj_get_content_width(notes_scroll));
    lv_label_set_text(lab, msg);
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lab, lv_color_hex(color), 0);
    ui_unlock(locked);
}

static double parse_mtime(const char *json) {
    // tiny parser: find "mtime": and read the number
    const char *p = strstr(json, "\"mtime\":");
    if (!p) return 0;
    p += 8;
    while (*p == ' ') p++;
    return atof(p);
}

static void add_block(lv_obj_t *parent, const char *type, const char *text, bool done, int indent) {
    lv_obj_t *lab = lv_label_create(parent);
    lv_label_set_long_mode(lab, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lab, lv_obj_get_content_width(parent));

    if (strcmp(type, "h1") == 0 || strcmp(type, "h2") == 0) {
        lv_label_set_text(lab, text);
        lv_obj_set_style_text_font(lab, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(lab, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_pad_top(lab, 10, 0);
        lv_obj_set_style_pad_bottom(lab, 4, 0);
    } else if (strcmp(type, "h3") == 0) {
        lv_label_set_text(lab, text);
        lv_obj_set_style_text_font(lab, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(lab, lv_color_hex(0x4ADE4A), 0);
        lv_obj_set_style_pad_top(lab, 6, 0);
    } else if (strcmp(type, "todo") == 0) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s %s",
                 done ? LV_SYMBOL_OK : "[ ]",
                 text);
        lv_label_set_text(lab, buf);
        lv_obj_set_style_text_font(lab, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lab,
            lv_color_hex(done ? 0x5E7A66 : 0xD3ECD8), 0);
        if (done) {
            lv_obj_set_style_text_decor(lab, LV_TEXT_DECOR_STRIKETHROUGH, 0);
        }
        lv_obj_set_style_pad_left(lab, 8, 0);
    } else if (strcmp(type, "bullet") == 0) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s %s",
                 LV_SYMBOL_BULLET, text);
        lv_label_set_text(lab, buf);
        lv_obj_set_style_text_font(lab, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lab, lv_color_hex(0xA3C5AD), 0);
        lv_obj_set_style_pad_left(lab, 8 + indent * 20, 0);
    } else if (strcmp(type, "spacer") == 0) {
        lv_label_set_text(lab, "");
        lv_obj_set_height(lab, 8);
    } else {
        lv_label_set_text(lab, text);
        lv_obj_set_style_text_font(lab, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lab, lv_color_hex(0xD3ECD8), 0);
    }
}

static void refresh_notes_if_changed() {
    if (!notes_available || WiFi.status() != WL_CONNECTED) return;

    // Cheap mtime check first
    int status = 0;
    char mtime_buf[128];
    if (!http_get_text("/diary/today/mtime", mtime_buf, sizeof(mtime_buf), &status)) {
        if (status == 404) {
            notes_available = false;
            notes_show_message("Diary is disabled on the server (config.yaml)", 0xFCBB00);
            Serial.println("diary: 404, polling stopped until the tab is reopened");
        }
        return;
    }

    double mt = parse_mtime(mtime_buf);
    if (mt == last_diary_mtime) return;

    // Fetch full AST only when the file actually changed
    if (!http_get_text("/diary/today.json", notes_text_buf, sizeof(notes_text_buf), &status)) {
        if (status == 404) {
            notes_available = false;
            notes_show_message("Diary is disabled on the server (config.yaml)", 0xFCBB00);
        }
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, notes_text_buf);
    if (err) {
        Serial.printf("notes JSON parse error: %s\n", err.c_str());
        return;
    }
    last_diary_mtime = mt;

    bool locked = ui_lock();
    // Wipe previous children (we keep the scroll container itself)
    lv_obj_clean(notes_scroll);
    // Set a vertical flex layout for automatic stacking
    lv_obj_set_flex_flow(notes_scroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(notes_scroll, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(notes_scroll, 2, 0);

    JsonArray blocks = doc["blocks"].as<JsonArray>();
    for (JsonObject b : blocks) {
        const char *type = b["type"] | "paragraph";
        const char *text = b["text"] | "";
        bool done = b["done"] | false;
        int indent = b["indent"] | 0;
        add_block(notes_scroll, type, text, done, indent);
    }
    ui_unlock(locked);
}
#endif  // ENABLE_NOTES_LAYER

#if ENABLE_NOTES_LAYER
static void switch_layer(bool notes);

static void tab_clicked_cb(lv_event_t *e) {
    bool to_notes = (bool)(intptr_t)lv_event_get_user_data(e);
    switch_layer(to_notes);
}
#endif

#if ENABLE_NOTES_LAYER
// Segmented-control look: the live tab is a filled lime pill with a glow, the
// other one is an outlined ghost button.
static void style_tab(lv_obj_t *btn, bool active) {
    if (!btn) return;
    if (active) {
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x4ADE4A), 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(0x04100A), 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 10, 0);
        lv_obj_set_style_shadow_color(btn, lv_color_hex(0x4ADE4A), 0);
        lv_obj_set_style_shadow_opa(btn, LV_OPA_40, 0);
    } else {
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(0x84A18C), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x1A4A30), 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
    }
}
#endif  // ENABLE_NOTES_LAYER

static void build_header_and_tabs(lv_obj_t *scr) {
    lv_obj_t *header = lv_obj_create(scr);
    lv_obj_set_size(header, SCREEN_W, 44);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x030906), 0);
    lv_obj_set_style_border_width(header, 2, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(header, lv_color_hex(0x4ADE4A), 0);
    lv_obj_set_style_border_opa(header, LV_OPA_40, 0);
    lv_obj_set_style_pad_all(header, 4, 0);
    lv_obj_set_scrollbar_mode(header, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

#if ENABLE_NOTES_LAYER
    tab_deck_btn = lv_btn_create(header);
    lv_obj_set_size(tab_deck_btn, 110, 36);
    lv_obj_align(tab_deck_btn, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_radius(tab_deck_btn, 8, 0);
    lv_obj_set_style_border_width(tab_deck_btn, 0, 0);
    lv_obj_set_style_shadow_width(tab_deck_btn, 0, 0);
    lv_obj_add_event_cb(tab_deck_btn, tab_clicked_cb, LV_EVENT_CLICKED, (void *)0);
    lv_obj_t *td_lab = lv_label_create(tab_deck_btn);
    lv_label_set_text(td_lab, "Deck");
    lv_obj_set_style_text_font(td_lab, &lv_font_montserrat_16, 0);
    lv_obj_center(td_lab);

    tab_notes_btn = lv_btn_create(header);
    lv_obj_set_size(tab_notes_btn, 110, 36);
    lv_obj_align(tab_notes_btn, LV_ALIGN_LEFT_MID, 120, 0);
    lv_obj_set_style_radius(tab_notes_btn, 8, 0);
    lv_obj_set_style_border_width(tab_notes_btn, 0, 0);
    lv_obj_set_style_shadow_width(tab_notes_btn, 0, 0);
    lv_obj_add_event_cb(tab_notes_btn, tab_clicked_cb, LV_EVENT_CLICKED, (void *)1);
    lv_obj_t *tn_lab = lv_label_create(tab_notes_btn);
    lv_label_set_text(tn_lab, "Notes");
    lv_obj_set_style_text_font(tn_lab, &lv_font_montserrat_16, 0);
    lv_obj_center(tn_lab);
#else
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "WiFi Stream Deck");
    lv_obj_set_style_text_color(title, lv_color_hex(0xD3ECD8), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 8, 0);
#endif

    clock_label = lv_label_create(header);
    lv_label_set_text(clock_label, "--:--");
    lv_obj_set_style_text_color(clock_label, lv_color_hex(0xD3ECD8), 0);
    lv_obj_set_style_text_font(clock_label, &lv_font_montserrat_24, 0);
    lv_obj_align(clock_label, LV_ALIGN_CENTER, 0, 0);

    ip_label = lv_label_create(header);
    lv_label_set_text(ip_label, "---");
    lv_obj_set_style_text_color(ip_label, lv_color_hex(0x84A18C), 0);
    lv_obj_set_style_text_font(ip_label, &lv_font_montserrat_14, 0);
    lv_obj_align(ip_label, LV_ALIGN_TOP_RIGHT, 0, 0);

    status_label = lv_label_create(header);
    lv_label_set_text(status_label, "booting...");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x84A18C), 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

#if ENABLE_NOTES_LAYER
    style_tab(tab_deck_btn, true);
    style_tab(tab_notes_btn, false);
#endif
}

static void build_deck_layer(lv_obj_t *parent) {
    layer_deck = lv_obj_create(parent);
    lv_obj_set_size(layer_deck, SCREEN_W, SCREEN_H - 44);
    lv_obj_align(layer_deck, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_set_style_bg_color(layer_deck, lv_color_hex(0x030906), 0);
    lv_obj_set_style_border_width(layer_deck, 0, 0);
    lv_obj_set_style_pad_all(layer_deck, 0, 0);
    lv_obj_set_scrollbar_mode(layer_deck, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(layer_deck, LV_OBJ_FLAG_SCROLLABLE);

    const int GAP = 12;
    const int MARGIN_X = 12;
    const int INNER_TOP = 8;
    const int INNER_BOT = 8;
    const int COLS = 4;
    const int ROWS = 4;
    const int inner_h = (SCREEN_H - 44);
    const int TILE_W = (SCREEN_W - 2 * MARGIN_X - (COLS - 1) * GAP) / COLS;
    const int TILE_H = (inner_h - INNER_TOP - INNER_BOT - (ROWS - 1) * GAP) / ROWS;

    // Faint technical grid behind the keys, static so it costs one draw.
    for (int x = 0; x < SCREEN_W; x += 40) {
        lv_obj_t *ln = lv_obj_create(layer_deck);
        lv_obj_remove_style_all(ln);
        lv_obj_set_size(ln, 1, inner_h);
        lv_obj_set_pos(ln, x, 0);
        lv_obj_set_style_bg_color(ln, lv_color_hex(0x0F3D26), 0);
        lv_obj_set_style_bg_opa(ln, LV_OPA_50, 0);
        lv_obj_clear_flag(ln, LV_OBJ_FLAG_CLICKABLE);
    }
    for (int y = 0; y < inner_h; y += 40) {
        lv_obj_t *ln = lv_obj_create(layer_deck);
        lv_obj_remove_style_all(ln);
        lv_obj_set_size(ln, SCREEN_W, 1);
        lv_obj_set_pos(ln, 0, y);
        lv_obj_set_style_bg_color(ln, lv_color_hex(0x0F3D26), 0);
        lv_obj_set_style_bg_opa(ln, LV_OPA_50, 0);
        lv_obj_clear_flag(ln, LV_OBJ_FLAG_CLICKABLE);
    }

    for (int i = 0; i < TILE_COUNT; i++) {
        int col = i % COLS;
        int row = i / COLS;
        const Tile *t = &TILES[i];

        lv_obj_t *tile = lv_btn_create(layer_deck);
        lv_obj_set_size(tile, TILE_W, TILE_H);
        lv_obj_set_pos(tile,
                       MARGIN_X + col * (TILE_W + GAP),
                       INNER_TOP + row * (TILE_H + GAP));

        // Tech panel: flat near-black green (RGB565 gradients band visibly on
        // this panel), 1 px keyline in the key's colour and a soft glow.
        lv_obj_set_style_bg_color(tile, lv_color_hex(0x0B2417), 0);
        lv_obj_set_style_bg_grad_dir(tile, LV_GRAD_DIR_NONE, 0);
        lv_obj_set_style_bg_color(tile, lv_color_hex(0x143D28), LV_STATE_PRESSED);
        lv_obj_set_style_radius(tile, 10, 0);
        lv_obj_set_style_border_width(tile, 1, 0);
        lv_obj_set_style_border_color(tile, lv_color_hex(t->accent), 0);
        lv_obj_set_style_border_opa(tile, LV_OPA_40, 0);
        lv_obj_set_style_border_opa(tile, LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_set_style_shadow_color(tile, lv_color_hex(t->accent), 0);
        lv_obj_set_style_shadow_width(tile, 10, 0);
        lv_obj_set_style_shadow_spread(tile, 0, 0);
        lv_obj_set_style_shadow_opa(tile, LV_OPA_20, 0);
        lv_obj_set_style_shadow_width(tile, 18, LV_STATE_PRESSED);
        lv_obj_set_style_shadow_opa(tile, LV_OPA_50, LV_STATE_PRESSED);
        lv_obj_set_style_pad_all(tile, 0, 0);

        // Slim glowing rail on the left edge, like an indicator LED.
        lv_obj_t *accent = lv_obj_create(tile);
        lv_obj_set_size(accent, 4, TILE_H - 24);
        lv_obj_align(accent, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_bg_color(accent, lv_color_hex(t->accent), 0);
        lv_obj_set_style_border_width(accent, 0, 0);
        lv_obj_set_style_radius(accent, 2, 0);
        lv_obj_set_style_shadow_color(accent, lv_color_hex(t->accent), 0);
        lv_obj_set_style_shadow_width(accent, 8, 0);
        lv_obj_set_style_shadow_opa(accent, LV_OPA_40, 0);
        lv_obj_set_scrollbar_mode(accent, LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(accent, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *icon = lv_label_create(tile);
        lv_label_set_text(icon, t->icon);
        lv_obj_set_style_text_color(icon, lv_color_hex(t->accent), 0);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_32, 0);
        lv_obj_align(icon, LV_ALIGN_CENTER, 0, -16);

        lv_obj_t *lab = lv_label_create(tile);
        lv_label_set_text(lab, t->label);
        lv_obj_set_style_text_color(lab, lv_color_hex(0xA3C5AD), 0);
        lv_obj_set_style_text_letter_space(lab, 1, 0);
        lv_obj_set_style_text_font(lab, &lv_font_montserrat_16, 0);
        lv_obj_align(lab, LV_ALIGN_CENTER, 0, 24);

        lv_obj_add_event_cb(tile, tile_clicked_cb, LV_EVENT_CLICKED, (void *)t);
    }
}

#if ENABLE_NOTES_LAYER
static void build_notes_layer(lv_obj_t *parent) {
    layer_notes = lv_obj_create(parent);
    lv_obj_set_size(layer_notes, SCREEN_W, SCREEN_H - 44);
    lv_obj_align(layer_notes, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_set_style_bg_color(layer_notes, lv_color_hex(0x030906), 0);
    lv_obj_set_style_border_width(layer_notes, 0, 0);
    lv_obj_set_style_pad_all(layer_notes, 12, 0);
    lv_obj_set_scrollbar_mode(layer_notes, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(layer_notes, LV_OBJ_FLAG_SCROLLABLE);

    // Scrollable container
    notes_scroll = lv_obj_create(layer_notes);
    lv_obj_set_size(notes_scroll, SCREEN_W - 24, SCREEN_H - 44 - 24);
    lv_obj_align(notes_scroll, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(notes_scroll, lv_color_hex(0x07160E), 0);
    lv_obj_set_style_radius(notes_scroll, 10, 0);
    lv_obj_set_style_border_width(notes_scroll, 1, 0);
    lv_obj_set_style_border_color(notes_scroll, lv_color_hex(0x1A4A30), 0);
    lv_obj_set_style_pad_all(notes_scroll, 16, 0);
    lv_obj_set_scroll_dir(notes_scroll, LV_DIR_VER);

    // Initial placeholder; refresh_notes_if_changed() replaces it.
    lv_obj_t *placeholder = lv_label_create(notes_scroll);
    lv_label_set_text(placeholder, "Loading today's diary...");
    lv_obj_set_style_text_color(placeholder, lv_color_hex(0x84A18C), 0);
    lv_obj_set_style_text_font(placeholder, &lv_font_montserrat_16, 0);
}

static void switch_layer(bool to_notes) {
    notes_layer_active = to_notes;
    if (to_notes) {
        lv_obj_add_flag(layer_deck, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(layer_notes, LV_OBJ_FLAG_HIDDEN);
        style_tab(tab_deck_btn, false);
        style_tab(tab_notes_btn, true);
        // Re-arm the diary: fetch now, and retry even if it 404'd last time.
        last_diary_mtime = -1;
        notes_poll_at = 0;
        notes_available = true;
    } else {
        lv_obj_clear_flag(layer_deck, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(layer_notes, LV_OBJ_FLAG_HIDDEN);
        style_tab(tab_deck_btn, true);
        style_tab(tab_notes_btn, false);
    }
}
#endif  // ENABLE_NOTES_LAYER

static void build_ui() {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x030906), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    build_header_and_tabs(scr);
    build_deck_layer(scr);
#if ENABLE_NOTES_LAYER
    build_notes_layer(scr);
    switch_layer(false);
#endif
}

// ---------- WiFi ----------
static uint32_t wifi_retry_at = 0;
static uint8_t wifi_retry_count = 0;

static void wifi_begin() {
    WiFi.persistent(false);   // don't wear flash writing credentials every boot
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("crowpanel-deck");
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);     // lower tap latency
    WiFi.begin(WIFI_SSID, WIFI_PASS);
}

static void wifi_report_connected() {
    // The IP now lives in its own header label, so the status just says online.
    set_status("online", 0x4ADE4A);
}

static void wifi_connect() {
    Serial.printf("WiFi: connecting to %s...\n", WIFI_SSID);
    set_status("WiFi connecting...", 0xFCBB00);
    wifi_begin();

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifi_retry_count = 0;
        Serial.printf("WiFi ok: %s  IP=%s  RSSI=%d\n",
                      WIFI_SSID, WiFi.localIP().toString().c_str(), WiFi.RSSI());
        wifi_report_connected();
    } else {
        Serial.println("WiFi FAILED");
        wifi_retry_count = 3;  // force a full re-scan on the next tick
        wifi_retry_at = millis() + WIFI_RETRY_MS;
        set_status("WiFi failed", 0xFB2C36);
    }
}

// Called from loop(); never blocks longer than a reconnect kick.
static void wifi_tick(uint32_t now) {
    if (WiFi.status() == WL_CONNECTED) {
        if (wifi_retry_count) {
            wifi_retry_count = 0;
            wifi_report_connected();
        }
        return;
    }
    if ((int32_t)(now - wifi_retry_at) < 0) return;

    if (wifi_retry_count < 3) {
        Serial.println("WiFi dropped, reconnecting...");
        WiFi.reconnect();
        wifi_retry_at = now + WIFI_RETRY_MS;
    } else {
        Serial.println("WiFi reconnect failed, re-scanning...");
        WiFi.disconnect();
        wifi_begin();
        wifi_retry_at = now + 15000;
    }
    wifi_retry_count++;
    if (wifi_retry_count == 1) set_status("WiFi reconnecting...", 0xFCBB00);
}

static void halt_with_screen(const char *msg) {
    Serial.println(msg);
    gfx->fillScreen(BLACK);
    gfx->setTextColor(gfx->color565(0xEF, 0x44, 0x44));
    gfx->setTextSize(2);
    gfx->setCursor(20, 20);
    gfx->print(msg);
    for (;;) delay(1000);
}

// ---------- Setup ----------
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== WiFi Stream Deck boot ===");

    backlight_init();

    if (!gfx->begin()) {
        Serial.println("gfx->begin() FAILED");
        while (true) delay(1000);
    }
    gfx->fillScreen(BLACK);

    // Hook VSYNC so each refresh starts on a frame boundary. attachInterrupt only
    // enables the input buffer; pinMode(INPUT) here would disable the LCD's output.
    attachInterrupt(digitalPinToInterrupt(TFT_VSYNC), on_rgb_vsync, FALLING);
    vsync_sync = vblank();
    Serial.printf("VSYNC sync: %s\n", vsync_sync ? "on" : "off");

    if (!psramFound()) {
        halt_with_screen("PSRAM not detected - wrong board variant?");
    }

    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setClock(100000);

#if FORCE_TOUCH_RESET
    gt911_reset_via_expander();
#endif

    if (gt911_probe(0x5D)) {
        gt911_addr = 0x5D;
    } else if (gt911_probe(0x14)) {
        gt911_addr = 0x14;
    } else {
        Serial.println("GT911 silent, pulsing reset via PCA9557");
        gt911_reset_via_expander();
        if (gt911_probe(0x5D)) gt911_addr = 0x5D;
        else if (gt911_probe(0x14)) gt911_addr = 0x14;
        else Serial.println("GT911 still silent");
    }
    Serial.printf("GT911 @ 0x%02X\n", gt911_addr);

    lv_init();
    // Render in internal SRAM: it keeps the PSRAM bus free for the RGB panel's
    // scanout, which otherwise starves and blanks the screen mid-tap.
    lvgl_buf = (lv_color_t *)heap_caps_malloc(SCREEN_W * DRAW_BUF_LINES * sizeof(lv_color_t),
                                              MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!lvgl_buf) {
        lvgl_buf = (lv_color_t *)heap_caps_malloc(SCREEN_W * DRAW_BUF_LINES * sizeof(lv_color_t),
                                                  MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!lvgl_buf) halt_with_screen("out of memory for LVGL buffer");
    Serial.printf("LVGL draw buffer: %u B, free internal %u B\n",
                  (unsigned)(SCREEN_W * DRAW_BUF_LINES * sizeof(lv_color_t)),
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    lv_disp_draw_buf_init(&draw_buf, lvgl_buf, nullptr, SCREEN_W * DRAW_BUF_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_W;
    disp_drv.ver_res = SCREEN_H;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_cb;
    lv_indev_drv_register(&indev_drv);

    lvgl_mux = xSemaphoreCreateMutex();
    if (!lvgl_mux) halt_with_screen("failed to create LVGL mutex");
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8 * 1024, nullptr, 2, nullptr, 1);

    // Taps are handed off to a dedicated task on core 0 so HTTP never stalls UI.
    tap_queue = xQueueCreate(TAP_QUEUE_LEN, sizeof(TapRequest));
    if (!tap_queue) halt_with_screen("failed to create tap queue");
    xTaskCreatePinnedToCore(net_task, "net", 8 * 1024, nullptr, 1, nullptr, 0);

    xSemaphoreTake(lvgl_mux, portMAX_DELAY);
    build_ui();
    xSemaphoreGive(lvgl_mux);

    wifi_connect();
    // Only once the WiFi stack exists: an HTTPClient request from another task
    // during WiFi.begin() trips a lwIP "Invalid mbox" assert and reboots.
    xTaskCreatePinnedToCore(clock_task, "clock", 8 * 1024, nullptr, 1, nullptr, 0);
    Serial.println("setup done");
}

void loop() {
    uint32_t now = millis();

    wifi_tick(now);

#if ENABLE_NOTES_LAYER
    if (notes_layer_active && (int32_t)(now - notes_poll_at) >= 0) {
        notes_poll_at = now + NOTE_POLL_MS;
        refresh_notes_if_changed();
    }
#endif

    vTaskDelay(pdMS_TO_TICKS(200));
}
