/**
 * @file net_ping.c
 * @brief ICMP Echo Request/Reply (ping) — Kernel implementation, Kyuzen OS.
 *
 * PENTING — CONTEXT MASALAH:
 *   kernel_ping() dipanggil dari syscall_handler() (INT 0x80).
 *   CPU OTOMATIS menghapus IF (Interrupt Flag) saat masuk IDT handler.
 *   Akibatnya, semua `hlt` di sini akan DEADLOCK karena:
 *     - Timer ISR tidak bisa fire (IF=0)
 *     - timer_get_ms() tidak pernah berubah
 *     - hlt menunggu selamanya
 *
 *   FIX: Seluruh wait loop menggunakan `sti; hlt` (bukan hanya `hlt`).
 *   `sti` mengembalikan IF=1 sebelum hlt, sehingga timer ISR bisa fire.
 *   Ini aman karena kita di Ring 0 dan tidak sedang memegang lock apapun.
 *
 * MASALAH ERR_RTE:
 *   raw_sendto() mengembalikan ERR_RTE jika netif tidak punya IP.
 *   Fix: cek ip_addr sebelum ping. Jika 0.0.0.0, tolak dengan pesan error.
 */

#include "lwip/opt.h"
#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/inet_chksum.h"
#include "lwip/ip4.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/dns.h"
#include "lwip/timeouts.h"
#include "lwip/netif.h"
#include "lwip/inet.h"
#include "kyuzen_netif.h"   /* g_kyuzen_netif */

#ifndef IP_PROTO_ICMP
#define IP_PROTO_ICMP 1
#endif

#include <stdint.h>
#include <stddef.h>

/* =========================================================================
 * KERNEL DEPENDENCIES
 * =========================================================================*/

extern void     kprint(const char *str);
extern void     kprint_num(uint64_t num);
extern uint64_t timer_get_ms(void);

/* =========================================================================
 * HELPER: sti_hlt() — sleep satu clock cycle, dengan interrupt enabled.
 *
 * WAJIB digunakan di semua wait loop yang dipanggil dari syscall context.
 * Tanpa `sti`, IF=0 dan hlt akan deadlock selamanya.
 *
 * x86_64 guarantees: `sti` efektif setelah instruksi berikutnya selesai.
 * Dengan `sti; hlt`:
 *   1. sti  → set IF=1
 *   2. hlt  → CPU tidur
 *   3. IRQ fire → CPU bangun, jalankan ISR, return ke sini
 * =========================================================================*/

static inline void sti_hlt(void) {
    __asm__ volatile("sti\n\t hlt" ::: "memory");
}

/* =========================================================================
 * INTERNAL STATE (volatile karena diakses dari ISR context juga)
 * =========================================================================*/

static struct raw_pcb    *ping_pcb      = NULL;
static volatile int       ping_done     = 0;
static volatile uint64_t  ping_rtt_ms   = 0;
static volatile uint16_t  ping_seq      = 0;
static uint64_t           ping_send_time = 0;

static volatile int       dns_done      = 0;
static volatile uint32_t  dns_result    = 0;

/* =========================================================================
 * DNS CALLBACK
 * =========================================================================*/

static void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *arg) {
    (void)name; (void)arg;
    dns_result = (ipaddr != NULL) ? ipaddr->addr : 0;
    dns_done   = 1;
}

/* =========================================================================
 * ICMP RX CALLBACK — dipanggil lwIP saat Echo Reply tiba
 * =========================================================================*/

static uint8_t ping_recv_cb(void *arg, struct raw_pcb *pcb,
                             struct pbuf *p, const ip_addr_t *addr) {
    (void)arg; (void)pcb; (void)addr;

    if (p->len < 20 + sizeof(struct icmp_echo_hdr)) {
        pbuf_free(p);
        return 0;
    }

    struct ip_hdr *iph = (struct ip_hdr *)p->payload;
    uint8_t ip_hdr_len = (uint8_t)(IPH_HL(iph) * 4);

    if (p->len < ip_hdr_len + sizeof(struct icmp_echo_hdr)) {
        pbuf_free(p);
        return 0;
    }

    struct icmp_echo_hdr *icmph =
        (struct icmp_echo_hdr *)((uint8_t *)p->payload + ip_hdr_len);

    if (ICMPH_TYPE(icmph) == ICMP_ER &&
        lwip_ntohs(icmph->seqno) == ping_seq) {
        ping_rtt_ms = timer_get_ms() - ping_send_time;
        ping_done   = 1;
        pbuf_free(p);
        return 1;
    }

    pbuf_free(p);
    return 0;
}

/* =========================================================================
 * SEND ICMP ECHO REQUEST
 * =========================================================================*/

static int ping_send(struct raw_pcb *pcb, const ip_addr_t *dest,
                     const ip_addr_t *src, uint16_t seq) {
    const uint16_t DATA_SIZE  = 32;
    const uint16_t ICMP_TOTAL = (uint16_t)(sizeof(struct icmp_echo_hdr) + DATA_SIZE);

    struct pbuf *p = pbuf_alloc(PBUF_IP, ICMP_TOTAL, PBUF_RAM);
    if (p == NULL) {
        kprint("[ping] ERROR: pbuf_alloc failed\n");
        return -1;
    }

    struct icmp_echo_hdr *icmph = (struct icmp_echo_hdr *)p->payload;
    ICMPH_TYPE_SET(icmph, ICMP_ECHO);
    ICMPH_CODE_SET(icmph, 0);
    icmph->chksum = 0;
    icmph->id     = lwip_htons(0x4B5A);
    icmph->seqno  = lwip_htons(seq);

    uint8_t *data = (uint8_t *)p->payload + sizeof(struct icmp_echo_hdr);
    for (uint16_t i = 0; i < DATA_SIZE; i++) data[i] = (uint8_t)i;

    icmph->chksum = inet_chksum(icmph, ICMP_TOTAL);

    /*
     * Gunakan raw_sendto_if_src() dengan source IP eksplisit.
     * Ini menghindari ERR_RTE karena lwIP tidak perlu mencari route —
     * kita beritahu langsung interface dan source IP yang dipakai.
     */
    err_t err;
    if (src != NULL && !ip4_addr_isany(ip_2_ip4(src))) {
        err = raw_sendto_if_src(pcb, p, dest, &g_kyuzen_netif, src);
    } else {
        err = raw_sendto(pcb, p, dest);
    }
    pbuf_free(p);

    if (err != ERR_OK) {
        kprint("[ping] ERROR: send failed (err=");
        kprint_num((uint64_t)(uint8_t)err);
        kprint(")\n");
        return -1;
    }
    return 0;
}

/* =========================================================================
 * net_print_ip() — cetak IPv4 dari network byte order uint32_t
 * =========================================================================*/

static void net_print_ip(uint32_t addr) {
    kprint_num(addr & 0xFF);        kprint(".");
    kprint_num((addr >> 8)  & 0xFF); kprint(".");
    kprint_num((addr >> 16) & 0xFF); kprint(".");
    kprint_num((addr >> 24) & 0xFF);
}

/* =========================================================================
 * net_ping_once() — kirim satu ICMP Echo, tunggu reply, return RTT
 *
 * Menggunakan sti_hlt() agar interrupts enabled selama menunggu.
 * =========================================================================*/

static int net_ping_once(const ip_addr_t *dest, const ip_addr_t *src,
                         uint32_t timeout_ms) {
    /* Buka raw ICMP PCB */
    ping_pcb = raw_new(IP_PROTO_ICMP);
    if (ping_pcb == NULL) {
        kprint("[ping] ERROR: raw_new() failed\n");
        return -1;
    }

    raw_recv(ping_pcb, ping_recv_cb, NULL);
    raw_bind(ping_pcb, IP_ADDR_ANY);

    /* Kirim Echo Request */
    ping_seq       = (uint16_t)(timer_get_ms() & 0xFFFF);
    ping_done      = 0;
    ping_rtt_ms    = 0;
    ping_send_time = timer_get_ms();

    if (ping_send(ping_pcb, dest, src, ping_seq) != 0) {
        raw_remove(ping_pcb);
        ping_pcb = NULL;
        return -1;
    }

    /* Tunggu Echo Reply — GUNAKAN sti_hlt(), BUKAN hlt biasa */
    uint64_t deadline = timer_get_ms() + timeout_ms;
    while (!ping_done && timer_get_ms() < deadline) {
        sti_hlt();
    }

    raw_remove(ping_pcb);
    ping_pcb = NULL;

    return ping_done ? (int)ping_rtt_ms : -1;
}

/* =========================================================================
 * kernel_ping() — entry point dari syscall #41
 *
 * PENTING: Memanggil `sti` di awal karena INT 0x80 clear IF.
 *          Tanpa ini, semua wait loop deadlock.
 * =========================================================================*/

int kernel_ping(const char *host) {
    if (host == NULL || host[0] == '\0') return -1;

    /*
     * RE-ENABLE INTERRUPTS — WAJIB!
     * INT 0x80 handler masuk dengan IF=0.
     * Kita perlu IF=1 agar:
     *   - timer ISR bisa fire → timer_get_ms() berubah
     *   - e1000_poll() dipanggil via cb_network → paket ICMP diterima
     * Aman karena kita tidak sedang memegang spinlock apapun.
     */
    __asm__ volatile("sti" ::: "memory");

    /* -----------------------------------------------------------------------
     * 1. Cek apakah netif sudah punya IP (DHCP complete)
     * ----------------------------------------------------------------------- */
    if (ip4_addr_isany_val(g_kyuzen_netif.ip_addr)) {
        kprint("[ping] ERROR: No IP address assigned yet.\n");
        kprint("[ping]        DHCP belum selesai. Tunggu beberapa detik lalu coba lagi.\n");
        kprint("[ping]        Tip: Pastikan QEMU dijalankan dengan -nic user,model=e1000\n");
        return -1;
    }

    /* -----------------------------------------------------------------------
     * 2. Resolve hostname → IP
     * ----------------------------------------------------------------------- */
    kprint("[ping] Resolving: ");
    kprint(host); kprint("\n");

    ip_addr_t dest_ip;
    dns_done   = 0;
    dns_result = 0;

    err_t dns_err = dns_gethostbyname(host, &dest_ip, dns_found_cb, NULL);
    if (dns_err == ERR_OK) {
        dns_result = dest_ip.addr;
        dns_done   = 1;
    } else if (dns_err == ERR_INPROGRESS) {
        uint64_t deadline = timer_get_ms() + 5000;
        while (!dns_done && timer_get_ms() < deadline) {
            sti_hlt();
        }
    }

    if (!dns_done || dns_result == 0) {
        kprint("[ping] Host not found: ");
        kprint(host); kprint("\n");
        return -1;
    }

    dest_ip.addr = dns_result;

    /* -----------------------------------------------------------------------
     * 3. Print info dan kirim 4 ping
     * ----------------------------------------------------------------------- */
    ip_addr_t src_ip;
    src_ip.addr = g_kyuzen_netif.ip_addr.addr;

    kprint("[ping] Pinging ");
    net_print_ip(dns_result);
    kprint(" ("); kprint(host); kprint(") from ");
    net_print_ip(src_ip.addr);
    kprint(" with 32 bytes of data:\n");

    int total_rtt = 0;
    int success   = 0;

    for (int i = 0; i < 4; i++) {
        int rtt = net_ping_once(&dest_ip, &src_ip, 4000);

        if (rtt >= 0) {
            kprint("Reply from "); net_print_ip(dns_result);
            kprint(": bytes=32 time=");
            kprint_num((uint64_t)rtt);
            kprint("ms TTL=64\n");
            total_rtt += rtt;
            success++;
        } else {
            kprint("Request timed out.\n");
        }

        /* Jeda 1 detik antara ping — gunakan sti_hlt() */
        if (i < 3) {
            uint64_t wait = timer_get_ms() + 1000;
            while (timer_get_ms() < wait) {
                sti_hlt();
            }
        }
    }

    /* -----------------------------------------------------------------------
     * 4. Statistik
     * ----------------------------------------------------------------------- */
    kprint("\nPing statistics for ");
    net_print_ip(dns_result);
    kprint(":\n  Packets: Sent=4, Received=");
    kprint_num((uint64_t)success);
    kprint(", Lost=");
    kprint_num((uint64_t)(4 - success));
    kprint(" (");
    kprint_num((uint64_t)(4 - success) * 25);
    kprint("% loss)\n");

    if (success > 0) {
        kprint("Approximate round trip times:\n  Average = ");
        kprint_num((uint64_t)(total_rtt / success));
        kprint("ms\n");
        return total_rtt / success;
    }

    return -1;
}
