 

#include "semihosting.h"






#define SYS_OPEN   0x01
#define SYS_WRITEC 0x03
#define SYS_WRITE  0x05
#define SYS_READC  0x07


#define OPEN_MODE_READ  (0) 
#define OPEN_MODE_WRITE (4) 

#ifndef __thumb__
#error Semihosting is only implemented for ARM microcontrollers.
#endif

static int mp_semihosting_stdout;

static uint32_t mp_semihosting_call(uint32_t num, const void *arg) {
    
    
    
    
    
    
    
    
    
    
    
    register uint32_t num_reg __asm__ ("r0") = num;
    register const void *args_reg __asm__ ("r1") = arg;
    __asm__ __volatile__ (
        "bkpt 0xAB\n"         
        : "+r" (num_reg)      
        : "r" (args_reg)      
        : "memory");          
    return num_reg; 
}

static int mp_semihosting_open_console(uint32_t mode) {
    struct {
        char *name;
        uint32_t mode;
        uint32_t name_len;
    } args = {
        .name = ":tt",     
        .mode = mode,      
        .name_len = 3,     
    };
    return mp_semihosting_call(SYS_OPEN, &args);
}

void mp_semihosting_init() {
    mp_semihosting_stdout = mp_semihosting_open_console(OPEN_MODE_WRITE);
}

int mp_semihosting_rx_char() {
    return mp_semihosting_call(SYS_READC, NULL);
}

static void mp_semihosting_tx_char(char c) {
    mp_semihosting_call(SYS_WRITEC, &c);
}

uint32_t mp_semihosting_tx_strn(const char *str, size_t len) {
    if (len == 0) {
        return 0; 
    }
    if (len == 1) {
        mp_semihosting_tx_char(*str); 
        return 0;
    }

    struct {
        uint32_t fd;
        const char *str;
        uint32_t len;
    } args = {
        .fd = mp_semihosting_stdout,
        .str = str,
        .len = len,
    };
    return mp_semihosting_call(SYS_WRITE, &args);
}

uint32_t mp_semihosting_tx_strn_cooked(const char *str, size_t len) {
    
    
    
    
    
    size_t start = 0;
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\n') {
            mp_semihosting_tx_strn(str + start, i - start);
            mp_semihosting_tx_char('\r');
            start = i;
        }
    }
    return mp_semihosting_tx_strn(str + start, len - start);
}
