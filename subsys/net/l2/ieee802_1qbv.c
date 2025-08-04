/*
 * Copyright (c) 2025 TSN Implementation Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_ieee802_1qbv, CONFIG_NET_L2_IEEE802_1QBV_LOG_LEVEL);

#include <zephyr/kernel.h>
#include <zephyr/net/ieee802_1qbv.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/net_time.h>

#define MAX_TSN_INTERFACES 4

static struct ieee802_1qbv_instance qbv_instances[MAX_TSN_INTERFACES];

static struct ieee802_1qbv_instance *get_qbv_instance(struct net_if *iface)
{
	int index = net_if_get_by_iface(iface);
	
	if (index < 0 || index >= MAX_TSN_INTERFACES) {
		return NULL;
	}
	
	return &qbv_instances[index];
}

static uint64_t get_current_time_ns(void)
{
	return (uint64_t)k_uptime_get() * 1000000ULL;  /* Convert ms to ns */
}

static void advance_gate_entry(struct ieee802_1qbv_instance *instance)
{
	struct ieee802_1qbv_gate_control_list *gcl = &instance->config.oper_control_list;
	struct ieee802_1qbv_gate_entry *entry;
	
	if (gcl->num_entries == 0 || !gcl->entries) {
		return;
	}
	
	entry = &gcl->entries[instance->current_entry_index];
	
	/* Update gate states based on current entry */
	for (int i = 0; i < IEEE802_1QBV_MAX_TRAFFIC_CLASSES; i++) {
		if (entry->traffic_class_mask & BIT(i)) {
			gcl->oper_gate_states[i] = 
				(entry->operation == IEEE802_1QBV_GATE_OPEN);
		}
	}
	
	/* Move to next entry */
	instance->current_entry_index = 
		(instance->current_entry_index + 1) % gcl->num_entries;
	
	LOG_DBG("Advanced to gate entry %u, mask=0x%02x, op=%d",
		instance->current_entry_index - 1, entry->traffic_class_mask,
		entry->operation);
}

static void qbv_cycle_timer_handler(struct k_timer *timer)
{
	struct ieee802_1qbv_instance *instance = 
		CONTAINER_OF(timer, struct ieee802_1qbv_instance, cycle_timer);
	struct ieee802_1qbv_gate_control_list *gcl = &instance->config.oper_control_list;
	struct ieee802_1qbv_gate_entry *current_entry;
	uint64_t current_time;
	
	if (!instance->config.gate_enabled || gcl->num_entries == 0) {
		return;
	}
	
	k_mutex_lock(&instance->mutex, K_FOREVER);
	
	current_time = get_current_time_ns();
	instance->current_time_ns = current_time;
	
	current_entry = &gcl->entries[instance->current_entry_index];
	
	/* Check if it's time to advance to the next gate entry */
	uint64_t elapsed_in_cycle = (current_time - instance->cycle_start_time_ns) % 
				    gcl->cycle_time_ns;
	
	/* Simple time-based gate control - in a real implementation,
	 * this would be more sophisticated */
	advance_gate_entry(instance);
	
	/* Schedule next timer based on current entry duration */
	if (current_entry->time_interval_ns > 0) {
		k_timer_start(&instance->cycle_timer, 
			      K_NSEC(current_entry->time_interval_ns), K_NO_WAIT);
	}
	
	k_mutex_unlock(&instance->mutex);
}

int ieee802_1qbv_init(struct net_if *iface)
{
	struct ieee802_1qbv_instance *instance = get_qbv_instance(iface);
	
	if (!instance) {
		return -EINVAL;
	}
	
	memset(instance, 0, sizeof(*instance));
	
	k_mutex_init(&instance->mutex);
	k_timer_init(&instance->cycle_timer, qbv_cycle_timer_handler, NULL);
	
	instance->iface = iface;
	instance->config.tick_granularity_ns = 8000;  /* 8ns default */
	
	/* Initialize all gates as open by default */
	for (int i = 0; i < IEEE802_1QBV_MAX_TRAFFIC_CLASSES; i++) {
		instance->config.admin_control_list.admin_gate_states[i] = true;
		instance->config.oper_control_list.oper_gate_states[i] = true;
	}
	
	LOG_INF("IEEE 802.1Qbv TAS initialized for interface %p", iface);
	
	return 0;
}

int ieee802_1qbv_configure_gates(struct net_if *iface,
				  const struct ieee802_1qbv_gate_control_list *config)
{
	struct ieee802_1qbv_instance *instance = get_qbv_instance(iface);
	struct ieee802_1qbv_gate_control_list *admin_gcl;
	
	if (!instance || !config) {
		return -EINVAL;
	}
	
	if (config->num_entries == 0 || config->num_entries > IEEE802_1QBV_MAX_GATE_ENTRIES) {
		return -EINVAL;
	}
	
	if (config->cycle_time_ns == 0 || config->cycle_time_ns > IEEE802_1QBV_MAX_CYCLE_TIME_NS) {
		return -EINVAL;
	}
	
	k_mutex_lock(&instance->mutex, K_FOREVER);
	
	admin_gcl = &instance->config.admin_control_list;
	
	/* Free existing entries if any */
	if (admin_gcl->entries) {
		k_free(admin_gcl->entries);
	}
	
	/* Allocate and copy new entries */
	admin_gcl->entries = k_malloc(sizeof(struct ieee802_1qbv_gate_entry) * 
				      config->num_entries);
	if (!admin_gcl->entries) {
		k_mutex_unlock(&instance->mutex);
		return -ENOMEM;
	}
	
	memcpy(admin_gcl->entries, config->entries,
	       sizeof(struct ieee802_1qbv_gate_entry) * config->num_entries);
	
	admin_gcl->num_entries = config->num_entries;
	admin_gcl->cycle_time_ns = config->cycle_time_ns;
	admin_gcl->cycle_time_extension_ns = config->cycle_time_extension_ns;
	admin_gcl->base_time_ns = config->base_time_ns;
	
	/* Copy admin gate states */
	memcpy(admin_gcl->admin_gate_states, config->admin_gate_states,
	       sizeof(admin_gcl->admin_gate_states));
	
	k_mutex_unlock(&instance->mutex);
	
	LOG_INF("Configured gate control list with %u entries, cycle time %llu ns",
		config->num_entries, config->cycle_time_ns);
	
	return 0;
}

int ieee802_1qbv_enable(struct net_if *iface, bool enable)
{
	struct ieee802_1qbv_instance *instance = get_qbv_instance(iface);
	struct ieee802_1qbv_gate_control_list *admin_gcl, *oper_gcl;
	
	if (!instance) {
		return -EINVAL;
	}
	
	k_mutex_lock(&instance->mutex, K_FOREVER);
	
	if (enable && !instance->config.gate_enabled) {
		admin_gcl = &instance->config.admin_control_list;
		oper_gcl = &instance->config.oper_control_list;
		
		if (admin_gcl->num_entries == 0) {
			k_mutex_unlock(&instance->mutex);
			LOG_ERR("Cannot enable TAS: no gate control list configured");
			return -EINVAL;
		}
		
		/* Copy admin config to operational config */
		if (oper_gcl->entries) {
			k_free(oper_gcl->entries);
		}
		
		oper_gcl->entries = k_malloc(sizeof(struct ieee802_1qbv_gate_entry) * 
					     admin_gcl->num_entries);
		if (!oper_gcl->entries) {
			k_mutex_unlock(&instance->mutex);
			return -ENOMEM;
		}
		
		memcpy(oper_gcl->entries, admin_gcl->entries,
		       sizeof(struct ieee802_1qbv_gate_entry) * admin_gcl->num_entries);
		
		oper_gcl->num_entries = admin_gcl->num_entries;
		oper_gcl->cycle_time_ns = admin_gcl->cycle_time_ns;
		oper_gcl->cycle_time_extension_ns = admin_gcl->cycle_time_extension_ns;
		oper_gcl->base_time_ns = admin_gcl->base_time_ns;
		
		/* Initialize operational state */
		instance->current_entry_index = 0;
		instance->cycle_start_time_ns = get_current_time_ns();
		
		/* Start with first gate entry */
		if (oper_gcl->entries[0].time_interval_ns > 0) {
			k_timer_start(&instance->cycle_timer,
				      K_NSEC(oper_gcl->entries[0].time_interval_ns),
				      K_NO_WAIT);
		}
		
		instance->config.gate_enabled = true;
		
		LOG_INF("TAS enabled with cycle time %llu ns", oper_gcl->cycle_time_ns);
		
	} else if (!enable && instance->config.gate_enabled) {
		k_timer_stop(&instance->cycle_timer);
		instance->config.gate_enabled = false;
		
		/* Reset all gates to open */
		for (int i = 0; i < IEEE802_1QBV_MAX_TRAFFIC_CLASSES; i++) {
			instance->config.oper_control_list.oper_gate_states[i] = true;
		}
		
		LOG_INF("TAS disabled");
	}
	
	k_mutex_unlock(&instance->mutex);
	
	return 0;
}

bool ieee802_1qbv_is_gate_open(struct net_if *iface, uint8_t traffic_class)
{
	struct ieee802_1qbv_instance *instance = get_qbv_instance(iface);
	bool is_open = true;
	
	if (!instance || traffic_class >= IEEE802_1QBV_MAX_TRAFFIC_CLASSES) {
		return true;  /* Default to open if invalid */
	}
	
	if (!instance->config.gate_enabled) {
		return true;  /* All gates open when TAS is disabled */
	}
	
	k_mutex_lock(&instance->mutex, K_FOREVER);
	is_open = instance->config.oper_control_list.oper_gate_states[traffic_class];
	k_mutex_unlock(&instance->mutex);
	
	return is_open;
}

int ieee802_1qbv_get_gate_states(struct net_if *iface, bool *gate_states)
{
	struct ieee802_1qbv_instance *instance = get_qbv_instance(iface);
	
	if (!instance || !gate_states) {
		return -EINVAL;
	}
	
	k_mutex_lock(&instance->mutex, K_FOREVER);
	
	if (instance->config.gate_enabled) {
		memcpy(gate_states, instance->config.oper_control_list.oper_gate_states,
		       sizeof(bool) * IEEE802_1QBV_MAX_TRAFFIC_CLASSES);
	} else {
		/* All gates open when disabled */
		for (int i = 0; i < IEEE802_1QBV_MAX_TRAFFIC_CLASSES; i++) {
			gate_states[i] = true;
		}
	}
	
	k_mutex_unlock(&instance->mutex);
	
	return 0;
}

int ieee802_1qbv_get_stats(struct net_if *iface, 
			    struct ieee802_1qbv_instance **instance_ptr)
{
	struct ieee802_1qbv_instance *instance = get_qbv_instance(iface);
	
	if (!instance || !instance_ptr) {
		return -EINVAL;
	}
	
	*instance_ptr = instance;
	return 0;
}