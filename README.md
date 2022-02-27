# ble2mqtt
ESP-IDF project that demonstrate firmware development using FreeRTOS on M5Stack device.

## Introduction
The project is C/C++ code for ESP32 processor that forward temperature and humidity values from Xiaomi Bluetooth Temperature and Humidity sensor to HiveMQ via MQTT protocol. I developed this example code for the TESA Tech Update event on Feb 25, 2022. Slides and record clip can be checked from [TESA website](https://www.tesa.or.th). The objective of this code is to show how to develop C/C++ for microcontroller using RTOS API. The hardware used is M5Stack core (ESP32) with ESP-IDF as the development toolchain. In addition to Bluetooth/WiFi/MQTT components, [LVGL driver](https://docs.lvgl.io/latest/en/html/get-started/espressif.html) is added as the GUI engine.

## Procedure
I had prepared this project using the following steps.
1. Install development tools: VS Code + ESP-IDF extension
2. Clone ESP32-port LVGL code: `git clone --recurse-submodules https://github.com/lvgl/lv_port_esp32.git ble2mqtt`
3. Setup LVGL for M5Stack
	1. Configure SDK: `idf.py menuconfig`
	2. Set Component config > LVGL configuration > resolution 320x240 and swap RGB565 bytes
	3. Set Component config > LVGL configuration > LVGL TFT Display controller > predefined M5Stack and landscape orientation
	4. Build and deploy to verify
4. Enable Bluetooth
	1. Configure SDK: `idf.py menuconfig`
	2. Set Component config > Bluetooth > enable BLE and Bluedroid
5. 
	