#include <sys/zfs_context.h>
#include <sys/zio_compress.h>
static int real_LZ4_compress(const char *source, char *dest, int isize,
    int osize);
static int LZ4_compressCtx(void *ctx, const char *source, char *dest,
    int isize, int osize);
static int LZ4_compress64kCtx(void *ctx, const char *source, char *dest,
    int isize, int osize);
int LZ4_uncompress_unknownOutputSize(const char *source, char *dest,
    int isize, int maxOutputSize);
static kmem_cache_t *lz4_cache;
size_t
lz4_compress_zfs(void *s_start, void *d_start, size_t s_len,
    size_t d_len, int n)
{
	(void) n;
	uint32_t bufsiz;
	char *dest = d_start;
	ASSERT(d_len >= sizeof (bufsiz));
	bufsiz = real_LZ4_compress(s_start, &dest[sizeof (bufsiz)], s_len,
	    d_len - sizeof (bufsiz));
	if (bufsiz == 0)
		return (s_len);
	*(uint32_t *)dest = BE_32(bufsiz);
	return (bufsiz + sizeof (bufsiz));
}
int
lz4_decompress_zfs(void *s_start, void *d_start, size_t s_len,
    size_t d_len, int n)
{
	(void) n;
	const char *src = s_start;
	uint32_t bufsiz = BE_IN32(src);
	if (bufsiz + sizeof (bufsiz) > s_len)
		return (1);
	return (LZ4_uncompress_unknownOutputSize(&src[sizeof (bufsiz)],
	    d_start, bufsiz, d_len) < 0);
}
#define	COMPRESSIONLEVEL 12
#define	NOTCOMPRESSIBLE_CONFIRMATION 6
#if defined(_LP64)
#define	LZ4_ARCH64 1
#else
#define	LZ4_ARCH64 0
#endif
#if defined(_ZFS_BIG_ENDIAN)
#define	LZ4_BIG_ENDIAN 1
#else
#undef LZ4_BIG_ENDIAN
#endif
#if defined(__ARM_FEATURE_UNALIGNED)
#define	LZ4_FORCE_UNALIGNED_ACCESS 1
#endif
#undef	LZ4_FORCE_SW_BITCOUNT
#if defined(__sparc)
#define	LZ4_FORCE_SW_BITCOUNT
#endif
#define	restrict
#ifdef GCC_VERSION
#undef GCC_VERSION
#endif
#define	GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#if (GCC_VERSION >= 302) || (__INTEL_COMPILER >= 800) || defined(__clang__)
#define	expect(expr, value)    (__builtin_expect((expr), (value)))
#else
#define	expect(expr, value)    (expr)
#endif
#ifndef likely
#define	likely(expr)	expect((expr) != 0, 1)
#endif
#ifndef unlikely
#define	unlikely(expr)	expect((expr) != 0, 0)
#endif
#define	lz4_bswap16(x) ((unsigned short int) ((((x) >> 8) & 0xffu) | \
	(((x) & 0xffu) << 8)))
#define	BYTE	uint8_t
#define	U16	uint16_t
#define	U32	uint32_t
#define	S32	int32_t
#define	U64	uint64_t
#ifndef LZ4_FORCE_UNALIGNED_ACCESS
#pragma pack(1)
#endif
typedef struct _U16_S {
	U16 v;
} U16_S;
typedef struct _U32_S {
	U32 v;
} U32_S;
typedef struct _U64_S {
	U64 v;
} U64_S;
#ifndef LZ4_FORCE_UNALIGNED_ACCESS
#pragma pack()
#endif
#define	A64(x) (((U64_S *)(x))->v)
#define	A32(x) (((U32_S *)(x))->v)
#define	A16(x) (((U16_S *)(x))->v)
#define	MINMATCH 4
#define	HASH_LOG COMPRESSIONLEVEL
#define	HASHTABLESIZE (1 << HASH_LOG)
#define	HASH_MASK (HASHTABLESIZE - 1)
#define	SKIPSTRENGTH (NOTCOMPRESSIBLE_CONFIRMATION > 2 ? \
	NOTCOMPRESSIBLE_CONFIRMATION : 2)
#define	COPYLENGTH 8
#define	LASTLITERALS 5
#define	MFLIMIT (COPYLENGTH + MINMATCH)
#define	MINLENGTH (MFLIMIT + 1)
#define	MAXD_LOG 16
#define	MAX_DISTANCE ((1 << MAXD_LOG) - 1)
#define	ML_BITS 4
#define	ML_MASK ((1U<<ML_BITS)-1)
#define	RUN_BITS (8-ML_BITS)
#define	RUN_MASK ((1U<<RUN_BITS)-1)
#if LZ4_ARCH64
#define	STEPSIZE 8
#define	UARCH U64
#define	AARCH A64
#define	LZ4_COPYSTEP(s, d)	A64(d) = A64(s); d += 8; s += 8;
#define	LZ4_COPYPACKET(s, d)	LZ4_COPYSTEP(s, d)
#define	LZ4_SECURECOPY(s, d, e)	if (d < e) LZ4_WILDCOPY(s, d, e)
#define	HTYPE U32
#define	INITBASE(base)		const BYTE* const base = ip
#else  
#define	STEPSIZE 4
#define	UARCH U32
#define	AARCH A32
#define	LZ4_COPYSTEP(s, d)	A32(d) = A32(s); d += 4; s += 4;
#define	LZ4_COPYPACKET(s, d)	LZ4_COPYSTEP(s, d); LZ4_COPYSTEP(s, d);
#define	LZ4_SECURECOPY		LZ4_WILDCOPY
#define	HTYPE const BYTE *
#define	INITBASE(base)		const int base = 0
#endif  
#if (defined(LZ4_BIG_ENDIAN) && !defined(BIG_ENDIAN_NATIVE_BUT_INCOMPATIBLE))
#define	LZ4_READ_LITTLEENDIAN_16(d, s, p) \
	{ U16 v = A16(p); v = lz4_bswap16(v); d = (s) - v; }
#define	LZ4_WRITE_LITTLEENDIAN_16(p, i) \
	{ U16 v = (U16)(i); v = lz4_bswap16(v); A16(p) = v; p += 2; }
#else
#define	LZ4_READ_LITTLEENDIAN_16(d, s, p) { d = (s) - A16(p); }
#define	LZ4_WRITE_LITTLEENDIAN_16(p, v)  { A16(p) = v; p += 2; }
#endif
struct refTables {
	HTYPE hashTable[HASHTABLESIZE];
};
#define	LZ4_HASH_FUNCTION(i) (((i) * 2654435761U) >> ((MINMATCH * 8) - \
	HASH_LOG))
#define	LZ4_HASH_VALUE(p) LZ4_HASH_FUNCTION(A32(p))
#define	LZ4_WILDCOPY(s, d, e) do { LZ4_COPYPACKET(s, d) } while (d < e);
#define	LZ4_BLINDCOPY(s, d, l) { BYTE* e = (d) + l; LZ4_WILDCOPY(s, d, e); \
	d = e; }
#if LZ4_ARCH64
static inline int
LZ4_NbCommonBytes(register U64 val)
{
#if defined(LZ4_BIG_ENDIAN)
#if ((defined(__GNUC__) && (GCC_VERSION >= 304)) || defined(__clang__)) && \
	!defined(LZ4_FORCE_SW_BITCOUNT)
	return (__builtin_clzll(val) >> 3);
#else
	int r;
	if (!(val >> 32)) {
		r = 4;
	} else {
		r = 0;
		val >>= 32;
	}
	if (!(val >> 16)) {
		r += 2;
		val >>= 8;
	} else {
		val >>= 24;
	}
	r += (!val);
	return (r);
#endif
#else
#if ((defined(__GNUC__) && (GCC_VERSION >= 304)) || defined(__clang__)) && \
	!defined(LZ4_FORCE_SW_BITCOUNT)
	return (__builtin_ctzll(val) >> 3);
#else
	static const int DeBruijnBytePos[64] =
	    { 0, 0, 0, 0, 0, 1, 1, 2, 0, 3, 1, 3, 1, 4, 2, 7, 0, 2, 3, 6, 1, 5,
		3, 5, 1, 3, 4, 4, 2, 5, 6, 7, 7, 0, 1, 2, 3, 3, 4, 6, 2, 6, 5,
		5, 3, 4, 5, 6, 7, 1, 2, 4, 6, 4,
		4, 5, 7, 2, 6, 5, 7, 6, 7, 7
	};
	return DeBruijnBytePos[((U64) ((val & -val) * 0x0218A392CDABBD3F)) >>
	    58];
#endif
#endif
}
#else
static inline int
LZ4_NbCommonBytes(register U32 val)
{
#if defined(LZ4_BIG_ENDIAN)
#if ((defined(__GNUC__) && (GCC_VERSION >= 304)) || defined(__clang__)) && \
	!defined(LZ4_FORCE_SW_BITCOUNT)
	return (__builtin_clz(val) >> 3);
#else
	int r;
	if (!(val >> 16)) {
		r = 2;
		val >>= 8;
	} else {
		r = 0;
		val >>= 24;
	}
	r += (!val);
	return (r);
#endif
#else
#if defined(__GNUC__) && (GCC_VERSION >= 304) && \
	!defined(LZ4_FORCE_SW_BITCOUNT)
	return (__builtin_ctz(val) >> 3);
#else
	static const int DeBruijnBytePos[32] = {
		0, 0, 3, 0, 3, 1, 3, 0,
		3, 2, 2, 1, 3, 2, 0, 1,
		3, 3, 1, 2, 2, 2, 2, 0,
		3, 1, 2, 0, 1, 0, 1, 1
	};
	return DeBruijnBytePos[((U32) ((val & -(S32) val) * 0x077CB531U)) >>
	    27];
#endif
#endif
}
#endif
static int
LZ4_compressCtx(void *ctx, const char *source, char *dest, int isize,
    int osize)
{
	struct refTables *srt = (struct refTables *)ctx;
	HTYPE *HashTable = (HTYPE *) (srt->hashTable);
	const BYTE *ip = (BYTE *) source;
	INITBASE(base);
	const BYTE *anchor = ip;
	const BYTE *const iend = ip + isize;
	const BYTE *const oend = (BYTE *) dest + osize;
	const BYTE *const mflimit = iend - MFLIMIT;
#define	matchlimit (iend - LASTLITERALS)
	BYTE *op = (BYTE *) dest;
	int len, length;
	const int skipStrength = SKIPSTRENGTH;
	U32 forwardH;
	if (isize < MINLENGTH)
		goto _last_literals;
	HashTable[LZ4_HASH_VALUE(ip)] = ip - base;
	ip++;
	forwardH = LZ4_HASH_VALUE(ip);
	for (;;) {
		int findMatchAttempts = (1U << skipStrength) + 3;
		const BYTE *forwardIp = ip;
		const BYTE *ref;
		BYTE *token;
		do {
			U32 h = forwardH;
			int step = findMatchAttempts++ >> skipStrength;
			ip = forwardIp;
			forwardIp = ip + step;
			if (unlikely(forwardIp > mflimit)) {
				goto _last_literals;
			}
			forwardH = LZ4_HASH_VALUE(forwardIp);
			ref = base + HashTable[h];
			HashTable[h] = ip - base;
		} while ((ref < ip - MAX_DISTANCE) || (A32(ref) != A32(ip)));
		while ((ip > anchor) && (ref > (BYTE *) source) &&
		    unlikely(ip[-1] == ref[-1])) {
			ip--;
			ref--;
		}
		length = ip - anchor;
		token = op++;
		if (unlikely(op + length + (2 + 1 + LASTLITERALS) +
		    (length >> 8) > oend))
			return (0);
		if (length >= (int)RUN_MASK) {
			*token = (RUN_MASK << ML_BITS);
			len = length - RUN_MASK;
			for (; len > 254; len -= 255)
				*op++ = 255;
			*op++ = (BYTE)len;
		} else
			*token = (length << ML_BITS);
		LZ4_BLINDCOPY(anchor, op, length);
		_next_match:
		LZ4_WRITE_LITTLEENDIAN_16(op, ip - ref);
		ip += MINMATCH;
		ref += MINMATCH;	 
		anchor = ip;
		while (likely(ip < matchlimit - (STEPSIZE - 1))) {
			UARCH diff = AARCH(ref) ^ AARCH(ip);
			if (!diff) {
				ip += STEPSIZE;
				ref += STEPSIZE;
				continue;
			}
			ip += LZ4_NbCommonBytes(diff);
			goto _endCount;
		}
#if LZ4_ARCH64
		if ((ip < (matchlimit - 3)) && (A32(ref) == A32(ip))) {
			ip += 4;
			ref += 4;
		}
#endif
		if ((ip < (matchlimit - 1)) && (A16(ref) == A16(ip))) {
			ip += 2;
			ref += 2;
		}
		if ((ip < matchlimit) && (*ref == *ip))
			ip++;
		_endCount:
		len = (ip - anchor);
		if (unlikely(op + (1 + LASTLITERALS) + (len >> 8) > oend))
			return (0);
		if (len >= (int)ML_MASK) {
			*token += ML_MASK;
			len -= ML_MASK;
			for (; len > 509; len -= 510) {
				*op++ = 255;
				*op++ = 255;
			}
			if (len > 254) {
				len -= 255;
				*op++ = 255;
			}
			*op++ = (BYTE)len;
		} else
			*token += len;
		if (ip > mflimit) {
			anchor = ip;
			break;
		}
		HashTable[LZ4_HASH_VALUE(ip - 2)] = ip - 2 - base;
		ref = base + HashTable[LZ4_HASH_VALUE(ip)];
		HashTable[LZ4_HASH_VALUE(ip)] = ip - base;
		if ((ref > ip - (MAX_DISTANCE + 1)) && (A32(ref) == A32(ip))) {
			token = op++;
			*token = 0;
			goto _next_match;
		}
		anchor = ip++;
		forwardH = LZ4_HASH_VALUE(ip);
	}
	_last_literals:
	{
		int lastRun = iend - anchor;
		if (op + lastRun + 1 + ((lastRun + 255 - RUN_MASK) / 255) >
		    oend)
			return (0);
		if (lastRun >= (int)RUN_MASK) {
			*op++ = (RUN_MASK << ML_BITS);
			lastRun -= RUN_MASK;
			for (; lastRun > 254; lastRun -= 255) {
				*op++ = 255;
			}
			*op++ = (BYTE)lastRun;
		} else
			*op++ = (lastRun << ML_BITS);
		(void) memcpy(op, anchor, iend - anchor);
		op += iend - anchor;
	}
	return (int)(((char *)op) - dest);
}
#define	LZ4_64KLIMIT ((1 << 16) + (MFLIMIT - 1))
#define	HASHLOG64K (HASH_LOG + 1)
#define	HASH64KTABLESIZE (1U << HASHLOG64K)
#define	LZ4_HASH64K_FUNCTION(i)	(((i) * 2654435761U) >> ((MINMATCH*8) - \
	HASHLOG64K))
#define	LZ4_HASH64K_VALUE(p)	LZ4_HASH64K_FUNCTION(A32(p))
static int
LZ4_compress64kCtx(void *ctx, const char *source, char *dest, int isize,
    int osize)
{
	struct refTables *srt = (struct refTables *)ctx;
	U16 *HashTable = (U16 *) (srt->hashTable);
	const BYTE *ip = (BYTE *) source;
	const BYTE *anchor = ip;
	const BYTE *const base = ip;
	const BYTE *const iend = ip + isize;
	const BYTE *const oend = (BYTE *) dest + osize;
	const BYTE *const mflimit = iend - MFLIMIT;
#define	matchlimit (iend - LASTLITERALS)
	BYTE *op = (BYTE *) dest;
	int len, length;
	const int skipStrength = SKIPSTRENGTH;
	U32 forwardH;
	if (isize < MINLENGTH)
		goto _last_literals;
	ip++;
	forwardH = LZ4_HASH64K_VALUE(ip);
	for (;;) {
		int findMatchAttempts = (1U << skipStrength) + 3;
		const BYTE *forwardIp = ip;
		const BYTE *ref;
		BYTE *token;
		do {
			U32 h = forwardH;
			int step = findMatchAttempts++ >> skipStrength;
			ip = forwardIp;
			forwardIp = ip + step;
			if (forwardIp > mflimit) {
				goto _last_literals;
			}
			forwardH = LZ4_HASH64K_VALUE(forwardIp);
			ref = base + HashTable[h];
			HashTable[h] = ip - base;
		} while (A32(ref) != A32(ip));
		while ((ip > anchor) && (ref > (BYTE *) source) &&
		    (ip[-1] == ref[-1])) {
			ip--;
			ref--;
		}
		length = ip - anchor;
		token = op++;
		if (unlikely(op + length + (2 + 1 + LASTLITERALS) +
		    (length >> 8) > oend))
			return (0);
		if (length >= (int)RUN_MASK) {
			*token = (RUN_MASK << ML_BITS);
			len = length - RUN_MASK;
			for (; len > 254; len -= 255)
				*op++ = 255;
			*op++ = (BYTE)len;
		} else
			*token = (length << ML_BITS);
		LZ4_BLINDCOPY(anchor, op, length);
		_next_match:
		LZ4_WRITE_LITTLEENDIAN_16(op, ip - ref);
		ip += MINMATCH;
		ref += MINMATCH;	 
		anchor = ip;
		while (ip < matchlimit - (STEPSIZE - 1)) {
			UARCH diff = AARCH(ref) ^ AARCH(ip);
			if (!diff) {
				ip += STEPSIZE;
				ref += STEPSIZE;
				continue;
			}
			ip += LZ4_NbCommonBytes(diff);
			goto _endCount;
		}
#if LZ4_ARCH64
		if ((ip < (matchlimit - 3)) && (A32(ref) == A32(ip))) {
			ip += 4;
			ref += 4;
		}
#endif
		if ((ip < (matchlimit - 1)) && (A16(ref) == A16(ip))) {
			ip += 2;
			ref += 2;
		}
		if ((ip < matchlimit) && (*ref == *ip))
			ip++;
		_endCount:
		len = (ip - anchor);
		if (unlikely(op + (1 + LASTLITERALS) + (len >> 8) > oend))
			return (0);
		if (len >= (int)ML_MASK) {
			*token += ML_MASK;
			len -= ML_MASK;
			for (; len > 509; len -= 510) {
				*op++ = 255;
				*op++ = 255;
			}
			if (len > 254) {
				len -= 255;
				*op++ = 255;
			}
			*op++ = (BYTE)len;
		} else
			*token += len;
		if (ip > mflimit) {
			anchor = ip;
			break;
		}
		HashTable[LZ4_HASH64K_VALUE(ip - 2)] = ip - 2 - base;
		ref = base + HashTable[LZ4_HASH64K_VALUE(ip)];
		HashTable[LZ4_HASH64K_VALUE(ip)] = ip - base;
		if (A32(ref) == A32(ip)) {
			token = op++;
			*token = 0;
			goto _next_match;
		}
		anchor = ip++;
		forwardH = LZ4_HASH64K_VALUE(ip);
	}
	_last_literals:
	{
		int lastRun = iend - anchor;
		if (op + lastRun + 1 + ((lastRun + 255 - RUN_MASK) / 255) >
		    oend)
			return (0);
		if (lastRun >= (int)RUN_MASK) {
			*op++ = (RUN_MASK << ML_BITS);
			lastRun -= RUN_MASK;
			for (; lastRun > 254; lastRun -= 255)
				*op++ = 255;
			*op++ = (BYTE)lastRun;
		} else
			*op++ = (lastRun << ML_BITS);
		(void) memcpy(op, anchor, iend - anchor);
		op += iend - anchor;
	}
	return (int)(((char *)op) - dest);
}
static int
real_LZ4_compress(const char *source, char *dest, int isize, int osize)
{
	void *ctx;
	int result;
	ASSERT(lz4_cache != NULL);
	ctx = kmem_cache_alloc(lz4_cache, KM_SLEEP);
	if (ctx == NULL)
		return (0);
	memset(ctx, 0, sizeof (struct refTables));
	if (isize < LZ4_64KLIMIT)
		result = LZ4_compress64kCtx(ctx, source, dest, isize, osize);
	else
		result = LZ4_compressCtx(ctx, source, dest, isize, osize);
	kmem_cache_free(lz4_cache, ctx);
	return (result);
}
void
lz4_init(void)
{
	lz4_cache = kmem_cache_create("lz4_cache",
	    sizeof (struct refTables), 0, NULL, NULL, NULL, NULL, NULL, 0);
}
void
lz4_fini(void)
{
	if (lz4_cache) {
		kmem_cache_destroy(lz4_cache);
		lz4_cache = NULL;
	}
}
