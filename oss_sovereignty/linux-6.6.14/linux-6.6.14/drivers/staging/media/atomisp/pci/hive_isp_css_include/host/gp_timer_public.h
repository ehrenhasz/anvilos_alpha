#ifndef __GP_TIMER_PUBLIC_H_INCLUDED__
#define __GP_TIMER_PUBLIC_H_INCLUDED__
#include "system_local.h"
extern void
gp_timer_init(gp_timer_ID_t ID);
extern uint32_t
gp_timer_read(gp_timer_ID_t ID);
#endif  
