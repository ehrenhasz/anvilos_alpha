

#ifndef __MICROPY_INCLUDED_LIB_TICKER_H__
#define __MICROPY_INCLUDED_LIB_TICKER_H__



#include "nrf.h"

typedef void (*callback_ptr)(void);
typedef int32_t (*ticker_callback_ptr)(void);

void ticker_init0(void);
void ticker_start(void);
void ticker_stop(void);
int clear_ticker_callback(uint32_t index);
int set_ticker_callback(uint32_t index, ticker_callback_ptr func, int32_t initial_delay_us);

void ticker_register_low_pri_callback(callback_ptr callback);

#define CYCLES_PER_MICROSECONDS 16

#define MICROSECONDS_PER_TICK 16
#define CYCLES_PER_TICK (CYCLES_PER_MICROSECONDS*MICROSECONDS_PER_TICK)

#define MICROSECONDS_PER_MACRO_TICK 6000
#define MILLISECONDS_PER_MACRO_TICK 6

#endif 
