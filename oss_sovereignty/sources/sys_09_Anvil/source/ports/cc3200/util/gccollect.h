
#ifndef MICROPY_INCLUDED_CC3200_UTIL_GCCOLLECT_H
#define MICROPY_INCLUDED_CC3200_UTIL_GCCOLLECT_H


extern uint32_t _etext;
extern uint32_t _data;
extern uint32_t _edata;
extern uint32_t _boot;
extern uint32_t _eboot;
extern uint32_t _bss;
extern uint32_t _ebss;
extern uint32_t _heap;
extern uint32_t _eheap;
extern uint32_t _stack;
extern uint32_t _estack;

void gc_collect(void);

#endif 
