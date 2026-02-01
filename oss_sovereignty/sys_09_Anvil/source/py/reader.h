 
#ifndef MICROPY_INCLUDED_PY_READER_H
#define MICROPY_INCLUDED_PY_READER_H

#include "py/obj.h"




#define MP_READER_EOF ((mp_uint_t)(-1))

typedef struct _mp_reader_t {
    void *data;
    mp_uint_t (*readbyte)(void *data);
    void (*close)(void *data);
} mp_reader_t;

void mp_reader_new_mem(mp_reader_t *reader, const byte *buf, size_t len, size_t free_len);
void mp_reader_new_file(mp_reader_t *reader, qstr filename);
void mp_reader_new_file_from_fd(mp_reader_t *reader, int fd, bool close_fd);

#endif 
