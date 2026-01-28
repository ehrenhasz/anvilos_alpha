
#ifndef MICROPY_INCLUDED_STM32_GCCOLLECT_H
#define MICROPY_INCLUDED_STM32_GCCOLLECT_H



extern uint32_t _etext;
extern uint32_t _sidata;
extern uint32_t _ram_start;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;
extern uint32_t _heap_start;
extern uint32_t _heap_end;
extern uint32_t _sstack;
extern uint32_t _estack;
extern uint32_t _ram_end;

#endif 
