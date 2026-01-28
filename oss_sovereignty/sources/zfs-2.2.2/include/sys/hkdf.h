



#ifndef	_SYS_HKDF_H_
#define	_SYS_HKDF_H_

#include <sys/types.h>

int hkdf_sha512(uint8_t *key_material, uint_t km_len, uint8_t *salt,
    uint_t salt_len, uint8_t *info, uint_t info_len, uint8_t *output_key,
    uint_t out_len);

#endif	
