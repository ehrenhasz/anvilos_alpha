
#ifndef MICROPY_INCLUDED_PY_RINGBUF_H
#define MICROPY_INCLUDED_PY_RINGBUF_H

#include <stddef.h>
#include <stdint.h>

#ifdef _MSC_VER
#include "py/mpconfig.h" 
#endif

typedef struct _ringbuf_t {
    uint8_t *buf;
    uint16_t size;
    uint16_t iget;
    uint16_t iput;
} ringbuf_t;






#define ringbuf_alloc(r, sz) \
    { \
        (r)->buf = m_new(uint8_t, sz); \
        (r)->size = sz; \
        (r)->iget = (r)->iput = 0; \
    }

static inline int ringbuf_get(ringbuf_t *r) {
    if (r->iget == r->iput) {
        return -1;
    }
    uint8_t v = r->buf[r->iget++];
    if (r->iget >= r->size) {
        r->iget = 0;
    }
    return v;
}

static inline int ringbuf_peek(ringbuf_t *r) {
    if (r->iget == r->iput) {
        return -1;
    }
    return r->buf[r->iget];
}

static inline int ringbuf_put(ringbuf_t *r, uint8_t v) {
    uint32_t iput_new = r->iput + 1;
    if (iput_new >= r->size) {
        iput_new = 0;
    }
    if (iput_new == r->iget) {
        return -1;
    }
    r->buf[r->iput] = v;
    r->iput = iput_new;
    return 0;
}

static inline size_t ringbuf_free(ringbuf_t *r) {
    return (r->size + r->iget - r->iput - 1) % r->size;
}

static inline size_t ringbuf_avail(ringbuf_t *r) {
    return (r->size + r->iput - r->iget) % r->size;
}


int ringbuf_get16(ringbuf_t *r);
int ringbuf_peek16(ringbuf_t *r);
int ringbuf_put16(ringbuf_t *r, uint16_t v);

int ringbuf_get_bytes(ringbuf_t *r, uint8_t *data, size_t data_len);
int ringbuf_put_bytes(ringbuf_t *r, const uint8_t *data, size_t data_len);

#endif 
