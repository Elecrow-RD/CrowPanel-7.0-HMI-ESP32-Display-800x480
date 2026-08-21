# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/bootloader/subproject")
  file(MAKE_DIRECTORY "J:/Advance_Code_New/NEW_ESP32/esp-idf/.espressif/v5.5.4/esp-idf/components/bootloader/subproject")
endif()
file(MAKE_DIRECTORY
  "J:/Basic_HMI_Code/NEW_Basic_Code/7.0/ESP_IDF/LVGL_Control_LED_70/build/bootloader"
  "J:/Basic_HMI_Code/NEW_Basic_Code/7.0/ESP_IDF/LVGL_Control_LED_70/build/bootloader-prefix"
  "J:/Basic_HMI_Code/NEW_Basic_Code/7.0/ESP_IDF/LVGL_Control_LED_70/build/bootloader-prefix/tmp"
  "J:/Basic_HMI_Code/NEW_Basic_Code/7.0/ESP_IDF/LVGL_Control_LED_70/build/bootloader-prefix/src/bootloader-stamp"
  "J:/Basic_HMI_Code/NEW_Basic_Code/7.0/ESP_IDF/LVGL_Control_LED_70/build/bootloader-prefix/src"
  "J:/Basic_HMI_Code/NEW_Basic_Code/7.0/ESP_IDF/LVGL_Control_LED_70/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "J:/Basic_HMI_Code/NEW_Basic_Code/7.0/ESP_IDF/LVGL_Control_LED_70/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "J:/Basic_HMI_Code/NEW_Basic_Code/7.0/ESP_IDF/LVGL_Control_LED_70/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
