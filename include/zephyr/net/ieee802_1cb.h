/*
 * Copyright (c) 2025 TSN Implementation Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_NET_IEEE802_1CB_H_
#define ZEPHYR_INCLUDE_NET_IEEE802_1CB_H_

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
 * @brief IEEE 802.1CB Frame Replication and Elimination (FRER) support
 * @defgroup ieee802_1cb IEEE 802.1CB FRER
 * @ingroup networking
 * @{
 */

#define IEEE802_1CB_MAX_STREAMS 32
#define IEEE802_1CB_MAX_SEQUENCE_NUM 65535
#define IEEE802_1CB_R_TAG_LEN 6

/**
 * @brief Stream identification methods
 */
enum ieee802_1cb_stream_id_type {
	IEEE802_1CB_STREAM_ID_NULL = 0,
	IEEE802_1CB_STREAM_ID_SOURCE_MAC_VLAN = 1,
	IEEE802_1CB_STREAM_ID_DEST_MAC_VLAN = 2,
	IEEE802_1CB_STREAM_ID_IP_STREAM = 3,
};

/**
 * @brief FRER stream configuration
 */
struct ieee802_1cb_stream_config {
	uint16_t stream_handle;
	enum ieee802_1cb_stream_id_type id_type;
	union {
		struct {
			uint8_t mac_addr[6];
			uint16_t vlan_id;
		} mac_vlan;
		struct {
			uint32_t src_ip;
			uint32_t dst_ip;
			uint16_t src_port;
			uint16_t dst_port;
			uint8_t protocol;
		} ip_stream;
	} id;
	bool replication_enabled;
	bool elimination_enabled;
	uint16_t history_length;
};

/**
 * @brief Sequence recovery functions
 */
struct ieee802_1cb_seq_recovery {
	uint16_t seq_num;
	uint32_t timestamp;
	uint8_t r_tag[IEEE802_1CB_R_TAG_LEN];
};

/**
 * @brief FRER stream state
 */
struct ieee802_1cb_stream {
	struct ieee802_1cb_stream_config config;
	struct ieee802_1cb_seq_recovery *history;
	uint16_t history_head;
	uint16_t next_seq_num;
	uint32_t replicated_frames;
	uint32_t eliminated_frames;
	uint32_t passed_frames;
	sys_snode_t node;
};

/**
 * @brief FRER instance
 */
struct ieee802_1cb_instance {
	sys_slist_t streams;
	struct k_mutex mutex;
	struct net_if *iface;
};

/**
 * @brief Initialize IEEE 802.1CB FRER for a network interface
 * @param iface Network interface
 * @return 0 on success, negative error code on failure
 */
int ieee802_1cb_init(struct net_if *iface);

/**
 * @brief Add a stream to FRER processing
 * @param iface Network interface
 * @param config Stream configuration
 * @return 0 on success, negative error code on failure
 */
int ieee802_1cb_stream_add(struct net_if *iface, 
			   const struct ieee802_1cb_stream_config *config);

/**
 * @brief Remove a stream from FRER processing
 * @param iface Network interface
 * @param stream_handle Stream handle to remove
 * @return 0 on success, negative error code on failure
 */
int ieee802_1cb_stream_remove(struct net_if *iface, uint16_t stream_handle);

/**
 * @brief Process frame replication for outgoing frames
 * @param iface Network interface
 * @param pkt Network packet to process
 * @return 0 on success, negative error code on failure
 */
int ieee802_1cb_replicate_frame(struct net_if *iface, struct net_pkt *pkt);

/**
 * @brief Process frame elimination for incoming frames
 * @param iface Network interface
 * @param pkt Network packet to process
 * @return true if frame should be processed, false if eliminated
 */
bool ieee802_1cb_eliminate_frame(struct net_if *iface, struct net_pkt *pkt);

/**
 * @brief Get stream statistics
 * @param iface Network interface
 * @param stream_handle Stream handle
 * @param stats Pointer to store statistics
 * @return 0 on success, negative error code on failure
 */
int ieee802_1cb_get_stream_stats(struct net_if *iface, uint16_t stream_handle,
				 struct ieee802_1cb_stream **stats);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_NET_IEEE802_1CB_H_ */