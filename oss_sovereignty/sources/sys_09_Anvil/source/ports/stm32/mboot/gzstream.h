
#ifndef MICROPY_INCLUDED_STM32_MBOOT_GZSTREAM_H
#define MICROPY_INCLUDED_STM32_MBOOT_GZSTREAM_H

#include <stddef.h>
#include <stdint.h>

typedef int (*stream_open_t)(void *stream, const char *fname);
typedef void (*stream_close_t)(void *stream);
typedef int (*stream_read_t)(void *stream, uint8_t *buf, size_t len);

typedef struct _stream_methods_t {
    stream_open_t open;
    stream_close_t close;
    stream_read_t read;
} stream_methods_t;

int gz_stream_init_from_raw_data(const uint8_t *data, size_t len);
int gz_stream_init_from_stream(void *stream_data, stream_read_t stream_read);
int gz_stream_read(size_t len, uint8_t *buf);

#endif 
