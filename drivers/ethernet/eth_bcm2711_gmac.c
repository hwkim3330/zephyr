/*
 * Copyright (c) 2025 TSN Implementation Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT brcm,bcm2711-genet

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(eth_bcm2711_gmac, CONFIG_ETH_BCM2711_LOG_LEVEL);

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/phy.h>
#include <zephyr/net/mdio.h>
#include <zephyr/net/ieee802_1cb.h>
#include <zephyr/sys/byteorder.h>
#include <ethernet/eth_stats.h>

/* BCM2711 GENET (Gigabit Ethernet) register definitions */
#define GENET_SYS_REV_CTRL		0x00
#define GENET_SYS_PORT_CTRL		0x04
#define GENET_GPHY_CTRL			0x08
#define GENET_SYS_RBUF_FLUSH_CTRL	0x0c
#define GENET_SYS_TBUF_FLUSH_CTRL	0x10

#define GENET_UMAC_CMD			0x808
#define GENET_UMAC_MAC0			0x80c
#define GENET_UMAC_MAC1			0x810
#define GENET_UMAC_MAX_FRAME_LEN	0x814
#define GENET_UMAC_MODE			0x844
#define GENET_UMAC_EEE_CTRL		0x848

#define GENET_INTRL2_0_CPU_STAT		0x200
#define GENET_INTRL2_0_CPU_SET		0x204
#define GENET_INTRL2_0_CPU_CLEAR	0x208
#define GENET_INTRL2_0_CPU_MASK_STATUS	0x20c
#define GENET_INTRL2_0_CPU_MASK_SET	0x210
#define GENET_INTRL2_0_CPU_MASK_CLEAR	0x214

#define GENET_INTRL2_1_CPU_STAT		0x240
#define GENET_INTRL2_1_CPU_SET		0x244
#define GENET_INTRL2_1_CPU_CLEAR	0x248
#define GENET_INTRL2_1_CPU_MASK_STATUS	0x24c
#define GENET_INTRL2_1_CPU_MASK_SET	0x250
#define GENET_INTRL2_1_CPU_MASK_CLEAR	0x254

/* DMA ring registers */
#define GENET_TDMA_RING_CFG		0x1040
#define GENET_TDMA_CTRL			0x1004
#define GENET_TDMA_STATUS		0x1008

#define GENET_RDMA_RING_CFG		0x2040
#define GENET_RDMA_CTRL			0x2004
#define GENET_RDMA_STATUS		0x2008

/* Command register bits */
#define CMD_TX_EN			BIT(0)
#define CMD_RX_EN			BIT(1)
#define CMD_SPEED_10			0
#define CMD_SPEED_100			BIT(2)
#define CMD_SPEED_1000			BIT(3)
#define CMD_PROMISC			BIT(4)
#define CMD_PAD_EN			BIT(5)
#define CMD_CRC_FWD			BIT(6)
#define CMD_PAUSE_FWD			BIT(7)
#define CMD_RX_PAUSE_IGNORE		BIT(8)
#define CMD_TX_ADDR_INS			BIT(9)
#define CMD_HD_EN			BIT(10)
#define CMD_SW_RESET			BIT(13)
#define CMD_LCL_LOOP_EN			BIT(15)
#define CMD_AUTO_CONFIG			BIT(22)
#define CMD_CNTL_FRM_EN			BIT(23)
#define CMD_NO_LEN_CHK			BIT(24)
#define CMD_RMT_LOOP_EN			BIT(25)
#define CMD_PRBL_EN			BIT(26)
#define CMD_TX_PAUSE_IGNORE		BIT(28)
#define CMD_TX_RX_EN			BIT(29)
#define CMD_RUNT_FILTER_DIS		BIT(30)

/* Interrupt bits */
#define UMAC_IRQ_SCB			BIT(0)
#define UMAC_IRQ_EPHY			BIT(1)
#define UMAC_IRQ_PHY_DET_R		BIT(2)
#define UMAC_IRQ_PHY_DET_F		BIT(3)
#define UMAC_IRQ_LINK_UP		BIT(4)
#define UMAC_IRQ_LINK_DOWN		BIT(5)
#define UMAC_IRQ_UMAC			BIT(6)
#define UMAC_IRQ_UMAC_TSV		BIT(7)
#define UMAC_IRQ_TBUF_UNDERRUN		BIT(8)
#define UMAC_IRQ_RBUF_OVERFLOW		BIT(9)
#define UMAC_IRQ_HFB_SM			BIT(10)
#define UMAC_IRQ_HFB_MM			BIT(11)
#define UMAC_IRQ_MPD_R			BIT(12)
#define UMAC_IRQ_RXDMA_MBDONE		BIT(13)
#define UMAC_IRQ_RXDMA_PDONE		BIT(14)
#define UMAC_IRQ_RXDMA_BDONE		BIT(15)
#define UMAC_IRQ_TXDMA_MBDONE		BIT(16)
#define UMAC_IRQ_TXDMA_PDONE		BIT(17)
#define UMAC_IRQ_TXDMA_BDONE		BIT(18)

#define BCM2711_GMAC_MTU		1500
#define BCM2711_GMAC_MAX_FRAME_SIZE	(BCM2711_GMAC_MTU + 18)  /* +ETH+VLAN */

struct bcm2711_gmac_config {
	uintptr_t base_addr;
	const struct device *clock_dev;
	clock_control_subsys_t clock_subsys;
	const struct device *mdio_dev;
	uint8_t phy_addr;
	struct gpio_dt_spec reset_gpio;
	int irq;
	void (*irq_config)(void);
};

struct bcm2711_gmac_data {
	struct net_if *iface;
	uint8_t mac_addr[6];
	struct k_sem tx_sem;
	struct k_sem rx_sem;
	bool link_up;
	uint32_t link_speed;
	bool full_duplex;
	
	/* TSN support */
	bool tsn_enabled;
	struct k_mutex tsn_mutex;
	
	/* Statistics */
	struct net_stats_eth stats;
};

static inline uint32_t bcm2711_read(const struct device *dev, uint32_t offset)
{
	const struct bcm2711_gmac_config *config = dev->config;
	return sys_read32(config->base_addr + offset);
}

static inline void bcm2711_write(const struct device *dev, uint32_t offset, uint32_t value)
{
	const struct bcm2711_gmac_config *config = dev->config;
	sys_write32(value, config->base_addr + offset);
}

static int bcm2711_gmac_tx(const struct device *dev, struct net_pkt *pkt)
{
	struct bcm2711_gmac_data *data = dev->data;
	size_t pkt_len = net_pkt_get_len(pkt);
	uint8_t *tx_buf;
	int ret;
	
	if (pkt_len > BCM2711_GMAC_MAX_FRAME_SIZE) {
		LOG_ERR("Packet too large: %zu bytes", pkt_len);
		return -EMSGSIZE;
	}
	
	if (!data->link_up) {
		LOG_WRN("Transmit attempted while link is down");
		return -ENETDOWN;
	}
	
	/* Process TSN Frame Replication if enabled */
	if (data->tsn_enabled) {
		ret = ieee802_1cb_replicate_frame(data->iface, pkt);
		if (ret < 0) {
			LOG_WRN("TSN frame replication failed: %d", ret);
		}
	}
	
	k_sem_take(&data->tx_sem, K_FOREVER);
	
	/* Simplified DMA transmission - would need proper DMA ring implementation */
	tx_buf = k_malloc(pkt_len);
	if (!tx_buf) {
		k_sem_give(&data->tx_sem);
		return -ENOMEM;
	}
	
	ret = net_pkt_read(pkt, tx_buf, pkt_len);
	if (ret < 0) {
		k_free(tx_buf);
		k_sem_give(&data->tx_sem);
		return ret;
	}
	
	/* TODO: Implement actual DMA transmission */
	LOG_DBG("Transmitting %zu bytes", pkt_len);
	
	data->stats.bytes_tx += pkt_len;
	data->stats.pkts_tx++;
	
	k_free(tx_buf);
	k_sem_give(&data->tx_sem);
	
	return 0;
}

static struct net_pkt *bcm2711_gmac_rx(const struct device *dev)
{
	struct bcm2711_gmac_data *data = dev->data;
	struct net_pkt *pkt = NULL;
	size_t frame_len = 0;
	uint8_t *rx_buf;
	
	/* TODO: Implement actual DMA reception */
	/* This is a simplified placeholder */
	
	if (frame_len == 0) {
		return NULL;
	}
	
	pkt = net_pkt_rx_alloc_with_buffer(data->iface, frame_len, AF_UNSPEC, 0, K_NO_WAIT);
	if (!pkt) {
		LOG_ERR("Failed to allocate RX packet");
		data->stats.rx_dropped++;
		return NULL;
	}
	
	rx_buf = k_malloc(frame_len);
	if (!rx_buf) {
		net_pkt_unref(pkt);
		data->stats.rx_dropped++;
		return NULL;
	}
	
	/* TODO: Copy from DMA buffer */
	
	if (net_pkt_write(pkt, rx_buf, frame_len) < 0) {
		LOG_ERR("Failed to write to RX packet");
		net_pkt_unref(pkt);
		k_free(rx_buf);
		data->stats.rx_dropped++;
		return NULL;
	}
	
	/* Process TSN Frame Elimination if enabled */
	if (data->tsn_enabled) {
		if (!ieee802_1cb_eliminate_frame(data->iface, pkt)) {
			LOG_DBG("Frame eliminated by TSN FRER");
			net_pkt_unref(pkt);
			k_free(rx_buf);
			return NULL;
		}
	}
	
	data->stats.bytes_rx += frame_len;
	data->stats.pkts_rx++;
	
	k_free(rx_buf);
	return pkt;
}

static void bcm2711_gmac_isr(const struct device *dev)
{
	struct bcm2711_gmac_data *data = dev->data;
	uint32_t status0, status1;
	
	status0 = bcm2711_read(dev, GENET_INTRL2_0_CPU_STAT);
	status1 = bcm2711_read(dev, GENET_INTRL2_1_CPU_STAT);
	
	/* Clear interrupts */
	bcm2711_write(dev, GENET_INTRL2_0_CPU_CLEAR, status0);
	bcm2711_write(dev, GENET_INTRL2_1_CPU_CLEAR, status1);
	
	if (status0 & (UMAC_IRQ_RXDMA_BDONE | UMAC_IRQ_RXDMA_PDONE | UMAC_IRQ_RXDMA_MBDONE)) {
		k_sem_give(&data->rx_sem);
	}
	
	if (status0 & UMAC_IRQ_LINK_UP) {
		data->link_up = true;
		LOG_INF("Link up");
	}
	
	if (status0 & UMAC_IRQ_LINK_DOWN) {
		data->link_up = false;
		LOG_INF("Link down");
	}
}

static int bcm2711_gmac_set_config(const struct device *dev,
				   enum ethernet_config_type type,
				   const struct ethernet_config *config)
{
	struct bcm2711_gmac_data *data = dev->data;
	int ret = 0;
	
	switch (type) {
	case ETHERNET_CONFIG_TYPE_MAC_ADDRESS:
		memcpy(data->mac_addr, config->mac_address.addr, 6);
		/* TODO: Program MAC address to hardware */
		break;
		
	case ETHERNET_CONFIG_TYPE_LINK:
		/* Link configuration handled by PHY */
		break;
		
	default:
		ret = -ENOTSUP;
		break;
	}
	
	return ret;
}

static enum ethernet_hw_caps bcm2711_gmac_get_capabilities(const struct device *dev)
{
	return ETHERNET_LINK_10BASE_T | ETHERNET_LINK_100BASE_T | 
	       ETHERNET_LINK_1000BASE_T | ETHERNET_AUTO_NEGOTIATION_SET |
	       ETHERNET_HW_FILTERING | ETHERNET_HW_VLAN;
}

static int bcm2711_gmac_enable_tsn(const struct device *dev, bool enable)
{
	struct bcm2711_gmac_data *data = dev->data;
	int ret = 0;
	
	k_mutex_lock(&data->tsn_mutex, K_FOREVER);
	
	if (enable && !data->tsn_enabled) {
		ret = ieee802_1cb_init(data->iface);
		if (ret == 0) {
			data->tsn_enabled = true;
			LOG_INF("TSN features enabled");
		}
	} else if (!enable && data->tsn_enabled) {
		data->tsn_enabled = false;
		LOG_INF("TSN features disabled");
	}
	
	k_mutex_unlock(&data->tsn_mutex);
	return ret;
}

static const struct ethernet_api bcm2711_gmac_api = {
	.iface_api.init = NULL,
	.get_capabilities = bcm2711_gmac_get_capabilities,
	.set_config = bcm2711_gmac_set_config,
	.send = bcm2711_gmac_tx,
};

static void bcm2711_gmac_iface_init(struct net_if *iface)
{
	const struct device *dev = net_if_get_device(iface);
	struct bcm2711_gmac_data *data = dev->data;
	
	data->iface = iface;
	
	/* Set MAC address */
	net_if_set_link_addr(iface, data->mac_addr, sizeof(data->mac_addr), NET_LINK_ETHERNET);
	
	/* Set MTU */
	net_if_set_mtu(iface, BCM2711_GMAC_MTU);
	
	/* Initialize Ethernet interface */
	ethernet_init(iface);
	
	LOG_INF("BCM2711 GMAC interface initialized");
}

static int bcm2711_gmac_init(const struct device *dev)
{
	const struct bcm2711_gmac_config *config = dev->config;
	struct bcm2711_gmac_data *data = dev->data;
	uint32_t reg_val;
	int ret;
	
	/* Initialize semaphores and mutexes */
	k_sem_init(&data->tx_sem, 1, 1);
	k_sem_init(&data->rx_sem, 0, 1);
	k_mutex_init(&data->tsn_mutex);
	
	/* Enable clock */
	if (config->clock_dev) {
		ret = clock_control_on(config->clock_dev, config->clock_subsys);
		if (ret < 0) {
			LOG_ERR("Failed to enable clock: %d", ret);
			return ret;
		}
	}
	
	/* Reset GPIO if configured */
	if (config->reset_gpio.port) {
		if (!device_is_ready(config->reset_gpio.port)) {
			LOG_ERR("Reset GPIO device not ready");
			return -ENODEV;
		}
		
		ret = gpio_pin_configure_dt(&config->reset_gpio, GPIO_OUTPUT_ACTIVE);
		if (ret < 0) {
			LOG_ERR("Failed to configure reset GPIO: %d", ret);
			return ret;
		}
		
		gpio_pin_set_dt(&config->reset_gpio, 1);
		k_msleep(10);
		gpio_pin_set_dt(&config->reset_gpio, 0);
		k_msleep(10);
	}
	
	/* Software reset */
	reg_val = bcm2711_read(dev, GENET_UMAC_CMD);
	reg_val |= CMD_SW_RESET;
	bcm2711_write(dev, GENET_UMAC_CMD, reg_val);
	
	k_msleep(10);
	
	reg_val = bcm2711_read(dev, GENET_UMAC_CMD);
	reg_val &= ~CMD_SW_RESET;
	bcm2711_write(dev, GENET_UMAC_CMD, reg_val);
	
	/* Set default MAC address */
	data->mac_addr[0] = 0xb8;
	data->mac_addr[1] = 0x27;
	data->mac_addr[2] = 0xeb;
	data->mac_addr[3] = 0x12;
	data->mac_addr[4] = 0x34;
	data->mac_addr[5] = 0x56;
	
	/* Configure UMAC */
	reg_val = CMD_SPEED_1000 | CMD_TX_EN | CMD_RX_EN | CMD_CRC_FWD;
	bcm2711_write(dev, GENET_UMAC_CMD, reg_val);
	
	/* Set maximum frame length */
	bcm2711_write(dev, GENET_UMAC_MAX_FRAME_LEN, BCM2711_GMAC_MAX_FRAME_SIZE);
	
	/* Initialize interrupt handling */
	if (config->irq_config) {
		config->irq_config();
	}
	
	/* Enable interrupts */
	bcm2711_write(dev, GENET_INTRL2_0_CPU_MASK_CLEAR, 
		      UMAC_IRQ_RXDMA_BDONE | UMAC_IRQ_RXDMA_PDONE | 
		      UMAC_IRQ_RXDMA_MBDONE | UMAC_IRQ_LINK_UP | UMAC_IRQ_LINK_DOWN);
	
	LOG_INF("BCM2711 GMAC driver initialized");
	
	return 0;
}

#define BCM2711_GMAC_IRQ_FUNC(n)						\
	static void bcm2711_gmac_irq_config_##n(void)				\
	{									\
		IRQ_CONNECT(DT_INST_IRQN(n),					\
			    DT_INST_IRQ(n, priority),				\
			    bcm2711_gmac_isr,					\
			    DEVICE_DT_INST_GET(n), 0);				\
		irq_enable(DT_INST_IRQN(n));					\
	}

#define BCM2711_GMAC_DEVICE(n)							\
	BCM2711_GMAC_IRQ_FUNC(n)						\
									\
	static const struct bcm2711_gmac_config bcm2711_gmac_config_##n = {	\
		.base_addr = DT_INST_REG_ADDR(n),				\
		.clock_dev = DEVICE_DT_GET_OR_NULL(DT_INST_CLOCKS_CTLR(n)),	\
		.clock_subsys = (clock_control_subsys_t)DT_INST_CLOCKS_CELL(n, name), \
		.mdio_dev = DEVICE_DT_GET_OR_NULL(DT_INST_PHANDLE(n, mdio)),	\
		.phy_addr = DT_INST_PROP_OR(n, phy_addr, 0),			\
		.reset_gpio = GPIO_DT_SPEC_INST_GET_OR(n, reset_gpios, {0}),	\
		.irq = DT_INST_IRQN(n),						\
		.irq_config = bcm2711_gmac_irq_config_##n,			\
	};									\
									\
	static struct bcm2711_gmac_data bcm2711_gmac_data_##n;			\
									\
	ETH_NET_DEVICE_DT_INST_DEFINE(n,					\
				      bcm2711_gmac_init,			\
				      NULL,					\
				      &bcm2711_gmac_data_##n,			\
				      &bcm2711_gmac_config_##n,			\
				      CONFIG_ETH_INIT_PRIORITY,			\
				      &bcm2711_gmac_api,			\
				      BCM2711_GMAC_MTU);

DT_INST_FOREACH_STATUS_OKAY(BCM2711_GMAC_DEVICE)