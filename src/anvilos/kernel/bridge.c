#include <stdint.h>
#include <unistd.h> // For types

#include "py/compile.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "shared/runtime/pyexec.h"
// #include "py/lexer.h" // Removed to avoid type issues
#include "py/qstr.h" // For qstr_pool_t

// --- VGA Driver ---
volatile uint16_t *vga_buffer = (uint16_t *)0xb8000;
int vga_col = 0;
int vga_row = 0;

void vga_write_char(char c) {
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
    } else if (c == '\r') {
        vga_col = 0;
    } else {
        vga_buffer[vga_row * 80 + vga_col] = (0x0F << 8) | c; // White on Black
        vga_col++;
        if (vga_col >= 80) {
            vga_col = 0;
            vga_row++;
        }
    }
    if (vga_row >= 25) vga_row = 0; // Wrap (Overwrite top) for now
}

mp_uint_t mp_hal_stdout_tx_strn(const char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        vga_write_char(str[i]);
    }
    return len;
}

// --- IO Ports ---
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

// --- Keyboard ---
static const char scancode_map[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
};

void keyboard_flush(void) {
    // Drain the keyboard buffer
    uint8_t status;
    while (((status = inb(0x64)) & 0x01) == 1) {
        inb(0x60); // Read and discard
    }
}

int mp_hal_stdin_rx_chr(void) {
    while (1) {
        uint8_t status = inb(0x64);
        if ((status & 0x01) != 0) { // Data Ready
            uint8_t scancode = inb(0x60);
            if ((status & 0x20) == 0) { // Not Mouse Data
                if (!(scancode & 0x80)) { // Key Pressed
                    char c = (scancode < sizeof(scancode_map)) ? scancode_map[scancode] : 0;
                    if (c) return c;
                }
            }
        }
        // Small busy-wait delay
        for (volatile int i = 0; i < 1000; i++);
    }
}

// --- LibC Stubs ---
void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    char *d = dest;
    const char *s = src;
    while (n--) *d++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    char *d = dest;
    const char *s = src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        const char *lasts = s + (n-1);
        char *lastd = d + (n-1);
        while (n--) *lastd-- = *lasts--;
    }
    return dest;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++; s2++; n--;
    }
    if (n == 0) return 0;
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = s1, *p2 = s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

char *strchr(const char *s, int c) {
    while (*s != (char)c) {
        if (!*s++) return 0;
    }
    return (char *)s;
}

void __assert_fail(const char *assertion, const char *file, int line, const char *function) {
    mp_hal_stdout_tx_strn("ASSERT FAIL: ", 13);
    mp_hal_stdout_tx_strn(assertion, strlen(assertion));
    for(;;);
}

int printf(const char *fmt, ...) {
    // minimal stub - ignores formatting
    mp_hal_stdout_tx_strn(fmt, strlen(fmt));
    return 0;
}

// --- MicroPython Stubs ---
#define MP_IMPORT_STAT_NO_EXIST 0

int mp_import_stat(const char *path) {
    return MP_IMPORT_STAT_NO_EXIST;
}

// mp_lexer_t is struct _mp_lexer_t. Declared in py/lexer.h.
// Since we removed lexer.h, we must treat it as void* or struct pointer.
// But qstr is defined in qstr.h.
struct _mp_lexer_t;
struct _mp_lexer_t *mp_lexer_new_from_file(qstr filename) {
    mp_raise_OSError(MP_ENOENT);
    return NULL;
}

// Stub readline to satisfy linker
int readline(void *line, const char *prompt) {
    mp_hal_stdout_tx_strn(prompt, strlen(prompt));
    return 0; // EOF
}

// --- Main ---
static char heap[64 * 1024];

void main_c(void) {
    // Clear screen
    for (int i = 0; i < 80 * 25; i++) vga_buffer[i] = (0x0F << 8) | ' ';
    
    mp_hal_stdout_tx_strn("AnvilOS Zero-C Kernel Starting...\n", 34);

    gc_init(heap, heap + sizeof(heap));
    mp_init();
    
    mp_hal_stdout_tx_strn("MicroPython Initialized.\n", 25);
    
    // Flush stale inputs (like the Enter key used to start QEMU)
    keyboard_flush();
    
    mp_hal_stdout_tx_strn("Starting Friendly REPL...\n", 26);
    
    // Start Friendly REPL (Interactive)
    if (pyexec_friendly_repl() != 0) {
        mp_hal_stdout_tx_strn("REPL Error\n", 11);
    }
}
