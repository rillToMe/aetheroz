// kernel/usercopy.c — FIX_005 Tahap 2: boundary copy user <-> kernel.
//
// Validasi TRANSISIONAL (lihat usercopy.h): halaman harus mapped di AS
// caller. Deref langsung setelah validasi aman karena:
//   - CR3 saat syscall = PML4 caller (int 0x80 tidak mengganti CR3), dan
//   - hanya task pemanggil yang bisa unmap AS-nya sendiri — dan dia sedang
//     berada di dalam syscall ini. (SMAP baru masuk di Tahap 4.)

#include "usercopy.h"
#include "paging.h"
#include "smp.h"
#include <stddef.h>

extern void* memcpy(void* dest, const void* src, size_t n);

void ucopy_ctx_init(ucopy_ctx_t* ctx, const registers_t* r) {
    ctx->from_user = ((r->cs & 3) == 3);
    int task_id = smp_current_task_id();
    ctx->pml4 = (task_id >= 0 && task_id < task_count)
                    ? tasks[task_id].pml4_phys : PHYS_NULL;
}

int user_range_ok(const ucopy_ctx_t* ctx, uint64_t uaddr, uint64_t len) {
    if (uaddr == 0) return 0;
    if (len == 0) return 1;
    if (len > UC_MAX_RANGE) return 0;
    if (uaddr + len < uaddr) return 0;   // wrap alamat
    if (!ctx->from_user) return 1;       // caller ring 0: pointer kernel sah

    uint64_t first = uaddr & ~0xFFFULL;
    uint64_t last  = (uaddr + len - 1) & ~0xFFFULL;
    for (uint64_t p = first;; p += 0x1000) {
        if (!paging_is_mapped_into(p, ctx->pml4)) return 0;
        if (p == last) break;
    }
    return 1;
}

int copy_from_user(const ucopy_ctx_t* ctx, void* kdst, uint64_t usrc, uint64_t len) {
    if (!user_range_ok(ctx, usrc, len)) return -1;
    memcpy(kdst, (const void*)usrc, len);
    return 0;
}

int copy_to_user(const ucopy_ctx_t* ctx, uint64_t udst, const void* ksrc, uint64_t len) {
    if (!user_range_ok(ctx, udst, len)) return -1;
    memcpy((void*)udst, ksrc, len);
    return 0;
}

int64_t strncpy_from_user(const ucopy_ctx_t* ctx, char* kdst, uint64_t usrc, uint64_t cap) {
    if (usrc == 0 || cap == 0) return -1;

    uint64_t i = 0;
    if (!ctx->from_user) {
        const char* s = (const char*)usrc;
        while (i < cap - 1 && s[i] != '\0') { kdst[i] = s[i]; i++; }
        kdst[i] = '\0';
        return (int64_t)i;
    }

    // Ring 3: validasi hanya halaman yang benar-benar disentuh — string
    // pendek di halaman terakhir yang mapped harus tetap berhasil.
    uint64_t checked_page = 1;   // bukan page-aligned → pasti != halaman pertama
    while (i < cap - 1) {
        uint64_t addr = usrc + i;
        if (addr < usrc) return -1;   // wrap
        uint64_t page = addr & ~0xFFFULL;
        if (page != checked_page) {
            if (!paging_is_mapped_into(page, ctx->pml4)) return -1;
            checked_page = page;
        }
        char c = *(const char*)addr;
        if (c == '\0') break;
        kdst[i++] = c;
    }
    kdst[i] = '\0';
    return (int64_t)i;
}
