/*
 * network.c
 *
 *  Created on: 14.11.2011
 *      Author: me
 */

#include <inc/lm3s9b92.h>
#include <inc/hw_types.h>
#include <inc/hw_memmap.h>
#include <inc/hw_ints.h>
#include <inc/hw_ethernet.h>

#include <driverlib/udma.h>
#include <driverlib/ethernet.h>
#include <driverlib/rom.h>

#include <lwip/init.h>
#include <lwip/udp.h>
#include <lwip/ip6.h>
#include <lwip/mld6.h>
#include <lwip/nd6.h>
#include <lwip/pbuf.h>
#include <ipv6/lwip/ethip6.h>
#include <netif/etharp.h>

static struct pbuf *active_dma;

static volatile char ethRxFlag = 0;

static struct netif interface;

static volatile char dma_running;

#pragma DATA_ALIGN(g_sDMAControlTable, 1024)
tDMAControlTable g_sDMAControlTable[8];

static void initDMA() {
	// Init DMA
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UDMA);
	ROM_uDMAEnable();
	ROM_uDMAControlBaseSet(g_sDMAControlTable);
	uDMAChannelAttributeDisable(UDMA_CHANNEL_ETH0TX, UDMA_ATTR_ALL);
	uDMAChannelControlSet(UDMA_CHANNEL_ETH0TX, UDMA_SIZE_32 | UDMA_SRC_INC_32 | UDMA_DST_INC_NONE | UDMA_ARB_8);
}

static void eth_rx_fifo_clear() {
	unsigned long conf = HWREG(ETH_BASE + MAC_O_RCTL);
	conf &= ~(MAC_RCTL_RXEN);
	HWREG(ETH_BASE + MAC_O_RCTL) = conf;
	conf |= MAC_RCTL_RSTFIFO;
	HWREG(ETH_BASE + MAC_O_RCTL) = conf;
	conf &= ~(MAC_RCTL_RSTFIFO);
	conf |= MAC_RCTL_RXEN;
	HWREG(ETH_BASE + MAC_O_RCTL) = conf;
}

void EthernetISR() {
	unsigned long temp = ROM_EthernetIntStatus(ETH_BASE, 0);
	ROM_EthernetIntClear(ETH_BASE, temp);

	if (temp & ETH_INT_RX) {
		ROM_EthernetIntDisable(ETH_BASE, ETH_INT_RX);
		ethRxFlag = 1;
	}

	if (temp & ETH_INT_RXOF) {
		/* Receive FIFO-Overflow, clear the FIFO */
		eth_rx_fifo_clear();
	}

	/*
	 * Check to see if the Ethernet TX uDMA channel was pending.
	 */
	if (dma_running) {
		/*
		 * Verify the channel transfer is done
		 */
		if (uDMAChannelModeGet(UDMA_CHANNEL_ETH0TX) == UDMA_MODE_STOP) {
			/*
			 * Trigger the transmission of the data.
			 */
			HWREG(ETH_BASE + MAC_O_TR) = MAC_TR_NEWTX;

			/*
			 * Indicate that a packet has been sent.
			 */
			dma_running = 0;
		}
	}
}

static void setMACAddress() {
	unsigned long ulUser0, ulUser1;

	/* Try to get the device MAC address from flash or use a fixed MAC to allow initial configuration */ROM_FlashUserGet(
			&ulUser0, &ulUser1);

	if ((ulUser0 == 0xffffffff) || (ulUser1 == 0xffffffff)) {
		interface.hwaddr[0] = 0xAA;
		interface.hwaddr[1] = 0x00;
		interface.hwaddr[2] = 0x00;
		interface.hwaddr[3] = 0xFF;
		interface.hwaddr[4] = 0xff;
		interface.hwaddr[5] = 0xff;
	} else {
		/* Convert the MAC address from flash into sequence of bytes. */
		interface.hwaddr[0] = ((ulUser0 >> 0) & 0xff);
		interface.hwaddr[1] = ((ulUser0 >> 8) & 0xff);
		interface.hwaddr[2] = ((ulUser0 >> 16) & 0xff);
		interface.hwaddr[3] = ((ulUser1 >> 0) & 0xff);
		interface.hwaddr[4] = ((ulUser1 >> 8) & 0xff);
		interface.hwaddr[5] = ((ulUser1 >> 16) & 0xff);
	}

	/* Program the MAC address. */
	EthernetMACAddrSet(ETH_BASE, interface.hwaddr);
	interface.hwaddr_len = 6;
}

/**
 * Callback for the initialization of the netif.
 *
 * @param netif the newly initialized netif
 * @return An error code
 */
static err_t netif_init_cb(struct netif *netif) {
	netif->ip6_autoconfig_enabled = 1;
	return ERR_OK;
}

/**
 * Send the data p over interface netif
 *
 * @param netif the netif to send the data
 * @param p the pbuf containing the data
 * @return An error code
 */
static err_t eth_output(struct netif *netif, struct pbuf *p) {
	/* Wait for previous DMA (one could also implement queueing here...) */
	while (dma_running)
		;

	/*
	 * Free the memory
	 */
	if (active_dma != NULL) {
		pbuf_free(active_dma);
#ifdef DEBUG_MALLOC
		UARTprintf("%x %d free, %s:%d;\r\n", active_dma, debug_ctr++, __FILE__, __LINE__);
#endif
	}

	active_dma = p;
	dma_running = 1;
	/* Reserve the pbuf so that it is not free()d */
	pbuf_ref(p);

	char *buf = p->payload;
	int len = p->len;
	uint16_t *len_buf = (uint16_t*)buf;
	*len_buf = len - PBUF_LINK_HLEN;

	len += 2;

	/**
	 * If this enters the infinite loop, the buffer is not correctly aligned!
	 */
	if (((unsigned long) buf & 3) != 0)
		for (;;)
			;

	/* Ensure that DMA sends all the data (and up to 3 bytes garbage extra) */
	while ((len & 3) != 0)
		len++;

	/*
	 * Configure the TX DMA channel to transfer the packet buffer.
	 */
	uDMAChannelTransferSet(UDMA_CHANNEL_ETH0TX, UDMA_MODE_AUTO,
			buf, (void *) (ETH_BASE + MAC_O_DATA), len >> 2);
	/*
	 * Enable the Ethernet Transmit DMA channel.
	 */
	uDMAChannelEnable(UDMA_CHANNEL_ETH0TX);

	/*
	 * Issue a software request to start the channel running.
	 */
	uDMAChannelRequest(UDMA_CHANNEL_ETH0TX);

	return ERR_OK;
}

void initialize_network() {
	lwip_init();

	netif_add(&interface, NULL, NULL, NULL, NULL, netif_init_cb, ip6_input);
	netif_create_ip6_linklocal_address(&interface, 1);
	interface.output_ip6 = ethip6_output;
	interface.linkoutput = eth_output;
	interface.input = ethernet_input;

	netif_set_up(&interface);
	netif_set_default(&interface);

	ip6_addr_t multicast_addr;
	ip6addr_aton("FF02::13C", &multicast_addr);
	mld6_joingroup(IP6_ADDR_ANY, &multicast_addr);
}

void initialize_network_hardware() {
	initDMA();

	/* Enable/Reset the Ethernet Controller */
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_ETH);
	ROM_SysCtlPeripheralReset(SYSCTL_PERIPH_ETH);

	ROM_EthernetIntDisable(ETH_BASE,
			(ETH_INT_PHY | ETH_INT_MDIO | ETH_INT_RXER | ETH_INT_RXOF | ETH_INT_TX | ETH_INT_TXER | ETH_INT_RX));

	/* Clear any interrupts that were already pending. */
	unsigned long temp = ROM_EthernetIntStatus(ETH_BASE, 0);
	ROM_EthernetIntClear(ETH_BASE, temp);

	/* Initialise the MAC and connect. */
	EthernetInit(ETH_BASE);
	ROM_EthernetConfigSet(ETH_BASE, (ETH_CFG_TX_DPLXEN | ETH_CFG_TX_CRCEN | ETH_CFG_TX_PADEN | ETH_CFG_RX_AMULEN));

	setMACAddress();

	ROM_EthernetEnable(ETH_BASE);
	ROM_IntEnable(INT_ETH);
	ROM_EthernetIntEnable(ETH_BASE, ETH_INT_RX);
}

/**
 * Receive data from the Ethernet and give it to the lwip-stack
 */
void eth_receive_data() {
	if (ethRxFlag == 0) return;
	ethRxFlag = 0;
	uint32_t firstbytes = HWREG(ETH_BASE + MAC_O_DATA);
	uint32_t length = firstbytes & 0xFFFF;
	struct pbuf *buf = pbuf_alloc(PBUF_RAW, length, PBUF_RAM);

	if (buf == NULL) {
#ifdef DEBUG_MALLOC_MM
		UARTprintf("oom when allocating %d byte at %s:%d\n", length, __FILE__, __LINE__);
		memmap();
#endif
		/* Clear the FIFO */
		eth_rx_fifo_clear();
	} else {
#ifdef DEBUG_MALLOC
		UARTprintf("%x %d alloc %d, %s:%d;\r\n", buf, debug_ctr++, length, __FILE__, __LINE__);
#endif

		*((uint32_t*) buf->payload) = firstbytes;
		uint32_t *bufptr = (uint32_t*) (((uint32_t*) buf->payload) + 1);
		if (length > 4) {
			/* We already read 4 bytes */
			length -= 4;
			unsigned int i;
			for (i = 0; i < length; i += sizeof(uint32_t)) {
				*bufptr = HWREG(ETH_BASE + MAC_O_DATA);
				bufptr++;
			}
			buf->len = length;
			interface.input(buf, &interface);
		} else {
#ifdef DEBUG_MALLOC
			UARTprintf("%x %d free, %s:%d;\r\n", buf, debug_ctr++, __FILE__, __LINE__);
#endif
			pbuf_free(buf);
		}
	}
	ROM_EthernetIntEnable(ETH_BASE, ETH_INT_RX);
}

