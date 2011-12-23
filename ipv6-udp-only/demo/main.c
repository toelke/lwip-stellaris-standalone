/*
 * main.c
 *
 *  Created on: 23.12.2011
 *      Author: me
 */

#include <inc/hw_types.h>

#include <driverlib/rom.h>
#include <driverlib/sysctl.h>

#include <ethernetdriver.h>

#include <lwip/netif.h>
#include <lwip/udp.h>
#include <lwip/nd6.h>

static volatile char tickFlag = 0;

/* Tick every 100ms */
#define TICKPERSECOND 10

void SystickISR() {
	/* Send the flag to "userspace" */
	tickFlag = 1;
}

static void recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, ip6_addr_t *addr, u16_t port) {
	void *data = p->payload;

	struct pbuf *p_s = pbuf_alloc(PBUF_TRANSPORT, p->len, PBUF_RAM);

	memcpy(p_s->payload, data, p->len);

	udp_sendto(pcb, p_s, (ip_addr_t*)addr, port);
}

int main(void) {
	/* Set the clock to 80MHz */
	ROM_SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);
	ROM_SysTickPeriodSet(ROM_SysCtlClockGet() / TICKPERSECOND);
	SysTickIntRegister(SystickISR);
	ROM_SysTickIntEnable();

	struct netif interface;

	initialize_network_hardware(&interface);
	initialize_network(&interface);

	/** Create the "connection" for incoming data */
	struct udp_pcb *conn = udp_new();
	udp_bind(conn, IP_ADDR_ANY, 7);
	conn->isipv6 = 1;
	udp_recv(conn, (udp_recv_fn) &recv, NULL);

	int counter = 0;
	ROM_SysTickEnable();
	while (1) {
		/* We got a Tick-Interrupt: do periodic work */
		if (1 == tickFlag) {
			tickFlag = 0;
			/* Call the nd6-code once every second */
			if ((counter % TICKPERSECOND) == 0) {
				nd6_tmr();
				counter = 0;
			} else
				counter++;
		}

		/* Check for received data */
		eth_receive_data();
	}
}
