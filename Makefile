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

# Direktori sumber (TAMBAHKAN 'apps' DI SINI)
SRC_DIRS = arch/x86 drivers kernel fs apps
INCLUDE_DIR = include

# Flags (-I$(INCLUDE_DIR) penting agar #include "io.h" tetap jalan)
# --- Flags Compiler ---
CFLAGS = --target=i686-pc-none-elf -m32 -ffreestanding -O2 -nostdlib -mno-sse -mno-sse2 -mno-mmx -msoft-float -I$(INCLUDE_DIR)
ASFLAGS = -f elf32
LDFLAGS = -flavor gnu -T linker.ld -m elf_i386 --build-id=none -nostdlib

# Cari semua file .c dan .asm di dalam SRC_DIRS
C_SOURCES = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
ASM_SOURCES = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.asm))

# Ubah ekstensi sumber menjadi target object (.o)
OBJS = $(C_SOURCES:.c=.o) $(ASM_SOURCES:.asm=.o)

# File output
TARGET = myos.bin

# Default target
all: $(TARGET)

# Tahap 3: Link Semuanya
$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(TARGET)

# Tahap 2: Compile C
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

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

# Bersihkan hanya file objek user_apps (bukan ELF output)
clean-apps:
	$(MAKE) -C user_apps clean

# ISO: tergantung pada kernel + ELF apps (auto-rebuild jika source berubah)
boot_image.iso: $(TARGET) apps limine.conf logo.png
	rm -rf iso_root
	mkdir -p iso_root
	cp $(TARGET) limine.conf logo.png fileman.elf viewer.elf limine/limine-bios.sys limine/limine-bios-cd.bin iso_root/
	xorriso -as mkisofs -R -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table -o boot_image.iso iso_root/
	./limine/limine.exe bios-install boot_image.iso

# Tahap 4: Boot up QEMU (Sekarang pakai CD-ROM untuk Boot, dan Hard Disk untuk Data!)
run: boot_image.iso
	qemu-system-x86_64.exe -cpu max -m 512M -boot d -drive file=disk.img,format=raw,index=0,media=disk -drive file=boot_image.iso,media=cdrom,index=2

# Bersihkan file hasil build
clean:
	rm -f $(OBJS) $(TARGET)