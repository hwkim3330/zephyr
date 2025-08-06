# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rpi_4b_tsn)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/tsn_app_simple.c)
target_sources(app PRIVATE src/benchmark.c)
target_sources(app PRIVATE src/serial_comm.c)