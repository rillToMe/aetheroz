#ifndef USERCOPY_H
#define USERCOPY_H

// FIX_005 Tahap 2 — boundary copy: syscall tidak lagi men-deref pointer user
// mentah. Pointer dari ring 3 divalidasi (mapped di AS caller) lalu di-copy
// in/out lewat helper modul ini.
//
// TRANSISIONAL: stack app & hasil sys_alloc masih heap kernel (higher-half),
// jadi predikat memeriksa "mapped di AS caller", BELUM "user range saja".
// Tahap 3 memperketat user_range_ok (cukup fungsi itu saja) ke PML4 idx < 256.

#include <stdint.h>
#include "pmm.h"
#include "task.h"

// --- Batas ukuran per kelas argumen (satu tempat, dipakai syscall.c) ---
#define UC_MAX_STR     1024u                    // string umum (print, draw_string)
#define UC_MAX_FNAME   64u                      // nama file KyuzenFS / path VFS
#define UC_MAX_HOST    128u                     // hostname/IP sys_ping
#define UC_MAX_KBD     512u                     // buffer baca keyboard
#define UC_MAX_FILE    (8u * 1024u * 1024u)     // isi file (create/read_to_buffer)
#define UC_MAX_IO      (1u * 1024u * 1024u)     // sys_read/sys_write per panggilan
#define UC_MAX_SOCK    (64u * 1024u)            // sock send/recv per panggilan
#define UC_MAX_ENTRIES 128u                     // entri sys_get_file_list
#define UC_MAX_RANGE   (64ULL * 1024u * 1024u)  // plafon absolut validasi range

// Konteks per-invocation syscall. HARUS hidup di stack syscall (bukan
// global/per-CPU): syscall bisa block (sti/hlt) dan task lain bisa masuk
// syscall bersarang di CPU yang sama.
typedef struct {
    int         from_user;  // 1 = caller ring 3 (RPL r->cs == 3)
    phys_addr_t pml4;       // AS caller (PHYS_NULL = kernel AS)
} ucopy_ctx_t;

void ucopy_ctx_init(ucopy_ctx_t* ctx, const registers_t* r);

// 1 = range [uaddr, uaddr+len) aman disentuh untuk caller ini.
// Ring 0: cukup non-NULL (pointer kernel sah — shell/login masuk via int 0x80).
// Ring 3: cek wrap alamat, plafon UC_MAX_RANGE, dan tiap halaman mapped di
// AS caller.
int user_range_ok(const ucopy_ctx_t* ctx, uint64_t uaddr, uint64_t len);

// Copy melewati boundary. Return 0 sukses, -1 gagal validasi.
int copy_from_user(const ucopy_ctx_t* ctx, void* kdst, uint64_t usrc, uint64_t len);
int copy_to_user(const ucopy_ctx_t* ctx, uint64_t udst, const void* ksrc, uint64_t len);

// Copy string NUL-terminated dari user, maksimal cap-1 char + NUL (truncate
// diam-diam, sama seperti copy kfname lama di sys_exec). Return panjang
// hasil (>= 0), atau -1 jika pointer tidak valid.
int64_t strncpy_from_user(const ucopy_ctx_t* ctx, char* kdst, uint64_t usrc, uint64_t cap);

#endif
