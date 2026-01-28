
#ifndef MICROPY_INCLUDED_RENESAS_RA_BOARDCTRL_H
#define MICROPY_INCLUDED_RENESAS_RA_BOARDCTRL_H

#include "py/mpconfig.h"


#ifndef MICROPY_BOARD_PENDSV_ENTRIES
#define MICROPY_BOARD_PENDSV_ENTRIES
#endif

#ifndef MICROPY_BOARD_STARTUP
#define MICROPY_BOARD_STARTUP powerctrl_check_enter_bootloader
#endif

#ifndef MICROPY_BOARD_ENTER_BOOTLOADER
#define MICROPY_BOARD_ENTER_BOOTLOADER(nargs, args)
#endif

#ifndef MICROPY_BOARD_EARLY_INIT
#define MICROPY_BOARD_EARLY_INIT()
#endif

#ifndef MICROPY_BOARD_BEFORE_SOFT_RESET_LOOP
#define MICROPY_BOARD_BEFORE_SOFT_RESET_LOOP boardctrl_before_soft_reset_loop
#endif

#ifndef MICROPY_BOARD_TOP_SOFT_RESET_LOOP
#define MICROPY_BOARD_TOP_SOFT_RESET_LOOP boardctrl_top_soft_reset_loop
#endif

#ifndef MICROPY_BOARD_RUN_BOOT_PY
#define MICROPY_BOARD_RUN_BOOT_PY boardctrl_run_boot_py
#endif

#ifndef MICROPY_BOARD_RUN_MAIN_PY
#define MICROPY_BOARD_RUN_MAIN_PY boardctrl_run_main_py
#endif

#ifndef MICROPY_BOARD_START_SOFT_RESET
#define MICROPY_BOARD_START_SOFT_RESET boardctrl_start_soft_reset
#endif

#ifndef MICROPY_BOARD_END_SOFT_RESET
#define MICROPY_BOARD_END_SOFT_RESET boardctrl_end_soft_reset
#endif


enum {
    BOARDCTRL_CONTINUE,
    BOARDCTRL_GOTO_SOFT_RESET_EXIT,
};


enum {
    BOARDCTRL_RESET_MODE_NORMAL = 1,
    BOARDCTRL_RESET_MODE_SAFE_MODE = 2,
    BOARDCTRL_RESET_MODE_FACTORY_FILESYSTEM = 3,
};

typedef struct _boardctrl_state_t {
    uint8_t reset_mode;
    bool log_soft_reset;
} boardctrl_state_t;

void boardctrl_before_soft_reset_loop(boardctrl_state_t *state);
void boardctrl_top_soft_reset_loop(boardctrl_state_t *state);
int boardctrl_run_boot_py(boardctrl_state_t *state);
int boardctrl_run_main_py(boardctrl_state_t *state);
void boardctrl_start_soft_reset(boardctrl_state_t *state);
void boardctrl_end_soft_reset(boardctrl_state_t *state);

#endif 
