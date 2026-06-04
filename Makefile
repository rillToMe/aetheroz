# ==========================================
# Kyuzen OS Build System
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
CFLAGS = --target=i686-pc-none-elf -m32 -ffreestanding -O0 -Wall -Wextra -I$(INCLUDE_DIR)
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

# Pastikan logo.png ada di baris pertama dan di perintah 'cp'
boot_image.iso: $(TARGET) limine.conf logo.png
	mkdir -p iso_root
	cp $(TARGET) limine.conf logo.png limine/limine-bios.sys limine/limine-bios-cd.bin iso_root/
	xorriso -as mkisofs -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table -o boot_image.iso iso_root/
	./limine/limine.exe bios-install boot_image.iso

# Tahap 4: Boot up QEMU (Sekarang pakai CD-ROM untuk Boot, dan Hard Disk untuk Data!)
run: boot_image.iso
	$(QEMU) -cpu max -m 256M -cdrom boot_image.iso -drive file=disk.img,format=raw,index=0,media=disk

# Bersihkan file hasil build
clean:
	rm -f $(OBJS) $(TARGET)