
#ifndef MICROPY_INCLUDED_RENESAS_RA_USRSW_H
#define MICROPY_INCLUDED_RENESAS_RA_USRSW_H

void switch_init0(void);
int switch_get(void);

extern const mp_obj_type_t pyb_switch_type;

#endif 
