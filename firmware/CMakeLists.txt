cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(solar-ball)

idf_component_register(
    SRCS
        src/main.c
        src/sensor_scan.c
        src/direction.c
        src/mqtt_4g.c
        src/power.c
        src/calibrate.c
        src/sun_calc.c
    INCLUDE_DIRS
        src
    REQUIRES
        driver
        nvs_flash
        esp_adc
        esp_hw_support
        esp_timer
)
