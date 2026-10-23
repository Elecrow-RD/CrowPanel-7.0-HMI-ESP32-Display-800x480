# WiFi Stream Deck

A DIY WiFi-based Stream Deck for **Fusion Automate**, running on the **Elecrow CrowPanel 7" ESP32-S3** (800x480 RGB panel,
GT911 touch). The panel draws a 4x4 tile grid in LVGL and POSTs
`{"key": "<id>"}` over WiFi to a small companion HTTP server, which runs the
mapped shell command.

Two independent halves:

```
CrowPanel (firmware)                    Your PC (server)
--------------------                    ----------------
 LVGL 4x4 grid        -- POST /tap -->   FastAPI + actions.yaml
 GT911 touch          <-  JSON out   --   runs the command
 optional Notes tab   <-- /diary/* ----   diary files (opt-in)
```

The firmware owns **keys and labels**; the server owns **what runs**. Change
behaviour in `server/actions.yaml` without reflashing.

## Hardware support

| Board | Status |
|---|---|
| CrowPanel 7" **V1 / V2** | supported (GT911 on I2C directly) |
| CrowPanel 7" **V3.0** | supported - GT911 reset sits behind a PCA9557 expander at `0x18`; the firmware probes the touch controller first and only pulses the expander if it doesn't answer |
| CrowPanel 7" **Advance / ESP32-P4** | not supported |

Target: **ESP32-S3-WROOM-1-N4R8** (4 MB flash, 8 MB OPI PSRAM). The vendored
board variant `boards/esp32-s3-devkitc1-n4r8.json` is required - the stock
`esp32-s3-devkitc-1` bootloader never initialises octal PSRAM.

## Stack

- Arduino framework on PlatformIO (`espressif32@6.9.0`)
- LVGL 8 for UI, Arduino_GFX `1.4.7` for the RGB parallel panel (pinned)
- GT911 driven directly over I2C, no third-party touch library
- Partial-refresh rendering: 48-line draw buffer in internal SRAM, dirty
  rectangles blitted straight into the live framebuffer, VSYNC-aligned
- FastAPI + PyYAML on the server; no database

## Open in VS Code (Windows)

No embedded experience needed - do these in order.

### Before you start (once)

1. **Visual Studio Code** - https://code.visualstudio.com/
2. **PlatformIO IDE** - in VS Code open the Extensions view (`Ctrl+Shift+X`), search for `PlatformIO IDE`, click **Install**, and reload the window when it asks.
3. **Python 3** (3.9 or newer) - https://www.python.org/downloads/ - tick **Add python.exe to PATH** in the installer. This runs the PC half of the project.
4. **CH340 USB driver** - only if the panel's COM port never shows up (Windows 10/11 usually installs it on its own).

### Steps

1. **File → Open Folder** and select this `WiFi_Stream_Deck` folder - the one with `platformio.ini` directly inside it, not a parent folder.
2. If VS Code asks **"Do you trust the authors of this folder?"**, choose **Yes, I trust the authors**. PlatformIO cannot build otherwise.
3. Wait for PlatformIO to finish setting up. The first open downloads the ESP32 toolchain and libraries (several hundred MB, a few minutes) - it only happens once. The status bar shows progress at the bottom.
4. Create your credentials: in the Explorer pane right-click `include/secrets.h.example` → **Copy**, paste it into `include/`, rename the copy to `secrets.h`, then fill in `WIFI_SSID`, `WIFI_PASS` (2.4 GHz network only) and `SERVER_HOST` (your PC's LAN IP, e.g. `192.168.1.42` - never `localhost`).
   - Prefer the terminal? Run `copy include\secrets.h.example include\secrets.h` instead.
   - `server/config.yaml` is **optional** - skip it unless you want the Notes tab or a shared token.
5. Start the PC server **first**: **Terminal → New Terminal**, then
   ```powershell
   .\server\run.ps1
   ```
   It creates a virtual environment, installs the Python dependencies, prints your PC's LAN addresses (one of these must be `SERVER_HOST`) and then listens on port 8787. Leave this terminal running - taps show `server down` without it. If PowerShell refuses to run the script, double-click `server\start-server.cmd`.
6. If Windows Firewall pops up, allow Python on **Private** networks.
7. Plug the panel into its **CH340 UART USB port** - not the native USB port (GPIO19/20 are used by the touch controller there).
8. Pick the serial port: click the port selector in the bottom status bar and choose the panel's `COMx`. To list ports, open the Command Palette (`Ctrl+Shift+P`) and run **PlatformIO: Devices**. No port at all? See Troubleshooting.
9. Click the **checkmark** (Build), then the **right arrow** (Upload). Both run in the terminal panel; wait for `SUCCESS`.
10. Click the **plug icon** (Serial Monitor) and set the baud rate to **115200** to watch the boot log. You should see the panel's WiFi IP and then `online`.
11. Tap a tile: the header shows `OK: <key>` (or `server down` if step 5 is not running). Everything after this is just editing `server/actions.yaml` - no reflashing.

If the PlatformIO toolbar is missing, open the Command Palette and run **PlatformIO: Home**.

## Build and flash firmware directly (Windows)

1. Open a terminal in this project folder (the folder containing `platformio.ini`).
2. Build the firmware:

   ```powershell
   python -m platformio run -e crowpanel_7
   ```

3. Connect the display to the **CH340 UART USB port** and check its COM port in **PlatformIO → Devices** or Windows Device Manager.
4. Upload the firmware, replacing `COM4` with the correct port:

   ```powershell
   python -m platformio run -e crowpanel_7 -t upload --upload-port COM4
   ```

5. To read startup messages, open the PlatformIO Serial Monitor and set its speed to **115200**.

The first build downloads the required tools and libraries and may take a few minutes. You can also run `./flash.ps1`; it builds the firmware and prompts for the COM port. Use the UART port, not the native USB port: GPIO19/20 are used by the touch controller.

`python -m platformio` and `pio` are the same tool - use whichever works on your `PATH`. Common alternatives:

```powershell
pio run -e crowpanel_7 -t upload              # auto-detect the only connected board
pio run -e crowpanel_7 -t clean               # wipe build/ after weird link errors
pio device monitor -b 115200                  # serial log after flashing
```

A full copy-paste sequence (PowerShell, adapt `COM4`):

```powershell
py -m pip install platformio                                   # once
copy include\secrets.h.example include\secrets.h                # then edit it
python -m platformio run -e crowpanel_7                        # build
python -m platformio run -e crowpanel_7 -t upload --upload-port COM4
python -m platformio device monitor -b 115200                  # optional: watch boot log
```

macOS / Linux equivalents: `pipx install platformio`, then `pio run -t upload --upload-port /dev/cu.usbserial-XX` and `pio device monitor -b 115200`.

## Quick start (Windows)

```powershell
# 1. toolchain (once)
py -m pip install platformio

# 2. credentials
copy include\secrets.h.example include\secrets.h
notepad include\secrets.h     # WiFi (2.4 GHz only!) and SERVER_HOST; SERVER_TOKEN is optional

# 3. flash: plug the panel into the CH340 UART port, find its COM port
.\flash.ps1

# 4. server, in a second window (leave it running)
.\server\run.ps1
```

`run.ps1` prints the PC's LAN addresses; `SERVER_HOST` must be one of them.
On Windows you can also double-click `server\start-server.cmd`, which wraps
`run.ps1` and works even when PowerShell script execution is restricted.

The shipped `server/actions.yaml` is Windows-specific (`win\deckctl.exe ...`).
On macOS/Linux, replace those commands with the native equivalents shown
further down.

Windows will prompt to allow Python through the firewall - allow it on private
networks, or add the rule yourself:

```powershell
New-NetFirewallRule -DisplayName "StreamDeck 8787" -Direction Inbound `
  -LocalPort 8787 -Protocol TCP -Action Allow
```

## Quick start (macOS / Linux)

```bash
pipx install platformio
cp include/secrets.h.example include/secrets.h
$EDITOR include/secrets.h
pio run -t upload --upload-port /dev/cu.usbserial-XX
cd server && ./run.sh           # venv + uvicorn on :8787
```

Upload speed is pinned to `460800`; the CH340 bridge chokes at 921600.

## Configuration

### `include/secrets.h` (gitignored)

```c
#define WIFI_SSID "my-2g-network"   // ESP32-S3 is 2.4 GHz only
#define WIFI_PASS "..."
#define SERVER_HOST "192.168.1.42"  // LAN IP of the server, not localhost
#define SERVER_PORT 8787
#define SERVER_TOKEN ""             // must match auth.token; empty = no auth
```

### `server/config.yaml` (gitignored, hot-reloaded, optional)

Copy `server/config.yaml.example` to `server/config.yaml` only if you need to change a default. With no file the server runs with `diary.enabled: false`, no `auth.token` and `exec.timeout: 10`.

```yaml
diary:
  enabled: false                     # Notes tab backing store
  path: "~/obsidian/vault/diary"     # YYYY-MM-DD.md files
auth:
  token: ""                          # optional shared secret
exec:
  timeout: 10                        # seconds before a hung action is killed
```

### `server/actions.yaml` (hot-reloaded on every tap)

```yaml
# fire-and-forget
open_slack:
  run: "open -a Slack"

# toggle (server tracks state in memory)
mute_mic:
  toggle: true
  on:  "osascript -e 'set volume input volume 0'"
  off: "osascript -e 'set volume input volume 75'"
```

Bare `on:`/`off:` keys are YAML 1.1 booleans (`true`/`false`); the server
accepts both spellings, so quoting them is optional.

On Windows, commands run through `cmd.exe`, so wrap Windows APIs. The shipped
defaults call `win\deckctl.exe` (a small C# helper, ~60 ms per call) instead of
PowerShell, because a PowerShell process per tap costs 0.3-1.3 s and the panel's
tap queue overflows:

```yaml
mute:
  run: 'win\deckctl.exe mute'      # Core Audio, one toggle per tap
esc:
  run: 'win\deckctl.exe esc'
```

Rebuild the helper after editing `server/win/deckctl.cs`:

```powershell
.\server\win\build-deckctl.ps1
```

## Project layout

```
src/main.cpp              firmware: UI, touch, WiFi, taps, Notes tab
include/secrets.h         WiFi + server address (gitignored; .example committed)
include/lv_conf.h         LVGL build config (fonts, 48 KB heap)
boards/                   vendored esp32-s3-devkitc1-n4r8 board definition
platformio.ini            env:crowpanel_7
flash.ps1                 build + upload, asks for the COM port
server/server.py          FastAPI app (tap dispatch, diary, /health, /state)
server/actions.yaml       key -> command map (hot-reloaded)
server/config.yaml        diary/auth/exec settings (optional, gitignored; .example committed)
server/win/deckctl.cs     C# helper behind the Windows actions
server/win/deckctl.exe    compiled helper (~60 ms per call)
server/run.ps1|run.sh     venv + uvicorn on :8787
server/start-server.cmd   double-click launcher for Windows
server/tests/             pytest suite
```

## Rendering

The 800x480 panel is RGB565 over a parallel bus at ~32 Hz. LVGL uses partial
refresh: a 48-line draw buffer in internal SRAM, with each dirty rectangle
blitted straight into the live framebuffer by `draw16bitRGBBitmap()` and gated
on the LCD's VSYNC (GPIO 40) so no frame is torn. Only the tapped tile is
repainted, which is why a tap costs ~1 ms of SPI-less blitting rather than a
full-screen redraw.

## Server API

| Endpoint | Purpose |
|---|---|
| `POST /tap` `{"key": "<id>"}` | run the mapped action, returns `rc`, `timed_out`, `out` |
| `GET /health` | `{"ok":true,"hhmm":"7:42 PM","date":...}` - also the panel's clock source |
| `GET /state` | current toggle states |
| `GET /` | service info: action keys, features, auth flag |
| `GET /diary/today`, `/diary/today/mtime`, `/diary/today.json` | Notes tab (404 when `diary.enabled` is false) |

`/tap` is dispatched from the panel's tap queue (8 deep) on a dedicated task, so
a slow action never freezes the touch UI.

## Look and feel

The UI follows the shop.fusionautomate.in palette and a dark "high-tech" style:
near-black green background (`#030906`) with a faint 40 px technical grid, flat
deep green key cards, a 1 px keyline and a soft glow in the key's own accent
colour, a slim LED-like rail on the left edge, and uppercase letter-spaced
labels. Cards are deliberately flat: RGB565 only has ~32 green levels, so any
gradient bands visibly on this panel.

| Token | Hex | Used for |
|---|---|---|
| Background | `#030906` | screen, header |
| Grid | `#0F3D26` @ 50% | technical grid |
| Card | `#0B2417` (pressed `#143D28`) | keys |
| Lime | `#4ADE4A` | media, start, live tab, success |
| Teal | `#3393E0` | volume, copy/paste |
| Sage | `#84A18C` | esc, desktop, window/tab switcher |
| Amber | `#FCBB00` | snip, terminal, warnings |
| Red | `#FB2C36` | mic, lock, errors |
| Mint | `#D3ECD8` / `#A3C5AD` | clock, labels |

All of it lives in the `C_*` constants and `TILES[]` at the top of
`src/main.cpp`; change a constant and reflash.

## Tiles

The panel renders a 4x4 grid (16 tiles). Every tile key must exist in
`server/actions.yaml`.

| Tile | Key | Windows default |
|---|---|---|
| Mute | `mute` | Core Audio output mute: one toggle per tap |
| Play/Pause | `play_pause` | media key (`VK_MEDIA_PLAY_PAUSE`) |
| Next | `next_track` | media key (`VK_MEDIA_NEXT_TRACK`) |
| Vol - / Vol + | `vol_down` / `vol_up` | Core Audio master volume, 2% per tap |
| Mic | `mic_mute` | Core Audio capture mute (default microphone) |
| Esc | `esc` | `VK_ESCAPE` |
| Snip | `snip` | Win+Shift+S (Snipping Tool) |
| Copy / Paste | `copy` / `paste` | Ctrl+C / Ctrl+V |
| Desktop | `show_desktop` | Win+D |
| Terminal | `open_terminal` | `wt.exe` |
| Lock | `lock_screen` | `rundll32 user32.dll,LockWorkStation` |
| Start | `start` | Win key (Start menu) |
| Alt+Tab | `alt_tab` | window switcher |
| Ctrl+Tab | `ctrl_tab` | tab switcher (browser/editor) |

The screen dims after 60 s without a tap and wakes on the next touch.

Edit the tile (label/icon/colour) in `TILES[]` in `src/main.cpp` and reflash;
edit the command in `actions.yaml` and just save. Refreshes are partial: only the
tapped tile's rectangle is redrawn, VSYNC-aligned.

## Notes tab (opt-in)

A second tab renders today's markdown (`YYYY-MM-DD.md`) as styled LVGL labels.
The panel polls `/diary/today/mtime` every 3 s while the tab is visible and only
refetches the block AST when the file changes.

- Firmware side: `-DENABLE_NOTES_LAYER=0` in `platformio.ini` compiles the tab
  out entirely (smaller, deck-only).
- Server side: `diary.enabled` in `config.yaml`. When it is off, `/diary/*`
  returns 404 and the panel stops polling and shows a single status line.

## Header

Left: Deck / Notes tabs. Centre: clock. Right: the panel's WiFi IP, with the
last tap result underneath.

The clock is **IST, 12-hour with AM/PM**, and it is computed on the PC, not the
ESP32: the panel polls `/health` every 5 s and mirrors `hhmm` from the server
(`UTC+05:30`, fixed offset - India has no DST), so both always agree without NTP
or timezone config on the device. If the server is down the last known time
stays on screen, and the IP falls back to `no wifi` when the panel loses the
network.

| Text | Meaning |
|---|---|
| `OK: <key>` | server returned 2xx |
| `<key>: <out>` | 2xx plus the command's stdout, e.g. `vol_up: 92%` |
| `server down` | connection refused - `run.ps1` not running |
| `timeout` | command ran but answered too slowly (use fire-and-forget, e.g. `start ""`) |
| `no network` | panel lost WiFi |
| `HTTP 404` | server is up, key missing from `actions.yaml` |
| `online` | idle, connected |

## Tests

```powershell
cd server
.\.venv\Scripts\python.exe -m pip install -r requirements-dev.txt
.\.venv\Scripts\python.exe -m pytest -q
```

Covers tap dispatch, toggle state, YAML hot-reload and last-good fallback,
command timeout/kill, optional auth, diary rendering/UTF-8, and the markdown
block parser.

## Security

`/tap` runs whatever is in `actions.yaml` via a shell, with no authentication by
default. That is fine on a trusted home LAN and a bad idea anywhere else.
Mitigations, in order of effort:

1. Set a token: `auth.token` in `config.yaml` **and** `SERVER_TOKEN` in
   `include/secrets.h`, then reflash. `POST /tap` without `X-Deck-Token` gets 401.
2. Keep the server on a VLAN/SSID that only the panel can reach.
3. Never expose port 8787 to the internet.

## Troubleshooting

| Symptom | Cause |
|---|---|
| `WiFi failed` on boot | wrong band (5 GHz), wrong password, or client isolation on a corporate AP |
| clock stuck at `--:--` | the panel cannot reach `/health`: server down, wrong `SERVER_HOST`, or firewall |
| IP label shows `no wifi` | association dropped; the panel retries on its own |
| taps show `server down` | server not running, or firewall blocking 8787 |
| a key does nothing and logs `HTTP 404` | key missing from `actions.yaml` (or misspelled) |
| taps show `timeout` but the action works | the command blocks; background it (`start "" ...`) or raise `exec.timeout` |
| blank/white screen | PSRAM not initialised - wrong board variant, or wrong `board_upload.flash_size` |
| touch dead on V3.0 | set `-DFORCE_TOUCH_RESET=1` in `platformio.ini` and reflash |
| no COM port when plugging in | install the WCH CH340 driver |
| flashing fails with `PermissionError` / `No serial data received` | you are on the native USB port; GPIO19/20 are the USB pins and are used for touch I2C. Flash through the UART/CH340 port instead |
