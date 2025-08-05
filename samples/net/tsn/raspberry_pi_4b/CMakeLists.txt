# TSN Sample Application for Raspberry Pi 4B
# Copyright (c) 2025 TSN Implementation Team
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Force system Python to avoid Miniconda conflicts
if(NOT DEFINED PYTHON_EXECUTABLE)
    find_program(PYTHON_EXECUTABLE
        NAMES python3.11 python3.10 python3.9 python3.8 python3
        PATHS /usr/bin /usr/local/bin /bin
        NO_DEFAULT_PATH
    )
endif()

if(PYTHON_EXECUTABLE)
    set(Python3_EXECUTABLE ${PYTHON_EXECUTABLE})
    set(PYTHON3_EXECUTABLE ${PYTHON_EXECUTABLE})
    message(STATUS "TSN Build: Using Python ${PYTHON_EXECUTABLE}")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(tsn_raspberry_pi_4b)

target_sources(app PRIVATE
    src/main.c
    src/tsn_config.c
    src/tsn_demo.c
)