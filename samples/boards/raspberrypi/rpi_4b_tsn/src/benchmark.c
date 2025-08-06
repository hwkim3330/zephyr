/*
 * Copyright 2025 TSN Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/socket.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(benchmark, LOG_LEVEL_INF);

/* Benchmark statistics */
struct benchmark_stats {
	uint64_t packets_sent;
	uint64_t packets_received;
	uint64_t bytes_sent;
	uint64_t bytes_received;
	uint32_t min_latency_us;
	uint32_t max_latency_us;
	uint64_t total_latency_us;
	uint32_t packet_loss;
	bool running;
};

static struct benchmark_stats stats = {0};
static struct k_timer benchmark_timer;

/* UDP benchmark configuration */
#define BENCHMARK_PORT 12345
#define BENCHMARK_PAYLOAD_SIZE 1024
#define BENCHMARK_INTERVAL_MS 10

static int benchmark_socket = -1;

static void benchmark_timer_handler(struct k_timer *timer)
{
	static uint8_t payload[BENCHMARK_PAYLOAD_SIZE];
	static uint32_t seq_num = 0;
	struct sockaddr_in dest_addr;
	int64_t timestamp;
	int ret;

	if (!stats.running || benchmark_socket < 0) {
		return;
	}

	/* Add timestamp and sequence number to payload */
	timestamp = k_uptime_get();
	memcpy(payload, &timestamp, sizeof(timestamp));
	memcpy(payload + sizeof(timestamp), &seq_num, sizeof(seq_num));
	seq_num++;

	/* Set destination address (broadcast for testing) */
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(BENCHMARK_PORT);
	dest_addr.sin_addr.s_addr = INADDR_BROADCAST;

	/* Send packet */
	ret = zsock_sendto(benchmark_socket, payload, BENCHMARK_PAYLOAD_SIZE, 0,
			   (struct sockaddr *)&dest_addr, sizeof(dest_addr));
	
	if (ret > 0) {
		stats.packets_sent++;
		stats.bytes_sent += ret;
	} else {
		LOG_WRN("Failed to send benchmark packet: %d", errno);
	}
}

static void benchmark_receiver_thread(void)
{
	struct sockaddr_in server_addr, client_addr;
	socklen_t client_len = sizeof(client_addr);
	uint8_t buffer[BENCHMARK_PAYLOAD_SIZE];
	int64_t rx_timestamp, tx_timestamp;
	uint32_t seq_num;
	int ret;

	LOG_INF("Starting benchmark receiver thread");

	/* Create UDP socket */
	int rx_socket = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (rx_socket < 0) {
		LOG_ERR("Failed to create receiver socket: %d", errno);
		return;
	}

	/* Bind to benchmark port */
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(BENCHMARK_PORT);

	ret = zsock_bind(rx_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (ret < 0) {
		LOG_ERR("Failed to bind receiver socket: %d", errno);
		zsock_close(rx_socket);
		return;
	}

	while (1) {
		ret = zsock_recvfrom(rx_socket, buffer, sizeof(buffer), 0,
				     (struct sockaddr *)&client_addr, &client_len);
		
		if (ret > 0) {
			rx_timestamp = k_uptime_get();
			
			/* Extract timestamp and sequence number */
			memcpy(&tx_timestamp, buffer, sizeof(tx_timestamp));
			memcpy(&seq_num, buffer + sizeof(tx_timestamp), sizeof(seq_num));
			
			/* Calculate latency */
			uint32_t latency_us = (uint32_t)((rx_timestamp - tx_timestamp) * 1000);
			
			/* Update statistics */
			stats.packets_received++;
			stats.bytes_received += ret;
			stats.total_latency_us += latency_us;
			
			if (stats.min_latency_us == 0 || latency_us < stats.min_latency_us) {
				stats.min_latency_us = latency_us;
			}
			if (latency_us > stats.max_latency_us) {
				stats.max_latency_us = latency_us;
			}
			
			LOG_DBG("Received packet %d, latency: %d us", seq_num, latency_us);
		}
		
		k_yield();
	}
	
	zsock_close(rx_socket);
}

K_THREAD_DEFINE(benchmark_rx_tid, 2048, benchmark_receiver_thread, 
		NULL, NULL, NULL, 6, 0, 0);

static void benchmark_report_thread(void)
{
	uint64_t last_packets_sent = 0;
	uint64_t last_packets_received = 0;
	uint64_t last_bytes_sent = 0;
	uint64_t last_bytes_received = 0;

	while (1) {
		k_sleep(K_SECONDS(10));

		if (stats.running && stats.packets_sent > 0) {
			uint64_t pps_sent = (stats.packets_sent - last_packets_sent) / 10;
			uint64_t pps_received = (stats.packets_received - last_packets_received) / 10;
			uint64_t bps_sent = (stats.bytes_sent - last_bytes_sent) / 10;
			uint64_t bps_received = (stats.bytes_received - last_bytes_received) / 10;
			uint32_t avg_latency = stats.packets_received > 0 ? 
				(uint32_t)(stats.total_latency_us / stats.packets_received) : 0;
			uint32_t loss_percent = stats.packets_sent > 0 ?
				(uint32_t)(((stats.packets_sent - stats.packets_received) * 100) / stats.packets_sent) : 0;

			LOG_INF("=== Benchmark Report ===");
			LOG_INF("TX: %lld pps, %lld bytes/s", pps_sent, bps_sent);
			LOG_INF("RX: %lld pps, %lld bytes/s", pps_received, bps_received);
			LOG_INF("Latency: min=%d us, max=%d us, avg=%d us", 
				stats.min_latency_us, stats.max_latency_us, avg_latency);
			LOG_INF("Packet Loss: %d%%", loss_percent);
			
			last_packets_sent = stats.packets_sent;
			last_packets_received = stats.packets_received;
			last_bytes_sent = stats.bytes_sent;
			last_bytes_received = stats.bytes_received;
		}
	}
}

K_THREAD_DEFINE(benchmark_report_tid, 2048, benchmark_report_thread,
		NULL, NULL, NULL, 8, 0, 0);

/* Shell commands */
static int cmd_benchmark_start(const struct shell *sh, size_t argc, char **argv)
{
	if (stats.running) {
		shell_error(sh, "Benchmark already running");
		return -EBUSY;
	}

	/* Create UDP socket for sending */
	benchmark_socket = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (benchmark_socket < 0) {
		shell_error(sh, "Failed to create benchmark socket: %d", errno);
		return -EIO;
	}

	/* Enable broadcast */
	int broadcast = 1;
	zsock_setsockopt(benchmark_socket, SOL_SOCKET, SO_BROADCAST,
			 &broadcast, sizeof(broadcast));

	/* Reset statistics */
	memset(&stats, 0, sizeof(stats));
	stats.running = true;

	/* Start benchmark timer */
	k_timer_start(&benchmark_timer, K_MSEC(BENCHMARK_INTERVAL_MS), 
		      K_MSEC(BENCHMARK_INTERVAL_MS));

	shell_print(sh, "Benchmark started - sending %d byte packets every %d ms",
		    BENCHMARK_PAYLOAD_SIZE, BENCHMARK_INTERVAL_MS);
	
	return 0;
}

static int cmd_benchmark_stop(const struct shell *sh, size_t argc, char **argv)
{
	if (!stats.running) {
		shell_error(sh, "Benchmark not running");
		return -EINVAL;
	}

	stats.running = false;
	k_timer_stop(&benchmark_timer);
	
	if (benchmark_socket >= 0) {
		zsock_close(benchmark_socket);
		benchmark_socket = -1;
	}

	shell_print(sh, "Benchmark stopped");
	return 0;
}

static int cmd_benchmark_status(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t avg_latency = stats.packets_received > 0 ? 
		(uint32_t)(stats.total_latency_us / stats.packets_received) : 0;
	uint32_t loss_percent = stats.packets_sent > 0 ?
		(uint32_t)(((stats.packets_sent - stats.packets_received) * 100) / stats.packets_sent) : 0;

	shell_print(sh, "Benchmark Status: %s", stats.running ? "Running" : "Stopped");
	shell_print(sh, "Packets Sent: %lld", stats.packets_sent);
	shell_print(sh, "Packets Received: %lld", stats.packets_received);
	shell_print(sh, "Bytes Sent: %lld", stats.bytes_sent);
	shell_print(sh, "Bytes Received: %lld", stats.bytes_received);
	shell_print(sh, "Min Latency: %d us", stats.min_latency_us);
	shell_print(sh, "Max Latency: %d us", stats.max_latency_us);
	shell_print(sh, "Avg Latency: %d us", avg_latency);
	shell_print(sh, "Packet Loss: %d%%", loss_percent);
	
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(benchmark_cmds,
	SHELL_CMD(start, NULL, "Start benchmark", cmd_benchmark_start),
	SHELL_CMD(stop, NULL, "Stop benchmark", cmd_benchmark_stop),
	SHELL_CMD(status, NULL, "Show benchmark status", cmd_benchmark_status),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(benchmark, &benchmark_cmds, "Network benchmark commands", NULL);

void benchmark_init(void)
{
	LOG_INF("Initializing benchmark tools...");

	k_timer_init(&benchmark_timer, benchmark_timer_handler, NULL);
	benchmark_socket = -1;

	LOG_INF("Benchmark tools initialized");
	LOG_INF("Use 'benchmark start' to begin testing");
}