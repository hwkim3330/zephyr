/*
 * Copyright (c) 2025 TSN Implementation Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/ieee802_1cb.h>
#include <zephyr/net/ieee802_1qbv.h>
#include <zephyr/random/random.h>

LOG_MODULE_REGISTER(tsn_demo, LOG_LEVEL_DBG);

/* Demo configuration */
#define DEMO_THREAD_STACK_SIZE 2048
#define DEMO_THREAD_PRIORITY 5
#define DEMO_SEND_INTERVAL_MS 100
#define DEMO_PACKET_SIZE 1000

/* Demo state */
static bool demo_running = false;
static struct net_if *demo_iface = NULL;
static struct k_thread demo_thread;
static K_THREAD_STACK_DEFINE(demo_thread_stack, DEMO_THREAD_STACK_SIZE);

/* Demo statistics */
static struct {
	uint32_t packets_sent;
	uint32_t packets_failed;
	uint32_t high_priority_sent;
	uint32_t low_priority_sent;
} demo_stats;

static int send_test_packet(struct net_if *iface, uint8_t traffic_class, 
			    const uint8_t *dest_mac, uint16_t vlan_id)
{
	struct net_pkt *pkt;
	struct net_buf *frag;
	struct net_eth_hdr *eth_hdr;
	struct net_eth_vlan_hdr *vlan_hdr;
	uint8_t *payload;
	int ret;
	
	/* Allocate packet */
	pkt = net_pkt_alloc_with_buffer(iface, DEMO_PACKET_SIZE + 
					sizeof(struct net_eth_hdr) + 
					sizeof(struct net_eth_vlan_hdr),
					AF_UNSPEC, 0, K_MSEC(100));
	if (!pkt) {
		LOG_ERR("Failed to allocate packet");
		return -ENOMEM;
	}
	
	/* Set traffic class priority */
	net_pkt_set_priority(pkt, traffic_class);
	
	frag = pkt->frags;
	
	/* Add Ethernet header */
	eth_hdr = (struct net_eth_hdr *)net_buf_add(frag, sizeof(*eth_hdr));
	memcpy(eth_hdr->dst.addr, dest_mac, 6);
	
	/* Get source MAC from interface */
	struct net_linkaddr *link_addr = net_if_get_link_addr(iface);
	if (link_addr && link_addr->len == 6) {
		memcpy(eth_hdr->src.addr, link_addr->addr, 6);
	} else {
		/* Use a default MAC if not available */
		memcpy(eth_hdr->src.addr, 
		       (uint8_t[]){0x02, 0x00, 0x00, 0x12, 0x34, 0x56}, 6);
	}
	
	/* Add VLAN header if VLAN ID is specified */
	if (vlan_id != 0) {
		eth_hdr->type = htons(NET_ETH_PTYPE_VLAN);
		vlan_hdr = (struct net_eth_vlan_hdr *)net_buf_add(frag, sizeof(*vlan_hdr));
		vlan_hdr->vlan.tci = htons((traffic_class << 13) | vlan_id);
		vlan_hdr->type = htons(0x0800); /* IPv4 */
	} else {
		eth_hdr->type = htons(0x0800); /* IPv4 */
	}
	
	/* Add test payload */
	payload = net_buf_add(frag, DEMO_PACKET_SIZE);
	for (int i = 0; i < DEMO_PACKET_SIZE; i++) {
		payload[i] = (uint8_t)(i & 0xFF);
	}
	
	/* Add timestamp and sequence number to payload */
	uint32_t timestamp = k_uptime_get_32();
	uint32_t seq_num = demo_stats.packets_sent;
	memcpy(&payload[0], &timestamp, sizeof(timestamp));
	memcpy(&payload[4], &seq_num, sizeof(seq_num));
	
	/* Send packet */
	ret = net_send_data(pkt);
	if (ret < 0) {
		LOG_ERR("Failed to send packet: %d", ret);
		net_pkt_unref(pkt);
		demo_stats.packets_failed++;
		return ret;
	}
	
	demo_stats.packets_sent++;
	if (traffic_class >= 6) {
		demo_stats.high_priority_sent++;
	} else {
		demo_stats.low_priority_sent++;
	}
	
	LOG_DBG("Sent packet TC%u, seq %u, size %u", 
		traffic_class, seq_num, DEMO_PACKET_SIZE);
	
	return 0;
}

static void demo_print_stats(void)
{
	LOG_INF("=== TSN Demo Statistics ===");
	LOG_INF("Total packets sent: %u", demo_stats.packets_sent);
	LOG_INF("Failed packets: %u", demo_stats.packets_failed);
	LOG_INF("High priority (TC6-7): %u", demo_stats.high_priority_sent);
	LOG_INF("Low priority (TC0-5): %u", demo_stats.low_priority_sent);
	
	/* Print TAS gate states if available */
	if (IS_ENABLED(CONFIG_NET_L2_IEEE802_1QBV)) {
		bool gate_states[8];
		int ret = ieee802_1qbv_get_gate_states(demo_iface, gate_states);
		if (ret == 0) {
			LOG_INF("Current TAS Gate States:");
			for (int i = 0; i < 8; i++) {
				LOG_INF("  TC%d: %s", i, 
					gate_states[i] ? "OPEN" : "CLOSED");
			}
		}
	}
	
	LOG_INF("==========================");
}

static void demo_thread_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	
	uint32_t packet_count = 0;
	
	LOG_INF("TSN demo thread started");
	
	/* Demo MAC addresses for different streams */
	const uint8_t stream1_mac[] = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E};
	const uint8_t stream2_mac[] = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x0F};
	const uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	
	while (demo_running) {
		/* Send different types of traffic to demonstrate TSN features */
		
		/* High priority traffic (TC7) - should get through TAS gates first */
		if (packet_count % 4 == 0) {
			send_test_packet(demo_iface, 7, stream1_mac, 100);
		}
		
		/* Medium priority traffic (TC4) */
		if (packet_count % 3 == 0) {
			send_test_packet(demo_iface, 4, stream2_mac, 200);
		}
		
		/* Low priority traffic (TC1) */
		if (packet_count % 2 == 0) {
			send_test_packet(demo_iface, 1, broadcast_mac, 0);
		}
		
		/* Background traffic (TC0) */
		send_test_packet(demo_iface, 0, broadcast_mac, 0);
		
		packet_count++;
		
		/* Print statistics every 50 packets */
		if (packet_count % 50 == 0) {
			demo_print_stats();
		}
		
		k_sleep(K_MSEC(DEMO_SEND_INTERVAL_MS));
	}
	
	LOG_INF("TSN demo thread stopped");
}

int tsn_demo_start(struct net_if *iface)
{
	if (!iface) {
		LOG_ERR("Invalid network interface");
		return -EINVAL;
	}
	
	if (demo_running) {
		LOG_WRN("Demo already running");
		return -EALREADY;
	}
	
	/* Reset statistics */
	memset(&demo_stats, 0, sizeof(demo_stats));
	
	demo_iface = iface;
	demo_running = true;
	
	/* Start demo thread */
	k_thread_create(&demo_thread, demo_thread_stack,
			K_THREAD_STACK_SIZEOF(demo_thread_stack),
			demo_thread_entry, NULL, NULL, NULL,
			DEMO_THREAD_PRIORITY, 0, K_NO_WAIT);
	
	k_thread_name_set(&demo_thread, "tsn_demo");
	
	LOG_INF("TSN demo started on interface %p", iface);
	LOG_INF("Sending packets every %d ms", DEMO_SEND_INTERVAL_MS);
	
	return 0;
}

void tsn_demo_stop(void)
{
	if (!demo_running) {
		LOG_WRN("Demo not running");
		return;
	}
	
	demo_running = false;
	
	/* Wait for thread to finish */
	k_thread_join(&demo_thread, K_SECONDS(5));
	
	/* Print final statistics */
	demo_print_stats();
	
	LOG_INF("TSN demo stopped");
}