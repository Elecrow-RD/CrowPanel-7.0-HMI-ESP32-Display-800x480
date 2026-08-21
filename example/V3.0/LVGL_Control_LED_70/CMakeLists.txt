# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

if(WIN32)
    set(CMAKE_NINJA_FORCE_RESPONSE_FILE ON CACHE BOOL "Use response files to avoid Windows command-line limits" FORCE)
endif()

set(EXTRA_COMPONENT_DIRS 
        components/ui 
        components/gt911-arduino-main 
        components/Arduino_GFX-master 
        components/DHT20 
        components/PCA9557
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Workaround for esp-dsp internal compiler error with GCC 14.2.0
# The error occurs in dspi_conv_f32_ansi.c due to compiler bug
# Apply workaround flags to esp-dsp component only
set(ESP_DSP_COMPILE_OPTIONS "-fno-strict-aliasing;-O1")
idf_build_set_property(__ESP_DSP_COMPILE_OPTIONS "${ESP_DSP_COMPILE_OPTIONS}")

project(CrowPanel_ESP32_7.0)
