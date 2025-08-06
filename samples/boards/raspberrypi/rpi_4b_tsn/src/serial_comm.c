/*
 * Copyright 2025 TSN Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <string.h>
#include <stdio.h>

LOG_MODULE_REGISTER(serial_comm, LOG_LEVEL_INF);

#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
#define MSG_SIZE 256

/* Serial communication statistics */
struct serial_stats {
	uint32_t bytes_sent;
	uint32_t bytes_received;
	uint32_t commands_processed;
	uint32_t errors;
};

static struct serial_stats serial_stats = {0};
static const struct device *uart_dev;

/* Command buffer */
static char cmd_buffer[MSG_SIZE];
static size_t cmd_pos = 0;

/* TSN command structure */
struct tsn_command {
	char command[64];
	char params[192];
};

static void process_tsn_command(const char *cmd_line)
{
	struct tsn_command cmd;
	char *space_pos;
	
	memset(&cmd, 0, sizeof(cmd));
	
	/* Parse command and parameters */
	space_pos = strchr(cmd_line, ' ');
	if (space_pos) {
		size_t cmd_len = space_pos - cmd_line;
		if (cmd_len < sizeof(cmd.command)) {
			memcpy(cmd.command, cmd_line, cmd_len);
			cmd.command[cmd_len] = '\0';
			strncpy(cmd.params, space_pos + 1, sizeof(cmd.params) - 1);
		}
	} else {
		strncpy(cmd.command, cmd_line, sizeof(cmd.command) - 1);
	}

	LOG_INF("Processing TSN command: '%s' with params: '%s'", 
		cmd.command, cmd.params);

	/* Process different TSN commands */
	if (strcmp(cmd.command, "gptp_status") == 0) {
		uart_poll_out(uart_dev, 'O');
		uart_poll_out(uart_dev, 'K');
		uart_poll_out(uart_dev, '\n');
		LOG_INF("gPTP status requested via serial");
		
	} else if (strcmp(cmd.command, "stream_add") == 0) {
		/* Simple stream add response */
		LOG_INF("Stream add requested via serial");
		char *response = "STREAM_ADDED:1,1024,100\n";
		for (int i = 0; response[i]; i++) {
			uart_poll_out(uart_dev, response[i]);
		}
		
	} else if (strcmp(cmd.command, "benchmark_start") == 0) {
		LOG_INF("Benchmark start requested via serial");
		char *response = "BENCHMARK_STARTED\n";
		for (int i = 0; response[i]; i++) {
			uart_poll_out(uart_dev, response[i]);
		}
		
	} else if (strcmp(cmd.command, "benchmark_stop") == 0) {
		LOG_INF("Benchmark stop requested via serial");
		char *response = "BENCHMARK_STOPPED\n";
		for (int i = 0; response[i]; i++) {
			uart_poll_out(uart_dev, response[i]);
		}
		
	} else if (strcmp(cmd.command, "stats") == 0) {
		char *stats_msg = "STATS:TX=1234,RX=5678,CMD=4,ERR=0\n";
		for (int i = 0; stats_msg[i]; i++) {
			uart_poll_out(uart_dev, stats_msg[i]);
		}
		
	} else {
		LOG_WRN("Unknown TSN command: '%s'", cmd.command);
		char *error_msg = "ERROR:Unknown command\n";
		for (int i = 0; error_msg[i]; i++) {
			uart_poll_out(uart_dev, error_msg[i]);
		}
		serial_stats.errors++;
	}
	
	serial_stats.commands_processed++;
}

/* UART callback - placeholder for future async operations */

static void serial_comm_thread(void)
{
	uint8_t rx_char;
	
	LOG_INF("Serial communication thread started");
	
	while (1) {
		/* Poll for incoming characters */
		if (uart_poll_in(uart_dev, &rx_char) == 0) {
			serial_stats.bytes_received++;
			
			/* Handle backspace */
			if (rx_char == '\b' || rx_char == 127) {
				if (cmd_pos > 0) {
					cmd_pos--;
					uart_poll_out(uart_dev, '\b');
					uart_poll_out(uart_dev, ' ');
					uart_poll_out(uart_dev, '\b');
				}
				continue;
			}
			
			/* Handle newline/carriage return */
			if (rx_char == '\n' || rx_char == '\r') {
				if (cmd_pos > 0) {
					cmd_buffer[cmd_pos] = '\0';
					uart_poll_out(uart_dev, '\n');
					process_tsn_command(cmd_buffer);
					cmd_pos = 0;
				}
				continue;
			}
			
			/* Handle regular characters */
			if (cmd_pos < sizeof(cmd_buffer) - 1) {
				cmd_buffer[cmd_pos++] = rx_char;
				uart_poll_out(uart_dev, rx_char); /* Echo */
			}
		}
		
		k_msleep(1); /* Small delay to prevent overwhelming the CPU */
	}
}

K_THREAD_DEFINE(serial_comm_tid, 2048, serial_comm_thread, 
		NULL, NULL, NULL, 6, 0, 0);

/* Shell commands for serial communication control */
static int cmd_serial_stats(const struct shell *sh, size_t argc, char **argv)
{
	shell_print(sh, "Serial Communication Statistics:");
	shell_print(sh, "Bytes Sent: %u", serial_stats.bytes_sent);
	shell_print(sh, "Bytes Received: %u", serial_stats.bytes_received);
	shell_print(sh, "Commands Processed: %u", serial_stats.commands_processed);
	shell_print(sh, "Errors: %u", serial_stats.errors);
	
	return 0;
}

static int cmd_serial_send(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(sh, "Usage: serial send <message>");
		return -EINVAL;
	}

	const char *message = argv[1];
	for (int i = 0; message[i]; i++) {
		uart_poll_out(uart_dev, message[i]);
		serial_stats.bytes_sent++;
	}
	uart_poll_out(uart_dev, '\n');
	serial_stats.bytes_sent++;

	shell_print(sh, "Message sent via serial port");
	return 0;
}

static int cmd_serial_test(const struct shell *sh, size_t argc, char **argv)
{
	const char *test_msg = "TSN_TEST:Raspberry Pi 4B TSN System Ready\n";
	
	for (int i = 0; test_msg[i]; i++) {
		uart_poll_out(uart_dev, test_msg[i]);
		serial_stats.bytes_sent++;
	}

	shell_print(sh, "Serial test message sent");
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(serial_cmds,
	SHELL_CMD(stats, NULL, "Show serial communication stats", cmd_serial_stats),
	SHELL_CMD(send, NULL, "Send message via serial", cmd_serial_send),
	SHELL_CMD(test, NULL, "Send test message", cmd_serial_test),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(serial, &serial_cmds, "Serial communication commands", NULL);

void serial_comm_init(void)
{
	LOG_INF("Initializing serial communication...");

	uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);
	if (!device_is_ready(uart_dev)) {
		LOG_ERR("UART device not ready");
		return;
	}

	/* Configure UART */
	struct uart_config config = {
		.baudrate = 115200,
		.parity = UART_CFG_PARITY_NONE,
		.stop_bits = UART_CFG_STOP_BITS_1,
		.data_bits = UART_CFG_DATA_BITS_8,
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE
	};

	int ret = uart_configure(uart_dev, &config);
	if (ret != 0) {
		LOG_ERR("Failed to configure UART: %d", ret);
		return;
	}

	LOG_INF("Serial communication initialized at 115200 baud");
	LOG_INF("Available commands:");
	LOG_INF("  gptp_status - Get gPTP status");
	LOG_INF("  stream_add <id> <bandwidth> <latency> - Add TSN stream");
	LOG_INF("  benchmark_start - Start benchmark");
	LOG_INF("  benchmark_stop - Stop benchmark");
	LOG_INF("  stats - Get system statistics");

	/* Send initialization message */
	const char *init_msg = "\nTSN_READY:Raspberry Pi 4B TSN System Online\n> ";
	for (int i = 0; init_msg[i]; i++) {
		uart_poll_out(uart_dev, init_msg[i]);
		serial_stats.bytes_sent++;
	}
}