# TSN Sample Application for Raspberry Pi 4B
# Copyright (c) 2025 TSN Implementation Team
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(tsn_raspberry_pi_4b)

target_sources(app PRIVATE
    src/main.c
    src/tsn_config.c
    src/tsn_demo.c
)