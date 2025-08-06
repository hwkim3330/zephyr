/*
 * Copyright 2025 TSN Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Forward declarations */
extern void tsn_app_init(void);
extern void benchmark_init(void);
extern void serial_comm_init(void);

/* GPIO definitions */
#define LED_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

/* Network callback - placeholder for future use */

static void init_networking(void)
{
	struct net_if *iface;

	LOG_INF("Initializing networking...");

	/* Get default network interface */
	iface = net_if_get_default();
	if (!iface) {
		LOG_ERR("No default network interface found");
		return;
	}

	/* Network management setup - placeholder */

	/* Bring up the interface */
	net_if_up(iface);

	LOG_INF("Network interface: %s", net_if_get_device(iface)->name);
	LOG_INF("Hardware address available");
}

static void blink_led(void)
{
	static bool led_state = false;
	
	if (device_is_ready(led.port)) {
		gpio_pin_set_dt(&led, led_state);
		led_state = !led_state;
	}
}

static void status_thread(void)
{
	LOG_INF("Raspberry Pi 4B TSN Application Started");
	LOG_INF("Features: gPTP, Serial Communication, Benchmarking");

	while (1) {
		blink_led();
		k_sleep(K_SECONDS(1));
	}
}

K_THREAD_DEFINE(status_tid, 1024, status_thread, NULL, NULL, NULL, 5, 0, 0);

int main(void)
{
	LOG_INF("Starting Raspberry Pi 4B TSN Application...");

	/* Initialize GPIO */
	if (device_is_ready(led.port)) {
		gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
		LOG_INF("LED initialized");
	} else {
		LOG_ERR("LED GPIO not ready");
	}

	/* Initialize networking */
	init_networking();

	/* Initialize TSN functionality */
	tsn_app_init();

	/* Initialize benchmarking tools */
	benchmark_init();

	/* Initialize serial communication */
	serial_comm_init();

	LOG_INF("All systems initialized successfully");

	return 0;
}