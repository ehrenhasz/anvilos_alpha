 

 

 
 

 

#ifndef _OPENSOLARIS_SYS_BYTEORDER_H_
#define	_OPENSOLARIS_SYS_BYTEORDER_H_

#include <sys/endian.h>

#ifdef __COVERITY__
 
#define	BSWAP_8(x)	((x) & 0xff)
#define	BSWAP_16(x)	((x) & 0xffff)
#define	BSWAP_32(x)	((x) & 0xffffffff)
#define	BSWAP_64(x)	(x)

#else  

 
#define	BSWAP_8(x)	((x) & 0xff)
#define	BSWAP_16(x)	((BSWAP_8(x) << 8) | BSWAP_8((x) >> 8))
#define	BSWAP_32(x)	((BSWAP_16(x) << 16) | BSWAP_16((x) >> 16))
#define	BSWAP_64(x)	((BSWAP_32(x) << 32) | BSWAP_32((x) >> 32))

#endif  

#define	BMASK_8(x)	((x) & 0xff)
#define	BMASK_16(x)	((x) & 0xffff)
#define	BMASK_32(x)	((x) & 0xffffffff)
#define	BMASK_64(x)	(x)

 
#if BYTE_ORDER == _BIG_ENDIAN
#define	BE_8(x)		BMASK_8(x)
#define	BE_16(x)	BMASK_16(x)
#define	BE_32(x)	BMASK_32(x)
#define	BE_64(x)	BMASK_64(x)
#define	LE_8(x)		BSWAP_8(x)
#define	LE_16(x)	BSWAP_16(x)
#define	LE_32(x)	BSWAP_32(x)
#define	LE_64(x)	BSWAP_64(x)
#else
#define	LE_8(x)		BMASK_8(x)
#define	LE_16(x)	BMASK_16(x)
#define	LE_32(x)	BMASK_32(x)
#define	LE_64(x)	BMASK_64(x)
#define	BE_8(x)		BSWAP_8(x)
#define	BE_16(x)	BSWAP_16(x)
#define	BE_32(x)	BSWAP_32(x)
#define	BE_64(x)	BSWAP_64(x)
#endif

#if !defined(_STANDALONE)
#if BYTE_ORDER == _BIG_ENDIAN
#define	htonll(x)	BMASK_64(x)
#define	ntohll(x)	BMASK_64(x)
#else  
#ifndef __LP64__
static __inline__ uint64_t
htonll(uint64_t n)
{
	return ((((uint64_t)htonl(n)) << 32) + htonl(n >> 32));
}

static __inline__ uint64_t
ntohll(uint64_t n)
{
	return ((((uint64_t)ntohl(n)) << 32) + ntohl(n >> 32));
}
#else	 
#define	htonll(x)	BSWAP_64(x)
#define	ntohll(x)	BSWAP_64(x)
#endif	 
#endif	 
#endif	 

#define	BE_IN32(xa)	htonl(*((uint32_t *)(void *)(xa)))

#endif  
