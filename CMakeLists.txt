# Freenove ESP32 Mini TV (FNK0112) — standalone ESP-IDF project.
# Dependencies (LVGL, Mooncake) are vendored under components/ — no network
# fetch is needed; `idf.py build` works on a fresh offline checkout.
cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(PROJECT_VER "0.1.0")   # shown in the device-info app

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_minitv)
