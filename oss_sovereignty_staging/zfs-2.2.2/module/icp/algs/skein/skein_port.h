 
 

#ifndef	_SKEIN_PORT_H_
#define	_SKEIN_PORT_H_

#include <sys/types.h>	 

#ifndef	RotL_64
#define	RotL_64(x, N)	(((x) << (N)) | ((x) >> (64 - (N))))
#endif

 
#ifndef	SKEIN_NEED_SWAP		 

#include <sys/isa_defs.h>	 

#if	defined(_ZFS_BIG_ENDIAN)
 
#define	SKEIN_NEED_SWAP   (1)
#else
 
#define	SKEIN_NEED_SWAP   (0)
#define	Skein_Put64_LSB_First(dst08, src64, bCnt) memcpy(dst08, src64, bCnt)
#define	Skein_Get64_LSB_First(dst64, src08, wCnt) \
	memcpy(dst64, src08, 8 * (wCnt))
#endif

#endif				 

 
#ifndef	Skein_Swap64	 
#if	SKEIN_NEED_SWAP
#define	Skein_Swap64(w64)				\
	(((((uint64_t)(w64)) & 0xFF) << 56) |		\
	(((((uint64_t)(w64)) >> 8) & 0xFF) << 48) |	\
	(((((uint64_t)(w64)) >> 16) & 0xFF) << 40) |	\
	(((((uint64_t)(w64)) >> 24) & 0xFF) << 32) |	\
	(((((uint64_t)(w64)) >> 32) & 0xFF) << 24) |	\
	(((((uint64_t)(w64)) >> 40) & 0xFF) << 16) |	\
	(((((uint64_t)(w64)) >> 48) & 0xFF) << 8) |	\
	(((((uint64_t)(w64)) >> 56) & 0xFF)))
#else
#define	Skein_Swap64(w64)  (w64)
#endif
#endif				 

#ifndef	Skein_Put64_LSB_First
static inline void
Skein_Put64_LSB_First(uint8_t *dst, const uint64_t *src, size_t bCnt)
{
	 
	size_t n;

	for (n = 0; n < bCnt; n++)
		dst[n] = (uint8_t)(src[n >> 3] >> (8 * (n & 7)));
}
#endif				 

#ifndef	Skein_Get64_LSB_First
static inline void
Skein_Get64_LSB_First(uint64_t *dst, const uint8_t *src, size_t wCnt)
{
	 
	size_t n;

	for (n = 0; n < 8 * wCnt; n += 8)
		dst[n / 8] = (((uint64_t)src[n])) +
		    (((uint64_t)src[n + 1]) << 8) +
		    (((uint64_t)src[n + 2]) << 16) +
		    (((uint64_t)src[n + 3]) << 24) +
		    (((uint64_t)src[n + 4]) << 32) +
		    (((uint64_t)src[n + 5]) << 40) +
		    (((uint64_t)src[n + 6]) << 48) +
		    (((uint64_t)src[n + 7]) << 56);
}
#endif				 

#endif	 
