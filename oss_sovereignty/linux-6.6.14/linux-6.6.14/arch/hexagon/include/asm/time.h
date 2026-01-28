#ifndef ASM_TIME_H
#define ASM_TIME_H
extern cycles_t        pcycle_freq_mhz;
extern cycles_t        thread_freq_mhz;
extern cycles_t        sleep_clk_freq;
void setup_percpu_clockdev(void);
void ipi_timer(void);
#endif
