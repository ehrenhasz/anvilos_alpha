 
 

#ifndef _SYS_BITOPS_H
#define	_SYS_BITOPS_H

#include <sys/zfs_context.h>

#ifdef	__cplusplus
extern "C" {
#endif

 
#define	BF32_DECODE(x, low, len)	P2PHASE((x) >> (low), 1U << (len))
#define	BF64_DECODE(x, low, len)	P2PHASE((x) >> (low), 1ULL << (len))
#define	BF32_ENCODE(x, low, len)	(P2PHASE((x), 1U << (len)) << (low))
#define	BF64_ENCODE(x, low, len)	(P2PHASE((x), 1ULL << (len)) << (low))

#define	BF32_GET(x, low, len)		BF32_DECODE(x, low, len)
#define	BF64_GET(x, low, len)		BF64_DECODE(x, low, len)

#define	BF32_SET(x, low, len, val) do { \
	ASSERT3U(val, <, 1U << (len)); \
	ASSERT3U(low + len, <=, 32); \
	(x) ^= BF32_ENCODE((x >> low) ^ (val), low, len); \
} while (0)

#define	BF64_SET(x, low, len, val) do { \
	ASSERT3U(val, <, 1ULL << (len)); \
	ASSERT3U(low + len, <=, 64); \
	((x) ^= BF64_ENCODE((x >> low) ^ (val), low, len)); \
} while (0)

#define	BF32_GET_SB(x, low, len, shift, bias)	\
	((BF32_GET(x, low, len) + (bias)) << (shift))
#define	BF64_GET_SB(x, low, len, shift, bias)	\
	((BF64_GET(x, low, len) + (bias)) << (shift))

 
#define	BF32_SET_SB(x, low, len, shift, bias, val) do { \
	ASSERT3U(IS_P2ALIGNED(val, 1U << shift), !=, B_FALSE); \
	ASSERT3S((val) >> (shift), >=, bias); \
	BF32_SET(x, low, len, ((val) >> (shift)) - (bias)); \
} while (0)
#define	BF64_SET_SB(x, low, len, shift, bias, val) do { \
	ASSERT3U(IS_P2ALIGNED(val, 1ULL << shift), !=, B_FALSE); \
	ASSERT3S((val) >> (shift), >=, bias); \
	BF64_SET(x, low, len, ((val) >> (shift)) - (bias)); \
} while (0)

#ifdef	__cplusplus
}
#endif

#endif  
