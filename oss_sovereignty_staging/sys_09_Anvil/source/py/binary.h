 
#ifndef MICROPY_INCLUDED_PY_BINARY_H
#define MICROPY_INCLUDED_PY_BINARY_H

#include "py/obj.h"




#define BYTEARRAY_TYPECODE 1

size_t mp_binary_get_size(char struct_type, char val_type, size_t *palign);
mp_obj_t mp_binary_get_val_array(char typecode, void *p, size_t index);
void mp_binary_set_val_array(char typecode, void *p, size_t index, mp_obj_t val_in);
void mp_binary_set_val_array_from_int(char typecode, void *p, size_t index, mp_int_t val);
mp_obj_t mp_binary_get_val(char struct_type, char val_type, byte *p_base, byte **ptr);
void mp_binary_set_val(char struct_type, char val_type, mp_obj_t val_in, byte *p_base, byte **ptr);
long long mp_binary_get_int(size_t size, bool is_signed, bool big_endian, const byte *src);
void mp_binary_set_int(size_t val_sz, bool big_endian, byte *dest, mp_uint_t val);

#endif 
