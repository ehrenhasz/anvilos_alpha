#include <stdint.h>
#include <unistd.h> // For types

#include "py/compile.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "shared/runtime/pyexec.h"
#include "py/qstr.h" 

// --- LibC Stubs (Forward Declarations) ---
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
char *strchr(const char *s, int c);

// --- IO Ports ---
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

// --- VGA Driver ---
volatile uint16_t *vga_buffer = (uint16_t *)0xb8000;
int vga_col = 0;
int vga_row = 0;

void vga_scroll(void) {
    // Move lines 1-24 up to 0-23
    memmove((void *)vga_buffer, (void *)(vga_buffer + 80), 80 * 24 * 2);
    // Clear line 24
    for (int i = 0; i < 80; i++) {
        vga_buffer[24 * 80 + i] = (0x0F << 8) | ' ';
    }
    vga_row = 24;
}

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
    if (vga_row >= 25) {
        vga_scroll();
    }
}

void debug_scancode(uint8_t code) {
    const char hex[] = "0123456789ABCDEF";
    // Top Right Corner
    vga_buffer[78] = (0x4F << 8) | hex[(code >> 4) & 0xF];
    vga_buffer[79] = (0x4F << 8) | hex[code & 0xF];
}

mp_uint_t mp_hal_stdout_tx_strn(const char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        vga_write_char(str[i]);
    }
    return len;
}

// --- Keyboard ---
static const char scancode_map[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
};

int mp_hal_stdin_rx_chr(void) {
    while (1) {
        uint8_t status = inb(0x64);
        if ((status & 0x01) != 0) { // Data Ready
            uint8_t scancode = inb(0x60);
            debug_scancode(scancode); 
            
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

void wait_for_f1(void) {
    mp_hal_stdout_tx_strn("Press F1 to Start System...\n", 28);
    while (1) {
        uint8_t status = inb(0x64);
        if ((status & 0x01) != 0) {
            uint8_t scancode = inb(0x60);
            debug_scancode(scancode);
            if (scancode == 0x3B) { // F1 Pressed
                // Consume it
                inb(0x60);
                return;
            }
        }
    }
}

// --- Readline Implementation ---
int readline(vstr_t *line, const char *prompt) {
    mp_hal_stdout_tx_strn(prompt, strlen(prompt));
    while (1) {
        int c = mp_hal_stdin_rx_chr();
        if (c == '\r' || c == '\n') {
            mp_hal_stdout_tx_strn("\r\n", 2);
            return 0;
        }
        if (c == '\b' || c == 127) { // Backspace
            if (line->len > 0) {
                vstr_cut_tail_bytes(line, 1);
                mp_hal_stdout_tx_strn("\b \b", 3);
            }
        } else if (c >= 32 && c <= 126) {
            char ch = (char)c;
            vstr_add_char(line, ch);
            mp_hal_stdout_tx_strn(&ch, 1);
        }
    }
}

// --- LibC Implementations ---
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
    mp_hal_stdout_tx_strn(fmt, strlen(fmt));
    return 0;
}

// --- MicroPython Stubs ---
#define MP_IMPORT_STAT_NO_EXIST 0

int mp_import_stat(const char *path) {
    return MP_IMPORT_STAT_NO_EXIST;
}

struct _mp_lexer_t;
struct _mp_lexer_t *mp_lexer_new_from_file(qstr filename) {
    mp_raise_OSError(MP_ENOENT);
    return NULL;
}

// --- Main ---
static char heap[64 * 1024];

void main_c(void) {
    // Clear screen
    for (int i = 0; i < 80 * 25; i++) vga_buffer[i] = (0x0F << 8) | ' ';
    
    // Gatekeeper
    wait_for_f1();
    
    mp_hal_stdout_tx_strn("AnvilOS Zero-C Kernel Starting (VGA)...\n", 40);

    gc_init(heap, heap + sizeof(heap));
    mp_init();
    
    mp_hal_stdout_tx_strn("MicroPython Initialized.\n", 25);
    mp_hal_stdout_tx_strn("Starting Friendly REPL...\n", 26);
    
    if (pyexec_friendly_repl() != 0) {
        mp_hal_stdout_tx_strn("REPL Error\n", 11);
    }
}