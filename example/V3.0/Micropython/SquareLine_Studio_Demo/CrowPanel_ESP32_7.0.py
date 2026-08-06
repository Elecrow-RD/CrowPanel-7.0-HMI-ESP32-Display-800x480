#Make by Elecrow
#Web：www.elecrow.com
import lvgl as lv
import lv_utils
import tft_config
import time
import fs_driver
import gt911
from machine import Pin, I2C
import ui_images
from DHT20 import DHT20

WIDTH = 800
HEIGHT = 480

# tft driver
tft = tft_config.config()

pin38 = Pin(38, Pin.OUT)
pin38.value(0)

# GT911 and DHT20 share this bus. 100 kHz is more stable on the long
# CrowPanel traces and avoids intermittent ESP32-S3 I2C timing errors.
i2c = I2C(1, scl=Pin(20), sda=Pin(19), freq=100000)

sensor = DHT20(i2c)

try:
    tp = gt911.GT911(i2c, width=800, height=480, addr=0x14)
    tp.set_rotation(tp.ROTATION_INVERTED)
except OSError as e:
    print(f"Touch init failed: {e}; continuing without touch")
    tp = None

lv.init()

if not lv_utils.event_loop.is_running():
    event_loop = lv_utils.event_loop()
    print(event_loop.is_running())

# create a display 0 buffer
disp_buf0 = lv.disp_draw_buf_t()
BUFFER_LINES = 100
buf1_0 = bytearray(WIDTH * BUFFER_LINES * lv.color_t.__SIZE__)
buf2_0 = bytearray(WIDTH * BUFFER_LINES * lv.color_t.__SIZE__)
disp_buf0.init(buf1_0, buf2_0, WIDTH * BUFFER_LINES)

# register display driver
disp_drv = lv.disp_drv_t()
disp_drv.init()
disp_drv.draw_buf = disp_buf0
disp_drv.flush_cb = tft.flush
disp_drv.hor_res = WIDTH
disp_drv.ver_res = HEIGHT
disp0 = disp_drv.register()
lv.disp_t.set_default(disp0)

# touch driver init
if tp is not None:
    touch_was_pressed = False

    def touch_read_no_redraw(drv, data):
        global touch_was_pressed
        tp.lvgl_read(drv, data)
        pressed = data.state == lv.INDEV_STATE.PRESSED

        # Handle the two fixed buttons directly, only on the initial press.
        # Keeping LVGL in RELEASED state prevents button-state redraws and
        # eliminates tearing caused by unsynchronised partial display flushes.
        if pressed and not touch_was_pressed:
            x = data.point.x
            y = data.point.y
            if 565 <= x <= 650 and 107 <= y <= 192:
                pin38.value(1)
            elif 562 <= x <= 647 and 237 <= y <= 322:
                pin38.value(0)

        touch_was_pressed = pressed
        data.state = lv.INDEV_STATE.RELEASED

    indev_drv = lv.indev_drv_t()
    indev_drv.init()
    indev_drv.disp = disp0
    indev_drv.type = lv.INDEV_TYPE.POINTER
    indev_drv.read_cb = touch_read_no_redraw
    indev = indev_drv.register()

dispp = lv.disp_get_default()
theme = lv.theme_default_init(dispp, lv.palette_main(lv.PALETTE.BLUE), lv.palette_main(lv.PALETTE.RED), False, lv.font_default())
dispp.set_theme(theme)

def SetFlag(obj, flag, value):
    if value:
        obj.add_flag(flag)
    else:
        obj.clear_flag(flag)
    return

_ui_comp_table = {}
_ui_comp_prev = None
_ui_name_prev = None
_ui_child_prev = None
_ui_comp_table.clear()

def _ui_comp_del_event(e):
    target = e.get_target()
    _ui_comp_table[id(target)].remove()

def ui_comp_get_child(comp, child_name):
    return _ui_comp_table[id(comp)][child_name]

def ui_comp_get_root_from_child(child, compname):
    for component in _ui_comp_table:
        if _ui_comp_table[component]["_CompName"] == compname:
            for part in _ui_comp_table[component]:
                if id(_ui_comp_table[component][part]) == id(child):
                    return _ui_comp_table[component]
    return None

def SetBarProperty(target, id, val):
    if id == 'Value_with_anim':
        target.set_value(val, lv.ANIM.ON)
    if id == 'Value':
        target.set_value(val, lv.ANIM.OFF)
    return

def SetPanelProperty(target, id, val):
    if id == 'Position_X':
        target.set_x(val)
    if id == 'Position_Y':
        target.set_y(val)
    if id == 'Width':
        target.set_width(val)
    if id == 'Height':
        target.set_height(val)
    return

def SetDropdownProperty(target, id, val):
    if id == 'Selected':
        target.set_selected(val)
    return

def SetImageProperty(target, id, val, val2):
    if id == 'Image':
        target.set_src(val)
    if id == 'Angle':
        target.set_angle(val2)
    if id == 'Zoom':
        target.set_zoom(val2)
    return

def SetLabelProperty(target, id, val):
    if id == 'Text':
        target.set_text(val)
    return

def SetRollerProperty(target, id, val):
    if id == 'Selected':
        target.set_selected(val, lv.ANIM.OFF)
    if id == 'Selected_with_anim':
        target.set_selected(val, lv.ANIM.ON)
    return

def SetSliderProperty(target, id, val):
    if id == 'Value_with_anim':
        target.set_value(val, lv.ANIM.ON)
    if id == 'Value':
        target.set_value(val, lv.ANIM.OFF)
    return

def ChangeScreen(src, fademode, speed, delay):
    lv.scr_load_anim(src, fademode, speed, delay, False)
    return

def DeleteScreen(src):
    return

def IncrementArc(trg, val):
    trg.set_value(trg.get_value() + val)
    lv.event_send(trg, lv.EVENT.VALUE_CHANGED, None)
    return

def IncrementBar(trg, val, anim):
    trg.set_value(trg.get_value() + val, anim)
    return

def IncrementSlider(trg, val, anim):
    trg.set_value(trg.get_value() + val, anim)
    lv.event_send(trg, lv.EVENT.VALUE_CHANGED, None)
    return

def KeyboardSetTarget(keyboard, textarea):
    keyboard.set_textarea(textarea)
    return

def ModifyFlag(obj, flag, value):
    if value == "TOGGLE":
        if obj.has_flag(flag):
            obj.clear_flag(flag)
        else:
            obj.add_flag(flag)
        return
    if value == "ADD":
        obj.add_flag(flag)
    else:
        obj.clear_flag(flag)
    return

def ModifyState(obj, state, value):
    if value == "TOGGLE":
        if obj.has_state(state):
            obj.clear_state(state)
        else:
            obj.add_state(state)
        return
    if value == "ADD":
        obj.add_state(state)
    else:
        obj.clear_state(state)
    return

def set_opacity(obj, v):
    obj.set_style_opa(v, lv.STATE.DEFAULT | lv.PART.MAIN)
    return

def SetTextValueArc(trg, src, prefix, postfix):
    trg.set_text(prefix + str(src.get_value()) + postfix)
    return

def SetTextValueSlider(trg, src, prefix, postfix):
    trg.set_text(prefix + str(src.get_value()) + postfix)
    return

def SetTextValueChecked(trg, src, txton, txtoff):
    if src.has_state(lv.STATE.CHECKED):
        trg.set_text(txton)
    else:
        trg.set_text(txtoff)
    return

def StepSpinbox(trg, val):
    if val == 1:
        trg.increment()
    if val == -1:
        trg.decrement()
    lv.event_send(trg, lv.EVENT.VALUE_CHANGED, None)
    return

# COMPONENTS
# COMPONENT Button2
def ui_Button2_create(comp_parent):
    cui_Button2 = lv.btn(comp_parent)
    cui_Button2.set_width(100)
    cui_Button2.set_height(50)
    cui_Button2.set_x(4)
    cui_Button2.set_y(32)
    cui_Button2.set_align(lv.ALIGN.CENTER)
    SetFlag(cui_Button2, lv.obj.FLAG.SCROLLABLE, False)
    SetFlag(cui_Button2, lv.obj.FLAG.SCROLL_ON_FOCUS, True)
    _ui_comp_table[id(cui_Button2)] = {"Button2": cui_Button2, "_CompName": "Button2"}
    return cui_Button2

ui____initial_actions0 = lv.obj()

def Button1_eventhandler(event_struct):
    event = event_struct.code
    if event == lv.EVENT.CLICKED and True:
        pin38.value(1)
    return

def Button2_eventhandler(event_struct):
    event = event_struct.code
    if event == lv.EVENT.CLICKED and True:
        pin38.value(0)
    return

ui_Screen1 = lv.obj()
SetFlag(ui_Screen1, lv.obj.FLAG.SCROLLABLE, False)
ui_Screen1.set_style_bg_color(lv.color_hex(0xEFF6DB), lv.PART.MAIN | lv.STATE.DEFAULT)
ui_Screen1.set_style_bg_opa(255, lv.PART.MAIN | lv.STATE.DEFAULT)
ui_Screen1.set_style_bg_img_src(ui_images.TemporaryImage, lv.PART.MAIN | lv.STATE.DEFAULT)

ui_Button1 = lv.btn(ui_Screen1)
ui_Button1.set_width(85)
ui_Button1.set_height(85)
ui_Button1.set_x(208)
ui_Button1.set_y(-90)
ui_Button1.set_align(lv.ALIGN.CENTER)
SetFlag(ui_Button1, lv.obj.FLAG.SCROLLABLE, False)
SetFlag(ui_Button1, lv.obj.FLAG.SCROLL_ON_FOCUS, True)
ui_Button1.set_style_bg_color(lv.color_hex(0xFFFFFF), lv.PART.MAIN | lv.STATE.DEFAULT)
ui_Button1.set_style_bg_opa(255, lv.PART.MAIN | lv.STATE.DEFAULT)
ui_Button1.set_style_bg_img_src(ui_images.ui_img_on_png, lv.PART.MAIN | lv.STATE.DEFAULT)
ui_Button1.set_style_bg_color(lv.color_hex(0xFFFFFF), lv.PART.MAIN | lv.STATE.PRESSED)
ui_Button1.set_style_bg_opa(255, lv.PART.MAIN | lv.STATE.PRESSED)
ui_Button1.set_style_bg_img_src(ui_images.ui_img_on_png, lv.PART.MAIN | lv.STATE.PRESSED)

ui_Button1.add_event_cb(Button1_eventhandler, lv.EVENT.ALL, None)

ui_Button2 = lv.btn(ui_Screen1)
ui_Button2.set_width(85)
ui_Button2.set_height(85)
ui_Button2.set_x(205)
ui_Button2.set_y(40)
ui_Button2.set_align(lv.ALIGN.CENTER)
SetFlag(ui_Button2, lv.obj.FLAG.SCROLLABLE, False)
SetFlag(ui_Button2, lv.obj.FLAG.SCROLL_ON_FOCUS, True)
ui_Button2.set_style_bg_color(lv.color_hex(0xFBF9F9), lv.PART.MAIN | lv.STATE.DEFAULT)
ui_Button2.set_style_bg_opa(255, lv.PART.MAIN | lv.STATE.DEFAULT)
ui_Button2.set_style_bg_img_src(ui_images.ui_img_off_png, lv.PART.MAIN | lv.STATE.DEFAULT)
ui_Button2.set_style_bg_color(lv.color_hex(0xFBF9F9), lv.PART.MAIN | lv.STATE.PRESSED)
ui_Button2.set_style_bg_opa(255, lv.PART.MAIN | lv.STATE.PRESSED)
ui_Button2.set_style_bg_img_src(ui_images.ui_img_off_png, lv.PART.MAIN | lv.STATE.PRESSED)

ui_Button2.add_event_cb(Button2_eventhandler, lv.EVENT.ALL, None)

ui_Image1 = lv.img(ui_Screen1)
ui_Image1.set_src(ui_images.ui_img_background1_png)
ui_Image1.set_width(lv.SIZE.CONTENT)
ui_Image1.set_height(lv.SIZE.CONTENT)
ui_Image1.set_x(-90)
ui_Image1.set_y(-14)
ui_Image1.set_align(lv.ALIGN.CENTER)
SetFlag(ui_Image1, lv.obj.FLAG.ADV_HITTEST, True)
SetFlag(ui_Image1, lv.obj.FLAG.SCROLLABLE, False)

ui_Label1 = lv.label(ui_Screen1)
ui_Label1.set_text("--")
ui_Label1.set_width(50)
ui_Label1.set_height(30)
ui_Label1.set_x(-49)
ui_Label1.set_y(-89)
ui_Label1.set_align(lv.ALIGN.CENTER)
ui_Label1.set_style_text_color(lv.color_hex(0xFDF9F9), lv.PART.MAIN | lv.STATE.DEFAULT)
ui_Label1.set_style_text_opa(255, lv.PART.MAIN | lv.STATE.DEFAULT)
ui_Label1.set_style_text_font(lv.font_montserrat_16, lv.PART.MAIN | lv.STATE.DEFAULT)

ui_Label2 = lv.label(ui_Screen1)
ui_Label2.set_text("--")
ui_Label2.set_width(50)
ui_Label2.set_height(30)
ui_Label2.set_x(-48)
ui_Label2.set_y(60)
ui_Label2.set_align(lv.ALIGN.CENTER)
ui_Label2.set_style_text_color(lv.color_hex(0xFDFBFB), lv.PART.MAIN | lv.STATE.DEFAULT)
ui_Label2.set_style_text_opa(255, lv.PART.MAIN | lv.STATE.DEFAULT)
ui_Label2.set_style_text_font(lv.font_montserrat_16, lv.PART.MAIN | lv.STATE.DEFAULT)

lv.scr_load(ui_Screen1)


time.sleep(2)  


try:
    temp = sensor.dht20_temperature()
    hum = sensor.dht20_humidity()
    print(f"Initial: Temp={temp:.1f}C, Hum={hum:.1f}%")
    ui_Label1.set_text(f"{round(temp)}")
    ui_Label2.set_text(f"{round(hum)}")
except Exception as e:
    print(f"Initial read failed: {e}")
    ui_Label1.set_text("--")
    ui_Label2.set_text("--")

last_read_time = time.ticks_ms()

while True:
    try:
        current_time = time.ticks_ms()

        
        if time.ticks_diff(current_time, last_read_time) >= 2000:
            temp = sensor.dht20_temperature()
            hum = sensor.dht20_humidity()

            
            if -40 <= temp <= 80 and 0 <= hum <= 100:
                ui_Label1.set_text(f"{round(temp)}")
                ui_Label2.set_text(f"{round(hum)}")
                print(f"Temperature: {temp:.1f} C, Humidity: {hum:.1f} %")
            else:
                print(f"Invalid data: Temp={temp}, Hum={hum}")

            last_read_time = current_time

        
        time.sleep_ms(20)

    except OSError as e:
        print(f"Failed to read sensor: {e}")
        time.sleep_ms(500)
    except Exception as e:
        print(f"Unexpected error: {e}")
        time.sleep_ms(500)
