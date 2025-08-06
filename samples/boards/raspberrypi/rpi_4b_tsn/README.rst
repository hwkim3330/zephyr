.. _rpi_4b_tsn:

Raspberry Pi 4B TSN Application
###############################

Overview
********

This sample demonstrates Time-Sensitive Networking (TSN) functionality on the 
Raspberry Pi 4 Model B using Zephyr RTOS. The application provides:

* IEEE 802.1AS generalized Precision Time Protocol (gPTP) support
* TSN stream management and configuration
* Serial communication interface for external control
* Network benchmarking tools
* Real-time performance monitoring

Features
********

* **TSN Support**: Full gPTP implementation for time synchronization
* **Stream Management**: Configure and manage TSN traffic streams
* **Serial Interface**: Command and control via UART (115200 baud)
* **Benchmarking**: Network performance testing with latency measurement
* **Shell Interface**: Interactive shell for configuration and monitoring
* **LED Status**: Visual indication of system status

Requirements
************

* Raspberry Pi 4 Model B
* Ethernet connection for TSN networking
* Serial connection for external control (optional)
* Zephyr SDK and development environment

Building
********

To build the TSN application for Raspberry Pi 4B:

.. code-block:: console

   west build -b rpi_4b/bcm2711 samples/boards/raspberrypi/rpi_4b_tsn -- -DCONF_FILE=prj.conf -DDTC_OVERLAY_FILE=rpi_4b_tsn.dts

Or using the custom TSN configuration:

.. code-block:: console

   west build -b rpi_4b/bcm2711 samples/boards/raspberrypi/rpi_4b_tsn -- -DCONF_FILE=../../../boards/raspberrypi/rpi_4b/rpi_4b_tsn_defconfig -DDTC_OVERLAY_FILE=../../../boards/raspberrypi/rpi_4b/rpi_4b_tsn.dts

Flashing
********

Flash the built image to an SD card:

.. code-block:: console

   west flash

Or manually copy the zephyr.bin file to the SD card as kernel8.img.

Configuration
*************

The application supports several configuration options:

TSN Configuration
=================

* **gPTP Domain**: Default domain 0
* **Clock Accuracy**: 100ns (configurable)
* **Grand Master Capable**: Yes
* **Number of Ports**: 1 (ethernet)

Serial Communication
====================

* **Baud Rate**: 115200
* **Data Bits**: 8
* **Stop Bits**: 1
* **Parity**: None
* **Flow Control**: None

Usage
*****

Shell Commands
==============

The application provides a comprehensive shell interface:

.. code-block:: console

   # TSN commands
   tsn gptp                 # Show gPTP status
   tsn stream add <id> <bw> <lat>  # Add TSN stream
   tsn stream list          # List active streams

   # Benchmark commands  
   benchmark start          # Start network benchmark
   benchmark stop           # Stop benchmark
   benchmark status         # Show benchmark results

   # Serial commands
   serial stats             # Show serial communication stats
   serial send <message>    # Send message via serial
   serial test              # Send test message

Serial Protocol
===============

The serial interface accepts these commands:

* ``gptp_status`` - Get gPTP synchronization status
* ``stream_add <id> <bandwidth> <latency>`` - Configure TSN stream
* ``benchmark_start`` - Start network performance test
* ``benchmark_stop`` - Stop performance test
* ``stats`` - Get system statistics

Example serial session:

.. code-block:: console

   > gptp_status
   OK
   > stream_add 1 1024 100
   STREAM_ADDED:1,1024,100
   > benchmark_start
   BENCHMARK_STARTED
   > stats
   STATS:TX=1234,RX=5678,CMD=4,ERR=0

TSN Stream Management
=====================

Configure TSN streams for different traffic classes:

.. code-block:: console

   # High priority stream (1MB/s, 100us max latency)
   tsn stream add 1 1024 100

   # Medium priority stream (512KB/s, 500us max latency)  
   tsn stream add 2 512 500

Network Benchmarking
====================

The benchmark tool measures:

* Packet rate (packets per second)
* Throughput (bytes per second)
* Latency (min/max/average)
* Packet loss percentage

Results are displayed every 10 seconds during active benchmarking.

Hardware Setup
**************

Raspberry Pi 4B Connections
============================

* **Ethernet**: Connect to TSN-capable switch or directly to another TSN device
* **GPIO 42**: Activity LED (built-in ACT LED)
* **UART1**: Serial communication (GPIO 14/15)

For serial communication, connect:

* GPIO 14 (TXD) to external device RXD
* GPIO 15 (RXD) to external device TXD  
* Ground connection

Troubleshooting
***************

Common Issues
=============

1. **No network interface detected**
   
   * Check ethernet cable connection
   * Verify device tree configuration
   * Check kernel logs for driver errors

2. **gPTP not synchronizing**

   * Verify TSN switch configuration
   * Check network topology
   * Enable gPTP debug logging

3. **Serial communication not working**

   * Verify baud rate (115200)
   * Check GPIO pin connections
   * Test with simple echo commands

Debug Information
=================

Enable additional logging:

.. code-block:: console

   # In prj.conf
   CONFIG_LOG_DEFAULT_LEVEL=4
   CONFIG_NET_GPTP_LOG_LEVEL_DBG=y
   CONFIG_ETHERNET_LOG_LEVEL_DBG=y

References
**********

* `IEEE 802.1AS-2011 Standard <https://standards.ieee.org/standard/802_1AS-2011.html>`_
* `Zephyr Networking Documentation <https://docs.zephyrproject.org/latest/connectivity/networking/index.html>`_
* `Raspberry Pi 4 Documentation <https://www.raspberrypi.org/documentation/>`_