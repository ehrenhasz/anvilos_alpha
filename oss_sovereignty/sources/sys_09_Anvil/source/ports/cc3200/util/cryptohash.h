
#ifndef MICROPY_INCLUDED_CC3200_UTIL_CRYPTOHASH_H
#define MICROPY_INCLUDED_CC3200_UTIL_CRYPTOHASH_H


extern void CRYPTOHASH_Init (void);
extern void CRYPTOHASH_SHAMD5Start (uint32_t algo, uint32_t blocklen);
extern void CRYPTOHASH_SHAMD5Update (uint8_t *data, uint32_t datalen);
extern void CRYPTOHASH_SHAMD5Read (uint8_t *hash);

#endif 
