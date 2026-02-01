 

#include "uzlib.h"

#define FTEXT    1
#define FHCRC    2
#define FEXTRA   4
#define FNAME    8
#define FCOMMENT 16

void tinf_skip_bytes(uzlib_uncomp_t *d, int num);
uint16_t tinf_get_uint16(uzlib_uncomp_t *d);

void tinf_skip_bytes(uzlib_uncomp_t *d, int num)
{
    while (num--) uzlib_get_byte(d);
}

uint16_t tinf_get_uint16(uzlib_uncomp_t *d)
{
    unsigned int v = uzlib_get_byte(d);
    v = (uzlib_get_byte(d) << 8) | v;
    return v;
}

int uzlib_parse_zlib_gzip_header(uzlib_uncomp_t *d, int *wbits)
{
     
    unsigned char cmf = uzlib_get_byte(d);
    unsigned char flg = uzlib_get_byte(d);

     
    if (cmf == 0x1f && flg == 0x8b) {
         
        if (uzlib_get_byte(d) != 8) return UZLIB_DATA_ERROR;

         
        flg = uzlib_get_byte(d);

         
        if (flg & 0xe0) return UZLIB_DATA_ERROR;

         

         
        tinf_skip_bytes(d, 6);

         
        if (flg & FEXTRA)
        {
           unsigned int xlen = tinf_get_uint16(d);
           tinf_skip_bytes(d, xlen);
        }

         
        if (flg & FNAME) { while (uzlib_get_byte(d)); }

         
        if (flg & FCOMMENT) { while (uzlib_get_byte(d)); }

         
        if (flg & FHCRC)
        {
             tinf_get_uint16(d);

            
    
    
        }

         
        d->checksum_type = UZLIB_CHKSUM_CRC;
        d->checksum = ~0;

         
        *wbits = 15;

        return UZLIB_HEADER_GZIP;
    } else {
        
       if ((256*cmf + flg) % 31) return UZLIB_DATA_ERROR;

        
       if ((cmf & 0x0f) != 8) return UZLIB_DATA_ERROR;

        
       if ((cmf >> 4) > 7) return UZLIB_DATA_ERROR;

        
       if (flg & 0x20) return UZLIB_DATA_ERROR;

        
       d->checksum_type = UZLIB_CHKSUM_ADLER;
       d->checksum = 1;

       *wbits = (cmf >> 4) + 8;

        return UZLIB_HEADER_ZLIB;
    }
}
