# Servidor do Cananeia Deep Sea para ESP32-S3 (ESP-IDF 6.1).
# Compilar: idf.py set-target esp32s3 && idf.py build flash monitor
cmake_minimum_required(VERSION 3.22)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cananeia-server)
