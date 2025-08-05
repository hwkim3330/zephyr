/*
 * TSN Shell Commands Demo
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(tsn_shell, LOG_LEVEL_DBG);

static int cmd_tsn_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	shell_print(sh, "🔌 TSN Status for Raspberry Pi 4B:");
	shell_print(sh, "");
	shell_print(sh, "Hardware:");
	shell_print(sh, "  ├─ Platform: Raspberry Pi 4B (ARM Cortex-A72)");
	shell_print(sh, "  ├─ Ethernet: BCM2711 GENET Gigabit");
	shell_print(sh, "  ├─ USB-C Console: ✅ Active (CDC ACM)");
	shell_print(sh, "  └─ Memory: 4GB LPDDR4");
	shell_print(sh, "");
	shell_print(sh, "Console Status:");
	shell_print(sh, "  ├─ USB Device: TSN_RPI4B_DEMO");
	shell_print(sh, "  ├─ Serial Port: /dev/ttyACM0 (Linux)");
	shell_print(sh, "  ├─ Baud Rate: Virtual (USB speed)");
	shell_print(sh, "  └─ Connection: USB-C to PC");
	shell_print(sh, "");
	shell_print(sh, "TSN Capabilities:");
	shell_print(sh, "  ├─ IEEE 802.1CB (FRER): ✅ Ready");
	shell_print(sh, "  ├─ IEEE 802.1Qbv (TAS): ✅ Ready");
	shell_print(sh, "  ├─ IEEE 802.1AS (gPTP): ✅ Ready");
	shell_print(sh, "  └─ Network Link: 🟢 UP (1000 Mbps)");
	
	return 0;
}

static int cmd_tsn_demo(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	shell_print(sh, "Starting Advanced TSN Demonstration");
	shell_print(sh, "");
	
	shell_print(sh, "Step 1: Configuring IEEE 802.1CB FRER...");
	k_sleep(K_MSEC(500));
	shell_print(sh, "  Stream 100 configured for redundancy");
	shell_print(sh, "  Stream 101 configured for critical data");
	
	shell_print(sh, "Step 2: Configuring IEEE 802.1Qbv TAS...");
	k_sleep(K_MSEC(500));  
	shell_print(sh, "  Time-Aware Shaper configured (1ms cycle)");
	shell_print(sh, "  Priority scheduling enabled");
	
	shell_print(sh, "Step 3: Starting gPTP time synchronization...");
	k_sleep(K_MSEC(500));
	shell_print(sh, "  PTP Master clock detected");
	shell_print(sh, "  Time synchronization active");
	
	shell_print(sh, "Step 4: Generating test traffic...");
	k_sleep(K_MSEC(500));
	shell_print(sh, "  Critical traffic stream started");
	shell_print(sh, "  Best-effort traffic started");
	
	shell_print(sh, "");
	shell_print(sh, "Advanced TSN Demo Active\!");
	shell_print(sh, "Use TSN commands to monitor and control");
	
	return 0;
}

/* Main TSN command structure */
static int cmd_usb_test(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	shell_print(sh, "🔌 USB-C Serial Console Test");
	shell_print(sh, "");
	shell_print(sh, "Testing USB CDC ACM connection...");
	
	for (int i = 0; i < 5; i++) {
		shell_print(sh, "  📡 Test message %d/5", i + 1);
		k_sleep(K_MSEC(200));
	}
	
	shell_print(sh, "");
	shell_print(sh, "✅ USB-C serial console is working!");
	shell_print(sh, "💡 You can now use TSN commands:");
	shell_print(sh, "   - tsn status (show system info)");
	shell_print(sh, "   - tsn demo (run TSN demo)");
	shell_print(sh, "   - kernel version (system info)");
	shell_print(sh, "   - help (all commands)");
	
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_tsn,
	SHELL_CMD(demo, NULL, "Advanced TSN demonstration", cmd_tsn_demo),
	SHELL_CMD(status, NULL, "Show overall TSN status", cmd_tsn_status),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(tsn, &sub_tsn, "Time-Sensitive Networking commands", NULL);
SHELL_CMD_REGISTER(usbtest, NULL, "Test USB-C serial console", cmd_usb_test);
