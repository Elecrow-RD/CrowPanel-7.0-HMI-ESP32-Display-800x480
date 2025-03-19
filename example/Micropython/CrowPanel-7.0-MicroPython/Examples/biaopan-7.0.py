#Make by Elecrow
#Web：www.elecrow.com
import lvgl as lv
import lv_utils
import tft_config
import time
import fs_driver
import gt911
from machine import Pin, I2C

WIDTH = 800
HEIGHT = 480


# tft drvier
tft = tft_config.config()

# touch drvier
i2c = I2C(1, scl=Pin(20), sda=Pin(19), freq=400000)

tp = gt911.GT911(i2c, width=800, height=480)
tp.set_rotation(tp.ROTATION_NORMAL)

lv.init()

if not lv_utils.event_loop.is_running():
    event_loop=lv_utils.event_loop()
    print(event_loop.is_running())

# create a display 0 buffer
disp_buf0 = lv.disp_draw_buf_t()
buf1_0 = bytearray(WIDTH * 50)
disp_buf0.init(buf1_0, None, len(buf1_0) // lv.color_t.__SIZE__)

# register display driver
disp_drv = lv.disp_drv_t()
disp_drv.init()
disp_drv.draw_buf = disp_buf0
disp_drv.flush_cb = tft.flush
disp_drv.hor_res = WIDTH
disp_drv.ver_res = HEIGHT
# disp_drv.user_data = {"swap": 0}
disp0 = disp_drv.register()
lv.disp_t.set_default(disp0)

# touch driver init
indev_drv = lv.indev_drv_t()
indev_drv.init()
indev_drv.disp = disp0
indev_drv.type = lv.INDEV_TYPE.POINTER
indev_drv.read_cb = tp.lvgl_read
indev = indev_drv.register()



# 1. Create a display screen. Will need to display the component added to the screen to display
scr = lv.obj()  # scr====> screen
fs_drv = lv.fs_drv_t()
fs_driver.fs_register(fs_drv, 'S')
scr = lv.scr_act()
scr.clean()


# 2. Encapsulate the component to display
class MyWidget():
    def __init__(self, scr):
        # 1. Create the dashboard object
        self.meter = lv.meter(scr)
        self.meter.center()
        self.meter.set_size(200, 200)  # width: 200 height: 200

        # 2. To create calibration object
        scale = self.meter.add_scale()

        self.meter.set_scale_ticks(scale, 51, 2, 10, lv.palette_main(lv.PALETTE.GREY))

        self.meter.set_scale_major_ticks(scale, 10, 4, 15, lv.color_black(), 20)

        # 3. Add warning scale line

        blue_arc = self.meter.add_arc(scale, 2, lv.palette_main(lv.PALETTE.BLUE), 0)
        self.meter.set_indicator_start_value(blue_arc, 0)
        self.meter.set_indicator_end_value(blue_arc, 20)


        blue_arc_scale = self.meter.add_scale_lines(scale, lv.palette_main(lv.PALETTE.BLUE), lv.palette_main(lv.PALETTE.BLUE), False, 0)
        self.meter.set_indicator_start_value(blue_arc_scale, 0)
        self.meter.set_indicator_end_value(blue_arc_scale, 20)


        red_arc = self.meter.add_arc(scale, 2, lv.palette_main(lv.PALETTE.RED), 0)
        self.meter.set_indicator_start_value(red_arc, 80)
        self.meter.set_indicator_end_value(red_arc, 100)


        red_arc_scale = self.meter.add_scale_lines(scale, lv.palette_main(lv.PALETTE.RED), lv.palette_main(lv.PALETTE.RED), False, 0)
        self.meter.set_indicator_start_value(red_arc_scale, 80)
        self.meter.set_indicator_end_value(red_arc_scale, 100)

        # 4. meter needle
        self.indic = self.meter.add_needle_line(scale, 4, lv.palette_main(lv.PALETTE.GREY), -10)

        # 5. Creating animated objects
        a = lv.anim_t()
        a.init()
        a.set_var(self.indic)
        a.set_values(0, 100)
        a.set_time(2000)
        a.set_repeat_delay(100)
        a.set_playback_time(500)
        a.set_playback_delay(100)
        a.set_repeat_count(lv.ANIM_REPEAT.INFINITE)
        a.set_custom_exec_cb(self.set_value)
        lv.anim_t.start(a)

    def set_value(self, anmi_obj, value):
        """Animation callbacks"""
        self.meter.set_indicator_value(self.indic, value)


# 3. Create the component to display
MyWidget(scr)

# 4. Displays the contents of the screen object
lv.scr_load(scr)


# ------------------------------ Guard dog to restart ESP32 equipment --start------------------------
try:
    from machine import WDT
    wdt = WDT(timeout=1000)  # enable it with a timeout of 2s
    print("Hint: Press Ctrl+C to end the program")
    while True:
        wdt.feed()
        time.sleep(0.9)
except KeyboardInterrupt as ret:
    print("The program stopped running, ESP32 has restarted...")
    tft.deinit()
    time.sleep(10)
# ------------------------------ Guard dog to restart ESP32 equipment --stop-------------------------
