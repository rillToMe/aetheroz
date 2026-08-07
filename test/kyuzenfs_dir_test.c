// Self-check Fase 1 direktori KyuzenFS: folder sebagai entry biasa + API
// path-aware. Host build (bukan task QEMU) — kyuzenfs.c di-include langsung,
// ATA & heap di-mock di RAM, spinlock di-mock (test/spinlock.h via -iquote test).
// Jalankan dari root repo:
//   clang -iquote test -iquote include test/kyuzenfs_dir_test.c -o /tmp/kfs && /tmp/kfs
//
// PENTING: make conc / make heap-stress mengglob test/*.c ke build kernel.
// Test ini WAJIB batal di situ — guard di bawah menonaktifkan seluruh isi saat
// CONC_TEST / HEAP_STRESS_TEST terdefinisi (kalau tidak: duplicate main,
// kmalloc mock, dst → link error).
#include <stdint.h>

#if !defined(CONC_TEST) && !defined(HEAP_STRESS_TEST)

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- heap mock ---
void* kmalloc(size_t s) { return malloc(s); }
void  kfree(void* p)    { free(p); }

// --- ATA mock: 8192 sektor (4MB) di RAM, sektor kosong = 0 ---
#define DISK_SEC 8192
static uint8_t DISK[512 * DISK_SEC];
void ata_read_sector(uint32_t sec, uint8_t* buf) {
    if (sec < DISK_SEC) memcpy(buf, DISK + sec * 512, 512); else memset(buf, 0, 512);
}
void ata_write_sector(uint32_t sec, uint8_t* buf) {
    if (sec < DISK_SEC) memcpy(DISK + sec * 512, buf, 512);
}
uint32_t ata_get_total_sectors(void) { return DISK_SEC; }

#include "../kernel/kyuzenfs.c"

// tty_node dipakai kprint; write = NULL → kprint no-op saat runtime, tapi
// simbol write_fs/compositor_flush tetap di-stub karena referensinya ter-link.
fs_node_t tty_node = { 0 };
uint32_t write_fs(fs_node_t* n, uint32_t o, uint32_t s, uint8_t* b) { (void)n;(void)o;(void)s;(void)b; return 0; }
void compositor_flush(void) {}

// file_info_t layout harus sama dengan typedef lokal di kfs_get_file_list.
typedef struct { char filename[24]; uint32_t size; uint8_t is_folder; } TFileInfo;

static int find_in(const TFileInfo* list, int n, const char* name) {
    for (int i = 0; i < n; i++) if (!strcmp(list[i].filename, name)) return i;
    return -1;
}

int main(void) {
    memset(DISK, 0, sizeof(DISK));       // "hard disk" baru, belum diformat
    kfs_init();                          // magic tak valid → format otomatis

    // resolve path
    uint32_t sec;
    assert(kfs_resolve_dir("/", &sec) && sec == current_fs.root_dir_sector);
    assert(kfs_resolve_dir("", &sec) && sec == current_fs.root_dir_sector);
    assert(kfs_resolve_dir("/nope", &sec) == 0);

    // folder di root
    assert(kfs_create_folder("/apps") == 1);
    assert(kfs_exists("/apps"));
    assert(kfs_create_folder("/apps") == 0);          // duplikat ditolak

    // file di dalam folder + read back path-aware
    const char data[] = "hello kyuzen";
    assert(kfs_create_file("/apps/test.elf", (char*)data, sizeof(data) - 1) == 1);
    assert(kfs_get_file_size("/apps/test.elf") == sizeof(data) - 1);
    char buf[64] = { 0 };
    assert(kfs_read_to_buffer("/apps/test.elf", buf, sizeof(buf)) == 1);
    assert(!strcmp(buf, "hello kyuzen"));

    // list /apps → 1 file, is_folder = 0
    TFileInfo list[8]; memset(list, 0, sizeof(list));
    assert(kfs_get_file_list("/apps", list, 8) == 1);
    assert(!list[0].is_folder && !strcmp(list[0].filename, "test.elf"));

    // list / → folder apps, is_folder = 1
    memset(list, 0, sizeof(list));
    assert(kfs_get_file_list("/", list, 8) == 1);
    assert(list[0].is_folder && !strcmp(list[0].filename, "apps"));

    // duplikat di folder yang sama ditolak; nama sama di root boleh
    assert(kfs_create_file("/apps/test.elf", (char*)data, 5) == 0);
    assert(kfs_create_file("test.elf", (char*)data, 5) == 1);

    // nested: /a/b/c.elf
    assert(kfs_create_folder("/a") == 1);
    assert(kfs_create_folder("/a/b") == 1);
    assert(kfs_create_file("/a/b/c.elf", (char*)data, 5) == 1);
    memset(list, 0, sizeof(list));
    assert(kfs_get_file_list("/a/b", list, 8) == 1);
    assert(!strcmp(list[0].filename, "c.elf"));

    // parent harus sudah ada; file tak bisa dipakai sebagai folder
    assert(kfs_create_folder("/x/y") == 0);
    assert(kfs_get_file_list("/apps/test.elf", list, 8) == 0);

    // delete path-aware; folder tak bisa dihapus (fase nanti)
    kfs_delete_file("/apps/test.elf");
    assert(kfs_exists("/apps/test.elf") == 0);
    assert(kfs_exists("/apps"));                     // folder tetap ada
    kfs_delete_file("/apps");                        // ditolak — folder
    assert(kfs_exists("/apps"));
    assert(kfs_exists("/a/b/c.elf"));

    // root terakhir: apps (folder), a (folder), test.elf (file) → 3 entry
    memset(list, 0, sizeof(list));
    assert(kfs_get_file_list("/", list, 8) == 3);
    assert(find_in(list, 3, "apps") >= 0);
    assert(find_in(list, 3, "a") >= 0);
    assert(find_in(list, 3, "test.elf") >= 0);

    printf("kyuzenfs dir: OK\n");
    return 0;
}

#endif // !CONC_TEST && !HEAP_STRESS_TEST
