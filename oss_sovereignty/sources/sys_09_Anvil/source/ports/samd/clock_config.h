

void init_clocks(uint32_t cpu_freq);
void set_cpu_freq(uint32_t cpu_freq);
uint32_t get_cpu_freq(void);
uint32_t get_peripheral_freq(void);
void check_usb_clock_recovery_mode(void);
void enable_sercom_clock(int id);
