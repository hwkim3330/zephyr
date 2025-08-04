# TSN Sample Application for Raspberry Pi 4B

This sample demonstrates Time-Sensitive Networking (TSN) features on Raspberry Pi 4B using Zephyr RTOS.

## Features Demonstrated

### IEEE 802.1CB Frame Replication and Elimination (FRER)
- Stream identification and configuration
- Frame replication for redundancy
- Duplicate frame elimination
- Sequence recovery

### IEEE 802.1Qbv Time-Aware Scheduling (TAS)
- Gate control list configuration
- Traffic class prioritization
- Deterministic transmission timing
- Cycle-based scheduling

### IEEE 802.1AS Precision Time Protocol (gPTP)
- Network-wide time synchronization
- Precise timestamping
- Clock correction

### Additional Features
- Network statistics collection
- Shell-based configuration and monitoring
- Traffic generation and analysis

## Building and Running

### Prerequisites
- Raspberry Pi 4B with Ethernet connectivity
- Zephyr development environment
- Cross-compilation toolchain for ARM64

### Build Instructions

```bash
# Set up Zephyr environment
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile
export CROSS_COMPILE=aarch64-linux-gnu-

# Navigate to the sample directory
cd samples/net/tsn/raspberry_pi_4b

# Build with TSN configuration
west build -b rpi_4b -- -DCONF_FILE=prj.conf

# Or use the pre-configured TSN defconfig
west build -b rpi_4b -- -DCONF_FILE=../../../boards/raspberrypi/rpi_4b/rpi_4b_tsn_defconfig
```

### Flash and Run

```bash
# Copy the built image to SD card (replace /dev/sdX with your SD card)
sudo dd if=build/zephyr/zephyr.bin of=/dev/sdX bs=1M
sync

# Insert SD card into Raspberry Pi 4B and power on
# Connect via UART console (115200 baud) or SSH if network is configured
```

## Usage

### Basic Commands

Once the application is running, use the shell interface:

```bash
# Check TSN status
uart:~$ tsn status

# Start the TSN demonstration
uart:~$ tsn demo_start

# Stop the demonstration  
uart:~$ tsn demo_stop

# Check network interface status
uart:~$ net iface

# View network statistics
uart:~$ net stats
```

### TSN Configuration

The application automatically configures:

1. **FRER Streams:**
   - Stream 1: MAC 01:80:C2:00:00:0E, VLAN 100
   - Stream 2: MAC 01:80:C2:00:00:0F, VLAN 200

2. **TAS Schedule:**
   - 500μs: All traffic classes open
   - 250μs: High priority only (TC 6,7)
   - 250μs: Medium priority only (TC 4,5)
   - Total cycle: 1ms

3. **Traffic Classes:**
   - TC 7: Critical control traffic
   - TC 6: High priority data
   - TC 4-5: Medium priority data
   - TC 0-3: Best effort traffic

### Testing TSN Features

#### Frame Replication and Elimination
1. Configure multiple network paths
2. Start the demo with `tsn demo_start`
3. Monitor duplicate elimination in logs
4. Check FRER statistics with `tsn status`

#### Time-Aware Scheduling
1. Generate traffic with different priorities
2. Observe gate control behavior in logs
3. Measure latency and jitter for high-priority traffic
4. Verify deterministic transmission timing

#### Time Synchronization
1. Connect to gPTP-enabled network
2. Monitor synchronization status in logs
3. Check time offset and clock drift
4. Verify synchronized timestamps

## Network Setup

### Basic Setup
```
[PC/Switch] ---- Ethernet ---- [Raspberry Pi 4B]
```

### TSN Test Setup
```
[Master Clock] ---- [TSN Switch] ---- [Raspberry Pi 4B]
                        |
                   [Other TSN Nodes]
```

### Configuration Requirements
- TSN-capable switch or bridge
- gPTP grandmaster clock
- Network with deterministic timing
- VLAN support for stream identification

## Troubleshooting

### Common Issues

1. **Network Interface Not Found**
   - Verify Ethernet cable connection
   - Check device tree configuration
   - Ensure BCM2711 driver is enabled

2. **TSN Features Not Working**
   - Verify CONFIG_NET_L2_IEEE802_TSN=y
   - Check specific TSN feature configs
   - Ensure sufficient memory allocation

3. **Time Synchronization Issues**
   - Verify gPTP network configuration
   - Check grandmaster clock presence
   - Monitor gPTP messages in logs

4. **Performance Issues**
   - Adjust thread priorities
   - Increase buffer pool sizes
   - Optimize timer resolution

### Debug Configuration

Enable additional debugging:

```kconfig
# Enhanced logging
CONFIG_LOG_DEFAULT_LEVEL=4
CONFIG_NET_L2_IEEE802_1CB_LOG_LEVEL=4
CONFIG_NET_L2_IEEE802_1QBV_LOG_LEVEL=4
CONFIG_NET_GPTP_LOG_LEVEL=4

# Network debugging
CONFIG_NET_LOG=y
CONFIG_NET_PKT_LOG_LEVEL=4
CONFIG_NET_IF_LOG_LEVEL=4
```

## Performance Metrics

### Expected Performance
- **Latency:** < 100μs for high-priority traffic
- **Jitter:** < 10μs with TAS enabled
- **Throughput:** Up to 1 Gbps aggregate
- **Time Sync Accuracy:** < 1μs with gPTP

### Measurement Tools
- Built-in statistics collection
- Wireshark for traffic analysis
- Hardware timestamping support
- Real-time performance monitoring

## References

- [IEEE 802.1CB-2017](https://standards.ieee.org/standard/802_1CB-2017.html) - Frame Replication and Elimination
- [IEEE 802.1Qbv-2015](https://standards.ieee.org/standard/802_1Qbv-2015.html) - Time-Aware Scheduling  
- [IEEE 802.1AS-2020](https://standards.ieee.org/standard/802_1AS-2020.html) - Timing and Synchronization
- [Zephyr Networking Documentation](https://docs.zephyrproject.org/latest/connectivity/networking/index.html)
- [TSN Standards Overview](https://1.ieee802.org/tsn/)

## License

This sample application is licensed under the Apache License 2.0.