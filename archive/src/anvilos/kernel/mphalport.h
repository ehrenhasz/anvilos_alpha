static inline mp_uint_t mp_hal_ticks_ms(void) {
    return 0;
}
static inline void mp_hal_set_interrupt_char(char c) {
}

// Forward declarations to satisfy the macro
mp_uint_t mp_hal_stdout_tx_strn(const char *str, size_t len);
size_t strlen(const char *s);

#define mp_hal_stdout_tx_str(str) mp_hal_stdout_tx_strn(str, strlen(str))
