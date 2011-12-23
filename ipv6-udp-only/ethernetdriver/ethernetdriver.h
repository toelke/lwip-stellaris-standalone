/*
 * network.h
 *
 *  Created on: 14.11.2011
 *      Author: me
 */

#ifndef NETWORK_H_
#define NETWORK_H_

void initialize_network();
void initialize_network_hardware();
void eth_receive_data();
void EthernetISR();

#endif /* NETWORK_H_ */
