/*
 * Copyright (c) 2025 TSN Implementation Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_NET_IEEE802_1QBV_H_
#define ZEPHYR_INCLUDE_NET_IEEE802_1QBV_H_

#include <zephyr/kernel.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/sys/slist.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief IEEE 802.1Qbv Time-Aware Scheduling (TAS) support
 * @defgroup ieee802_1qbv IEEE 802.1Qbv TAS
 * @ingroup networking
 * @{
 */

#define IEEE802_1QBV_MAX_TRAFFIC_CLASSES 8
#define IEEE802_1QBV_MAX_GATE_ENTRIES 1024
#define IEEE802_1QBV_MAX_CYCLE_TIME_NS 1000000000ULL  /* 1 second */

/**
 * @brief Gate operation types
 */
enum ieee802_1qbv_gate_operation {
	IEEE802_1QBV_GATE_CLOSE = 0,
	IEEE802_1QBV_GATE_OPEN = 1,
};

/**
 * @brief Gate control entry
 */
struct ieee802_1qbv_gate_entry {
	enum ieee802_1qbv_gate_operation operation;
	uint8_t traffic_class_mask;  /* Bit mask for traffic classes */
	uint32_t time_interval_ns;   /* Duration in nanoseconds */
};

/**
 * @brief Gate control list
 */
struct ieee802_1qbv_gate_control_list {
	struct ieee802_1qbv_gate_entry *entries;
	uint16_t num_entries;
	uint64_t cycle_time_ns;
	uint64_t cycle_time_extension_ns;
	uint64_t base_time_ns;
	bool admin_gate_states[IEEE802_1QBV_MAX_TRAFFIC_CLASSES];
	bool oper_gate_states[IEEE802_1QBV_MAX_TRAFFIC_CLASSES];
};

/**
 * @brief TAS port configuration
 */
struct ieee802_1qbv_port_config {
	bool gate_enabled;
	uint32_t config_change_time_ns;
	uint64_t tick_granularity_ns;
	uint32_t config_change_error_ns;
	struct ieee802_1qbv_gate_control_list admin_control_list;
	struct ieee802_1qbv_gate_control_list oper_control_list;
};

/**
 * @brief TAS instance
 */
struct ieee802_1qbv_instance {
	struct ieee802_1qbv_port_config config;
	struct k_timer cycle_timer;
	struct k_mutex mutex;
	struct net_if *iface;
	uint32_t current_entry_index;
	uint64_t cycle_start_time_ns;
	uint64_t current_time_ns;
	
	/* Statistics */
	uint64_t transmitted_frames[IEEE802_1QBV_MAX_TRAFFIC_CLASSES];
	uint64_t dropped_frames[IEEE802_1QBV_MAX_TRAFFIC_CLASSES];
};

/**
 * @brief Initialize IEEE 802.1Qbv TAS for a network interface
 * @param iface Network interface
 * @return 0 on success, negative error code on failure
 */
int ieee802_1qbv_init(struct net_if *iface);

/**
 * @brief Configure gate control list
 * @param iface Network interface
 * @param config Gate control list configuration
 * @return 0 on success, negative error code on failure
 */
int ieee802_1qbv_configure_gates(struct net_if *iface,
				  const struct ieee802_1qbv_gate_control_list *config);

/**
 * @brief Enable/disable time-aware scheduling
 * @param iface Network interface
 * @param enable Enable or disable TAS
 * @return 0 on success, negative error code on failure
 */
int ieee802_1qbv_enable(struct net_if *iface, bool enable);

/**
 * @brief Check if traffic class gate is open for transmission
 * @param iface Network interface
 * @param traffic_class Traffic class (0-7)
 * @return true if gate is open, false if closed
 */
bool ieee802_1qbv_is_gate_open(struct net_if *iface, uint8_t traffic_class);

/**
 * @brief Get current gate states
 * @param iface Network interface
 * @param gate_states Array to store gate states (8 entries)
 * @return 0 on success, negative error code on failure
 */
int ieee802_1qbv_get_gate_states(struct net_if *iface, bool *gate_states);

/**
 * @brief Get TAS statistics
 * @param iface Network interface
 * @param instance Pointer to store TAS instance
 * @return 0 on success, negative error code on failure
 */
int ieee802_1qbv_get_stats(struct net_if *iface, 
			    struct ieee802_1qbv_instance **instance);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_NET_IEEE802_1QBV_H_ */