/* WARNING: fos4X File, make sure there settings are still used after updating LWIP */
#ifndef LWIPOPTS_H_
#define LWIPOPTS_H_

#define LWIP_COMPAT_MUTEX               1
#define NO_SYS                          1
#define NO_SYS_NO_TIMERS                1
#define LWIP_IPV6                       1
#define LWIP_IPV6_NUM_ADDRESSES         1
#define LWIP_ETHERNET                   1
#define MEM_LIBC_MALLOC                 1
#define LWIP_IPV6_MLD                   1

/* The Ethernet controller expects two byte before the data */
#define ETH_PAD_SIZE                    2

#define LWIP_SOCKET                     0
#define LWIP_NETCONN                    0
#define LWIP_ARP                        0
#define LWIP_RAW                        0
#define LWIP_TCP                        0
#define LWIP_COMPAT_SOCKETS             0
#define LWIP_STATS                      0
#define LWIP_IPV6_REASS                 0

#endif /* LWIPOPTS_H_ */
