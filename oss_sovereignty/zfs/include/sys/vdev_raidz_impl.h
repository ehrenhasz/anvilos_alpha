#ifndef _VDEV_RAIDZ_H
#define	_VDEV_RAIDZ_H
#include <sys/types.h>
#include <sys/debug.h>
#include <sys/kstat.h>
#include <sys/abd.h>
#include <sys/vdev_impl.h>
#ifdef  __cplusplus
extern "C" {
#endif
#define	CODE_P		(0U)
#define	CODE_Q		(1U)
#define	CODE_R		(2U)
#define	PARITY_P	(1U)
#define	PARITY_PQ	(2U)
#define	PARITY_PQR	(3U)
#define	TARGET_X	(0U)
#define	TARGET_Y	(1U)
#define	TARGET_Z	(2U)
enum raidz_math_gen_op {
	RAIDZ_GEN_P = 0,
	RAIDZ_GEN_PQ,
	RAIDZ_GEN_PQR,
	RAIDZ_GEN_NUM = 3
};
enum raidz_rec_op {
	RAIDZ_REC_P = 0,
	RAIDZ_REC_Q,
	RAIDZ_REC_R,
	RAIDZ_REC_PQ,
	RAIDZ_REC_PR,
	RAIDZ_REC_QR,
	RAIDZ_REC_PQR,
	RAIDZ_REC_NUM = 7
};
extern const char *const raidz_gen_name[RAIDZ_GEN_NUM];
extern const char *const raidz_rec_name[RAIDZ_REC_NUM];
typedef void		(*raidz_gen_f)(void *);
typedef int		(*raidz_rec_f)(void *, const int *);
typedef boolean_t	(*will_work_f)(void);
typedef void		(*init_impl_f)(void);
typedef void		(*fini_impl_f)(void);
#define	RAIDZ_IMPL_NAME_MAX	(20)
typedef struct raidz_impl_ops {
	init_impl_f init;
	fini_impl_f fini;
	raidz_gen_f gen[RAIDZ_GEN_NUM];	 
	raidz_rec_f rec[RAIDZ_REC_NUM];	 
	will_work_f is_supported;	 
	char name[RAIDZ_IMPL_NAME_MAX];	 
} raidz_impl_ops_t;
typedef struct raidz_col {
	uint64_t rc_devidx;		 
	uint64_t rc_offset;		 
	uint64_t rc_size;		 
	abd_t rc_abdstruct;		 
	abd_t *rc_abd;			 
	abd_t *rc_orig_data;		 
	int rc_error;			 
	uint8_t rc_tried;		 
	uint8_t rc_skipped;		 
	uint8_t rc_need_orig_restore;	 
	uint8_t rc_force_repair;	 
	uint8_t rc_allow_repair;	 
} raidz_col_t;
typedef struct raidz_row {
	uint64_t rr_cols;		 
	uint64_t rr_scols;		 
	uint64_t rr_bigcols;		 
	uint64_t rr_missingdata;	 
	uint64_t rr_missingparity;	 
	uint64_t rr_firstdatacol;	 
	abd_t *rr_abd_empty;		 
	int rr_nempty;			 
#ifdef ZFS_DEBUG
	uint64_t rr_offset;		 
	uint64_t rr_size;		 
#endif
	raidz_col_t rr_col[];		 
} raidz_row_t;
typedef struct raidz_map {
	boolean_t rm_ecksuminjected;	 
	int rm_nrows;			 
	int rm_nskip;			 
	int rm_skipstart;		 
	const raidz_impl_ops_t *rm_ops;	 
	raidz_row_t *rm_row[];		 
} raidz_map_t;
#define	RAIDZ_ORIGINAL_IMPL	(INT_MAX)
extern const raidz_impl_ops_t vdev_raidz_scalar_impl;
extern boolean_t raidz_will_scalar_work(void);
#if defined(__x86_64) && defined(HAVE_SSE2)	 
extern const raidz_impl_ops_t vdev_raidz_sse2_impl;
#endif
#if defined(__x86_64) && defined(HAVE_SSSE3)	 
extern const raidz_impl_ops_t vdev_raidz_ssse3_impl;
#endif
#if defined(__x86_64) && defined(HAVE_AVX2)	 
extern const raidz_impl_ops_t vdev_raidz_avx2_impl;
#endif
#if defined(__x86_64) && defined(HAVE_AVX512F)	 
extern const raidz_impl_ops_t vdev_raidz_avx512f_impl;
#endif
#if defined(__x86_64) && defined(HAVE_AVX512BW)	 
extern const raidz_impl_ops_t vdev_raidz_avx512bw_impl;
#endif
#if defined(__aarch64__)
extern const raidz_impl_ops_t vdev_raidz_aarch64_neon_impl;
extern const raidz_impl_ops_t vdev_raidz_aarch64_neonx2_impl;
#endif
#if defined(__powerpc__)
extern const raidz_impl_ops_t vdev_raidz_powerpc_altivec_impl;
#endif
#define	raidz_parity(rm)	((rm)->rm_row[0]->rr_firstdatacol)
#define	raidz_ncols(rm)		((rm)->rm_row[0]->rr_cols)
#define	raidz_nbigcols(rm)	((rm)->rm_bigcols)
#define	raidz_col_p(rm, c)	((rm)->rm_col + (c))
#define	raidz_col_size(rm, c)	((rm)->rm_col[c].rc_size)
#define	raidz_big_size(rm)	(raidz_col_size(rm, CODE_P))
#define	raidz_short_size(rm)	(raidz_col_size(rm, raidz_ncols(rm)-1))
#define	_RAIDZ_GEN_WRAP(code, impl)					\
static void								\
impl ## _gen_ ## code(void *rrp)					\
{									\
	raidz_row_t *rr = (raidz_row_t *)rrp;				\
	raidz_generate_## code ## _impl(rr);				\
}
#define	_RAIDZ_REC_WRAP(code, impl)					\
static int								\
impl ## _rec_ ## code(void *rrp, const int *tgtidx)			\
{									\
	raidz_row_t *rr = (raidz_row_t *)rrp;				\
	return (raidz_reconstruct_## code ## _impl(rr, tgtidx));	\
}
#define	DEFINE_GEN_METHODS(impl)					\
	_RAIDZ_GEN_WRAP(p, impl);					\
	_RAIDZ_GEN_WRAP(pq, impl);					\
	_RAIDZ_GEN_WRAP(pqr, impl)
#define	DEFINE_REC_METHODS(impl)					\
	_RAIDZ_REC_WRAP(p, impl);					\
	_RAIDZ_REC_WRAP(q, impl);					\
	_RAIDZ_REC_WRAP(r, impl);					\
	_RAIDZ_REC_WRAP(pq, impl);					\
	_RAIDZ_REC_WRAP(pr, impl);					\
	_RAIDZ_REC_WRAP(qr, impl);					\
	_RAIDZ_REC_WRAP(pqr, impl)
#define	RAIDZ_GEN_METHODS(impl)						\
{									\
	[RAIDZ_GEN_P] = & impl ## _gen_p,				\
	[RAIDZ_GEN_PQ] = & impl ## _gen_pq,				\
	[RAIDZ_GEN_PQR] = & impl ## _gen_pqr				\
}
#define	RAIDZ_REC_METHODS(impl)						\
{									\
	[RAIDZ_REC_P] = & impl ## _rec_p,				\
	[RAIDZ_REC_Q] = & impl ## _rec_q,				\
	[RAIDZ_REC_R] = & impl ## _rec_r,				\
	[RAIDZ_REC_PQ] = & impl ## _rec_pq,				\
	[RAIDZ_REC_PR] = & impl ## _rec_pr,				\
	[RAIDZ_REC_QR] = & impl ## _rec_qr,				\
	[RAIDZ_REC_PQR] = & impl ## _rec_pqr				\
}
typedef struct raidz_impl_kstat {
	uint64_t gen[RAIDZ_GEN_NUM];	 
	uint64_t rec[RAIDZ_REC_NUM];	 
} raidz_impl_kstat_t;
typedef enum raidz_mul_info {
	MUL_Q_X		= 0,
	MUL_R_X		= 0,
	MUL_PQ_X	= 0,
	MUL_PQ_Y	= 1,
	MUL_PR_X	= 0,
	MUL_PR_Y	= 1,
	MUL_QR_XQ	= 0,
	MUL_QR_X	= 1,
	MUL_QR_YQ	= 2,
	MUL_QR_Y	= 3,
	MUL_PQR_XP	= 0,
	MUL_PQR_XQ	= 1,
	MUL_PQR_XR	= 2,
	MUL_PQR_YU	= 3,
	MUL_PQR_YP	= 4,
	MUL_PQR_YQ	= 5,
	MUL_CNT		= 6
} raidz_mul_info_t;
extern const uint8_t vdev_raidz_pow2[256] __attribute__((aligned(256)));
extern const uint8_t vdev_raidz_log2[256] __attribute__((aligned(256)));
static inline uint8_t
vdev_raidz_exp2(const uint8_t a, const unsigned exp)
{
	if (a == 0)
		return (0);
	return (vdev_raidz_pow2[(exp + (unsigned)vdev_raidz_log2[a]) % 255]);
}
typedef unsigned gf_t;
typedef unsigned gf_log_t;
static inline gf_t
gf_mul(const gf_t a, const gf_t b)
{
	gf_log_t logsum;
	if (a == 0 || b == 0)
		return (0);
	logsum = (gf_log_t)vdev_raidz_log2[a] + (gf_log_t)vdev_raidz_log2[b];
	return ((gf_t)vdev_raidz_pow2[logsum % 255]);
}
static inline gf_t
gf_div(const gf_t  a, const gf_t b)
{
	gf_log_t logsum;
	ASSERT3U(b, >, 0);
	if (a == 0)
		return (0);
	logsum = (gf_log_t)255 + (gf_log_t)vdev_raidz_log2[a] -
	    (gf_log_t)vdev_raidz_log2[b];
	return ((gf_t)vdev_raidz_pow2[logsum % 255]);
}
static inline gf_t
gf_inv(const gf_t a)
{
	gf_log_t logsum;
	ASSERT3U(a, >, 0);
	logsum = (gf_log_t)255 - (gf_log_t)vdev_raidz_log2[a];
	return ((gf_t)vdev_raidz_pow2[logsum]);
}
static inline gf_t
gf_exp2(gf_log_t exp)
{
	return (vdev_raidz_pow2[exp % 255]);
}
static inline gf_t
gf_exp4(gf_log_t exp)
{
	ASSERT3U(exp, <=, 255);
	return ((gf_t)vdev_raidz_pow2[(2 * exp) % 255]);
}
#ifdef  __cplusplus
}
#endif
#endif  
