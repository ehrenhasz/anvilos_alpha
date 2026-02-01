 

#ifndef UZLIB_H_INCLUDED
#define UZLIB_H_INCLUDED

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "uzlib_conf.h"
#if UZLIB_CONF_DEBUG_LOG
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

 
#define UZLIB_OK             0
 
#define UZLIB_DONE           1
#define UZLIB_DATA_ERROR    (-3)
#define UZLIB_CHKSUM_ERROR  (-4)
#define UZLIB_DICT_ERROR    (-5)

 
#define UZLIB_CHKSUM_NONE  0
#define UZLIB_CHKSUM_ADLER 1
#define UZLIB_CHKSUM_CRC   2

 
#define TINF_ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*(arr)))

 

typedef struct {
   unsigned short table[16];   
   unsigned short trans[288];  
} TINF_TREE;

typedef struct _uzlib_uncomp_t {
     
    const unsigned char *source;
     
    const unsigned char *source_limit;
     
    void *source_read_data;
    int (*source_read_cb)(void *);

    unsigned int tag;
    unsigned int bitcount;

     
    unsigned char *dest_start;
     
    unsigned char *dest;
     
    unsigned char *dest_limit;

     
    unsigned int checksum;
    char checksum_type;
    bool eof;

    int btype;
    int bfinal;
    unsigned int curlen;
    int lzOff;
    unsigned char *dict_ring;
    unsigned int dict_size;
    unsigned int dict_idx;

    TINF_TREE ltree;  
    TINF_TREE dtree;  
} uzlib_uncomp_t;

#define TINF_PUT(d, c) \
    { \
        *d->dest++ = c; \
        if (d->dict_ring) { d->dict_ring[d->dict_idx++] = c; if (d->dict_idx == d->dict_size) d->dict_idx = 0; } \
    }

unsigned char uzlib_get_byte(uzlib_uncomp_t *d);

 

void uzlib_uncompress_init(uzlib_uncomp_t *d, void *dict, unsigned int dictLen);
int  uzlib_uncompress(uzlib_uncomp_t *d);
int  uzlib_uncompress_chksum(uzlib_uncomp_t *d);

#define UZLIB_HEADER_ZLIB             0
#define UZLIB_HEADER_GZIP             1
int uzlib_parse_zlib_gzip_header(uzlib_uncomp_t *d, int *wbits);

 

typedef struct {
    void *dest_write_data;
    void (*dest_write_cb)(void *data, uint8_t byte);
    unsigned long outbits;
    int noutbits;
    uint8_t *hist_buf;
    size_t hist_max;
    size_t hist_start;
    size_t hist_len;
} uzlib_lz77_state_t;

void uzlib_lz77_init(uzlib_lz77_state_t *state, uint8_t *hist, size_t hist_max);
void uzlib_lz77_compress(uzlib_lz77_state_t *state, const uint8_t *src, unsigned len);

void uzlib_start_block(uzlib_lz77_state_t *state);
void uzlib_finish_block(uzlib_lz77_state_t *state);

 

 
uint32_t uzlib_adler32(const void *data, unsigned int length, uint32_t prev_sum);
 
uint32_t uzlib_crc32(const void *data, unsigned int length, uint32_t crc);

#ifdef __cplusplus
}  
#endif

#endif  
