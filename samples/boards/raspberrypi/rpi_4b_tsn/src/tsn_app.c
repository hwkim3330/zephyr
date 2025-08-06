/*
 * Copyright 2025 TSN Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/net/gptp.h>
#include <zephyr/net/ptp.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/net_if.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(tsn_app, LOG_LEVEL_INF);

static struct gptp_domain *default_domain;
static struct net_if *tsn_iface;

/* TSN Traffic Control */
struct tsn_stream {
	uint16_t stream_id;
	uint32_t bandwidth;
	uint32_t latency_max;
	bool active;
};

static struct tsn_stream streams[8];
static int stream_count = 0;

/* gPTP callback functions */
static void gm_capable_cb(uint8_t *gm_identity, uint16_t time_base, 
			  struct gptp_scaled_ns *last_gm_freq_change)
{
	LOG_INF("Grand Master detected: GM Identity changed");
}

static void sync_cb(struct gptp_scaled_ns *sync_ts, 
		    struct gptp_scaled_ns *sync_correction)
{
	LOG_DBG("Sync timestamp received");
}

static struct gptp_callbacks gptp_cb = {
	.gm_capable = gm_capable_cb,
	.sync_timestamp = sync_cb,
};

static void tsn_stream_config(uint16_t stream_id, uint32_t bandwidth, 
			      uint32_t max_latency)
{
	if (stream_count >= ARRAY_SIZE(streams)) {
		LOG_ERR("Maximum number of streams reached");
		return;
	}

	streams[stream_count].stream_id = stream_id;
	streams[stream_count].bandwidth = bandwidth;
	streams[stream_count].latency_max = max_latency;
	streams[stream_count].active = true;
	stream_count++;

	LOG_INF("TSN Stream %d configured: BW=%d KB/s, Max Latency=%d us", 
		stream_id, bandwidth / 1024, max_latency);
}

static void print_gptp_status(void)
{
	struct net_if *iface = net_if_get_default();
	struct gptp_port_ds *port_ds;
	
	if (!iface) {
		LOG_ERR("No network interface available");
		return;
	}

	port_ds = gptp_get_port_ds(iface, 0);
	if (port_ds) {
		LOG_INF("gPTP Port Status:");
		LOG_INF("  Port State: %s", 
			port_ds->port_state == GPTP_PORT_INITIALIZING ? "INITIALIZING" :
			port_ds->port_state == GPTP_PORT_FAULTY ? "FAULTY" :
			port_ds->port_state == GPTP_PORT_DISABLED ? "DISABLED" :
			port_ds->port_state == GPTP_PORT_LISTENING ? "LISTENING" :
			port_ds->port_state == GPTP_PORT_PRE_MASTER ? "PRE_MASTER" :
			port_ds->port_state == GPTP_PORT_MASTER ? "MASTER" :
			port_ds->port_state == GPTP_PORT_PASSIVE ? "PASSIVE" :
			port_ds->port_state == GPTP_PORT_UNCALIBRATED ? "UNCALIBRATED" :
			port_ds->port_state == GPTP_PORT_SLAVE ? "SLAVE" : "UNKNOWN");
		
		LOG_INF("  AS Capable: %s", port_ds->as_capable ? "Yes" : "No");
		LOG_INF("  Sync Interval: %d", port_ds->log_sync_interval);
		LOG_INF("  Announce Interval: %d", port_ds->log_announce_interval);
	}
}

static void tsn_monitor_thread(void)
{
	while (1) {
		print_gptp_status();
		
		/* Monitor TSN streams */
		for (int i = 0; i < stream_count; i++) {
			if (streams[i].active) {
				LOG_DBG("Stream %d: BW=%d KB/s", 
					streams[i].stream_id, 
					streams[i].bandwidth / 1024);
			}
		}
		
		k_sleep(K_SECONDS(30));
	}
}

K_THREAD_DEFINE(tsn_monitor_tid, 2048, tsn_monitor_thread, NULL, NULL, NULL, 7, 0, 0);

/* Shell commands for TSN control */
static int cmd_gptp_status(const struct shell *sh, size_t argc, char **argv)
{
	print_gptp_status();
	return 0;
}

static int cmd_tsn_stream_add(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 4) {
		shell_error(sh, "Usage: tsn stream add <stream_id> <bandwidth_kbps> <max_latency_us>");
		return -EINVAL;
	}

	uint16_t stream_id = strtoul(argv[1], NULL, 10);
	uint32_t bandwidth = strtoul(argv[2], NULL, 10) * 1024; // Convert to bytes/s
	uint32_t max_latency = strtoul(argv[3], NULL, 10);

	tsn_stream_config(stream_id, bandwidth, max_latency);
	shell_print(sh, "TSN stream %d added successfully", stream_id);
	
	return 0;
}

static int cmd_tsn_stream_list(const struct shell *sh, size_t argc, char **argv)
{
	shell_print(sh, "Active TSN Streams:");
	shell_print(sh, "ID\tBandwidth\tMax Latency\tStatus");
	shell_print(sh, "---\t---------\t-----------\t------");
	
	for (int i = 0; i < stream_count; i++) {
		shell_print(sh, "%d\t%d KB/s\t\t%d us\t\t%s",
			    streams[i].stream_id,
			    streams[i].bandwidth / 1024,
			    streams[i].latency_max,
			    streams[i].active ? "Active" : "Inactive");
	}
	
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(tsn_stream_cmds,
	SHELL_CMD(add, NULL, "Add TSN stream", cmd_tsn_stream_add),
	SHELL_CMD(list, NULL, "List TSN streams", cmd_tsn_stream_list),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(tsn_cmds,
	SHELL_CMD(gptp, NULL, "Show gPTP status", cmd_gptp_status),
	SHELL_CMD(stream, &tsn_stream_cmds, "TSN stream management", NULL),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(tsn, &tsn_cmds, "TSN commands", NULL);

void tsn_app_init(void)
{
	LOG_INF("Initializing TSN application...");

	/* Get default network interface for TSN */
	tsn_iface = net_if_get_default();
	if (!tsn_iface) {
		LOG_ERR("No network interface available for TSN");
		return;
	}

	/* Initialize gPTP */
	default_domain = gptp_get_domain();
	if (default_domain) {
		gptp_register_callbacks(&gptp_cb);
		LOG_INF("gPTP initialized successfully");
	} else {
		LOG_ERR("Failed to initialize gPTP");
	}

	/* Configure some default TSN streams */
	tsn_stream_config(1, 1024 * 1024, 100);  // 1MB/s, 100us max latency
	tsn_stream_config(2, 512 * 1024, 500);   // 512KB/s, 500us max latency
	
	LOG_INF("TSN application initialized with %d streams", stream_count);
}