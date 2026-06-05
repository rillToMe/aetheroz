#include <stdint.h>

// ============================================================
// KYUZEN OS — Event Queue
// Digunakan oleh keyboard_handler dan mouse_handler untuk
// mengirim event ke kernel / syscall_handler.
// ============================================================

// --- Tipe Data Event (harus cocok dengan userlib.h) ---
#define EVENT_NONE          0
#define EVENT_KEY_PRESS     1
#define EVENT_MOUSE_MOVE    2
#define EVENT_MOUSE_CLICK   3

typedef struct {
    uint32_t type;
    int32_t  param1;
    int32_t  param2;
    int32_t  param3;
} kyuzen_event_t;

// --- Circular Buffer ---
#define EVENT_QUEUE_SIZE 64

static kyuzen_event_t event_queue[EVENT_QUEUE_SIZE];
static volatile uint32_t eq_head = 0; // Produsen (IRQ handler) menulis ke sini
static volatile uint32_t eq_tail = 0; // Konsumen (syscall_handler) membaca dari sini

// Masukkan event ke dalam antrian (dipanggil dari IRQ handler)
void push_event(uint32_t type, int32_t p1, int32_t p2, int32_t p3) {
    uint32_t next_head = (eq_head + 1) % EVENT_QUEUE_SIZE;
    if (next_head == eq_tail) {
        // Buffer penuh — buang event tertua (overwrite)
        eq_tail = (eq_tail + 1) % EVENT_QUEUE_SIZE;
    }
    event_queue[eq_head].type   = type;
    event_queue[eq_head].param1 = p1;
    event_queue[eq_head].param2 = p2;
    event_queue[eq_head].param3 = p3;
    eq_head = next_head;
}

// Ambil event dari antrian. Mengembalikan 1 jika ada event, 0 jika kosong.
// (Disiapkan untuk penggunaan masa depan oleh syscall_handler yang lebih canggih)
int pop_event(kyuzen_event_t* out) {
    if (eq_head == eq_tail) return 0; // Antrian kosong
    *out = event_queue[eq_tail];
    eq_tail = (eq_tail + 1) % EVENT_QUEUE_SIZE;
    return 1;
}
