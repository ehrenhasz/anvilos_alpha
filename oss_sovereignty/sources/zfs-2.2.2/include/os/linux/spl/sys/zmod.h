

#ifndef _SPL_ZMOD_H
#define	_SPL_ZMOD_H

#include <sys/types.h>
#include <linux/zlib.h>

extern int z_compress_level(void *dest, size_t *destLen, const void *source,
    size_t sourceLen, int level);
extern int z_uncompress(void *dest, size_t *destLen, const void *source,
    size_t sourceLen);

int spl_zlib_init(void);
void spl_zlib_fini(void);

#endif 
