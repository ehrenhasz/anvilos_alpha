#ifndef G10_MPI_INTERNAL_H
#define G10_MPI_INTERNAL_H
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/mpi.h>
#include <linux/errno.h>
#define log_debug printk
#define log_bug printk
#define assert(x) \
	do { \
		if (!x) \
			log_bug("failed assertion\n"); \
	} while (0);
#ifndef KARATSUBA_THRESHOLD
#define KARATSUBA_THRESHOLD 16
#endif
#if KARATSUBA_THRESHOLD < 2
#undef KARATSUBA_THRESHOLD
#define KARATSUBA_THRESHOLD 2
#endif
typedef mpi_limb_t *mpi_ptr_t;	 
typedef int mpi_size_t;		 
#define RESIZE_IF_NEEDED(a, b)			\
	do {					\
		if ((a)->alloced < (b))		\
			mpi_resize((a), (b));	\
	} while (0)
#define MPN_COPY(d, s, n) \
	do {					\
		mpi_size_t _i;			\
		for (_i = 0; _i < (n); _i++)	\
			(d)[_i] = (s)[_i];	\
	} while (0)
#define MPN_COPY_INCR(d, s, n)		\
	do {					\
		mpi_size_t _i;			\
		for (_i = 0; _i < (n); _i++)	\
			(d)[_i] = (s)[_i];	\
	} while (0)
#define MPN_COPY_DECR(d, s, n) \
	do {					\
		mpi_size_t _i;			\
		for (_i = (n)-1; _i >= 0; _i--) \
			(d)[_i] = (s)[_i];	\
	} while (0)
#define MPN_ZERO(d, n) \
	do {					\
		int  _i;			\
		for (_i = 0; _i < (n); _i++)	\
			(d)[_i] = 0;		\
	} while (0)
#define MPN_NORMALIZE(d, n)  \
	do {					\
		while ((n) > 0) {		\
			if ((d)[(n)-1])		\
				break;		\
			(n)--;			\
		}				\
	} while (0)
#define MPN_MUL_N_RECURSE(prodp, up, vp, size, tspace) \
	do {							\
		if ((size) < KARATSUBA_THRESHOLD)		\
			mul_n_basecase(prodp, up, vp, size);	\
		else						\
			mul_n(prodp, up, vp, size, tspace);	\
	} while (0);
#define UDIV_QRNND_PREINV(q, r, nh, nl, d, di)				\
	do {								\
		mpi_limb_t _ql __maybe_unused;				\
		mpi_limb_t _q, _r;					\
		mpi_limb_t _xh, _xl;					\
		umul_ppmm(_q, _ql, (nh), (di));				\
		_q += (nh);	  \
		umul_ppmm(_xh, _xl, _q, (d));				\
		sub_ddmmss(_xh, _r, (nh), (nl), _xh, _xl);		\
		if (_xh) {						\
			sub_ddmmss(_xh, _r, _xh, _r, 0, (d));		\
			_q++;						\
			if (_xh) {					\
				sub_ddmmss(_xh, _r, _xh, _r, 0, (d));	\
				_q++;					\
			}						\
		}							\
		if (_r >= (d)) {					\
			_r -= (d);					\
			_q++;						\
		}							\
		(r) = _r;						\
		(q) = _q;						\
	} while (0)
mpi_ptr_t mpi_alloc_limb_space(unsigned nlimbs);
void mpi_free_limb_space(mpi_ptr_t a);
void mpi_assign_limb_space(MPI a, mpi_ptr_t ap, unsigned nlimbs);
static inline mpi_limb_t mpihelp_add_1(mpi_ptr_t res_ptr, mpi_ptr_t s1_ptr,
			 mpi_size_t s1_size, mpi_limb_t s2_limb);
mpi_limb_t mpihelp_add_n(mpi_ptr_t res_ptr, mpi_ptr_t s1_ptr,
			 mpi_ptr_t s2_ptr, mpi_size_t size);
static inline mpi_limb_t mpihelp_add(mpi_ptr_t res_ptr, mpi_ptr_t s1_ptr, mpi_size_t s1_size,
		       mpi_ptr_t s2_ptr, mpi_size_t s2_size);
static inline mpi_limb_t mpihelp_sub_1(mpi_ptr_t res_ptr, mpi_ptr_t s1_ptr,
			 mpi_size_t s1_size, mpi_limb_t s2_limb);
mpi_limb_t mpihelp_sub_n(mpi_ptr_t res_ptr, mpi_ptr_t s1_ptr,
			 mpi_ptr_t s2_ptr, mpi_size_t size);
static inline mpi_limb_t mpihelp_sub(mpi_ptr_t res_ptr, mpi_ptr_t s1_ptr, mpi_size_t s1_size,
		       mpi_ptr_t s2_ptr, mpi_size_t s2_size);
int mpihelp_cmp(mpi_ptr_t op1_ptr, mpi_ptr_t op2_ptr, mpi_size_t size);
struct karatsuba_ctx {
	struct karatsuba_ctx *next;
	mpi_ptr_t tspace;
	mpi_size_t tspace_size;
	mpi_ptr_t tp;
	mpi_size_t tp_size;
};
void mpihelp_release_karatsuba_ctx(struct karatsuba_ctx *ctx);
mpi_limb_t mpihelp_addmul_1(mpi_ptr_t res_ptr, mpi_ptr_t s1_ptr,
			    mpi_size_t s1_size, mpi_limb_t s2_limb);
mpi_limb_t mpihelp_submul_1(mpi_ptr_t res_ptr, mpi_ptr_t s1_ptr,
			    mpi_size_t s1_size, mpi_limb_t s2_limb);
int mpihelp_mul(mpi_ptr_t prodp, mpi_ptr_t up, mpi_size_t usize,
		mpi_ptr_t vp, mpi_size_t vsize, mpi_limb_t *_result);
void mpih_sqr_n_basecase(mpi_ptr_t prodp, mpi_ptr_t up, mpi_size_t size);
void mpih_sqr_n(mpi_ptr_t prodp, mpi_ptr_t up, mpi_size_t size,
		mpi_ptr_t tspace);
void mpihelp_mul_n(mpi_ptr_t prodp,
		mpi_ptr_t up, mpi_ptr_t vp, mpi_size_t size);
int mpihelp_mul_karatsuba_case(mpi_ptr_t prodp,
			       mpi_ptr_t up, mpi_size_t usize,
			       mpi_ptr_t vp, mpi_size_t vsize,
			       struct karatsuba_ctx *ctx);
mpi_limb_t mpihelp_mul_1(mpi_ptr_t res_ptr, mpi_ptr_t s1_ptr,
			 mpi_size_t s1_size, mpi_limb_t s2_limb);
mpi_limb_t mpihelp_mod_1(mpi_ptr_t dividend_ptr, mpi_size_t dividend_size,
			 mpi_limb_t divisor_limb);
mpi_limb_t mpihelp_divrem(mpi_ptr_t qp, mpi_size_t qextra_limbs,
			  mpi_ptr_t np, mpi_size_t nsize,
			  mpi_ptr_t dp, mpi_size_t dsize);
mpi_limb_t mpihelp_divmod_1(mpi_ptr_t quot_ptr,
			    mpi_ptr_t dividend_ptr, mpi_size_t dividend_size,
			    mpi_limb_t divisor_limb);
mpi_limb_t mpihelp_lshift(mpi_ptr_t wp, mpi_ptr_t up, mpi_size_t usize,
			  unsigned cnt);
mpi_limb_t mpihelp_rshift(mpi_ptr_t wp, mpi_ptr_t up, mpi_size_t usize,
			  unsigned cnt);
#define W_TYPE_SIZE BITS_PER_MPI_LIMB
typedef mpi_limb_t UWtype;
typedef unsigned int UHWtype;
#if defined(__GNUC__)
typedef unsigned int UQItype __attribute__ ((mode(QI)));
typedef int SItype __attribute__ ((mode(SI)));
typedef unsigned int USItype __attribute__ ((mode(SI)));
typedef int DItype __attribute__ ((mode(DI)));
typedef unsigned int UDItype __attribute__ ((mode(DI)));
#else
typedef unsigned char UQItype;
typedef long SItype;
typedef unsigned long USItype;
#endif
#ifdef __GNUC__
#include "mpi-inline.h"
#endif
#endif  
