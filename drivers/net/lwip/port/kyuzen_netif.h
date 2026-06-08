/**
 * @file kyuzen_netif.h
 * @brief Public API — Kyuzen OS lwIP ↔ e1000 glue layer.
 *
 * Include ini dari kernel_main() atau net_init() untuk menginisialisasi
 * network stack dan mendapatkan akses ke g_kyuzen_netif global.
 *
 * Typical usage di kernel.c:
 *
 *   #include "drivers/net/lwip/port/kyuzen_netif.h"
 *   #include "lwip/init.h"
 *   #include "lwip/dhcp.h"
 *   #include "lwip/timeouts.h"
 *
 *   void net_init(void) {
 *       lwip_init();
 *
 *       // IP semua 0 = minta DHCP
 *       ip4_addr_t ip = {0}, mask = {0}, gw = {0};
 *
 *       // netif_add() memanggil kyuzen_netif_init() yang:
 *       //   - memanggil e1000_init()
 *       //   - membaca MAC dari EEPROM
 *       //   - set flags + callbacks
 *       netif_add(&g_kyuzen_netif, &ip, &mask, &gw,
 *                 NULL, kyuzen_netif_init, ethernet_input);
 *
 *       netif_set_default(&g_kyuzen_netif);
 *       netif_set_up(&g_kyuzen_netif);
 *
 *       // Mulai DHCP — IP otomatis dari QEMU (10.0.2.15)
 *       dhcp_start(&g_kyuzen_netif);
 *   }
 */

#ifndef KYUZEN_NETIF_H
#define KYUZEN_NETIF_H

#include <stdint.h>

/* Forward-declare lwIP types untuk menghindari full header pull */
struct netif;
typedef int8_t err_t;   /* sesuai lwip/err.h: typedef s8_t err_t */

/* ===========================================================================
 * GLOBAL NETIF INSTANCE
 *
 * Dideklarasikan di kyuzen_netif.c, diakses dari net_init() dan net_ping().
 * Satu instance cukup untuk satu NIC (Kyuzen OS single-NIC model).
 * =========================================================================*/
extern struct netif g_kyuzen_netif;

/* ===========================================================================
 * PUBLIC API
 * =========================================================================*/

/**
 * kyuzen_netif_init() — lwIP netif init callback.
 *
 * Diberikan ke netif_add() sebagai parameter 'init'.
 * Memanggil e1000_init(), membaca MAC dari EEPROM, dan mengisi semua field
 * yang dibutuhkan lwIP (hwaddr, mtu, flags, output, linkoutput).
 *
 * @param  netif  Descriptor lwIP yang akan di-populate.
 * @return ERR_OK jika sukses, ERR_IF jika e1000 tidak ditemukan.
 */
err_t kyuzen_netif_init(struct netif *netif);

#endif /* KYUZEN_NETIF_H */
