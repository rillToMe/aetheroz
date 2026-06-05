# ==========================================================
# Kyuzen OS — User Apps Build System (64-bit)
# ==========================================================
# Menghasilkan ELF64 apps yang siap dimuat oleh Kernel via
# syscall sys_load_elf(). Setiap app di-link ke zona
# memori yang sudah dipetakan oleh dynamic paging.
#
# Zona Memori User Apps:
#   fileman.elf  →  0x4000000  (64 MB)
#   viewer.elf   →  0x4400000  (68 MB)
#   clock.elf    →  0x6000000  (96 MB)  ← jauh dari 72MB PMM lock boundary
#
# Penggunaan:
#   make          → Compile semua app
#   make fileman  → Compile hanya fileman.elf
#   make viewer   → Compile hanya viewer.elf
#   make clock    → Compile hanya clock.elf
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

# --- Flags Compiler 64-bit ---
# 1. Target x86_64-pc-none-elf
# 2. Hapus -m32
# 3. Tambahkan -mno-red-zone (Agar tidak bertabrakan saat terjadi interrupt/syscall)
CFLAGS_COMMON = --target=x86_64-pc-none-elf -ffreestanding -nostdlib -mno-red-zone -mno-sse -mno-sse2 -mno-mmx -msoft-float -I$(INC_DIR)
CFLAGS_APP    = $(CFLAGS_COMMON) -O0
CFLAGS_LIB    = $(CFLAGS_COMMON) -O2

# --- Flags Linker 64-bit ---
# Format output diubah menjadi elf_x86_64
LDFLAGS = -m elf_x86_64 -nostdlib

# --- File Objek Bersama ---
USERLIB_OBJ = userlib.o

# --- Target ELF Akhir (ditulis ke root project) ---
FILEMAN_ELF = $(OUT_DIR)/fileman.elf
VIEWER_ELF  = $(OUT_DIR)/viewer.elf
CLOCK_ELF   = $(OUT_DIR)/clock.elf

# ==========================================================
# TARGET UTAMA
# ==========================================================
.PHONY: all fileman viewer clean

all: $(FILEMAN_ELF) $(VIEWER_ELF) $(CLOCK_ELF)
	@echo ""
	@echo "[USER APPS 64-BIT] Build selesai!"
	@echo "  fileman.elf → $(FILEMAN_ELF)"
	@echo "  viewer.elf  → $(VIEWER_ELF)"
	@echo "  clock.elf   → $(CLOCK_ELF)"

# ==========================================================
# COMPILE SHARED USERLIB
# Dipakai oleh semua app sebagai runtime library Ring 3
# ==========================================================
$(USERLIB_OBJ): $(APPS_DIR)/userlib.c
	@echo "[CC] Mengompilasi userlib (64-bit)..."
	$(CC) $(CFLAGS_LIB) -c $< -o $@

# ==========================================================
# FILEMAN — File Manager (link ke 0x4000000)
# ==========================================================
fileman.o: fileman.c
	@echo "[CC] Mengompilasi fileman.c (64-bit)..."
	$(CC) $(CFLAGS_APP) -c $< -o $@

$(FILEMAN_ELF): fileman.o $(USERLIB_OBJ)
	@echo "[LD] Linking fileman.elf ke 0x4000000..."
	$(LD) $(LDFLAGS) -e main -Ttext 0x4000000 fileman.o $(USERLIB_OBJ) -o $@
	@echo "[OK] fileman.elf siap!"

fileman: $(FILEMAN_ELF)

# ==========================================================
# VIEWER — Image Viewer PNG (link ke 0x4400000)
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
# CLOCK — Jam Digital Real-Time (link ke 0x4800000)
# ==========================================================
clock.o: clock.c
	@echo "[CC] Mengompilasi clock.c (64-bit)..."
	$(CC) $(CFLAGS_APP) -c $< -o $@

$(CLOCK_ELF): clock.o $(USERLIB_OBJ)
	@echo "[LD] Linking clock.elf ke 0x6000000..."
	$(LD) $(LDFLAGS) -e main -Ttext 0x6000000 clock.o $(USERLIB_OBJ) -o $@
	@echo "[OK] clock.elf siap!"

clock: $(CLOCK_ELF)

# ==========================================================
# CLEAN — Hapus semua file hasil build
# ==========================================================
clean:
	@echo "[CLEAN] Membersihkan file objek user_apps..."
	rm -f fileman.o viewer.o clock.o $(USERLIB_OBJ)
	@echo "[CLEAN] Selesai."