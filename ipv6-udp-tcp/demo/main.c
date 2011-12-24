/*
 * main.c
 *
 *  Created on: 23.12.2011
 *      Author: me
 */

#include <inc/hw_types.h>
#include <inc/hw_memmap.h>

#include <driverlib/rom.h>
#include <driverlib/systick.h>
#include <driverlib/sysctl.h>
#include <driverlib/gpio.h>

#include <ethernetdriver.h>

#include <lwip/netif.h>
#include <lwip/udp.h>
#include <lwip/tcp.h>
#include <lwip/nd6.h>

static volatile char tickFlag = 0;

/* Tick every 100ms */
#define TICKPERSECOND 100

void SystickISR() {
	/* Send the flag to "userspace" */
	tickFlag = 1;
}

static void recv_udp(void *arg, struct udp_pcb *pcb, struct pbuf *p, ip6_addr_t *addr, u16_t port) {
	void *data = p->payload;

	struct pbuf *p_s = pbuf_alloc(PBUF_TRANSPORT, p->len, PBUF_RAM);

	memcpy(p_s->payload, data, p->len);

	udp_sendto(pcb, p_s, (ip_addr_t*)addr, port);

	pbuf_free(p);
	pbuf_free(p_s);
}

static err_t recv_tcp(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
	void *data = p->payload;

	tcp_recved(pcb, p->tot_len);

	tcp_write(pcb, data, p->len, TCP_WRITE_FLAG_COPY);

	pbuf_free(p);

	return ERR_OK;
}

static err_t accept_tcp(void *arg, struct tcp_pcb *newpcb, err_t err) {
	tcp_recv(newpcb, &recv_tcp);
	return ERR_OK;
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

	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
	GPIOPinConfigure(GPIO_PF2_LED1);
	GPIOPinConfigure(GPIO_PF3_LED0);
	GPIOPinTypeEthernetLED(GPIO_PORTF_BASE, GPIO_PIN_2 | GPIO_PIN_3);

	/** Create the "connection" for incoming data */
	struct udp_pcb *conn_udp = udp_new();
	udp_bind(conn_udp, IP_ADDR_ANY, 7);
	conn_udp->isipv6 = 1;
	udp_recv(conn_udp, (udp_recv_fn) &recv_udp, NULL);

	struct tcp_pcb *conn_tcp = tcp_new();
	tcp_bind(conn_tcp, IP_ADDR_ANY, 7);
	conn_tcp->isipv6 = 1;
	conn_tcp = tcp_listen_with_backlog(conn_tcp, 2);
	tcp_accept(conn_tcp, &accept_tcp);

	int nd6_counter = 0;
	int tcp_counter = 0;
	ROM_SysTickEnable();
	while (1) {
		/* We got a Tick-Interrupt: do periodic work */
		if (1 == tickFlag) {
			tickFlag = 0;
			/* Call the nd6-code once every second */
			if ((nd6_counter % TICKPERSECOND) == 0) {
				nd6_tmr();
				nd6_counter = 0;
			} else
				nd6_counter++;

			if ((tcp_counter % (TICKPERSECOND/4) == 0)) {
				tcp_tmr();
				tcp_counter = 0;
			} else
				tcp_counter++;
		}

		/* Check for received data */
		eth_receive_data();
	}
}
