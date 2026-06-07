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

# Direktori sumber (Biarkan tetap seperti ini dulu)
SRC_DIRS = arch/x86 drivers kernel fs apps
INCLUDE_DIR = include

# --- Flags Compiler 64-bit ---
# 1. Target diubah menjadi x86_64
# 2. -m32 DIHAPUS
# 3. DITAMBAHKAN -mno-red-zone (SANGAT PENTING!)
# 4. -mcmodel=kernel: wajib untuk higher-half kernel — mencegah R_X86_64_32
#    relocation error saat simbol berada di atas 4GB (0xFFFFFFFF80000000)
CFLAGS = --target=x86_64-pc-none-elf -ffreestanding -O2 -nostdlib -mcmodel=kernel -mno-red-zone -mno-sse -mno-sse2 -mno-mmx -msoft-float -I$(INCLUDE_DIR)

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
		-drive file=disk.img,format=raw,index=0,media=disk \
		-drive file=boot_image.iso,media=cdrom,index=2 \

		

# Bersihkan file hasil build
clean:
	rm -f $(OBJS) $(TARGET)

# run: boot_image.iso
# 	qemu-system-x86_64.exe -cpu max -m 512M -boot d \
# 		-drive file=disk.img,format=raw,index=0,media=disk \
# 		-drive file=boot_image.iso,media=cdrom,index=2 \
# 		-no-reboot -no-shutdown