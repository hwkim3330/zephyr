/*
 * Copyright (c) 2025 TSN Implementation Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_config.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/ieee802_1cb.h>
#include <zephyr/net/ieee802_1qbv.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(tsn_main, LOG_LEVEL_DBG);

/* External functions from other source files */
extern int tsn_config_init(struct net_if *iface);
extern int tsn_demo_start(struct net_if *iface);
extern void tsn_demo_stop(void);

static struct net_if *main_iface = NULL;

static void iface_cb(struct net_if *iface, void *user_data)
{
	if (net_if_l2(iface) == &NET_L2_GET_NAME(ETHERNET)) {
		main_iface = iface;
		LOG_INF("Found Ethernet interface: %p", iface);
	}
}

static int init_network_interface(void)
{
	LOG_INF("Initializing network interface...");
	
	/* Find the main Ethernet interface */
	net_if_foreach(iface_cb, NULL);
	
	if (!main_iface) {
		LOG_ERR("No Ethernet interface found");
		return -ENODEV;
	}
	
	/* Wait for interface to be up */
	int timeout = 100; /* 10 seconds */
	while (!net_if_is_up(main_iface) && timeout > 0) {
		k_sleep(K_MSEC(100));
		timeout--;
	}
	
	if (!net_if_is_up(main_iface)) {
		LOG_WRN("Interface not up after timeout, continuing anyway");
	} else {
		LOG_INF("Network interface is up");
	}
	
	return 0;
}

static void print_interface_info(struct net_if *iface)
{
	struct net_linkaddr *link_addr;
	char addr_str[18]; /* xx:xx:xx:xx:xx:xx + null terminator */
	
	if (!iface) {
		return;
	}
	
	link_addr = net_if_get_link_addr(iface);
	if (link_addr && link_addr->len == 6) {
		snprintf(addr_str, sizeof(addr_str), 
			 "%02x:%02x:%02x:%02x:%02x:%02x",
			 link_addr->addr[0], link_addr->addr[1], link_addr->addr[2],
			 link_addr->addr[3], link_addr->addr[4], link_addr->addr[5]);
		LOG_INF("MAC Address: %s", addr_str);
	}
	
	LOG_INF("MTU: %d", net_if_get_mtu(iface));
	LOG_INF("Link: %s", net_if_is_up(iface) ? "UP" : "DOWN");
}

static void print_tsn_capabilities(void)
{
	LOG_INF("TSN Capabilities:");
	LOG_INF("  IEEE 802.1CB (FRER): %s",
		IS_ENABLED(CONFIG_NET_L2_IEEE802_1CB) ? "Enabled" : "Disabled");
	LOG_INF("  IEEE 802.1Qbv (TAS): %s", 
		IS_ENABLED(CONFIG_NET_L2_IEEE802_1QBV) ? "Enabled" : "Disabled");
	LOG_INF("  IEEE 802.1Qbu (Frame Preemption): %s",
		IS_ENABLED(CONFIG_NET_L2_IEEE802_1QBU) ? "Enabled" : "Disabled");
	LOG_INF("  IEEE 802.1AS (gPTP): %s",
		IS_ENABLED(CONFIG_NET_GPTP) ? "Enabled" : "Disabled");
}

/* Shell command to show TSN status */
static int cmd_tsn_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	if (!main_iface) {
		shell_print(sh, "No network interface available");
		return -ENODEV;
	}
	
	shell_print(sh, "TSN Status for interface %p:", main_iface);
	
	/* Print interface info */
	print_interface_info(main_iface);
	
	/* Print TSN capabilities */
	print_tsn_capabilities();
	
	/* Get and print FRER stats if available */
	if (IS_ENABLED(CONFIG_NET_L2_IEEE802_1CB)) {
		/* This would show FRER statistics */
		shell_print(sh, "FRER streams: configured streams would be listed here");
	}
	
	/* Get and print TAS stats if available */
	if (IS_ENABLED(CONFIG_NET_L2_IEEE802_1QBV)) {
		bool gate_states[8];
		int ret = ieee802_1qbv_get_gate_states(main_iface, gate_states);
		if (ret == 0) {
			shell_print(sh, "TAS Gate States:");
			for (int i = 0; i < 8; i++) {
				shell_print(sh, "  TC%d: %s", i, 
					    gate_states[i] ? "OPEN" : "CLOSED");
			}
		}
	}
	
	return 0;
}

/* Shell command to start TSN demo */
static int cmd_tsn_demo_start(const struct shell *sh, size_t argc, char **argv)
{
	int ret;
	
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	if (!main_iface) {
		shell_print(sh, "No network interface available");
		return -ENODEV;
	}
	
	ret = tsn_demo_start(main_iface);
	if (ret == 0) {
		shell_print(sh, "TSN demo started");
	} else {
		shell_print(sh, "Failed to start TSN demo: %d", ret);
	}
	
	return ret;
}

/* Shell command to stop TSN demo */
static int cmd_tsn_demo_stop(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	tsn_demo_stop();
	shell_print(sh, "TSN demo stopped");
	
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_tsn,
	SHELL_CMD(status, NULL, "Show TSN status", cmd_tsn_status),
	SHELL_CMD(demo_start, NULL, "Start TSN demo", cmd_tsn_demo_start),
	SHELL_CMD(demo_stop, NULL, "Stop TSN demo", cmd_tsn_demo_stop),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(tsn, &sub_tsn, "TSN commands", NULL);

int main(void)
{
	int ret;
	
	LOG_INF("=== Zephyr TSN Demo for Raspberry Pi 4B ===");
	LOG_INF("Starting TSN demonstration application...");
	
	/* Initialize network interface */
	ret = init_network_interface();
	if (ret < 0) {
		LOG_ERR("Failed to initialize network interface: %d", ret);
		return ret;
	}
	
	/* Print interface and capability information */
	print_interface_info(main_iface);
	print_tsn_capabilities();
	
	/* Initialize TSN configuration */
	ret = tsn_config_init(main_iface);
	if (ret < 0) {
		LOG_ERR("Failed to initialize TSN configuration: %d", ret);
		return ret;
	}
	
	LOG_INF("TSN application initialized successfully");
	LOG_INF("Use 'tsn status' to check status");
	LOG_INF("Use 'tsn demo_start' to start the demo");
	LOG_INF("Use 'tsn demo_stop' to stop the demo");
	
	/* Main application loop */
	while (1) {
		k_sleep(K_SECONDS(10));
		
		/* Periodic status updates */
		LOG_DBG("Application running, interface %s", 
			net_if_is_up(main_iface) ? "UP" : "DOWN");
	}
	
	return 0;
}