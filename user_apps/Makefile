# ==========================================================
# Kyuzen OS — User Apps Build System
# ==========================================================
# Menghasilkan ELF apps yang siap dimuat oleh Kernel via
# syscall sys_load_elf(). Setiap app di-link ke zona
# memori yang sudah dipetakan oleh dynamic paging.
#
# Zona Memori User Apps:
#   fileman.elf  →  0xC00000  (PD[3], 12MB)
#   viewer.elf   →  0xE00000  (PD[3], 14MB)
#
# Penggunaan:
#   make          → Compile semua app
#   make fileman  → Compile hanya fileman.elf
#   make viewer   → Compile hanya viewer.elf
#   make clean    → Hapus semua file hasil build
# ==========================================================

# --- Toolchain ---
CC    = clang
LD    = ld.lld

# --- Direktori (relatif dari user_apps/) ---
ROOT_DIR  = ..
APPS_DIR  = $(ROOT_DIR)/apps
INC_DIR   = $(ROOT_DIR)/include
OUT_DIR   = $(ROOT_DIR)

# --- Flags Compiler ---
# -ffreestanding : Tidak pakai libc standar (bare metal)
# -nostdlib      : Jangan link library apapun
# -O2            : Optimasi sedang (aman untuk userlib)
# -O0            : Tanpa optimasi (debug-friendly untuk app utama)
# --- Flags Compiler ---
CFLAGS_COMMON = --target=i686-pc-none-elf -m32 -ffreestanding -nostdlib -mno-sse -mno-sse2 -mno-mmx -msoft-float -I$(INC_DIR)
CFLAGS_APP    = $(CFLAGS_COMMON) -O0
CFLAGS_LIB    = $(CFLAGS_COMMON) -O2

# --- Flags Linker ---
LDFLAGS = -m elf_i386 -nostdlib

# --- File Objek Bersama ---
USERLIB_OBJ = userlib.o

# --- Target ELF Akhir (ditulis ke root project) ---
FILEMAN_ELF = $(OUT_DIR)/fileman.elf
VIEWER_ELF  = $(OUT_DIR)/viewer.elf

# ==========================================================
# TARGET UTAMA
# ==========================================================
.PHONY: all fileman viewer clean

all: $(FILEMAN_ELF) $(VIEWER_ELF)
	@echo ""
	@echo "[USER APPS] Build selesai!"
	@echo "  fileman.elf → $(FILEMAN_ELF)"
	@echo "  viewer.elf  → $(VIEWER_ELF)"

# ==========================================================
# COMPILE SHARED USERLIB
# Dipakai oleh semua app sebagai runtime library Ring 3
# ==========================================================
$(USERLIB_OBJ): $(APPS_DIR)/userlib.c
	@echo "[CC] Mengompilasi userlib..."
	$(CC) $(CFLAGS_LIB) -c $< -o $@

# ==========================================================
# FILEMAN — File Manager (link ke 0xC00000)
# ==========================================================
fileman.o: fileman.c
	@echo "[CC] Mengompilasi fileman.c..."
	$(CC) $(CFLAGS_APP) -c $< -o $@

$(FILEMAN_ELF): fileman.o $(USERLIB_OBJ)
	@echo "[LD] Linking fileman.elf ke 0x4000000..."
	$(LD) $(LDFLAGS) -e main -Ttext 0x4000000 fileman.o $(USERLIB_OBJ) -o $@
	@echo "[OK] fileman.elf siap!"

fileman: $(FILEMAN_ELF)

# ==========================================================
# VIEWER — Image Viewer PNG (link ke 0xE00000)
# ==========================================================
viewer.o: viewer.c
	@echo "[CC] Mengompilasi viewer.c (O2 — stb_image butuh optimasi!)..."
	$(CC) $(CFLAGS_LIB) -c $< -o $@

$(VIEWER_ELF): viewer.o $(USERLIB_OBJ)
	@echo "[LD] Linking viewer.elf ke 0x4400000..."
	$(LD) $(LDFLAGS) -e main -Ttext 0x4400000 viewer.o $(USERLIB_OBJ) -o $@
	@echo "[OK] viewer.elf siap!"

viewer: $(VIEWER_ELF)

# ==========================================================
# CLEAN — Hapus semua file hasil build
# ==========================================================
clean:
	@echo "[CLEAN] Membersihkan file objek user_apps..."
	rm -f fileman.o viewer.o $(USERLIB_OBJ)
	@echo "[CLEAN] Selesai."
