



















 

#include <cityhash.h>

#define	HASH_K1 0xb492b66fbe98f273ULL
#define	HASH_K2 0x9ae16a3b2f90404fULL

 
static inline uint64_t
rotate(uint64_t val, int shift)
{
	
	return (shift == 0 ? val : (val >> shift) | (val << (64 - shift)));
}

static inline uint64_t
cityhash_helper(uint64_t u, uint64_t v, uint64_t mul)
{
	uint64_t a = (u ^ v) * mul;
	a ^= (a >> 47);
	uint64_t b = (v ^ a) * mul;
	b ^= (b >> 47);
	b *= mul;
	return (b);
}

uint64_t
cityhash4(uint64_t w1, uint64_t w2, uint64_t w3, uint64_t w4)
{
	uint64_t mul = HASH_K2 + 64;
	uint64_t a = w1 * HASH_K1;
	uint64_t b = w2;
	uint64_t c = w4 * mul;
	uint64_t d = w3 * HASH_K2;
	return (cityhash_helper(rotate(a + b, 43) + rotate(c, 30) + d,
	    a + rotate(b + HASH_K2, 18) + c, mul));

}

#if defined(_KERNEL)
EXPORT_SYMBOL(cityhash4);
#endif
