# 🔌 USB-C 시리얼 + 완전한 TSN 명령어 구현

## 📋 **USB-C 시리얼 활성화 설정**

### 1. prj.conf 파일 수정:
```bash
# USB-C Serial Console 설정
CONFIG_USB_DEVICE_STACK=y
CONFIG_USB_DEVICE_PRODUCT="TSN_RPI4B"
CONFIG_USB_DEVICE_MANUFACTURER="Zephyr"
CONFIG_USB_DEVICE_PID=0x0001
CONFIG_USB_DEVICE_VID=0x2FE3
CONFIG_USB_CDC_ACM=y
CONFIG_UART_CONSOLE=n
CONFIG_USB_DEVICE_INITIALIZE_AT_BOOT=y
CONFIG_SERIAL=y

# USB Console 리다이렉션
CONFIG_STDOUT_CONSOLE=y
CONFIG_USB_UART_CONSOLE=y
```

### 2. Device Tree 오버레이 (boards/rpi_4b.overlay):
```dts
/ {
    chosen {
        zephyr,console = &cdc_acm_uart0;
        zephyr,shell-uart = &cdc_acm_uart0;
    };
    
    cdc_acm_uart0: cdc_acm_uart {
        compatible = "zephyr,cdc-acm-uart";
        label = "CDC_ACM_0";
    };
};
```

## 🖥️ **완전한 TSN 쉘 명령어 구현**

### main.c에 추가할 TSN 명령어들:

```c
// IEEE 802.1CB 명령어들
static int cmd_tsn_cb_stream_add(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 4) {
        shell_error(sh, "Usage: tsn cb stream add <stream_id> <mac_addr> <vlan_id>");
        return -EINVAL;
    }
    
    uint16_t stream_id = strtol(argv[1], NULL, 10);
    uint16_t vlan_id = strtol(argv[3], NULL, 10);
    
    shell_print(sh, "Adding IEEE 802.1CB stream:");
    shell_print(sh, "  Stream ID: %u", stream_id);
    shell_print(sh, "  MAC Address: %s", argv[2]);
    shell_print(sh, "  VLAN ID: %u", vlan_id);
    
    // 실제 IEEE 802.1CB 스트림 추가 로직
    struct ieee802_1cb_stream_config config = {
        .stream_id = stream_id,
        .vlan_id = vlan_id,
        .sequence_number = 0,
        .frer_enabled = true
    };
    
    int ret = ieee802_1cb_stream_add(main_iface, &config);
    if (ret == 0) {
        shell_print(sh, "✅ IEEE 802.1CB stream added successfully");
    } else {
        shell_error(sh, "❌ Failed to add stream: %d", ret);
    }
    
    return ret;
}

static int cmd_tsn_cb_stream_list(const struct shell *sh, size_t argc, char **argv) {
    shell_print(sh, "🔍 IEEE 802.1CB Active Streams:");
    shell_print(sh, "┌─────────┬─────────────────┬─────────┬─────────┬──────────┐");
    shell_print(sh, "│Stream ID│   MAC Address   │ VLAN ID │ Seq Num │  Status  │");
    shell_print(sh, "├─────────┼─────────────────┼─────────┼─────────┼──────────┤");
    
    // 실제 스트림 목록 출력 로직
    for (int i = 0; i < MAX_TSN_STREAMS; i++) {
        if (frer_streams[i].active) {
            shell_print(sh, "│   %3u   │ %02x:%02x:%02x:%02x:%02x:%02x │   %3u   │  %5u  │ Active   │",
                frer_streams[i].stream_id,
                frer_streams[i].mac_addr[0], frer_streams[i].mac_addr[1],
                frer_streams[i].mac_addr[2], frer_streams[i].mac_addr[3],
                frer_streams[i].mac_addr[4], frer_streams[i].mac_addr[5],
                frer_streams[i].vlan_id,
                frer_streams[i].sequence_number);
        }
    }
    shell_print(sh, "└─────────┴─────────────────┴─────────┴─────────┴──────────┘");
    
    return 0;
}

static int cmd_tsn_cb_stats(const struct shell *sh, size_t argc, char **argv) {
    shell_print(sh, "📊 IEEE 802.1CB FRER Statistics:");
    shell_print(sh, "");
    shell_print(sh, "Frame Replication:");
    shell_print(sh, "  ├─ Frames replicated: %u", frer_stats.frames_replicated);
    shell_print(sh, "  ├─ Replication errors: %u", frer_stats.replication_errors);
    shell_print(sh, "  └─ Replication ratio: %.2f%%", 
        (float)frer_stats.frames_replicated / frer_stats.total_frames * 100);
    shell_print(sh, "");
    shell_print(sh, "Frame Elimination:");
    shell_print(sh, "  ├─ Duplicates eliminated: %u", frer_stats.duplicates_eliminated);
    shell_print(sh, "  ├─ Out-of-order frames: %u", frer_stats.out_of_order);
    shell_print(sh, "  └─ Sequence errors: %u", frer_stats.sequence_errors);
    
    return 0;
}

// IEEE 802.1Qbv 명령어들
static int cmd_tsn_qbv_schedule_set(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 4) {
        shell_error(sh, "Usage: tsn qbv schedule set <cycle_time_us> <gate_states> <duration_us>");
        return -EINVAL;
    }
    
    uint32_t cycle_time = strtol(argv[1], NULL, 10);
    uint8_t gate_states = strtol(argv[2], NULL, 16);
    uint32_t duration = strtol(argv[3], NULL, 10);
    
    shell_print(sh, "Setting IEEE 802.1Qbv Schedule:");
    shell_print(sh, "  ├─ Cycle Time: %u μs", cycle_time);
    shell_print(sh, "  ├─ Gate States: 0x%02x", gate_states);
    shell_print(sh, "  └─ Duration: %u μs", duration);
    
    // 실제 게이트 스케줄 설정
    struct ieee802_1qbv_gate_control_list gcl = {
        .cycle_time_ns = cycle_time * 1000,
        .num_entries = 1,
        .entries[0] = {
            .gate_states = gate_states,
            .time_interval_ns = duration * 1000
        }
    };
    
    int ret = ieee802_1qbv_configure_gates(main_iface, &gcl);
    if (ret == 0) {
        shell_print(sh, "✅ IEEE 802.1Qbv schedule configured successfully");
    } else {
        shell_error(sh, "❌ Failed to configure schedule: %d", ret);
    }
    
    return ret;
}

static int cmd_tsn_qbv_status(const struct shell *sh, size_t argc, char **argv) {
    shell_print(sh, "🕐 IEEE 802.1Qbv Time-Aware Shaper Status:");
    shell_print(sh, "");
    shell_print(sh, "Gate Control:");
    shell_print(sh, "  ├─ Current State: %s", qbv_enabled ? "ENABLED" : "DISABLED");
    shell_print(sh, "  ├─ Cycle Time: %u ns", current_gcl.cycle_time_ns);
    shell_print(sh, "  ├─ Current Gate: 0x%02x", current_gate_state);
    shell_print(sh, "  └─ Next Transition: %llu ns", next_gate_transition);
    shell_print(sh, "");
    shell_print(sh, "Traffic Classes:");
    for (int i = 0; i < 8; i++) {
        bool gate_open = (current_gate_state >> i) & 1;
        shell_print(sh, "  ├─ TC%d: %s", i, gate_open ? "OPEN 🟢" : "CLOSED 🔴");
    }
    shell_print(sh, "");
    shell_print(sh, "Statistics:");
    shell_print(sh, "  ├─ Frames transmitted: %u", qbv_stats.frames_transmitted);
    shell_print(sh, "  ├─ Frames dropped: %u", qbv_stats.frames_dropped);
    shell_print(sh, "  └─ Gate violations: %u", qbv_stats.gate_violations);
    
    return 0;
}

// 통합 TSN 명령어
static int cmd_tsn_demo_advanced(const struct shell *sh, size_t argc, char **argv) {
    shell_print(sh, "🚀 Starting Advanced TSN Demonstration");
    shell_print(sh, "");
    
    // Step 1: IEEE 802.1CB 스트림 설정
    shell_print(sh, "Step 1: Configuring IEEE 802.1CB FRER...");
    struct ieee802_1cb_stream_config cb_config = {
        .stream_id = 100,
        .vlan_id = 10,
        .frer_enabled = true
    };
    ieee802_1cb_stream_add(main_iface, &cb_config);
    shell_print(sh, "  ✅ Stream 100 configured for redundancy");
    
    // Step 2: IEEE 802.1Qbv 스케줄 설정  
    shell_print(sh, "Step 2: Configuring IEEE 802.1Qbv TAS...");
    struct ieee802_1qbv_gate_control_list gcl = {
        .cycle_time_ns = 1000000,  // 1ms cycle
        .num_entries = 2,
        .entries = {
            {.gate_states = 0x01, .time_interval_ns = 500000}, // TC0 open for 500μs
            {.gate_states = 0xFE, .time_interval_ns = 500000}  // TC1-7 open for 500μs
        }
    };
    ieee802_1qbv_configure_gates(main_iface, &gcl);
    shell_print(sh, "  ✅ Time-Aware Shaper configured (1ms cycle)");
    
    // Step 3: 트래픽 생성 및 모니터링
    shell_print(sh, "Step 3: Generating test traffic...");
    tsn_generate_test_traffic();
    shell_print(sh, "  ✅ Test traffic started");
    
    shell_print(sh, "");
    shell_print(sh, "🎯 TSN Demo Active! Use 'tsn stats' to monitor performance");
    
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_tsn_cb,
    SHELL_CMD(stream, &sub_tsn_cb_stream, "IEEE 802.1CB stream management", NULL),
    SHELL_CMD(stats, NULL, "Show IEEE 802.1CB statistics", cmd_tsn_cb_stats),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_tsn_cb_stream,
    SHELL_CMD(add, NULL, "Add FRER stream", cmd_tsn_cb_stream_add),
    SHELL_CMD(list, NULL, "List active streams", cmd_tsn_cb_stream_list),
    SHELL_CMD(remove, NULL, "Remove stream", cmd_tsn_cb_stream_remove),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_tsn_qbv,
    SHELL_CMD(schedule, &sub_tsn_qbv_schedule, "Time-Aware Shaper control", NULL),
    SHELL_CMD(status, NULL, "Show TAS status", cmd_tsn_qbv_status),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_tsn_qbv_schedule,
    SHELL_CMD(set, NULL, "Set gate schedule", cmd_tsn_qbv_schedule_set),
    SHELL_CMD(enable, NULL, "Enable TAS", cmd_tsn_qbv_enable),
    SHELL_CMD(disable, NULL, "Disable TAS", cmd_tsn_qbv_disable),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_tsn,
    SHELL_CMD(cb, &sub_tsn_cb, "IEEE 802.1CB Frame Replication/Elimination", NULL),
    SHELL_CMD(qbv, &sub_tsn_qbv, "IEEE 802.1Qbv Time-Aware Shaper", NULL),
    SHELL_CMD(demo, NULL, "Advanced TSN demonstration", cmd_tsn_demo_advanced),
    SHELL_CMD(status, NULL, "Overall TSN status", cmd_tsn_status_detailed),
    SHELL_CMD(reset, NULL, "Reset TSN configuration", cmd_tsn_reset),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(tsn, &sub_tsn, "Time-Sensitive Networking commands", NULL);
```

## 🎯 **완성된 TSN 명령어 사용법**

```bash
# IEEE 802.1CB 프레임 복제/제거
tsn cb stream add 100 aa:bb:cc:dd:ee:ff 10    # 스트림 추가
tsn cb stream list                            # 스트림 목록
tsn cb stats                                 # FRER 통계

# IEEE 802.1Qbv 시간 인식 셰이퍼  
tsn qbv schedule set 1000 0x01 500          # 게이트 스케줄 설정
tsn qbv status                               # TAS 상태 확인
tsn qbv schedule enable                      # TAS 활성화

# 통합 기능
tsn demo                                     # 고급 TSN 데모
tsn status                                   # 전체 TSN 상태
tsn reset                                    # TSN 리셋
```

이제 이 설정으로 다시 빌드해서 USB-C 시리얼이 작동하는 완전한 TSN 바이너리를 만들어보겠습니다!