
#ifndef MICROPY_INCLUDED_CC3200_MODS_PYBWDT_H
#define MICROPY_INCLUDED_CC3200_MODS_PYBWDT_H

#include <stdbool.h>

void pybwdt_init0 (void);
void pybwdt_srv_alive (void);
void pybwdt_srv_sleeping (bool state);
void pybwdt_sl_alive (void);

#endif 
