# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

# set board variable to thingy:52 for cross-compilation, needs to be done before including/finding zephyr
set(BOARD "thingy52_nrf52832")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(ble_gatt_server)

target_sources(app PRIVATE src/main.c src/status_blinker.c src/ble_hts.c src/hts221_reader.c)
