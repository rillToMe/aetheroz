/**
 * @file kyuzen_netif.c
 * @brief lwIP network interface driver for Kyuzen OS.
 *
 * This file implements the "glue layer" between the lwIP stack and your
 * hardware NIC driver (e.g., Intel e1000 / RTL8139).
 *
 * Call graph overview:
 *
 *   Kernel init
 *     └─ kyuzen_netif_init()          ← registers the netif with lwIP
 *          ├─ low_level_init()         ← programs the NIC hardware
 *          └─ netif->output    = etharp_output   (set by ethernetif_init)
 *             netif->linkoutput= low_level_output
 *
 *   NIC interrupt / polling path
 *     └─ kyuzen_netif_input()         ← called by your IRQ/polling handler
 *          └─ ethernet_input()         ← lwIP demultiplexes frame (ARP/IP)
 *
 *   Timer tick (PIT / APIC interrupt)
 *     └─ sys_check_timeouts()         ← drives TCP retransmits, ARP expiry
 *
 * Build this file with:
 *   clang -target x86_64-pc-none-elf  \
 *         -ffreestanding -fno-builtin \
 *         -mno-red-zone -mcmodel=kernel \
 *         -std=c11 -O2                 \
 *         -Idrivers/net/lwip/port      \
 *         -Idrivers/net/lwip/src/include \
 *         -c drivers/net/lwip/port/kyuzen_netif.c
 */

#include <stdint.h>
#include <stddef.h>

/* Kyuzen OS freestanding string utilities (memcpy, memset) */
#include "../../../../include/string.h"

/* lwIP core headers --------------------------------------------------------*/
#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/etharp.h"
#include "lwip/netif.h"
#include "netif/ethernet.h"

/* ===========================================================================
 * CONSTANTS & CONFIGURATION
 * =========================================================================*/

/** Ethernet MTU — standard 1500-byte payload limit. */
#define KYUZEN_NETIF_MTU            1500U

/**
 * Placeholder MAC address.
 * Replace with the actual bytes read from your NIC's EEPROM/register.
 * Format: { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF }
 */
static const uint8_t KYUZEN_MAC_ADDR[6] = {
    0x52, 0x54, 0x00, 0x12, 0x34, 0x56   /* QEMU/KVM safe unicast address */
};

/* ===========================================================================
 * HARDWARE ABSTRACTION — STUB FUNCTIONS
 *
 * These stubs are the ONLY parts you need to replace with real NIC driver
 * code. Everything above this line is portable lwIP glue.
 * =========================================================================*/

/**
 * @brief  (STUB) Read the MAC address from the NIC hardware.
 * @param  out_mac  Buffer of exactly 6 bytes to receive the MAC.
 *
 * Replace this function body with real MMIO / I/O port register reads for
 * your target NIC (e1000: RAL/RAH registers; RTL8139: IDR0-IDR5 I/O ports).
 */
static void hw_read_mac(uint8_t out_mac[6]) {
    /* TODO: read MAC from NIC registers. For now, copy the compile-time stub */
    memcpy(out_mac, KYUZEN_MAC_ADDR, 6);
}

/**
 * @brief  (STUB) Send a raw Ethernet frame to the NIC transmit ring.
 * @param  data   Pointer to the frame bytes (Ethernet header + payload).
 * @param  length Frame length in bytes (must be ≤ KYUZEN_NETIF_MTU + 14).
 * @return 0 on success, non-zero on error.
 *
 * Replace with your NIC's TX descriptor ring write + doorbell kick.
 * For e1000: write to TDT (Tail Descriptor Pointer) after filling the TX desc.
 * For RTL8139: copy to a TX buffer and write its physical address + length
 * to TxStatus / TxAddr registers.
 */
static int hw_send_frame(const void *data, uint16_t length) {
    /* TODO: DMA / PIO the frame to the NIC TX ring */
    (void)data;
    (void)length;
    return 0; /* 0 = success */
}

/* ===========================================================================
 * LOW-LEVEL INIT
 * =========================================================================*/

/**
 * @brief  Initialises the hardware and populates the lwIP netif structure.
 * @param  netif  The lwIP network interface descriptor being registered.
 *
 * Called once by kyuzen_netif_init() via netif_add(). Performs:
 *  1. MAC address programming
 *  2. NIC hardware initialisation (MTU, promiscuous mode, etc.)
 *  3. Populates netif->hwaddr, netif->mtu, netif->flags
 */
static void low_level_init(struct netif *netif) {
    /* --- 1. MAC address -------------------------------------------------- */
    netif->hwaddr_len = ETH_HWADDR_LEN;           /* 6 bytes */
    hw_read_mac(netif->hwaddr);

    /* --- 2. MTU ---------------------------------------------------------- */
    netif->mtu = KYUZEN_NETIF_MTU;

    /* --- 3. Capabilities flags ------------------------------------------- */
    /*
     * NETIF_FLAG_BROADCAST: supports broadcast frames
     * NETIF_FLAG_ETHARP:    ARP over Ethernet enabled
     * NETIF_FLAG_ETHERNET:  this is an Ethernet interface
     * NETIF_FLAG_LINK_UP:   treat link as up (set dynamically once you have
     *                        real link-state detection from the PHY).
     */
    netif->flags = NETIF_FLAG_BROADCAST |
                   NETIF_FLAG_ETHARP    |
                   NETIF_FLAG_ETHERNET  |
                   NETIF_FLAG_LINK_UP;

    /* --- 4. NIC hardware bringup ---------------------------------------- */
    /*
     * TODO: Call your NIC initialisation routine here, e.g.:
     *   e1000_init();        — reset controller, set up RX/TX rings, enable IRQ
     *   rtl8139_init();
     */
}

/* ===========================================================================
 * LOW-LEVEL OUTPUT  (lwIP → NIC)
 * =========================================================================*/

/**
 * @brief  Transmits an outgoing Ethernet frame from the lwIP stack to the NIC.
 * @param  netif  The network interface sending the frame.
 * @param  p      The pbuf chain containing the complete Ethernet frame.
 * @return ERR_OK on success, ERR_IF on hardware error.
 *
 * This is the linkoutput callback registered on the netif. lwIP guarantees
 * that @p p already contains a valid Ethernet header (built by etharp_output
 * → ethernet_output). We only need to copy it to the NIC.
 *
 * lwIP may give us a pbuf *chain* (multiple segments). We walk the chain
 * and either copy to a single linear buffer (safe, simple) or pass each
 * segment as a gather-list scatter/gather DMA descriptor (advanced).
 */
static err_t low_level_output(struct netif *netif, struct pbuf *p) {
    (void)netif; /* netif is unused in this simple implementation */

    /*
     * Strategy: copy the entire pbuf chain into a single contiguous scratch
     * buffer, then hand it off to the NIC in one DMA operation.
     *
     * For a production driver you would use scatter/gather DMA to avoid the
     * copy — each pbuf->payload segment maps directly to a TX descriptor.
     */

    /* Stack-allocated scratch buffer. 1518 = max Ethernet frame (no VLAN). */
    uint8_t scratch[1518];
    uint16_t total_len = 0;

    /* Walk the pbuf chain */
    for (struct pbuf *q = p; q != NULL; q = q->next) {
        if (total_len + q->len > sizeof(scratch)) {
            /* Frame too large — should never happen with a correct MTU */
            return ERR_IF;
        }
        memcpy(scratch + total_len, q->payload, q->len);
        total_len += (uint16_t)q->len;
    }

    /* Pad to minimum Ethernet frame size (60 bytes payload + 4 CRC = 64).
     * Most NICs handle this automatically; include it here for safety. */
    if (total_len < 60U) {
        memset(scratch + total_len, 0, 60U - total_len);
        total_len = 60U;
    }

    /* Hand off to the NIC hardware stub */
    if (hw_send_frame(scratch, total_len) != 0) {
        /* Update lwIP stats on TX error */
        MIB2_STATS_NETIF_INC(netif, ifouterrors);
        return ERR_IF;
    }

    /* Update lwIP accounting */
    LINK_STATS_INC(link.xmit);
    MIB2_STATS_NETIF_ADD(netif, ifoutoctets, total_len);

    return ERR_OK;
}

/* ===========================================================================
 * PACKET INPUT CALLBACK  (NIC → lwIP)
 * =========================================================================*/

/**
 * @brief  Injects a received Ethernet frame into the lwIP stack.
 * @param  netif   The network interface that received the frame.
 * @param  data    Pointer to the raw frame bytes (Ethernet header + payload).
 * @param  length  Frame length in bytes.
 *
 * Call this function from your NIC interrupt handler or polling loop every
 * time a new frame is available in the RX ring / RX buffer.
 *
 * Example usage in your NIC IRQ handler:
 *
 *   void e1000_irq_handler(void) {
 *       uint8_t  *frame     = rx_ring[rx_tail].buffer_addr;  // DMA buffer
 *       uint16_t  frame_len = rx_ring[rx_tail].length;
 *       kyuzen_netif_input(&g_netif, frame, frame_len);
 *       advance_rx_ring();
 *   }
 */
void kyuzen_netif_input(struct netif *netif, const uint8_t *data,
                        uint16_t length) {
    if (data == NULL || length == 0U || length > 1518U) {
        /* Silently discard malformed frames */
        LINK_STATS_INC(link.lenerr);
        return;
    }

    /*
     * Allocate a pbuf from the pool (PBUF_POOL memory, PBUF_RAW type).
     * PBUF_POOL is the correct type for received frames — it chains pool
     * pbufs together for frames larger than PBUF_POOL_BUFSIZE without an
     * additional heap allocation.
     */
    struct pbuf *p = pbuf_alloc(PBUF_RAW, length, PBUF_POOL);
    if (p == NULL) {
        /* Pool exhausted — drop the frame and update stats */
        LINK_STATS_INC(link.drop);
        MIB2_STATS_NETIF_INC(netif, ifindiscards);
        return;
    }

    /*
     * Copy the raw frame bytes into the pbuf chain.
     * pbuf_take() handles chains: it copies 'length' bytes from the
     * linear 'data' buffer into a (potentially segmented) pbuf chain.
     */
    if (pbuf_take(p, data, length) != ERR_OK) {
        pbuf_free(p);
        LINK_STATS_INC(link.drop);
        return;
    }

    /* Update reception stats */
    LINK_STATS_INC(link.recv);
    MIB2_STATS_NETIF_ADD(netif, ifinoctets, length);

    /*
     * Pass the pbuf to the lwIP Ethernet input function.
     * ethernet_input() will:
     *   - Parse the Ethernet header (ethertype field)
     *   - Dispatch ARP frames to etharp_input()
     *   - Dispatch IPv4 frames to ip4_input()
     *
     * NOTE: ethernet_input() frees the pbuf on success. On error it also
     * frees it internally. We must NOT free p after this call.
     */
    if (netif->input(p, netif) != ERR_OK) {
        /*
         * netif->input returned an error (e.g., the IP module rejected the
         * packet). Strictly, lwIP should have freed p already, but we call
         * pbuf_free() defensively only if we are certain it wasn't freed.
         * The safest policy in NO_SYS mode: trust lwIP's contract and do
         * nothing here.
         */
        LINK_STATS_INC(link.drop);
    }
}

/* ===========================================================================
 * NETIF INIT CALLBACK  (registered with netif_add)
 * =========================================================================*/

/**
 * @brief  lwIP netif initialisation callback — entry point for this driver.
 * @param  netif  The netif structure allocated by lwIP, to be populated.
 * @return ERR_OK always (hardware errors would return ERR_IF).
 *
 * Register this netif from your kernel network subsystem init:
 *
 *   #include "kyuzen_netif.h"   // (forward-declare kyuzen_netif_init)
 *
 *   struct netif g_netif;
 *   ip4_addr_t   ip, mask, gw;
 *
 *   IP4_ADDR(&gw,   192,168,  1,  1);
 *   IP4_ADDR(&ip,   192,168,  1, 10);
 *   IP4_ADDR(&mask, 255,255,255,  0);
 *
 *   // Add the interface (calls kyuzen_netif_init internally)
 *   netif_add(&g_netif, &ip, &mask, &gw, NULL,
 *             kyuzen_netif_init, ethernet_input);
 *
 *   netif_set_default(&g_netif);
 *   netif_set_up(&g_netif);
 *
 *   // Then in your timer/polling loop:
 *   sys_check_timeouts();
 */
err_t kyuzen_netif_init(struct netif *netif) {
    /* Human-readable interface name (2 chars max, visible in debug output) */
    netif->name[0] = 'k';
    netif->name[1] = 'z';

    /*
     * Output callbacks:
     *  - output:      called by lwIP IP layer to build ARP + send
     *  - linkoutput:  called by etharp to hand final frame to us
     */
    netif->output     = etharp_output;
    netif->linkoutput = low_level_output;

    /* Run hardware-level initialisation */
    low_level_init(netif);

    return ERR_OK;
}
