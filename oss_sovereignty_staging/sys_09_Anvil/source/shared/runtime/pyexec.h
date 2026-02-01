 
#ifndef MICROPY_INCLUDED_LIB_UTILS_PYEXEC_H
#define MICROPY_INCLUDED_LIB_UTILS_PYEXEC_H

#include "py/obj.h"

typedef enum {
    PYEXEC_MODE_FRIENDLY_REPL,
    PYEXEC_MODE_RAW_REPL,
} pyexec_mode_kind_t;

extern pyexec_mode_kind_t pyexec_mode_kind;




extern int pyexec_system_exit;

#define PYEXEC_FORCED_EXIT (0x100)

int pyexec_raw_repl(void);
int pyexec_friendly_repl(void);
int pyexec_file(const char *filename);
int pyexec_file_if_exists(const char *filename);
int pyexec_frozen_module(const char *name, bool allow_keyboard_interrupt);
void pyexec_event_repl_init(void);
int pyexec_event_repl_process_char(int c);
extern uint8_t pyexec_repl_active;

#if MICROPY_REPL_INFO
mp_obj_t pyb_set_repl_info(mp_obj_t o_value);
MP_DECLARE_CONST_FUN_OBJ_1(pyb_set_repl_info_obj);
#endif

#endif 
