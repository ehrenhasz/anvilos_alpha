
#ifndef MICROPY_INCLUDED_CC3200_MODS_PYBSD_H
#define MICROPY_INCLUDED_CC3200_MODS_PYBSD_H


typedef struct {
    mp_obj_base_t base;
    mp_obj_t      pin_clk;
    bool          enabled;
} pybsd_obj_t;


extern pybsd_obj_t pybsd_obj;
extern const mp_obj_type_t pyb_sd_type;

#endif 
