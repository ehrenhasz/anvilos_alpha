 

 

#ifndef _SYS_BLKPTR_H
#define	_SYS_BLKPTR_H

#include <sys/spa.h>
#include <sys/zio.h>

#ifdef	__cplusplus
extern "C" {
#endif

void encode_embedded_bp_compressed(blkptr_t *, void *,
    enum zio_compress, int, int);
void decode_embedded_bp_compressed(const blkptr_t *, void *);
int decode_embedded_bp(const blkptr_t *, void *, int);

#ifdef	__cplusplus
}
#endif

#endif	 
