file(GLOB_RECURSE LIB_SRCS
        "..//components/DHT20/*.CPP"
)

idf_component_register(SRCS "CrowPanel_ESP32_7.0.cpp"
                    ${LIB_SRCS}
                    INCLUDE_DIRS "."
                    "../components/arduino-esp32-master"                 
                    "../components/lvgl-3"
                    "../components/UI"
                    "../components/gt911-arduino-main" 
                    "../components/Arduino_GFX-master"
                    "../components/LovyanGFX-master"   
                    "../components/DHT20"               
                    )
