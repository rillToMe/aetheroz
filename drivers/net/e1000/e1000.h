/**
 * @file e1000.h
 * @brief Intel 82540EM (e1000) Gigabit Ethernet Driver — Public API
 *
 * Kyuzen OS — Monolithic 64-bit Higher-Half Kernel
 * Toolchain : Clang/LLVM, -ffreestanding, -mcmodel=kernel, -mno-red-zone
 *
 * Mendukung QEMU virtio -nic user,model=e1000.
 * Tidak ada floating-point, tidak ada libc.
 *
 * Alur lengkap:
 *   kernel_main()
 *     └─ e1000_init()          ← scan PCI, setup MMIO, TX/RX ring, IRQ
 *         └─ e1000_send()      ← dipanggil oleh kyuzen_netif_output()
 *         └─ e1000_poll()      ← dipanggil oleh timer callback tiap tick
 *             └─ ethernet_input() / lwip_input() ← inject ke lwIP stack
 */

#ifndef E1000_H
#define E1000_H

#include <stdint.h>
#include <stddef.h>

/* ===========================================================================
 * PCI IDENTIFIERS
 * =========================================================================*/
#define E1000_VENDOR_ID     0x8086  /* Intel Corporation */
#define E1000_DEVICE_82540EM 0x100E /* 82540EM Gigabit Ethernet Controller */
#define E1000_DEVICE_82545EM 0x100F /* 82545EM (juga umum di QEMU) */
#define E1000_DEVICE_82574L  0x10D3 /* 82574L — dipakai oleh beberapa QEMU build */

/* ===========================================================================
 * MMIO REGISTER OFFSETS (Intel SDM Vol.2 Networking, Tabel 13-3)
 * Semua offset adalah byte offset dari BAR0 base address.
 * =========================================================================*/

/* General Control & Status */
#define E1000_REG_CTRL      0x0000  /* Device Control */
#define E1000_REG_STATUS    0x0008  /* Device Status */
#define E1000_REG_EECD      0x0010  /* EEPROM/Flash Control & Data */
#define E1000_REG_EERD      0x0014  /* EEPROM Read */
#define E1000_REG_CTRL_EXT  0x0018  /* Extended Device Control */
#define E1000_REG_FCAL      0x0028  /* Flow Control Address Low */
#define E1000_REG_FCAH      0x002C  /* Flow Control Address High */
#define E1000_REG_FCT       0x0030  /* Flow Control Type */

/* Interrupt */
#define E1000_REG_ICR       0x00C0  /* Interrupt Cause Read */
#define E1000_REG_ICS       0x00C8  /* Interrupt Cause Set */
#define E1000_REG_IMS       0x00D0  /* Interrupt Mask Set/Read */
#define E1000_REG_IMC       0x00D8  /* Interrupt Mask Clear */

/* Receive Control */
#define E1000_REG_RCTL      0x0100  /* Receive Control */
#define E1000_REG_RDBAL     0x2800  /* RX Descriptor Base Low */
#define E1000_REG_RDBAH     0x2804  /* RX Descriptor Base High */
#define E1000_REG_RDLEN     0x2808  /* RX Descriptor Ring Length */
#define E1000_REG_RDH       0x2810  /* RX Descriptor Head */
#define E1000_REG_RDT       0x2818  /* RX Descriptor Tail */

/* Transmit Control */
#define E1000_REG_TCTL      0x0400  /* Transmit Control */
#define E1000_REG_TIPG      0x0410  /* Transmit Inter-Packet Gap */
#define E1000_REG_TDBAL     0x3800  /* TX Descriptor Base Low */
#define E1000_REG_TDBAH     0x3804  /* TX Descriptor Base High */
#define E1000_REG_TDLEN     0x3808  /* TX Descriptor Ring Length */
#define E1000_REG_TDH       0x3810  /* TX Descriptor Head */
#define E1000_REG_TDT       0x3818  /* TX Descriptor Tail */

/* Receive Address (MAC address) */
#define E1000_REG_RAL0      0x5400  /* Receive Address Low — bits 31:0 dari MAC */
#define E1000_REG_RAH0      0x5404  /* Receive Address High — bits 47:32 + AV bit */

/* ===========================================================================
 * CTRL REGISTER BITS (E1000_REG_CTRL)
 * =========================================================================*/
#define E1000_CTRL_FD       (1U << 0)   /* Full-Duplex */
#define E1000_CTRL_LRST     (1U << 3)   /* Link Reset */
#define E1000_CTRL_ASDE     (1U << 5)   /* Auto-Speed Detection Enable */
#define E1000_CTRL_SLU      (1U << 6)   /* Set Link Up */
#define E1000_CTRL_ILOS     (1U << 7)   /* Invert Loss-Of-Signal */
#define E1000_CTRL_RST      (1U << 26)  /* Device Reset */
#define E1000_CTRL_RFCE     (1U << 27)  /* Receive Flow Control Enable */
#define E1000_CTRL_TFCE     (1U << 28)  /* Transmit Flow Control Enable */
#define E1000_CTRL_VME      (1U << 30)  /* VLAN Mode Enable */
#define E1000_CTRL_PHY_RST  (1U << 31)  /* PHY Reset */

/* ===========================================================================
 * RCTL REGISTER BITS (E1000_REG_RCTL)
 * =========================================================================*/
#define E1000_RCTL_EN       (1U << 1)   /* Receiver Enable */
#define E1000_RCTL_SBP      (1U << 2)   /* Store Bad Packets */
#define E1000_RCTL_UPE      (1U << 3)   /* Unicast Promiscuous Enable */
#define E1000_RCTL_MPE      (1U << 4)   /* Multicast Promiscuous Enable */
#define E1000_RCTL_LPE      (1U << 5)   /* Long Packet Enable (Jumbo frames) */
#define E1000_RCTL_BAM      (1U << 15)  /* Broadcast Accept Mode */
#define E1000_RCTL_BSIZE_2048 (0U << 16) /* Buffer size 2048 bytes */
#define E1000_RCTL_BSIZE_1024 (1U << 16)
#define E1000_RCTL_BSIZE_512  (2U << 16)
#define E1000_RCTL_BSIZE_256  (3U << 16)
#define E1000_RCTL_SECRC    (1U << 26)  /* Strip Ethernet CRC from received packet */

/* ===========================================================================
 * TCTL REGISTER BITS (E1000_REG_TCTL)
 * =========================================================================*/
#define E1000_TCTL_EN       (1U << 1)   /* Transmit Enable */
#define E1000_TCTL_PSP      (1U << 3)   /* Pad Short Packets */
#define E1000_TCTL_CT_SHIFT  4          /* Collision Threshold shift */
#define E1000_TCTL_COLD_SHIFT 12        /* Collision Distance shift */

/* ===========================================================================
 * INTERRUPT CAUSE BITS (E1000_REG_ICR)
 * =========================================================================*/
#define E1000_ICR_TXDW      (1U << 0)   /* TX Descriptor Written Back */
#define E1000_ICR_TXQE      (1U << 1)   /* TX Queue Empty */
#define E1000_ICR_LSC       (1U << 2)   /* Link Status Change */
#define E1000_ICR_RXSEQ     (1U << 3)   /* RX Sequence Error */
#define E1000_ICR_RXDMT0    (1U << 4)   /* RX Descriptor Min. Threshold Reached */
#define E1000_ICR_RXO       (1U << 6)   /* RX Overrun */
#define E1000_ICR_RXT0      (1U << 7)   /* RX Timer Interrupt */

/* ===========================================================================
 * EEPROM READ (EERD) BITS
 * =========================================================================*/
#define E1000_EERD_START    (1U << 0)   /* Start EEPROM read */
#define E1000_EERD_DONE     (1U << 4)   /* Read Done (bit 4 on older chips) */
#define E1000_EERD_DONE_NEW (1U << 1)   /* Done on newer models */
#define E1000_EERD_ADDR_SHIFT 8         /* Address shift */
#define E1000_EERD_DATA_SHIFT 16        /* Data shift */

/* ===========================================================================
 * TX / RX DESCRIPTOR STRUCTURES
 *
 * Format sesuai Intel 82540EM Software Developer's Manual, Section 3.3 & 3.4
 * Kedua struct HARUS 16 byte dan packed — hardware mengaksesnya langsung via DMA.
 * =========================================================================*/

/** TX descriptor (Legacy mode) */
typedef struct __attribute__((packed)) {
    uint64_t addr;          /* Phys address dari TX buffer */
    uint16_t length;        /* Panjang data dalam byte */
    uint8_t  cso;           /* Checksum Offset */
    uint8_t  cmd;           /* Command field (lihat E1000_TXCMD_*) */
    uint8_t  status;        /* Status (TXSTA_DD = done) */
    uint8_t  css;           /* Checksum Start */
    uint16_t special;       /* Special field (VLAN, dll) */
} e1000_tx_desc_t;

/** RX descriptor */
typedef struct __attribute__((packed)) {
    uint64_t addr;          /* Phys address dari RX buffer */
    uint16_t length;        /* Panjang paket yang diterima */
    uint16_t checksum;      /* Checksum (jika hardware offload aktif) */
    uint8_t  status;        /* Status bits */
    uint8_t  errors;        /* Error bits */
    uint16_t special;       /* VLAN tag */
} e1000_rx_desc_t;

/* TX Command bits */
#define E1000_TXCMD_EOP     (1U << 0)   /* End Of Packet */
#define E1000_TXCMD_IFCS    (1U << 1)   /* Insert FCS (CRC) */
#define E1000_TXCMD_RS      (1U << 3)   /* Report Status (set DD bit saat done) */

/* TX Status bits */
#define E1000_TXSTA_DD      (1U << 0)   /* Descriptor Done */

/* RX Status bits */
#define E1000_RXSTA_DD      (1U << 0)   /* Descriptor Done (paket siap dibaca) */
#define E1000_RXSTA_EOP     (1U << 1)   /* End Of Packet */

/* ===========================================================================
 * RING BUFFER SIZING
 *
 * Jumlah descriptor harus kelipatan 8 (hardware requirement).
 * Setiap RX descriptor butuh satu buffer 2048 byte.
 * Total RX memory = E1000_NUM_RX_DESC * 2048 = 16 KB (aman untuk heap kernel)
 * =========================================================================*/
#define E1000_NUM_TX_DESC   16   /* Ukuran TX ring (kelipatan 8) */
#define E1000_NUM_RX_DESC   16   /* Ukuran RX ring (kelipatan 8) */
#define E1000_RX_BUF_SIZE   2048 /* Harus cocok dengan RCTL_BSIZE */

/* ===========================================================================
 * PUBLIC API
 * =========================================================================*/

/**
 * e1000_init() — Inisialisasi driver e1000.
 *
 * Dipanggil dari kernel_main() setelah PCI probe dan sebelum lwip_init().
 * Melakukan: PCI discovery → MMIO map → reset → MAC read →
 *            TX ring init → RX ring init → interrupt enable.
 *
 * @return 0 jika sukses, -1 jika e1000 tidak ditemukan di PCI.
 */
int e1000_init(void);

/**
 * e1000_send() — Kirim satu Ethernet frame.
 *
 * Dipanggil oleh kyuzen_netif_output() dari lwIP stack.
 * Thread-safety: disable interrupt sebelum panggil (sudah dilakukan via
 * SYS_ARCH_PROTECT di cc.h jika relevan), atau pastikan dipanggil dari
 * konteks single-threaded (NO_SYS=1).
 *
 * @param data  Pointer ke buffer frame (Ethernet header + payload).
 * @param len   Panjang total frame dalam byte (max 1514).
 * @return 0 jika sukses, -1 jika TX ring penuh.
 */
int e1000_send(const void *data, uint16_t len);

/**
 * e1000_poll() — Periksa RX ring dan proses paket yang masuk.
 *
 * Dipanggil dari timer callback (setiap PIT tick, ~1ms).
 * Untuk setiap paket yang tersedia di RX ring:
 *   1. Salin ke pbuf lwIP
 *   2. Panggil netif->input() untuk inject ke lwIP stack
 *   3. Return descriptor ke hardware
 *
 * Dalam model NO_SYS=1, ini adalah satu-satunya mekanisme RX processing.
 */
void e1000_poll(void);

/**
 * e1000_get_mac() — Baca MAC address yang dideteksi dari EEPROM.
 *
 * @param out  Buffer 6 byte untuk menerima MAC address.
 */
void e1000_get_mac(uint8_t out[6]);

/**
 * e1000_link_up() — Cek apakah link (kabel) terhubung.
 *
 * @return 1 jika link up, 0 jika link down.
 */
int e1000_link_up(void);

#endif /* E1000_H */
