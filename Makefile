# ==========================================
# Kyuzen OS Build System
# ==========================================
#
# Target penting:
#   make          → Compile kernel (myos.bin)
#   make apps     → Compile user_apps (fileman.elf, viewer.elf)
#   make boot_image.iso → Build kernel + apps + ISO
#   make run      → Build + Boot di QEMU
#   make clean    → Bersihkan kernel objects
#   make clean-apps → Bersihkan user_apps objects
# ==========================================

# Tools
CC = clang
AS = nasm
LD = ld.lld
QEMU = qemu-system-x86_64.exe

# Direktori sumber kernel (Ring 0)
SRC_DIRS = arch/x86 drivers kernel fs apps

# ==========================================
# lwIP Network Stack
# ==========================================
# Semua file .c dari lwIP core, netif, dan port driver kita.
# File port/sys_arch.c TIDAK diperlukan saat NO_SYS=1 — hanya sys_now()
# yang perlu diimplementasikan di kernel/timer.c atau sejenisnya.
LWIP_CORE_DIR  = drivers/net/lwip/src/core
LWIP_NETIF_DIR = drivers/net/lwip/src/netif
LWIP_PORT_DIR  = drivers/net/lwip/port
LWIP_INC_DIR   = drivers/net/lwip/src/include

# Kumpulkan semua source lwIP secara otomatis
LWIP_CORE_SRCS = $(wildcard $(LWIP_CORE_DIR)/*.c)       \
                 $(wildcard $(LWIP_CORE_DIR)/ipv4/*.c)

# Daftar eksplisit netif yang kita butuhkan:
#   ethernet.c   — Ethernet frame input/output (wajib untuk LWIP_ETHERNET=1)
# File-file berikut SENGAJA TIDAK diinclude:
#   slipif.c     — Serial Line IP (butuh sio_open/sio_send/sio_tryread)
#   zepif.c      — IEEE 802.15.4 ZEP encapsulation
#   bridgeif*.c  — L2 bridge (butuh infrastruktur terpisah)
#   lowpan6*.c   — 6LoWPAN untuk IoT (butuh LWIP_IPV6)
#   ppp/         — Point-to-Point Protocol
LWIP_NETIF_SRCS= $(LWIP_NETIF_DIR)/ethernet.c

LWIP_PORT_SRCS = $(LWIP_PORT_DIR)/kyuzen_netif.c \
                 $(LWIP_PORT_DIR)/sys_arch.c

# Driver e1000 (Intel 82540EM NIC) — dikompilasi dengan CFLAGS kernel biasa
# (tidak perlu lwIP headers, driver ini standalone)
E1000_DIR  = drivers/net/e1000
E1000_SRCS = $(E1000_DIR)/e1000.c
E1000_OBJS = $(E1000_SRCS:.c=.o)

# Gabung semua source lwIP + e1000
LWIP_SRCS      = $(LWIP_CORE_SRCS) $(LWIP_NETIF_SRCS) $(LWIP_PORT_SRCS)

# Object files lwIP + e1000 (keduanya di-link bersama)
LWIP_OBJS      = $(LWIP_SRCS:.c=.o) $(E1000_OBJS)

# LWIP_CFLAGS akan didefinisikan di bawah, setelah CFLAGS kernel tersedia

# --- Flags Compiler 64-bit ---
# 1. Target diubah menjadi x86_64
# 2. -m32 DIHAPUS
# 3. DITAMBAHKAN -mno-red-zone (SANGAT PENTING!)
# 4. -mcmodel=kernel: wajib untuk higher-half kernel — mencegah R_X86_64_32
#    relocation error saat simbol berada di atas 4GB (0xFFFFFFFF80000000)
INCLUDE_DIR = include
CFLAGS = --target=x86_64-pc-none-elf -ffreestanding -O2 -nostdlib -mcmodel=kernel -mno-red-zone -mno-sse -mno-sse2 -mno-mmx -msoft-float -I$(INCLUDE_DIR)

# Flags compiler untuk unit lwIP:
#   - Mewarisi semua flag kernel (freestanding, mcmodel, mno-red-zone, dll.)
#   - Tambahkan path header lwIP dan port Kyuzen
#   - -Wno-error mencegah warning internal lwIP memblok build
LWIP_CFLAGS    = $(CFLAGS) -std=c11 -I$(LWIP_INC_DIR) -I$(LWIP_PORT_DIR) -Wno-error

# --- Flags Assembler ---
# NASM sekarang merakit output 64-bit
ASFLAGS = -f elf64

# --- Flags Linker ---
# LLD sekarang menyatukan file dengan format x86_64
LDFLAGS = -flavor gnu -T linker.ld -m elf_x86_64 --build-id=none -nostdlib

# Cari semua file .c dan .asm di dalam SRC_DIRS
C_SOURCES_RAW = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
ASM_SOURCES = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.asm))

# Exclude file user-space — dikompilasi terpisah oleh user_apps/Makefile,
# BUKAN bagian dari kernel Ring 0 (myos.bin).
#   apps/userlib.c  → berisi int $0x80 syscall wrappers
#   apps/libgui.c   → GUI framework (mendefinisikan font8x16, dll)
C_SOURCES = $(filter-out apps/userlib.c apps/libgui.c,$(C_SOURCES_RAW))

# Ubah ekstensi sumber menjadi target object (.o)
# Kernel + arch + drivers object files
OBJS = $(C_SOURCES:.c=.o) $(ASM_SOURCES:.asm=.o)

# File output
TARGET = myos.bin

# Default target
all: $(TARGET)

# Tahap 3: Link Semuanya
$(TARGET): $(OBJS) $(LWIP_OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(LWIP_OBJS) -o $(TARGET)

# Tahap 2: Compile C (kernel/arch/drivers/fs/apps)
%.o: %.c
	$(CC) $(CFLAGS) -std=c11 -c $< -o $@

# Tahap 2b: Compile lwIP source files
# Aturan eksplisit ini harus muncul SEBELUM aturan generic %.o: %.c
# agar lwIP mendapat LWIP_CFLAGS (termasuk -I path yang benar).
$(LWIP_CORE_DIR)/%.o: $(LWIP_CORE_DIR)/%.c
	$(CC) $(LWIP_CFLAGS) -c $< -o $@

$(LWIP_CORE_DIR)/ipv4/%.o: $(LWIP_CORE_DIR)/ipv4/%.c
	$(CC) $(LWIP_CFLAGS) -c $< -o $@

$(LWIP_NETIF_DIR)/%.o: $(LWIP_NETIF_DIR)/%.c
	$(CC) $(LWIP_CFLAGS) -c $< -o $@

$(LWIP_PORT_DIR)/%.o: $(LWIP_PORT_DIR)/%.c
	$(CC) $(LWIP_CFLAGS) -c $< -o $@

# e1000 driver: pakai CFLAGS kernel biasa (bukan LWIP_CFLAGS)
# e1000.c tidak butuh lwIP headers — hanya kernel headers (heap, string, pci)
$(E1000_DIR)/%.o: $(E1000_DIR)/%.c
	$(CC) $(CFLAGS) -std=c11 -I$(INCLUDE_DIR) -c $< -o $@

# net_init.c: butuh LWIP_CFLAGS karena include lwIP headers (dhcp.h, dns.h, dll)
# Aturan ini OVERRIDE aturan generic %.o:%.c untuk file ini saja.
kernel/net_init.o: kernel/net_init.c
	$(CC) $(LWIP_CFLAGS) -c $< -o $@

# net_ping.c: butuh LWIP_CFLAGS karena include lwIP raw/icmp/dns headers
kernel/net_ping.o: kernel/net_ping.c
	$(CC) $(LWIP_CFLAGS) -c $< -o $@

# Tahap 1: Compile Assembly
%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

# --- USER APPS (ELF Terpisah, dimuat oleh Kernel via sys_load_elf) ---
# Panggil Makefile di dalam user_apps/ untuk mengompilasi fileman & viewer
.PHONY: apps
apps:
	$(MAKE) -C user_apps all

# Shortcut: bangun ELF secara individual
fileman.elf:
	$(MAKE) -C user_apps fileman

viewer.elf:
	$(MAKE) -C user_apps viewer

clock.elf:
	$(MAKE) -C user_apps clock

calc.elf:
	$(MAKE) -C user_apps calc

taskmgr.elf:
	$(MAKE) -C user_apps taskmgr

notepad.elf:
	$(MAKE) -C user_apps notepad

# Bersihkan hanya file objek user_apps (bukan ELF output)
clean-apps:
	$(MAKE) -C user_apps clean

# ISO: tergantung pada kernel + ELF apps (auto-rebuild jika source berubah)
# Tahap 3: Pembuatan ISO Hybrid (BIOS + UEFI 64-bit)
boot_image.iso: $(TARGET) apps limine.conf kyuzen.png logo.png
	rm -rf iso_root
	mkdir -p iso_root
	# Buat folder EFI untuk standar boot UEFI 64-bit
	mkdir -p iso_root/EFI/BOOT
	cp limine/BOOTX64.EFI iso_root/EFI/BOOT/
	
	# Salin semua kebutuhan (termasuk limine-uefi-cd.bin)
	cp $(TARGET) limine.conf kyuzen.png logo.png fileman.elf viewer.elf clock.elf calc.elf taskmgr.elf notepad.elf limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin iso_root/
	
	# Xorriso sakti: Menggabungkan BIOS dan UEFI ke dalam 1 file ISO!
	xorriso -as mkisofs -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o boot_image.iso
		
	./limine/limine.exe bios-install boot_image.iso

# Tahap 4: Boot up QEMU (Dengan Fitur Debugging 64-bit)
run: boot_image.iso
	qemu-system-x86_64.exe -cpu max -m 512M -boot d \
		-smp 4 \
		-drive file=disk.img,format=raw,index=0,media=disk \
		-drive file=boot_image.iso,media=cdrom,index=2 \
		-nic user,model=e1000

		

# Bersihkan file hasil build (kernel + lwIP objects)
clean:
	rm -f $(OBJS) $(LWIP_OBJS) $(TARGET)

# run: boot_image.iso
# 	qemu-system-x86_64.exe -cpu max -m 512M -boot d \
# 		-drive file=disk.img,format=raw,index=0,media=disk \
# 		-drive file=boot_image.iso,media=cdrom,index=2 \
# 		-no-reboot -no-shutdown
