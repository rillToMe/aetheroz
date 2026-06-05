# ==========================================================
# Kyuzen OS — User Apps Build System (64-bit)
# ==========================================================
# DESAIN BARU: SATU SLOT VIRTUAL UNTUK SEMUA APP
#
# Semua user apps di-link ke virtual address 0x4000000.
# Kernel (ELF loader) yang otomatis mencari physical RAM
# kosong dari PMM dan memetakannya ke virtual 0x4000000.
#
# Saat app selesai (exit), kernel memanggil vmm_unmap_user_space()
# yang MEMBEBASKAN semua physical pages ke PMM — RAM langsung
# tersedia kembali untuk app berikutnya.
#
# Tidak perlu lagi manual menentukan alamat RAM per app!
#
# Zona Virtual Tunggal:
#   SEMUA apps  →  0x4000000  (64 MB virtual)
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
CFLAGS_COMMON = --target=x86_64-pc-none-elf -ffreestanding -nostdlib -mno-red-zone -mno-sse -mno-sse2 -mno-mmx -msoft-float -I$(INC_DIR)
CFLAGS_APP    = $(CFLAGS_COMMON) -O0
CFLAGS_LIB    = $(CFLAGS_COMMON) -O2

# --- Flags Linker 64-bit ---
LDFLAGS = -m elf_x86_64 -nostdlib -T app.ld

# --- Alamat Virtual Tunggal — OS yang alokasi physical RAM otomatis ---
APP_BASE = 0x4000000

# --- File Objek Bersama ---
USERLIB_OBJ = userlib.o

# --- Target ELF Akhir ---
FILEMAN_ELF = $(OUT_DIR)/fileman.elf
VIEWER_ELF  = $(OUT_DIR)/viewer.elf
CLOCK_ELF   = $(OUT_DIR)/clock.elf
CALC_ELF    = $(OUT_DIR)/calc.elf

# ==========================================================
# TARGET UTAMA
# ==========================================================
.PHONY: all fileman viewer clock clean

all: $(FILEMAN_ELF) $(VIEWER_ELF) $(CLOCK_ELF) $(CALC_ELF)
	@echo ""
	@echo "[USER APPS] Build selesai! Semua app di virtual 0x4000000"
	@echo "  fileman.elf → $(FILEMAN_ELF)"
	@echo "  viewer.elf  → $(VIEWER_ELF)"
	@echo "  clock.elf   → $(CLOCK_ELF)"
	@echo "  calc.elf    → $(CALC_ELF)"
	@echo "  OS akan otomatis alokasi physical RAM saat dijalankan."

# ==========================================================
# COMPILE SHARED USERLIB
# ==========================================================
$(USERLIB_OBJ): $(APPS_DIR)/userlib.c
	@echo "[CC] Mengompilasi userlib (64-bit)..."
	$(CC) $(CFLAGS_LIB) -c $< -o $@

# ==========================================================
# FILEMAN — File Manager (virtual 0x4000000, phys by OS)
# ==========================================================
fileman.o: fileman.c
	@echo "[CC] Mengompilasi fileman.c..."
	$(CC) $(CFLAGS_APP) -c $< -o $@

$(FILEMAN_ELF): fileman.o $(USERLIB_OBJ)
	@echo "[LD] Linking fileman.elf ke $(APP_BASE) (physical RAM dicari otomatis)..."
	$(LD) $(LDFLAGS) fileman.o $(USERLIB_OBJ) -o $@
	@echo "[OK] fileman.elf siap!"

fileman: $(FILEMAN_ELF)

# ==========================================================
# VIEWER — Image Viewer PNG (virtual 0x4000000, phys by OS)
# ==========================================================
viewer.o: viewer.c
	@echo "[CC] Mengompilasi viewer.c (O2)..."
	$(CC) $(CFLAGS_LIB) -c $< -o $@

$(VIEWER_ELF): viewer.o $(USERLIB_OBJ)
	@echo "[LD] Linking viewer.elf ke $(APP_BASE) (physical RAM dicari otomatis)..."
	$(LD) $(LDFLAGS) viewer.o $(USERLIB_OBJ) -o $@
	@echo "[OK] viewer.elf siap!"

viewer: $(VIEWER_ELF)

# ==========================================================
# CLOCK — Jam Digital Real-Time (virtual 0x4000000, phys by OS)
# ==========================================================
clock.o: clock.c
	@echo "[CC] Mengompilasi clock.c..."
	$(CC) $(CFLAGS_APP) -c $< -o $@

$(CLOCK_ELF): clock.o $(USERLIB_OBJ)
	@echo "[LD] Linking clock.elf ke $(APP_BASE) (physical RAM dicari otomatis)..."
	$(LD) $(LDFLAGS) clock.o $(USERLIB_OBJ) -o $@
	@echo "[OK] clock.elf siap!"

clock: $(CLOCK_ELF)

# ==========================================================
# CALC — Kalkulator GUI (virtual 0x4000000, phys by OS)
# ==========================================================
calc.o: calc.c
	@echo "[CC] Mengompilasi calc.c..."
	$(CC) $(CFLAGS_APP) -c $< -o $@

$(CALC_ELF): calc.o $(USERLIB_OBJ)
	@echo "[LD] Linking calc.elf ke $(APP_BASE)..."
	$(LD) $(LDFLAGS) calc.o $(USERLIB_OBJ) -o $@
	@echo "[OK] calc.elf siap!"

calc: $(CALC_ELF)

# ==========================================================
# CLEAN
# ==========================================================
clean:
	@echo "[CLEAN] Membersihkan file objek user_apps..."
	rm -f fileman.o viewer.o clock.o calc.o $(USERLIB_OBJ)
	@echo "[CLEAN] Selesai."