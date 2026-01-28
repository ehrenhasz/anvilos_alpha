
#ifndef MICROPY_INCLUDED_STM32_MBOOT_PACK_H
#define MICROPY_INCLUDED_STM32_MBOOT_PACK_H

#include <stdint.h>
#include "py/mphal.h"

#if MBOOT_ENABLE_PACKING

#include "lib/libhydrogen/hydrogen.h"






#define MBOOT_PACK_HEADER_VERSION (1)


#define MBOOT_PACK_HYDRO_CONTEXT "mbootenc"


#define MBOOT_PACK_DFU_CHUNK_BUF_SIZE (MBOOT_PACK_CHUNKSIZE + hydro_secretbox_HEADERBYTES)

enum mboot_pack_chunk_format {
    MBOOT_PACK_CHUNK_META = 0,
    MBOOT_PACK_CHUNK_FULL_SIG = 1,
    MBOOT_PACK_CHUNK_FW_RAW = 2,
    MBOOT_PACK_CHUNK_FW_GZIP = 3,
};



typedef struct _mboot_pack_chunk_buf_t {
    struct {
        uint8_t header_vers;
        uint8_t format;  
        uint8_t _pad[2];
        uint32_t address;
        uint32_t length; 
    } header;
    uint8_t data[MBOOT_PACK_DFU_CHUNK_BUF_SIZE];
    uint8_t signature[hydro_sign_BYTES];
} mboot_pack_chunk_buf_t;


extern const uint8_t mboot_pack_sign_public_key[hydro_sign_PUBLICKEYBYTES];
extern const uint8_t mboot_pack_secretbox_key[hydro_secretbox_KEYBYTES];




void mboot_pack_init(void);
int mboot_pack_write(uint32_t addr, const uint8_t *src8, size_t len, bool dry_run);

#endif 

#endif 
