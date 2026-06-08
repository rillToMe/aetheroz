/**
 * @file kyuzen_netif.h
 * @brief Public API for the Kyuzen OS lwIP network interface driver.
 *
 * Include this header from anywhere in the kernel that needs to:
 *   1. Initialise the network stack  → kyuzen_netif_init()
 *   2. Feed received frames into lwIP → kyuzen_netif_input()
 *
 * Typical kernel network subsystem bootstrap:
 *
 *   #include "network/lwip/port/kyuzen_netif.h"
 *   #include "lwip/init.h"
 *   #include "lwip/timeouts.h"
 *   #include "lwip/netif.h"
 *   #include "netif/ethernet.h"
 *
 *   struct netif g_netif;
 *
 *   void network_init(void) {
 *       lwip_init();
 *
 *       ip4_addr_t ip, mask, gw;
 *       IP4_ADDR(&gw,   192,168,  1,  1);
 *       IP4_ADDR(&ip,   192,168,  1, 10);
 *       IP4_ADDR(&mask, 255,255,255,  0);
 *
 *       netif_add(&g_netif, &ip, &mask, &gw,
 *                 NULL, kyuzen_netif_init, ethernet_input);
 *       netif_set_default(&g_netif);
 *       netif_set_up(&g_netif);
 *   }
 *
 *   // In NIC IRQ handler:
 *   void e1000_irq(void) {
 *       kyuzen_netif_input(&g_netif, rx_buf, rx_len);
 *       sys_check_timeouts();
 *   }
 */

#ifndef KYUZEN_NETIF_H
#define KYUZEN_NETIF_H

#include <stdint.h>

/* Forward-declare lwIP types to avoid pulling in the full lwIP headers
 * from translation units that only need the function signatures. */
struct netif;
typedef int8_t err_t;   /* matches lwip/err.h: typedef s8_t err_t */

/**
 * @brief lwIP netif initialisation callback.
 *
 * Pass this function pointer as the 'init' argument to netif_add().
 * It populates netif->hwaddr, netif->mtu, netif->flags, netif->output,
 * and netif->linkoutput, then calls low_level_init() to bring up the NIC.
 *
 * @param  netif  The network interface descriptor allocated by lwIP.
 * @return ERR_OK (0) on success.
 */
err_t kyuzen_netif_init(struct netif *netif);

/**
 * @brief Inject a raw Ethernet frame received from the NIC into lwIP.
 *
 * Call this from your NIC interrupt handler or polling routine whenever
 * a new frame is available. The function allocates a pbuf, copies the
 * frame data, then calls netif->input (ethernet_input) to demultiplex
 * the frame into ARP or IP processing.
 *
 * @param  netif   The registered lwIP network interface.
 * @param  data    Pointer to the raw frame buffer (Ethernet hdr + payload).
 * @param  length  Total frame length in bytes (max 1518).
 */
void kyuzen_netif_input(struct netif *netif, const uint8_t *data,
                        uint16_t length);

#endif /* KYUZEN_NETIF_H */
