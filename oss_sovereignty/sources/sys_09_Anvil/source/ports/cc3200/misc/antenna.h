
#ifndef MICROPY_INCLUDED_CC3200_MISC_ANTENNA_H
#define MICROPY_INCLUDED_CC3200_MISC_ANTENNA_H

typedef enum {
    ANTENNA_TYPE_INTERNAL = 0,
    ANTENNA_TYPE_EXTERNAL
} antenna_type_t;

extern void antenna_init0 (void);
extern void antenna_select (antenna_type_t antenna_type);

#endif 
