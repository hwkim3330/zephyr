/*
 * Copyright (c) 2025 TSN Implementation Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_ieee802_1cb, CONFIG_NET_L2_IEEE802_1CB_LOG_LEVEL);

#include <zephyr/kernel.h>
#include <zephyr/net/ieee802_1cb.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/sys/byteorder.h>

static struct ieee802_1cb_instance frer_instances[CONFIG_NET_IF_MAX_IPV4_COUNT];

static struct ieee802_1cb_instance *get_frer_instance(struct net_if *iface)
{
	int index = net_if_get_by_iface(iface);
	
	if (index < 0 || index >= CONFIG_NET_IF_MAX_IPV4_COUNT) {
		return NULL;
	}
	
	return &frer_instances[index];
}

static struct ieee802_1cb_stream *find_stream_by_handle(
	struct ieee802_1cb_instance *instance, uint16_t handle)
{
	struct ieee802_1cb_stream *stream;
	
	SYS_SLIST_FOR_EACH_CONTAINER(&instance->streams, stream, node) {
		if (stream->config.stream_handle == handle) {
			return stream;
		}
	}
	
	return NULL;
}

static bool match_stream_id(const struct ieee802_1cb_stream_config *config,
			    struct net_pkt *pkt)
{
	struct net_eth_hdr *eth_hdr;
	
	switch (config->id_type) {
	case IEEE802_1CB_STREAM_ID_SOURCE_MAC_VLAN:
		eth_hdr = NET_ETH_HDR(pkt);
		if (memcmp(eth_hdr->src.addr, config->id.mac_vlan.mac_addr, 6) == 0) {
			return true;
		}
		break;
		
	case IEEE802_1CB_STREAM_ID_DEST_MAC_VLAN:
		eth_hdr = NET_ETH_HDR(pkt);
		if (memcmp(eth_hdr->dst.addr, config->id.mac_vlan.mac_addr, 6) == 0) {
			return true;
		}
		break;
		
	default:
		break;
	}
	
	return false;
}

static struct ieee802_1cb_stream *find_stream_for_packet(
	struct ieee802_1cb_instance *instance, struct net_pkt *pkt)
{
	struct ieee802_1cb_stream *stream;
	
	SYS_SLIST_FOR_EACH_CONTAINER(&instance->streams, stream, node) {
		if (match_stream_id(&stream->config, pkt)) {
			return stream;
		}
	}
	
	return NULL;
}

static int add_r_tag(struct net_pkt *pkt, uint16_t seq_num)
{
	struct net_buf *frag;
	uint8_t r_tag[IEEE802_1CB_R_TAG_LEN];
	
	/* R-TAG format: 
	 * 2 bytes: EtherType (0x893F for R-TAG)
	 * 2 bytes: Sequence Number
	 * 2 bytes: Reserved/Flags
	 */
	sys_put_be16(0x893F, &r_tag[0]);
	sys_put_be16(seq_num, &r_tag[2]);
	sys_put_be16(0x0000, &r_tag[4]);
	
	frag = net_pkt_get_frag(pkt, K_NO_WAIT);
	if (!frag) {
		return -ENOMEM;
	}
	
	if (net_buf_tailroom(frag) < IEEE802_1CB_R_TAG_LEN) {
		net_pkt_frag_unref(frag);
		return -ENOBUFS;
	}
	
	net_buf_add_mem(frag, r_tag, IEEE802_1CB_R_TAG_LEN);
	net_pkt_frag_insert(pkt, frag);
	
	return 0;
}

static bool parse_r_tag(struct net_pkt *pkt, uint16_t *seq_num)
{
	struct net_buf *frag = pkt->frags;
	uint8_t r_tag[IEEE802_1CB_R_TAG_LEN];
	uint16_t ethertype;
	
	if (!frag || net_buf_frags_len(frag) < IEEE802_1CB_R_TAG_LEN) {
		return false;
	}
	
	if (net_buf_linearize(r_tag, IEEE802_1CB_R_TAG_LEN, frag, 14, 
			      IEEE802_1CB_R_TAG_LEN) < IEEE802_1CB_R_TAG_LEN) {
		return false;
	}
	
	ethertype = sys_get_be16(&r_tag[0]);
	if (ethertype != 0x893F) {
		return false;
	}
	
	*seq_num = sys_get_be16(&r_tag[2]);
	return true;
}

static bool is_duplicate(struct ieee802_1cb_stream *stream, uint16_t seq_num)
{
	for (int i = 0; i < stream->config.history_length; i++) {
		if (stream->history[i].seq_num == seq_num) {
			return true;
		}
	}
	
	return false;
}

static void add_to_history(struct ieee802_1cb_stream *stream, uint16_t seq_num)
{
	stream->history[stream->history_head].seq_num = seq_num;
	stream->history[stream->history_head].timestamp = k_uptime_get_32();
	
	stream->history_head = (stream->history_head + 1) % stream->config.history_length;
}

int ieee802_1cb_init(struct net_if *iface)
{
	struct ieee802_1cb_instance *instance = get_frer_instance(iface);
	
	if (!instance) {
		return -EINVAL;
	}
	
	sys_slist_init(&instance->streams);
	k_mutex_init(&instance->mutex);
	instance->iface = iface;
	
	LOG_INF("IEEE 802.1CB FRER initialized for interface %p", iface);
	
	return 0;
}

int ieee802_1cb_stream_add(struct net_if *iface, 
			   const struct ieee802_1cb_stream_config *config)
{
	struct ieee802_1cb_instance *instance = get_frer_instance(iface);
	struct ieee802_1cb_stream *stream;
	
	if (!instance || !config) {
		return -EINVAL;
	}
	
	if (config->history_length == 0 || config->history_length > 1024) {
		return -EINVAL;
	}
	
	k_mutex_lock(&instance->mutex, K_FOREVER);
	
	if (find_stream_by_handle(instance, config->stream_handle)) {
		k_mutex_unlock(&instance->mutex);
		return -EEXIST;
	}
	
	stream = k_malloc(sizeof(*stream));
	if (!stream) {
		k_mutex_unlock(&instance->mutex);
		return -ENOMEM;
	}
	
	stream->history = k_calloc(config->history_length, 
				   sizeof(struct ieee802_1cb_seq_recovery));
	if (!stream->history) {
		k_free(stream);
		k_mutex_unlock(&instance->mutex);
		return -ENOMEM;
	}
	
	memcpy(&stream->config, config, sizeof(*config));
	stream->history_head = 0;
	stream->next_seq_num = 0;
	stream->replicated_frames = 0;
	stream->eliminated_frames = 0;
	stream->passed_frames = 0;
	
	sys_slist_append(&instance->streams, &stream->node);
	
	k_mutex_unlock(&instance->mutex);
	
	LOG_INF("Added stream %u for FRER processing", config->stream_handle);
	
	return 0;
}

int ieee802_1cb_stream_remove(struct net_if *iface, uint16_t stream_handle)
{
	struct ieee802_1cb_instance *instance = get_frer_instance(iface);
	struct ieee802_1cb_stream *stream;
	
	if (!instance) {
		return -EINVAL;
	}
	
	k_mutex_lock(&instance->mutex, K_FOREVER);
	
	stream = find_stream_by_handle(instance, stream_handle);
	if (!stream) {
		k_mutex_unlock(&instance->mutex);
		return -ENOENT;
	}
	
	sys_slist_remove(&instance->streams, NULL, &stream->node);
	k_free(stream->history);
	k_free(stream);
	
	k_mutex_unlock(&instance->mutex);
	
	LOG_INF("Removed stream %u from FRER processing", stream_handle);
	
	return 0;
}

int ieee802_1cb_replicate_frame(struct net_if *iface, struct net_pkt *pkt)
{
	struct ieee802_1cb_instance *instance = get_frer_instance(iface);
	struct ieee802_1cb_stream *stream;
	int ret;
	
	if (!instance || !pkt) {
		return -EINVAL;
	}
	
	k_mutex_lock(&instance->mutex, K_FOREVER);
	
	stream = find_stream_for_packet(instance, pkt);
	if (!stream || !stream->config.replication_enabled) {
		k_mutex_unlock(&instance->mutex);
		return 0;
	}
	
	ret = add_r_tag(pkt, stream->next_seq_num);
	if (ret == 0) {
		stream->next_seq_num = (stream->next_seq_num + 1) % 
				       (IEEE802_1CB_MAX_SEQUENCE_NUM + 1);
		stream->replicated_frames++;
		
		LOG_DBG("Replicated frame for stream %u, seq %u", 
			stream->config.stream_handle, stream->next_seq_num - 1);
	}
	
	k_mutex_unlock(&instance->mutex);
	
	return ret;
}

bool ieee802_1cb_eliminate_frame(struct net_if *iface, struct net_pkt *pkt)
{
	struct ieee802_1cb_instance *instance = get_frer_instance(iface);
	struct ieee802_1cb_stream *stream;
	uint16_t seq_num;
	bool should_process = true;
	
	if (!instance || !pkt) {
		return true;
	}
	
	if (!parse_r_tag(pkt, &seq_num)) {
		return true;
	}
	
	k_mutex_lock(&instance->mutex, K_FOREVER);
	
	stream = find_stream_for_packet(instance, pkt);
	if (!stream || !stream->config.elimination_enabled) {
		k_mutex_unlock(&instance->mutex);
		return true;
	}
	
	if (is_duplicate(stream, seq_num)) {
		should_process = false;
		stream->eliminated_frames++;
		
		LOG_DBG("Eliminated duplicate frame for stream %u, seq %u", 
			stream->config.stream_handle, seq_num);
	} else {
		add_to_history(stream, seq_num);
		stream->passed_frames++;
		
		LOG_DBG("Passed frame for stream %u, seq %u", 
			stream->config.stream_handle, seq_num);
	}
	
	k_mutex_unlock(&instance->mutex);
	
	return should_process;
}

int ieee802_1cb_get_stream_stats(struct net_if *iface, uint16_t stream_handle,
				 struct ieee802_1cb_stream **stats)
{
	struct ieee802_1cb_instance *instance = get_frer_instance(iface);
	struct ieee802_1cb_stream *stream;
	
	if (!instance || !stats) {
		return -EINVAL;
	}
	
	k_mutex_lock(&instance->mutex, K_FOREVER);
	
	stream = find_stream_by_handle(instance, stream_handle);
	if (!stream) {
		k_mutex_unlock(&instance->mutex);
		return -ENOENT;
	}
	
	*stats = stream;
	
	k_mutex_unlock(&instance->mutex);
	
	return 0;
}