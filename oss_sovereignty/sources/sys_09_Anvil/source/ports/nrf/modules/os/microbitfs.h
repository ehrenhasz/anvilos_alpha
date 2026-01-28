
#ifndef __MICROPY_INCLUDED_FILESYSTEM_H__
#define __MICROPY_INCLUDED_FILESYSTEM_H__

#include "py/builtin.h"
#include "py/obj.h"
#include "py/lexer.h"

#ifndef MBFS_LOG_CHUNK_SIZE





#define MBFS_LOG_CHUNK_SIZE 7
#endif

mp_obj_t os_mbfs_open(size_t n_args, const mp_obj_t *args);
void microbit_filesystem_init(void);
mp_lexer_t *os_mbfs_new_reader(const char *filename);
mp_import_stat_t os_mbfs_import_stat(const char *path);

MP_DECLARE_CONST_FUN_OBJ_0(os_mbfs_listdir_obj);
MP_DECLARE_CONST_FUN_OBJ_0(os_mbfs_ilistdir_obj);
MP_DECLARE_CONST_FUN_OBJ_1(os_mbfs_remove_obj);
MP_DECLARE_CONST_FUN_OBJ_1(os_mbfs_stat_obj);

#endif 
