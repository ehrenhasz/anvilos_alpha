#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "py/runtime.h"
#include "py/mphal.h"
#include "py/objstr.h"

// anvil.check_output(cmd_str)
// Returns bytes containing stdout. Raises OSError on failure or non-zero exit.
static mp_obj_t anvil_check_output(mp_obj_t cmd_in) {
    const char *cmd = mp_obj_str_get_str(cmd_in);

    // We use popen to read stdout.
    // NOTE: This does not capture stderr.
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        mp_raise_OSError(errno);
    }

    vstr_t vstr;
    vstr_init(&vstr, 128);

    char buf[1024];
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        vstr_add_str(&vstr, buf);
    }

    int status = pclose(fp);
    if (status == -1) {
        vstr_clear(&vstr);
        mp_raise_OSError(errno);
    }

    int exit_code = 0;
    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    } else {
        exit_code = -1; // Terminated by signal
    }

    if (exit_code != 0) {
        vstr_clear(&vstr);
        // Raise OSError with the exit code as the error code.
        mp_raise_OSError(exit_code);
    }

    return mp_obj_new_bytes_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_1(anvil_check_output_obj, anvil_check_output);

static const mp_rom_map_elem_t anvil_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_anvil) },
    { MP_ROM_QSTR(MP_QSTR_check_output), MP_ROM_PTR(&anvil_check_output_obj) },
};
static MP_DEFINE_CONST_DICT(anvil_module_globals, anvil_module_globals_table);

const mp_obj_module_t mp_module_anvil = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&anvil_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_anvil, mp_module_anvil);
