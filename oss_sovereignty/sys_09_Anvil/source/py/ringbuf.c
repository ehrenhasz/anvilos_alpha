 

#include <string.h>

#include "ringbuf.h"

int ringbuf_get16(ringbuf_t *r) {
    int v = ringbuf_peek16(r);
    if (v == -1) {
        return v;
    }
    r->iget += 2;
    if (r->iget >= r->size) {
        r->iget -= r->size;
    }
    return v;
}

int ringbuf_peek16(ringbuf_t *r) {
    if (r->iget == r->iput) {
        return -1;
    }
    uint32_t iget_a = r->iget + 1;
    if (iget_a == r->size) {
        iget_a = 0;
    }
    if (iget_a == r->iput) {
        return -1;
    }
    return (r->buf[r->iget] << 8) | (r->buf[iget_a]);
}

int ringbuf_put16(ringbuf_t *r, uint16_t v) {
    uint32_t iput_a = r->iput + 1;
    if (iput_a == r->size) {
        iput_a = 0;
    }
    if (iput_a == r->iget) {
        return -1;
    }
    uint32_t iput_b = iput_a + 1;
    if (iput_b == r->size) {
        iput_b = 0;
    }
    if (iput_b == r->iget) {
        return -1;
    }
    r->buf[r->iput] = (v >> 8) & 0xff;
    r->buf[iput_a] = v & 0xff;
    r->iput = iput_b;
    return 0;
}





int ringbuf_get_bytes(ringbuf_t *r, uint8_t *data, size_t data_len) {
    if (ringbuf_avail(r) < data_len) {
        return (r->size <= data_len) ? -2 : -1;
    }
    uint32_t iget = r->iget;
    uint32_t iget_a = (iget + data_len) % r->size;
    uint8_t *datap = data;
    if (iget_a < iget) {
        
        memcpy(datap, r->buf + iget, r->size - iget);
        datap += (r->size - iget);
        iget = 0;
    }
    memcpy(datap, r->buf + iget, iget_a - iget);
    r->iget = iget_a;
    return 0;
}





int ringbuf_put_bytes(ringbuf_t *r, const uint8_t *data, size_t data_len) {
    if (ringbuf_free(r) < data_len) {
        return (r->size <= data_len) ? -2 : -1;
    }
    uint32_t iput = r->iput;
    uint32_t iput_a = (iput + data_len) % r->size;
    const uint8_t *datap = data;
    if (iput_a < iput) {
        
        memcpy(r->buf + iput, datap, r->size - iput);
        datap += (r->size - iput);
        iput = 0;
    }
    memcpy(r->buf + iput, datap, iput_a - iput);
    r->iput = iput_a;
    return 0;
}
