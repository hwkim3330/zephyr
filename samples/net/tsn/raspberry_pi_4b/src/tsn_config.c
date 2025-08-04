/*
 * Copyright (c) 2025 TSN Implementation Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#if defined(CONFIG_NET_L2_IEEE802_1CB)
#include <zephyr/net/ieee802_1cb.h>
#endif

#if defined(CONFIG_NET_L2_IEEE802_1QBV)
#include <zephyr/net/ieee802_1qbv.h>
#endif

LOG_MODULE_REGISTER(tsn_config, LOG_LEVEL_DBG);

/* Example FRER stream configuration */
static struct ieee802_1cb_stream_config example_streams[] = {
	{
		.stream_handle = 1,
		.id_type = IEEE802_1CB_STREAM_ID_DEST_MAC_VLAN,
		.id.mac_vlan = {
			.mac_addr = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E},
			.vlan_id = 100
		},
		.replication_enabled = true,
		.elimination_enabled = true,
		.history_length = 32
	},
	{
		.stream_handle = 2,
		.id_type = IEEE802_1CB_STREAM_ID_DEST_MAC_VLAN,
		.id.mac_vlan = {
			.mac_addr = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x0F},
			.vlan_id = 200
		},
		.replication_enabled = false,
		.elimination_enabled = true,
		.history_length = 16
	}
};

/* Example TAS gate control list */
static struct ieee802_1qbv_gate_entry example_gate_entries[] = {
	/* Entry 0: All gates open for 500μs */
	{
		.operation = IEEE802_1QBV_GATE_OPEN,
		.traffic_class_mask = 0xFF,  /* All 8 traffic classes */
		.time_interval_ns = 500000   /* 500 microseconds */
	},
	/* Entry 1: Only high priority traffic (TC 6,7) for 250μs */
	{
		.operation = IEEE802_1QBV_GATE_OPEN,
		.traffic_class_mask = 0xC0,  /* TC 6,7 only */
		.time_interval_ns = 250000   /* 250 microseconds */
	},
	/* Entry 2: Medium priority traffic (TC 4,5) for 250μs */
	{
		.operation = IEEE802_1QBV_GATE_OPEN,
		.traffic_class_mask = 0x30,  /* TC 4,5 only */
		.time_interval_ns = 250000   /* 250 microseconds */
	}
};

static struct ieee802_1qbv_gate_control_list example_gcl = {
	.entries = example_gate_entries,
	.num_entries = ARRAY_SIZE(example_gate_entries),
	.cycle_time_ns = 1000000,  /* 1ms cycle */
	.cycle_time_extension_ns = 0,
	.base_time_ns = 0,
	.admin_gate_states = {true, true, true, true, true, true, true, true}
};

static int configure_frer(struct net_if *iface)
{
	int ret;
	
	if (!IS_ENABLED(CONFIG_NET_L2_IEEE802_1CB)) {
		LOG_INF("IEEE 802.1CB not enabled, skipping FRER configuration");
		return 0;
	}
	
	LOG_INF("Configuring IEEE 802.1CB FRER...");
	
	/* Initialize FRER */
	ret = ieee802_1cb_init(iface);
	if (ret < 0) {
		LOG_ERR("Failed to initialize FRER: %d", ret);
		return ret;
	}
	
	/* Add example streams */
	for (int i = 0; i < ARRAY_SIZE(example_streams); i++) {
		ret = ieee802_1cb_stream_add(iface, &example_streams[i]);
		if (ret < 0) {
			LOG_ERR("Failed to add FRER stream %d: %d", 
				example_streams[i].stream_handle, ret);
			return ret;
		}
		
		LOG_INF("Added FRER stream %d (replication: %s, elimination: %s)",
			example_streams[i].stream_handle,
			example_streams[i].replication_enabled ? "ON" : "OFF",
			example_streams[i].elimination_enabled ? "ON" : "OFF");
	}
	
	LOG_INF("FRER configuration completed");
	return 0;
}

static int configure_tas(struct net_if *iface)
{
	int ret;
	
	if (!IS_ENABLED(CONFIG_NET_L2_IEEE802_1QBV)) {
		LOG_INF("IEEE 802.1Qbv not enabled, skipping TAS configuration");
		return 0;
	}
	
	LOG_INF("Configuring IEEE 802.1Qbv TAS...");
	
	/* Initialize TAS */
	ret = ieee802_1qbv_init(iface);
	if (ret < 0) {
		LOG_ERR("Failed to initialize TAS: %d", ret);
		return ret;
	}
	
	/* Configure gate control list */
	ret = ieee802_1qbv_configure_gates(iface, &example_gcl);
	if (ret < 0) {
		LOG_ERR("Failed to configure TAS gates: %d", ret);
		return ret;
	}
	
	LOG_INF("Configured TAS with %u gate entries, cycle time: %llu ns",
		example_gcl.num_entries, example_gcl.cycle_time_ns);
	
	/* Enable TAS */
	ret = ieee802_1qbv_enable(iface, true);
	if (ret < 0) {
		LOG_ERR("Failed to enable TAS: %d", ret);
		return ret;
	}
	
	LOG_INF("TAS configuration completed and enabled");
	return 0;
}

static int configure_gptp(struct net_if *iface)
{
	if (!IS_ENABLED(CONFIG_NET_GPTP)) {
		LOG_INF("gPTP not enabled, skipping time synchronization setup");
		return 0;
	}
	
	LOG_INF("gPTP is enabled for time synchronization");
	
	/* gPTP is automatically initialized by the networking stack */
	/* Additional gPTP configuration could go here if needed */
	
	return 0;
}

static void print_configuration_summary(void)
{
	LOG_INF("=== TSN Configuration Summary ===");
	
	if (IS_ENABLED(CONFIG_NET_L2_IEEE802_1CB)) {
		LOG_INF("IEEE 802.1CB FRER: %u streams configured", 
			ARRAY_SIZE(example_streams));
	}
	
	if (IS_ENABLED(CONFIG_NET_L2_IEEE802_1QBV)) {
		LOG_INF("IEEE 802.1Qbv TAS: %u gate entries, %llu ns cycle",
			example_gcl.num_entries, example_gcl.cycle_time_ns);
	}
	
	if (IS_ENABLED(CONFIG_NET_L2_IEEE802_1QBU)) {
		LOG_INF("IEEE 802.1Qbu Frame Preemption: Enabled");
	}
	
	if (IS_ENABLED(CONFIG_NET_GPTP)) {
		LOG_INF("IEEE 802.1AS gPTP: Enabled for time sync");
	}
	
	LOG_INF("===============================");
}

int tsn_config_init(struct net_if *iface)
{
	int ret;
	
	if (!iface) {
		LOG_ERR("Invalid network interface");
		return -EINVAL;
	}
	
	LOG_INF("Initializing TSN configuration for interface %p", iface);
	
	/* Configure FRER (Frame Replication and Elimination) */
	ret = configure_frer(iface);
	if (ret < 0) {
		LOG_ERR("FRER configuration failed: %d", ret);
		return ret;
	}
	
	/* Configure TAS (Time-Aware Scheduling) */
	ret = configure_tas(iface);
	if (ret < 0) {
		LOG_ERR("TAS configuration failed: %d", ret);
		return ret;
	}
	
	/* Configure gPTP (time synchronization) */
	ret = configure_gptp(iface);
	if (ret < 0) {
		LOG_ERR("gPTP configuration failed: %d", ret);
		return ret;
	}
	
	/* Print configuration summary */
	print_configuration_summary();
	
	LOG_INF("TSN configuration initialization completed successfully");
	return 0;
}