idf_component_register(
    SRCS "CrowPanel_ESP32_7.0.cpp"
    INCLUDE_DIRS "."
    REQUIRES arduino-esp32 lvgl-9 ui gt911-arduino-main Arduino_GFX-master DHT20 PCA9557
)
