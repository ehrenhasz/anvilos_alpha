#ifndef _h_PSTMATH
#define _h_PSTMATH
#ifndef DISABLE_PSTM
#ifndef CHAR_BIT
#define CHAR_BIT	8
#endif
#ifndef HAVE_NATIVE_INT64
	#define PSTM_16BIT
#endif  
#ifdef PSTM_8BIT
	typedef unsigned char		pstm_digit;
	typedef unsigned short		pstm_word;
	#define DIGIT_BIT			8
#elif defined(PSTM_16BIT)
	typedef unsigned short		pstm_digit;
	typedef unsigned long		pstm_word;
	#define	DIGIT_BIT			16
#elif defined(PSTM_64BIT)
	#ifndef __GNUC__
	#error "64bit digits requires GCC"
	#endif
	typedef unsigned long		pstm_digit;
	typedef unsigned long		pstm_word __attribute__ ((mode(TI)));
	#define DIGIT_BIT			64
#else
	typedef uint32			pstm_digit;
	typedef uint64			pstm_word;
	#define DIGIT_BIT		32
	#define PSTM_32BIT
#endif  
#define PSTM_MASK			(pstm_digit)(-1)
#define PSTM_DIGIT_MAX		PSTM_MASK
#define PSTM_LT			-1		 
#define PSTM_EQ			0		 
#define PSTM_GT			1		 
#define PSTM_ZPOS		0		 
#define PSTM_NEG		1		 
#define PSTM_OKAY		PS_SUCCESS
#define PSTM_MEM		PS_MEM_FAIL
#define PSTM_DEFAULT_INIT 64		 
#define PSTM_MAX_SIZE	4096
typedef struct  {
	int	used, alloc, sign;  
	pstm_digit	*dp;
} pstm_int;
#define pstm_iszero(a) (((a)->used == 0) ? PS_TRUE : PS_FALSE)
#define pstm_iseven(a) (((a)->used > 0 && (((a)->dp[0] & 1) == 0)) ? PS_TRUE : PS_FALSE)
#define pstm_isodd(a)  (((a)->used > 0 && (((a)->dp[0] & 1) == 1)) ? PS_TRUE : PS_FALSE)
#define pstm_abs(a, b)  { pstm_copy(a, b); (b)->sign  = 0; }
#define pstm_init(pool, a) \
        pstm_init(      a)
#define pstm_init_size(pool, a, size) \
        pstm_init_size(      a, size)
extern int32 pstm_init_size(psPool_t *pool, pstm_int * a, uint32 size) FAST_FUNC;
#define pstm_init_copy(pool, a, b, toSqr) \
        pstm_init_copy(      a, b, toSqr)
#define pstm_init_for_read_unsigned_bin(pool, a, len) \
        pstm_init_for_read_unsigned_bin(      a, len)
extern int32 pstm_init_for_read_unsigned_bin(psPool_t *pool, pstm_int *a,
				uint32 len) FAST_FUNC;
extern int32 pstm_read_unsigned_bin(pstm_int *a, unsigned char *b, int32 c) FAST_FUNC;
extern int32 pstm_unsigned_bin_size(pstm_int *a) FAST_FUNC;
extern int32 pstm_copy(pstm_int * a, pstm_int * b);
extern void pstm_clear(pstm_int * a) FAST_FUNC;
extern void pstm_clear_multi(pstm_int *mp0, pstm_int *mp1, pstm_int *mp2,
				pstm_int *mp3, pstm_int *mp4, pstm_int *mp5, pstm_int *mp6,
				pstm_int *mp7) FAST_FUNC;
extern int32 pstm_grow(pstm_int * a, int size) FAST_FUNC;  
extern void pstm_clamp(pstm_int * a) FAST_FUNC;
extern int32 pstm_cmp(pstm_int * a, pstm_int * b) FAST_FUNC;
extern int32 pstm_cmp_mag(pstm_int * a, pstm_int * b) FAST_FUNC;
#define pstm_div(pool, a, b, c, d) \
        pstm_div(      a, b, c, d)
#define pstm_div_2d(pool, a, b, c, d) \
        pstm_div_2d(      a, b, c, d)
extern int32 pstm_div_2(pstm_int * a, pstm_int * b) FAST_FUNC;
extern int32 s_pstm_sub(pstm_int *a, pstm_int *b, pstm_int *c) FAST_FUNC;
extern int32 pstm_sub(pstm_int *a, pstm_int *b, pstm_int *c) FAST_FUNC;
#define pstm_sub_d(pool, a, b, c) \
        pstm_sub_d(      a, b, c)
extern int32 pstm_sub_d(psPool_t *pool, pstm_int *a, pstm_digit b, pstm_int *c) FAST_FUNC;
extern int32 pstm_mul_2(pstm_int * a, pstm_int * b) FAST_FUNC;
#define pstm_mod(pool, a, b, c) \
        pstm_mod(      a, b, c)
#define pstm_mulmod(pool, a, b, c, d) \
        pstm_mulmod(      a, b, c, d)
extern int32 pstm_mulmod(psPool_t *pool, pstm_int *a, pstm_int *b, pstm_int *c,
				pstm_int *d) FAST_FUNC;
#define pstm_exptmod(pool, G, X, P, Y) \
        pstm_exptmod(      G, X, P, Y)
extern int32 pstm_exptmod(psPool_t *pool, pstm_int *G, pstm_int *X, pstm_int *P,
				pstm_int *Y) FAST_FUNC;
extern int32 pstm_add(pstm_int *a, pstm_int *b, pstm_int *c) FAST_FUNC;
#define pstm_to_unsigned_bin(pool, a, b) \
        pstm_to_unsigned_bin(      a, b)
extern int32 pstm_to_unsigned_bin(psPool_t *pool, pstm_int *a,
				unsigned char *b) FAST_FUNC;
#define pstm_to_unsigned_bin_nr(pool, a, b) \
        pstm_to_unsigned_bin_nr(      a, b)
extern int32 pstm_to_unsigned_bin_nr(psPool_t *pool, pstm_int *a,
				unsigned char *b) FAST_FUNC;
#define pstm_montgomery_reduce(pool, a, m, mp, paD, paDlen) \
        pstm_montgomery_reduce(      a, m, mp, paD, paDlen)
extern int32 pstm_montgomery_reduce(psPool_t *pool, pstm_int *a, pstm_int *m,
				pstm_digit mp, pstm_digit *paD, uint32 paDlen) FAST_FUNC;
#define pstm_mul_comba(pool, A, B, C, paD, paDlen) \
        pstm_mul_comba(      A, B, C, paD, paDlen)
extern int32 pstm_mul_comba(psPool_t *pool, pstm_int *A, pstm_int *B,
				pstm_int *C, pstm_digit *paD, uint32 paDlen) FAST_FUNC;
#define pstm_sqr_comba(pool, A, B, paD, paDlen) \
        pstm_sqr_comba(      A, B, paD, paDlen)
extern int32 pstm_sqr_comba(psPool_t *pool, pstm_int *A, pstm_int *B,
				pstm_digit *paD, uint32 paDlen) FAST_FUNC;
#define pstm_invmod(pool, a, b, c) \
        pstm_invmod(      a, b, c)
extern int32 pstm_invmod(psPool_t *pool, pstm_int * a, pstm_int * b,
				pstm_int * c) FAST_FUNC;
#else  
	typedef int32 pstm_int;
#endif  
#endif  
