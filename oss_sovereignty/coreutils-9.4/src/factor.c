 

 

 
#ifndef PROVE_PRIMALITY
# define PROVE_PRIMALITY 1
#endif

 
#ifndef USE_SQUFOF
# define USE_SQUFOF 0
#endif

 
#ifndef STAT_SQUFOF
# define STAT_SQUFOF 0
#endif


#include <config.h>
#include <getopt.h>
#include <stdio.h>
#include <gmp.h>

#include "system.h"
#include "assure.h"
#include "full-write.h"
#include "quote.h"
#include "readtokens.h"
#include "xstrtol.h"

 
#define PROGRAM_NAME "factor"

#define AUTHORS \
  proper_name ("Paul Rubin"),                                           \
  proper_name_lite ("Torbjorn Granlund", "Torbj\303\266rn Granlund"),   \
  proper_name_lite ("Niels Moller", "Niels M\303\266ller")

 
#define DELIM "\n\t "

#ifndef USE_LONGLONG_H
 
# if LONG_MAX == INTMAX_MAX
#  define USE_LONGLONG_H 1
# endif
#endif

#if USE_LONGLONG_H

 

 
# if UINTMAX_MAX == UINT32_MAX
#  define W_TYPE_SIZE 32
# elif UINTMAX_MAX == UINT64_MAX
#  define W_TYPE_SIZE 64
# elif UINTMAX_MAX == UINT128_MAX
#  define W_TYPE_SIZE 128
# endif

# define UWtype  uintmax_t
# define UHWtype unsigned long int
# undef UDWtype
# if HAVE_ATTRIBUTE_MODE
typedef unsigned int UQItype    __attribute__ ((mode (QI)));
typedef          int SItype     __attribute__ ((mode (SI)));
typedef unsigned int USItype    __attribute__ ((mode (SI)));
typedef          int DItype     __attribute__ ((mode (DI)));
typedef unsigned int UDItype    __attribute__ ((mode (DI)));
# else
typedef unsigned char UQItype;
typedef          long SItype;
typedef unsigned long int USItype;
#  if HAVE_LONG_LONG_INT
typedef long long int DItype;
typedef unsigned long long int UDItype;
#  else  
typedef long int DItype;
typedef unsigned long int UDItype;
#  endif
# endif
# define LONGLONG_STANDALONE      
# define ASSERT(x)                
# define __GMP_DECLSPEC           
# define __clz_tab factor_clz_tab  
# ifndef __GMP_GNUC_PREREQ
#  define __GMP_GNUC_PREREQ(a,b) 1
# endif

 
# if __GMP_GNUC_PREREQ (1,1) && defined __clz_tab
ASSERT (1)
__GMP_DECLSPEC
# endif

# if _ARCH_PPC
#  define HAVE_HOST_CPU_FAMILY_powerpc 1
# endif
# include "longlong.h"
# ifdef COUNT_LEADING_ZEROS_NEED_CLZ_TAB
const unsigned char factor_clz_tab[129] =
{
  1,2,3,3,4,4,4,4,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
  8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
  9
};
# endif

#else  

# define W_TYPE_SIZE (8 * sizeof (uintmax_t))
# define __ll_B ((uintmax_t) 1 << (W_TYPE_SIZE / 2))
# define __ll_lowpart(t)  ((uintmax_t) (t) & (__ll_B - 1))
# define __ll_highpart(t) ((uintmax_t) (t) >> (W_TYPE_SIZE / 2))

#endif

#if !defined __clz_tab && !defined UHWtype
 
#endif

 
#define MAX_NFACTS 26

enum
{
  DEV_DEBUG_OPTION = CHAR_MAX + 1
};

static struct option const long_options[] =
{
  {"exponents", no_argument, nullptr, 'h'},
  {"-debug", no_argument, nullptr, DEV_DEBUG_OPTION},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {nullptr, 0, nullptr, 0}
};

 
static bool print_exponents;

struct factors
{
  uintmax_t     plarge[2];  
  uintmax_t     p[MAX_NFACTS];
  unsigned char e[MAX_NFACTS];
  unsigned char nfactors;
};

struct mp_factors
{
  mpz_t             *p;
  unsigned long int *e;
  idx_t nfactors;
};

static void factor (uintmax_t, uintmax_t, struct factors *);

#ifndef umul_ppmm
# define umul_ppmm(w1, w0, u, v)                                        \
  do {                                                                  \
    uintmax_t __x0, __x1, __x2, __x3;                                   \
    unsigned long int __ul, __vl, __uh, __vh;                           \
    uintmax_t __u = (u), __v = (v);                                     \
                                                                        \
    __ul = __ll_lowpart (__u);                                          \
    __uh = __ll_highpart (__u);                                         \
    __vl = __ll_lowpart (__v);                                          \
    __vh = __ll_highpart (__v);                                         \
                                                                        \
    __x0 = (uintmax_t) __ul * __vl;                                     \
    __x1 = (uintmax_t) __ul * __vh;                                     \
    __x2 = (uintmax_t) __uh * __vl;                                     \
    __x3 = (uintmax_t) __uh * __vh;                                     \
                                                                        \
    __x1 += __ll_highpart (__x0); 		\
    __x1 += __x2;		 		\
    if (__x1 < __x2)		 			\
      __x3 += __ll_B;		 	\
                                                                        \
    (w1) = __x3 + __ll_highpart (__x1);                                 \
    (w0) = (__x1 << W_TYPE_SIZE / 2) + __ll_lowpart (__x0);             \
  } while (0)
#endif

#if !defined udiv_qrnnd || defined UDIV_NEEDS_NORMALIZATION
 
# undef udiv_qrnnd
# define udiv_qrnnd(q, r, n1, n0, d)                                    \
  do {                                                                  \
    uintmax_t __d1, __d0, __q, __r1, __r0;                              \
                                                                        \
    __d1 = (d); __d0 = 0;                                               \
    __r1 = (n1); __r0 = (n0);                                           \
    affirm (__r1 < __d1);                                               \
    __q = 0;                                                            \
    for (int __i = W_TYPE_SIZE; __i > 0; __i--)                         \
      {                                                                 \
        rsh2 (__d1, __d0, __d1, __d0, 1);                               \
        __q <<= 1;                                                      \
        if (ge2 (__r1, __r0, __d1, __d0))                               \
          {                                                             \
            __q++;                                                      \
            sub_ddmmss (__r1, __r0, __r1, __r0, __d1, __d0);            \
          }                                                             \
      }                                                                 \
    (r) = __r0;                                                         \
    (q) = __q;                                                          \
  } while (0)
#endif

#if !defined add_ssaaaa
# define add_ssaaaa(sh, sl, ah, al, bh, bl)                             \
  do {                                                                  \
    uintmax_t _add_x;                                                   \
    _add_x = (al) + (bl);                                               \
    (sh) = (ah) + (bh) + (_add_x < (al));                               \
    (sl) = _add_x;                                                      \
  } while (0)
#endif

#define rsh2(rh, rl, ah, al, cnt)                                       \
  do {                                                                  \
    (rl) = ((ah) << (W_TYPE_SIZE - (cnt))) | ((al) >> (cnt));           \
    (rh) = (ah) >> (cnt);                                               \
  } while (0)

#define lsh2(rh, rl, ah, al, cnt)                                       \
  do {                                                                  \
    (rh) = ((ah) << cnt) | ((al) >> (W_TYPE_SIZE - (cnt)));             \
    (rl) = (al) << (cnt);                                               \
  } while (0)

#define ge2(ah, al, bh, bl)                                             \
  ((ah) > (bh) || ((ah) == (bh) && (al) >= (bl)))

#define gt2(ah, al, bh, bl)                                             \
  ((ah) > (bh) || ((ah) == (bh) && (al) > (bl)))

#ifndef sub_ddmmss
# define sub_ddmmss(rh, rl, ah, al, bh, bl)                             \
  do {                                                                  \
    uintmax_t _cy;                                                      \
    _cy = (al) < (bl);                                                  \
    (rl) = (al) - (bl);                                                 \
    (rh) = (ah) - (bh) - _cy;                                           \
  } while (0)
#endif

#ifndef count_leading_zeros
# define count_leading_zeros(count, x) do {                             \
    uintmax_t __clz_x = (x);                                            \
    int __clz_c;                                                        \
    for (__clz_c = 0;                                                   \
         (__clz_x & ((uintmax_t) 0xff << (W_TYPE_SIZE - 8))) == 0;      \
         __clz_c += 8)                                                  \
      __clz_x <<= 8;                                                    \
    for (; (intmax_t)__clz_x >= 0; __clz_c++)                           \
      __clz_x <<= 1;                                                    \
    (count) = __clz_c;                                                  \
  } while (0)
#endif

#ifndef count_trailing_zeros
# define count_trailing_zeros(count, x) do {                            \
    uintmax_t __ctz_x = (x);                                            \
    int __ctz_c = 0;                                                    \
    while ((__ctz_x & 1) == 0)                                          \
      {                                                                 \
        __ctz_x >>= 1;                                                  \
        __ctz_c++;                                                      \
      }                                                                 \
    (count) = __ctz_c;                                                  \
  } while (0)
#endif

 
#define submod(r,a,b,n)                                                 \
  do {                                                                  \
    uintmax_t _t = - (uintmax_t) (a < b);                               \
    (r) = ((n) & _t) + (a) - (b);                                       \
  } while (0)

#define addmod(r,a,b,n)                                                 \
  submod ((r), (a), ((n) - (b)), (n))

 
#define addmod2(r1, r0, a1, a0, b1, b0, n1, n0)                         \
  do {                                                                  \
    add_ssaaaa ((r1), (r0), (a1), (a0), (b1), (b0));                    \
    if (ge2 ((r1), (r0), (n1), (n0)))                                   \
      sub_ddmmss ((r1), (r0), (r1), (r0), (n1), (n0));                  \
  } while (0)
#define submod2(r1, r0, a1, a0, b1, b0, n1, n0)                         \
  do {                                                                  \
    sub_ddmmss ((r1), (r0), (a1), (a0), (b1), (b0));                    \
    if ((intmax_t) (r1) < 0)                                            \
      add_ssaaaa ((r1), (r0), (r1), (r0), (n1), (n0));                  \
  } while (0)

#define HIGHBIT_TO_MASK(x)                                              \
  (((intmax_t)-1 >> 1) < 0                                              \
   ? (uintmax_t)((intmax_t)(x) >> (W_TYPE_SIZE - 1))                    \
   : ((x) & ((uintmax_t) 1 << (W_TYPE_SIZE - 1))                        \
      ? UINTMAX_MAX : (uintmax_t) 0))

 
static uintmax_t
mod2 (uintmax_t *r1, uintmax_t a1, uintmax_t a0, uintmax_t d1, uintmax_t d0)
{
  int cntd, cnta;

  affirm (d1 != 0);

  if (a1 == 0)
    {
      *r1 = 0;
      return a0;
    }

  count_leading_zeros (cntd, d1);
  count_leading_zeros (cnta, a1);
  int cnt = cntd - cnta;
  lsh2 (d1, d0, d1, d0, cnt);
  for (int i = 0; i < cnt; i++)
    {
      if (ge2 (a1, a0, d1, d0))
        sub_ddmmss (a1, a0, a1, a0, d1, d0);
      rsh2 (d1, d0, d1, d0, 1);
    }

  *r1 = a1;
  return a0;
}

ATTRIBUTE_CONST
static uintmax_t
gcd_odd (uintmax_t a, uintmax_t b)
{
  if ((b & 1) == 0)
    {
      uintmax_t t = b;
      b = a;
      a = t;
    }
  if (a == 0)
    return b;

   
  b >>= 1;

  for (;;)
    {
      uintmax_t t;
      uintmax_t bgta;

      while ((a & 1) == 0)
        a >>= 1;
      a >>= 1;

      t = a - b;
      if (t == 0)
        return (a << 1) + 1;

      bgta = HIGHBIT_TO_MASK (t);

       
      b += (bgta & t);

       
      a = (t ^ bgta) - bgta;
    }
}

static uintmax_t
gcd2_odd (uintmax_t *r1, uintmax_t a1, uintmax_t a0, uintmax_t b1, uintmax_t b0)
{
  affirm (b0 & 1);

  if ((a0 | a1) == 0)
    {
      *r1 = b1;
      return b0;
    }

  while ((a0 & 1) == 0)
    rsh2 (a1, a0, a1, a0, 1);

  for (;;)
    {
      if ((b1 | a1) == 0)
        {
          *r1 = 0;
          return gcd_odd (b0, a0);
        }

      if (gt2 (a1, a0, b1, b0))
        {
          sub_ddmmss (a1, a0, a1, a0, b1, b0);
          do
            rsh2 (a1, a0, a1, a0, 1);
          while ((a0 & 1) == 0);
        }
      else if (gt2 (b1, b0, a1, a0))
        {
          sub_ddmmss (b1, b0, b1, b0, a1, a0);
          do
            rsh2 (b1, b0, b1, b0, 1);
          while ((b0 & 1) == 0);
        }
      else
        break;
    }

  *r1 = a1;
  return a0;
}

static void
factor_insert_multiplicity (struct factors *factors,
                            uintmax_t prime, int m)
{
  int nfactors = factors->nfactors;
  uintmax_t *p = factors->p;
  unsigned char *e = factors->e;

   
  int i;
  for (i = nfactors - 1; i >= 0; i--)
    {
      if (p[i] <= prime)
        break;
    }

  if (i < 0 || p[i] != prime)
    {
      for (int j = nfactors - 1; j > i; j--)
        {
          p[j + 1] = p[j];
          e[j + 1] = e[j];
        }
      p[i + 1] = prime;
      e[i + 1] = m;
      factors->nfactors = nfactors + 1;
    }
  else
    {
      e[i] += m;
    }
}

#define factor_insert(f, p) factor_insert_multiplicity (f, p, 1)

static void
factor_insert_large (struct factors *factors,
                     uintmax_t p1, uintmax_t p0)
{
  if (p1 > 0)
    {
      affirm (factors->plarge[1] == 0);
      factors->plarge[0] = p0;
      factors->plarge[1] = p1;
    }
  else
    factor_insert (factors, p0);
}

#ifndef mpz_inits

# include <stdarg.h>

# define mpz_inits(...) mpz_va_init (mpz_init, __VA_ARGS__)
# define mpz_clears(...) mpz_va_init (mpz_clear, __VA_ARGS__)

static void
mpz_va_init (void (*mpz_single_init)(mpz_t), ...)
{
  va_list ap;

  va_start (ap, mpz_single_init);

  mpz_t *mpz;
  while ((mpz = va_arg (ap, mpz_t *)))
    mpz_single_init (*mpz);

  va_end (ap);
}
#endif

static void mp_factor (mpz_t, struct mp_factors *);

static void
mp_factor_init (struct mp_factors *factors)
{
  factors->p = nullptr;
  factors->e = nullptr;
  factors->nfactors = 0;
}

static void
mp_factor_clear (struct mp_factors *factors)
{
  for (idx_t i = 0; i < factors->nfactors; i++)
    mpz_clear (factors->p[i]);

  free (factors->p);
  free (factors->e);
}

static void
mp_factor_insert (struct mp_factors *factors, mpz_t prime)
{
  idx_t nfactors = factors->nfactors;
  mpz_t *p = factors->p;
  unsigned long int *e = factors->e;
  ptrdiff_t i;

   
  for (i = nfactors - 1; i >= 0; i--)
    {
      if (mpz_cmp (p[i], prime) <= 0)
        break;
    }

  if (i < 0 || mpz_cmp (p[i], prime) != 0)
    {
      p = xireallocarray (p, nfactors + 1, sizeof p[0]);
      e = xireallocarray (e, nfactors + 1, sizeof e[0]);

      mpz_init (p[nfactors]);
      for (long j = nfactors - 1; j > i; j--)
        {
          mpz_set (p[j + 1], p[j]);
          e[j + 1] = e[j];
        }
      mpz_set (p[i + 1], prime);
      e[i + 1] = 1;

      factors->p = p;
      factors->e = e;
      factors->nfactors = nfactors + 1;
    }
  else
    {
      e[i] += 1;
    }
}

static void
mp_factor_insert_ui (struct mp_factors *factors, unsigned long int prime)
{
  mpz_t pz;

  mpz_init_set_ui (pz, prime);
  mp_factor_insert (factors, pz);
  mpz_clear (pz);
}


 
enum { W = sizeof (uintmax_t) * CHAR_BIT };

 
static_assert (UINTMAX_MAX >> (W - 1) == 1);

#define P(a,b,c,d) a,
static const unsigned char primes_diff[] = {
#include "primes.h"
0,0,0,0,0,0,0                            
};
#undef P

#define PRIMES_PTAB_ENTRIES \
  (sizeof (primes_diff) / sizeof (primes_diff[0]) - 8 + 1)

#define P(a,b,c,d) b,
static const unsigned char primes_diff8[] = {
#include "primes.h"
0,0,0,0,0,0,0                            
};
#undef P

struct primes_dtab
{
  uintmax_t binv, lim;
};

#define P(a,b,c,d) {c,d},
static const struct primes_dtab primes_dtab[] = {
#include "primes.h"
{1,0},{1,0},{1,0},{1,0},{1,0},{1,0},{1,0}  
};
#undef P

 
static_assert (W <= WIDE_UINT_BITS);

 
static bool dev_debug = false;

 
static bool flag_prove_primality = PROVE_PRIMALITY;

 
#define MR_REPS 25

static void
factor_insert_refind (struct factors *factors, uintmax_t p, int i, int off)
{
  for (int j = 0; j < off; j++)
    p += primes_diff[i + j];
  factor_insert (factors, p);
}

 

static uintmax_t
factor_using_division (uintmax_t *t1p, uintmax_t t1, uintmax_t t0,
                       struct factors *factors)
{
  if (t0 % 2 == 0)
    {
      int cnt;

      if (t0 == 0)
        {
          count_trailing_zeros (cnt, t1);
          t0 = t1 >> cnt;
          t1 = 0;
          cnt += W_TYPE_SIZE;
        }
      else
        {
          count_trailing_zeros (cnt, t0);
          rsh2 (t1, t0, t1, t0, cnt);
        }

      factor_insert_multiplicity (factors, 2, cnt);
    }

  uintmax_t p = 3;
  idx_t i;
  for (i = 0; t1 > 0 && i < PRIMES_PTAB_ENTRIES; i++)
    {
      for (;;)
        {
          uintmax_t q1, q0, hi;
          MAYBE_UNUSED uintmax_t lo;

          q0 = t0 * primes_dtab[i].binv;
          umul_ppmm (hi, lo, q0, p);
          if (hi > t1)
            break;
          hi = t1 - hi;
          q1 = hi * primes_dtab[i].binv;
          if (LIKELY (q1 > primes_dtab[i].lim))
            break;
          t1 = q1; t0 = q0;
          factor_insert (factors, p);
        }
      p += primes_diff[i + 1];
    }
  if (t1p)
    *t1p = t1;

#define DIVBLOCK(I)                                                     \
  do {                                                                  \
    for (;;)                                                            \
      {                                                                 \
        q = t0 * pd[I].binv;                                            \
        if (LIKELY (q > pd[I].lim))                                     \
          break;                                                        \
        t0 = q;                                                         \
        factor_insert_refind (factors, p, i + 1, I);                    \
      }                                                                 \
  } while (0)

  for (; i < PRIMES_PTAB_ENTRIES; i += 8)
    {
      uintmax_t q;
      const struct primes_dtab *pd = &primes_dtab[i];
      DIVBLOCK (0);
      DIVBLOCK (1);
      DIVBLOCK (2);
      DIVBLOCK (3);
      DIVBLOCK (4);
      DIVBLOCK (5);
      DIVBLOCK (6);
      DIVBLOCK (7);

      p += primes_diff8[i];
      if (p * p > t0)
        break;
    }

  return t0;
}

static void
mp_factor_using_division (mpz_t t, struct mp_factors *factors)
{
  mpz_t q;
  mp_bitcnt_t p;

  devmsg ("[trial division] ");

  mpz_init (q);

  p = mpz_scan1 (t, 0);
  mpz_fdiv_q_2exp (t, t, p);
  while (p)
    {
      mp_factor_insert_ui (factors, 2);
      --p;
    }

  unsigned long int d = 3;
  for (idx_t i = 1; i <= PRIMES_PTAB_ENTRIES;)
    {
      if (! mpz_divisible_ui_p (t, d))
        {
          d += primes_diff[i++];
          if (mpz_cmp_ui (t, d * d) < 0)
            break;
        }
      else
        {
          mpz_tdiv_q_ui (t, t, d);
          mp_factor_insert_ui (factors, d);
        }
    }

  mpz_clear (q);
}

 
static const unsigned char  binvert_table[128] =
{
  0x01, 0xAB, 0xCD, 0xB7, 0x39, 0xA3, 0xC5, 0xEF,
  0xF1, 0x1B, 0x3D, 0xA7, 0x29, 0x13, 0x35, 0xDF,
  0xE1, 0x8B, 0xAD, 0x97, 0x19, 0x83, 0xA5, 0xCF,
  0xD1, 0xFB, 0x1D, 0x87, 0x09, 0xF3, 0x15, 0xBF,
  0xC1, 0x6B, 0x8D, 0x77, 0xF9, 0x63, 0x85, 0xAF,
  0xB1, 0xDB, 0xFD, 0x67, 0xE9, 0xD3, 0xF5, 0x9F,
  0xA1, 0x4B, 0x6D, 0x57, 0xD9, 0x43, 0x65, 0x8F,
  0x91, 0xBB, 0xDD, 0x47, 0xC9, 0xB3, 0xD5, 0x7F,
  0x81, 0x2B, 0x4D, 0x37, 0xB9, 0x23, 0x45, 0x6F,
  0x71, 0x9B, 0xBD, 0x27, 0xA9, 0x93, 0xB5, 0x5F,
  0x61, 0x0B, 0x2D, 0x17, 0x99, 0x03, 0x25, 0x4F,
  0x51, 0x7B, 0x9D, 0x07, 0x89, 0x73, 0x95, 0x3F,
  0x41, 0xEB, 0x0D, 0xF7, 0x79, 0xE3, 0x05, 0x2F,
  0x31, 0x5B, 0x7D, 0xE7, 0x69, 0x53, 0x75, 0x1F,
  0x21, 0xCB, 0xED, 0xD7, 0x59, 0xC3, 0xE5, 0x0F,
  0x11, 0x3B, 0x5D, 0xC7, 0x49, 0x33, 0x55, 0xFF
};

 
#define binv(inv,n)                                                     \
  do {                                                                  \
    uintmax_t  __n = (n);                                               \
    uintmax_t  __inv;                                                   \
                                                                        \
    __inv = binvert_table[(__n / 2) & 0x7F];                     \
    if (W_TYPE_SIZE > 8)   __inv = 2 * __inv - __inv * __inv * __n;     \
    if (W_TYPE_SIZE > 16)  __inv = 2 * __inv - __inv * __inv * __n;     \
    if (W_TYPE_SIZE > 32)  __inv = 2 * __inv - __inv * __inv * __n;     \
                                                                        \
    if (W_TYPE_SIZE > 64)                                               \
      {                                                                 \
        int  __invbits = 64;                                            \
        do {                                                            \
          __inv = 2 * __inv - __inv * __inv * __n;                      \
          __invbits *= 2;                                               \
        } while (__invbits < W_TYPE_SIZE);                              \
      }                                                                 \
                                                                        \
    (inv) = __inv;                                                      \
  } while (0)

 
#define divexact_21(q1, q0, u1, u0, d)                                  \
  do {                                                                  \
    uintmax_t _di, _q0;                                                 \
    binv (_di, (d));                                                    \
    _q0 = (u0) * _di;                                                   \
    if ((u1) >= (d))                                                    \
      {                                                                 \
        uintmax_t _p1;                                                  \
        MAYBE_UNUSED intmax_t _p0;                                      \
        umul_ppmm (_p1, _p0, _q0, d);                                   \
        (q1) = ((u1) - _p1) * _di;                                      \
        (q0) = _q0;                                                     \
      }                                                                 \
    else                                                                \
      {                                                                 \
        (q0) = _q0;                                                     \
        (q1) = 0;                                                       \
      }                                                                 \
  } while (0)

 
#define redcify(r_prim, r, n)                                           \
  do {                                                                  \
    MAYBE_UNUSED uintmax_t _redcify_q;					\
    udiv_qrnnd (_redcify_q, r_prim, r, 0, n);                           \
  } while (0)

 
#define redcify2(r1, r0, x, n1, n0)                                     \
  do {                                                                  \
    uintmax_t _r1, _r0, _i;                                             \
    if ((x) < (n1))                                                     \
      {                                                                 \
        _r1 = (x); _r0 = 0;                                             \
        _i = W_TYPE_SIZE;                                               \
      }                                                                 \
    else                                                                \
      {                                                                 \
        _r1 = 0; _r0 = (x);                                             \
        _i = 2 * W_TYPE_SIZE;                                           \
      }                                                                 \
    while (_i-- > 0)                                                    \
      {                                                                 \
        lsh2 (_r1, _r0, _r1, _r0, 1);                                   \
        if (ge2 (_r1, _r0, (n1), (n0)))                                 \
          sub_ddmmss (_r1, _r0, _r1, _r0, (n1), (n0));                  \
      }                                                                 \
    (r1) = _r1;                                                         \
    (r0) = _r0;                                                         \
  } while (0)

 
static inline uintmax_t
mulredc (uintmax_t a, uintmax_t b, uintmax_t m, uintmax_t mi)
{
  uintmax_t rh, rl, q, th, xh;
  MAYBE_UNUSED uintmax_t tl;

  umul_ppmm (rh, rl, a, b);
  q = rl * mi;
  umul_ppmm (th, tl, q, m);
  xh = rh - th;
  if (rh < th)
    xh += m;

  return xh;
}

 
static uintmax_t
mulredc2 (uintmax_t *r1p,
          uintmax_t a1, uintmax_t a0, uintmax_t b1, uintmax_t b0,
          uintmax_t m1, uintmax_t m0, uintmax_t mi)
{
  uintmax_t r1, r0, q, p1, t1, t0, s1, s0;
  MAYBE_UNUSED uintmax_t p0;
  mi = -mi;
  affirm ((a1 >> (W_TYPE_SIZE - 1)) == 0);
  affirm ((b1 >> (W_TYPE_SIZE - 1)) == 0);
  affirm ((m1 >> (W_TYPE_SIZE - 1)) == 0);

   
  umul_ppmm (t1, t0, a0, b0);
  umul_ppmm (r1, r0, a0, b1);
  q = mi * t0;
  umul_ppmm (p1, p0, q, m0);
  umul_ppmm (s1, s0, q, m1);
  r0 += (t0 != 0);  
  add_ssaaaa (r1, r0, r1, r0, 0, p1);
  add_ssaaaa (r1, r0, r1, r0, 0, t1);
  add_ssaaaa (r1, r0, r1, r0, s1, s0);

   
  umul_ppmm (t1, t0, a1, b0);
  umul_ppmm (s1, s0, a1, b1);
  add_ssaaaa (t1, t0, t1, t0, 0, r0);
  q = mi * t0;
  add_ssaaaa (r1, r0, s1, s0, 0, r1);
  umul_ppmm (p1, p0, q, m0);
  umul_ppmm (s1, s0, q, m1);
  r0 += (t0 != 0);  
  add_ssaaaa (r1, r0, r1, r0, 0, p1);
  add_ssaaaa (r1, r0, r1, r0, 0, t1);
  add_ssaaaa (r1, r0, r1, r0, s1, s0);

  if (ge2 (r1, r0, m1, m0))
    sub_ddmmss (r1, r0, r1, r0, m1, m0);

  *r1p = r1;
  return r0;
}

ATTRIBUTE_CONST
static uintmax_t
powm (uintmax_t b, uintmax_t e, uintmax_t n, uintmax_t ni, uintmax_t one)
{
  uintmax_t y = one;

  if (e & 1)
    y = b;

  while (e != 0)
    {
      b = mulredc (b, b, n, ni);
      e >>= 1;

      if (e & 1)
        y = mulredc (y, b, n, ni);
    }

  return y;
}

static uintmax_t
powm2 (uintmax_t *r1m,
       const uintmax_t *bp, const uintmax_t *ep, const uintmax_t *np,
       uintmax_t ni, const uintmax_t *one)
{
  uintmax_t r1, r0, b1, b0, n1, n0;
  int i;
  uintmax_t e;

  b0 = bp[0];
  b1 = bp[1];
  n0 = np[0];
  n1 = np[1];

  r0 = one[0];
  r1 = one[1];

  for (e = ep[0], i = W_TYPE_SIZE; i > 0; i--, e >>= 1)
    {
      if (e & 1)
        {
          r0 = mulredc2 (r1m, r1, r0, b1, b0, n1, n0, ni);
          r1 = *r1m;
        }
      b0 = mulredc2 (r1m, b1, b0, b1, b0, n1, n0, ni);
      b1 = *r1m;
    }
  for (e = ep[1]; e > 0; e >>= 1)
    {
      if (e & 1)
        {
          r0 = mulredc2 (r1m, r1, r0, b1, b0, n1, n0, ni);
          r1 = *r1m;
        }
      b0 = mulredc2 (r1m, b1, b0, b1, b0, n1, n0, ni);
      b1 = *r1m;
    }
  *r1m = r1;
  return r0;
}

ATTRIBUTE_CONST
static bool
millerrabin (uintmax_t n, uintmax_t ni, uintmax_t b, uintmax_t q,
             int k, uintmax_t one)
{
  uintmax_t y = powm (b, q, n, ni, one);

  uintmax_t nm1 = n - one;       

  if (y == one || y == nm1)
    return true;

  for (int i = 1; i < k; i++)
    {
      y = mulredc (y, y, n, ni);

      if (y == nm1)
        return true;
      if (y == one)
        return false;
    }
  return false;
}

ATTRIBUTE_PURE static bool
millerrabin2 (const uintmax_t *np, uintmax_t ni, const uintmax_t *bp,
              const uintmax_t *qp, int k, const uintmax_t *one)
{
  uintmax_t y1, y0, nm1_1, nm1_0, r1m;

  y0 = powm2 (&r1m, bp, qp, np, ni, one);
  y1 = r1m;

  if (y0 == one[0] && y1 == one[1])
    return true;

  sub_ddmmss (nm1_1, nm1_0, np[1], np[0], one[1], one[0]);

  if (y0 == nm1_0 && y1 == nm1_1)
    return true;

  for (int i = 1; i < k; i++)
    {
      y0 = mulredc2 (&r1m, y1, y0, y1, y0, np[1], np[0], ni);
      y1 = r1m;

      if (y0 == nm1_0 && y1 == nm1_1)
        return true;
      if (y0 == one[0] && y1 == one[1])
        return false;
    }
  return false;
}

static bool
mp_millerrabin (mpz_srcptr n, mpz_srcptr nm1, mpz_ptr x, mpz_ptr y,
                mpz_srcptr q, mp_bitcnt_t k)
{
  mpz_powm (y, x, q, n);

  if (mpz_cmp_ui (y, 1) == 0 || mpz_cmp (y, nm1) == 0)
    return true;

  for (mp_bitcnt_t i = 1; i < k; i++)
    {
      mpz_powm_ui (y, y, 2, n);
      if (mpz_cmp (y, nm1) == 0)
        return true;
      if (mpz_cmp_ui (y, 1) == 0)
        return false;
    }
  return false;
}

 
static bool ATTRIBUTE_PURE
prime_p (uintmax_t n)
{
  mp_bitcnt_t k;
  bool is_prime;
  uintmax_t a_prim, one, ni;
  struct factors factors;

  if (n <= 1)
    return false;

   
  if (n < (uintmax_t) FIRST_OMITTED_PRIME * FIRST_OMITTED_PRIME)
    return true;

   
  uintmax_t q = n - 1;
  for (k = 0; (q & 1) == 0; k++)
    q >>= 1;

  uintmax_t a = 2;
  binv (ni, n);                  
  redcify (one, 1, n);
  addmod (a_prim, one, one, n);  

   
  if (!millerrabin (n, ni, a_prim, q, k, one))
    return false;

  if (flag_prove_primality)
    {
       
      factor (0, n - 1, &factors);
    }

   
  for (idx_t r = 0; r < PRIMES_PTAB_ENTRIES; r++)
    {
      if (flag_prove_primality)
        {
          is_prime = true;
          for (int i = 0; i < factors.nfactors && is_prime; i++)
            {
              is_prime
                = powm (a_prim, (n - 1) / factors.p[i], n, ni, one) != one;
            }
        }
      else
        {
           
          is_prime = (r == MR_REPS - 1);
        }

      if (is_prime)
        return true;

      a += primes_diff[r];       

       
      {
        uintmax_t s1, s0;
        umul_ppmm (s1, s0, one, a);
        if (LIKELY (s1 == 0))
          a_prim = s0 % n;
        else
          {
            MAYBE_UNUSED uintmax_t dummy;
            udiv_qrnnd (dummy, a_prim, s1, s0, n);
          }
      }

      if (!millerrabin (n, ni, a_prim, q, k, one))
        return false;
    }

  affirm (!"Lucas prime test failure.  This should not happen");
}

static bool ATTRIBUTE_PURE
prime2_p (uintmax_t n1, uintmax_t n0)
{
  uintmax_t q[2], nm1[2];
  uintmax_t a_prim[2];
  uintmax_t one[2];
  uintmax_t na[2];
  uintmax_t ni;
  int k;
  struct factors factors;

  if (n1 == 0)
    return prime_p (n0);

  nm1[1] = n1 - (n0 == 0);
  nm1[0] = n0 - 1;
  if (nm1[0] == 0)
    {
      count_trailing_zeros (k, nm1[1]);

      q[0] = nm1[1] >> k;
      q[1] = 0;
      k += W_TYPE_SIZE;
    }
  else
    {
      count_trailing_zeros (k, nm1[0]);
      rsh2 (q[1], q[0], nm1[1], nm1[0], k);
    }

  uintmax_t a = 2;
  binv (ni, n0);
  redcify2 (one[1], one[0], 1, n1, n0);
  addmod2 (a_prim[1], a_prim[0], one[1], one[0], one[1], one[0], n1, n0);

   
  na[0] = n0;
  na[1] = n1;

  if (!millerrabin2 (na, ni, a_prim, q, k, one))
    return false;

  if (flag_prove_primality)
    {
       
      factor (nm1[1], nm1[0], &factors);
    }

   
  for (idx_t r = 0; r < PRIMES_PTAB_ENTRIES; r++)
    {
      bool is_prime;
      uintmax_t e[2], y[2];

      if (flag_prove_primality)
        {
          is_prime = true;
          if (factors.plarge[1])
            {
              uintmax_t pi;
              binv (pi, factors.plarge[0]);
              e[0] = pi * nm1[0];
              e[1] = 0;
              y[0] = powm2 (&y[1], a_prim, e, na, ni, one);
              is_prime = (y[0] != one[0] || y[1] != one[1]);
            }
          for (int i = 0; i < factors.nfactors && is_prime; i++)
            {
               
              if (factors.p[i] == 2)
                rsh2 (e[1], e[0], nm1[1], nm1[0], 1);
              else
                divexact_21 (e[1], e[0], nm1[1], nm1[0], factors.p[i]);
              y[0] = powm2 (&y[1], a_prim, e, na, ni, one);
              is_prime = (y[0] != one[0] || y[1] != one[1]);
            }
        }
      else
        {
           
          is_prime = (r == MR_REPS - 1);
        }

      if (is_prime)
        return true;

      a += primes_diff[r];       
      redcify2 (a_prim[1], a_prim[0], a, n1, n0);

      if (!millerrabin2 (na, ni, a_prim, q, k, one))
        return false;
    }

  affirm (!"Lucas prime test failure.  This should not happen");
}

static bool
mp_prime_p (mpz_t n)
{
  bool is_prime;
  mpz_t q, a, nm1, tmp;
  struct mp_factors factors;

  if (mpz_cmp_ui (n, 1) <= 0)
    return false;

   
  if (mpz_cmp_ui (n, (long) FIRST_OMITTED_PRIME * FIRST_OMITTED_PRIME) < 0)
    return true;

  mpz_inits (q, a, nm1, tmp, nullptr);

   
  mpz_sub_ui (nm1, n, 1);

   
  mp_bitcnt_t k = mpz_scan1 (nm1, 0);
  mpz_tdiv_q_2exp (q, nm1, k);

  mpz_set_ui (a, 2);

   
  if (!mp_millerrabin (n, nm1, a, tmp, q, k))
    {
      is_prime = false;
      goto ret2;
    }

  if (flag_prove_primality)
    {
       
      mpz_set (tmp, nm1);
      mp_factor (tmp, &factors);
    }

   
  for (idx_t r = 0; r < PRIMES_PTAB_ENTRIES; r++)
    {
      if (flag_prove_primality)
        {
          is_prime = true;
          for (idx_t i = 0; i < factors.nfactors && is_prime; i++)
            {
              mpz_divexact (tmp, nm1, factors.p[i]);
              mpz_powm (tmp, a, tmp, n);
              is_prime = mpz_cmp_ui (tmp, 1) != 0;
            }
        }
      else
        {
           
          is_prime = (r == MR_REPS - 1);
        }

      if (is_prime)
        goto ret1;

      mpz_add_ui (a, a, primes_diff[r]);         

      if (!mp_millerrabin (n, nm1, a, tmp, q, k))
        {
          is_prime = false;
          goto ret1;
        }
    }

  affirm (!"Lucas prime test failure.  This should not happen");

 ret1:
  if (flag_prove_primality)
    mp_factor_clear (&factors);
 ret2:
  mpz_clears (q, a, nm1, tmp, nullptr);

  return is_prime;
}

static void
factor_using_pollard_rho (uintmax_t n, unsigned long int a,
                          struct factors *factors)
{
  uintmax_t x, z, y, P, t, ni, g;

  unsigned long int k = 1;
  unsigned long int l = 1;

  redcify (P, 1, n);
  addmod (x, P, P, n);           
  y = z = x;

  while (n != 1)
    {
      affirm (a < n);

      binv (ni, n);              

      for (;;)
        {
          do
            {
              x = mulredc (x, x, n, ni);
              addmod (x, x, a, n);

              submod (t, z, x, n);
              P = mulredc (P, t, n, ni);

              if (k % 32 == 1)
                {
                  if (gcd_odd (P, n) != 1)
                    goto factor_found;
                  y = x;
                }
            }
          while (--k != 0);

          z = x;
          k = l;
          l = 2 * l;
          for (unsigned long int i = 0; i < k; i++)
            {
              x = mulredc (x, x, n, ni);
              addmod (x, x, a, n);
            }
          y = x;
        }

    factor_found:
      do
        {
          y = mulredc (y, y, n, ni);
          addmod (y, y, a, n);

          submod (t, z, y, n);
          g = gcd_odd (t, n);
        }
      while (g == 1);

      if (n == g)
        {
           
          factor_using_pollard_rho (n, a + 1, factors);
          return;
        }

      n = n / g;

      if (!prime_p (g))
        factor_using_pollard_rho (g, a + 1, factors);
      else
        factor_insert (factors, g);

      if (prime_p (n))
        {
          factor_insert (factors, n);
          break;
        }

      x = x % n;
      z = z % n;
      y = y % n;
    }
}

static void
factor_using_pollard_rho2 (uintmax_t n1, uintmax_t n0, unsigned long int a,
                           struct factors *factors)
{
  uintmax_t x1, x0, z1, z0, y1, y0, P1, P0, t1, t0, ni, g1, g0, r1m;

  unsigned long int k = 1;
  unsigned long int l = 1;

  redcify2 (P1, P0, 1, n1, n0);
  addmod2 (x1, x0, P1, P0, P1, P0, n1, n0);  
  y1 = z1 = x1;
  y0 = z0 = x0;

  while (n1 != 0 || n0 != 1)
    {
      binv (ni, n0);

      for (;;)
        {
          do
            {
              x0 = mulredc2 (&r1m, x1, x0, x1, x0, n1, n0, ni);
              x1 = r1m;
              addmod2 (x1, x0, x1, x0, 0, (uintmax_t) a, n1, n0);

              submod2 (t1, t0, z1, z0, x1, x0, n1, n0);
              P0 = mulredc2 (&r1m, P1, P0, t1, t0, n1, n0, ni);
              P1 = r1m;

              if (k % 32 == 1)
                {
                  g0 = gcd2_odd (&g1, P1, P0, n1, n0);
                  if (g1 != 0 || g0 != 1)
                    goto factor_found;
                  y1 = x1; y0 = x0;
                }
            }
          while (--k != 0);

          z1 = x1; z0 = x0;
          k = l;
          l = 2 * l;
          for (unsigned long int i = 0; i < k; i++)
            {
              x0 = mulredc2 (&r1m, x1, x0, x1, x0, n1, n0, ni);
              x1 = r1m;
              addmod2 (x1, x0, x1, x0, 0, (uintmax_t) a, n1, n0);
            }
          y1 = x1; y0 = x0;
        }

    factor_found:
      do
        {
          y0 = mulredc2 (&r1m, y1, y0, y1, y0, n1, n0, ni);
          y1 = r1m;
          addmod2 (y1, y0, y1, y0, 0, (uintmax_t) a, n1, n0);

          submod2 (t1, t0, z1, z0, y1, y0, n1, n0);
          g0 = gcd2_odd (&g1, t1, t0, n1, n0);
        }
      while (g1 == 0 && g0 == 1);

      if (g1 == 0)
        {
           
          divexact_21 (n1, n0, n1, n0, g0);      

          if (!prime_p (g0))
            factor_using_pollard_rho (g0, a + 1, factors);
          else
            factor_insert (factors, g0);
        }
      else
        {
           
          uintmax_t ginv;

          if (n1 == g1 && n0 == g0)
            {
               
              factor_using_pollard_rho2 (n1, n0, a + 1, factors);
              return;
            }

           
          binv (ginv, g0);
          n0 = ginv * n0;
          n1 = 0;

          if (!prime2_p (g1, g0))
            factor_using_pollard_rho2 (g1, g0, a + 1, factors);
          else
            factor_insert_large (factors, g1, g0);
        }

      if (n1 == 0)
        {
          if (prime_p (n0))
            {
              factor_insert (factors, n0);
              break;
            }

          factor_using_pollard_rho (n0, a, factors);
          return;
        }

      if (prime2_p (n1, n0))
        {
          factor_insert_large (factors, n1, n0);
          break;
        }

      x0 = mod2 (&x1, x1, x0, n1, n0);
      z0 = mod2 (&z1, z1, z0, n1, n0);
      y0 = mod2 (&y1, y1, y0, n1, n0);
    }
}

static void
mp_factor_using_pollard_rho (mpz_t n, unsigned long int a,
                             struct mp_factors *factors)
{
  mpz_t x, z, y, P;
  mpz_t t, t2;

  devmsg ("[pollard-rho (%lu)] ", a);

  mpz_inits (t, t2, nullptr);
  mpz_init_set_si (y, 2);
  mpz_init_set_si (x, 2);
  mpz_init_set_si (z, 2);
  mpz_init_set_ui (P, 1);

  unsigned long long int k = 1;
  unsigned long long int l = 1;

  while (mpz_cmp_ui (n, 1) != 0)
    {
      for (;;)
        {
          do
            {
              mpz_mul (t, x, x);
              mpz_mod (x, t, n);
              mpz_add_ui (x, x, a);

              mpz_sub (t, z, x);
              mpz_mul (t2, P, t);
              mpz_mod (P, t2, n);

              if (k % 32 == 1)
                {
                  mpz_gcd (t, P, n);
                  if (mpz_cmp_ui (t, 1) != 0)
                    goto factor_found;
                  mpz_set (y, x);
                }
            }
          while (--k != 0);

          mpz_set (z, x);
          k = l;
          l = 2 * l;
          for (unsigned long long int i = 0; i < k; i++)
            {
              mpz_mul (t, x, x);
              mpz_mod (x, t, n);
              mpz_add_ui (x, x, a);
            }
          mpz_set (y, x);
        }

    factor_found:
      do
        {
          mpz_mul (t, y, y);
          mpz_mod (y, t, n);
          mpz_add_ui (y, y, a);

          mpz_sub (t, z, y);
          mpz_gcd (t, t, n);
        }
      while (mpz_cmp_ui (t, 1) == 0);

      mpz_divexact (n, n, t);    

      if (!mp_prime_p (t))
        {
          devmsg ("[composite factor--restarting pollard-rho] ");
          mp_factor_using_pollard_rho (t, a + 1, factors);
        }
      else
        {
          mp_factor_insert (factors, t);
        }

      if (mp_prime_p (n))
        {
          mp_factor_insert (factors, n);
          break;
        }

      mpz_mod (x, x, n);
      mpz_mod (z, z, n);
      mpz_mod (y, y, n);
    }

  mpz_clears (P, t2, t, z, x, y, nullptr);
}

#if USE_SQUFOF
 
ATTRIBUTE_CONST
static uintmax_t
isqrt (uintmax_t n)
{
  uintmax_t x;
  int c;
  if (n == 0)
    return 0;

  count_leading_zeros (c, n);

   
  x = (uintmax_t) 1 << ((W_TYPE_SIZE + 1 - c) >> 1);

  for (;;)
    {
      uintmax_t y = (x + n / x) / 2;
      if (y >= x)
        return x;

      x = y;
    }
}

ATTRIBUTE_CONST
static uintmax_t
isqrt2 (uintmax_t nh, uintmax_t nl)
{
  int shift;
  uintmax_t x;

   
  affirm (nh < ((uintmax_t) 1 << (W_TYPE_SIZE - 2)));

  if (nh == 0)
    return isqrt (nl);

  count_leading_zeros (shift, nh);
  shift &= ~1;

   
  x = isqrt ((nh << shift) + (nl >> (W_TYPE_SIZE - shift))) + 1;
  x <<= (W_TYPE_SIZE - shift) >> 1;

   
  for (;;)
    {
      MAYBE_UNUSED uintmax_t r;
      uintmax_t q, y;
      udiv_qrnnd (q, r, nh, nl, x);
      y = (x + q) / 2;

      if (y >= x)
        {
          uintmax_t hi, lo;
          umul_ppmm (hi, lo, x + 1, x + 1);
          affirm (gt2 (hi, lo, nh, nl));

          umul_ppmm (hi, lo, x, x);
          affirm (ge2 (nh, nl, hi, lo));
          sub_ddmmss (hi, lo, nh, nl, hi, lo);
          affirm (hi == 0);

          return x;
        }

      x = y;
    }
}

 
# define MAGIC64 0x0202021202030213ULL
# define MAGIC63 0x0402483012450293ULL
# define MAGIC65 0x218a019866014613ULL
# define MAGIC11 0x23b

 
ATTRIBUTE_CONST
static uintmax_t
is_square (uintmax_t x)
{
   
  if (((MAGIC64 >> (x & 63)) & 1)
      && ((MAGIC63 >> (x % 63)) & 1)
       
      && ((MAGIC65 >> ((x % 65) & 63)) & 1)
      && ((MAGIC11 >> (x % 11) & 1)))
    {
      uintmax_t r = isqrt (x);
      if (r * r == x)
        return r;
    }
  return 0;
}

 
static short const invtab[0x81] =
  {
    0x200,
    0x1fc, 0x1f8, 0x1f4, 0x1f0, 0x1ec, 0x1e9, 0x1e5, 0x1e1,
    0x1de, 0x1da, 0x1d7, 0x1d4, 0x1d0, 0x1cd, 0x1ca, 0x1c7,
    0x1c3, 0x1c0, 0x1bd, 0x1ba, 0x1b7, 0x1b4, 0x1b2, 0x1af,
    0x1ac, 0x1a9, 0x1a6, 0x1a4, 0x1a1, 0x19e, 0x19c, 0x199,
    0x197, 0x194, 0x192, 0x18f, 0x18d, 0x18a, 0x188, 0x186,
    0x183, 0x181, 0x17f, 0x17d, 0x17a, 0x178, 0x176, 0x174,
    0x172, 0x170, 0x16e, 0x16c, 0x16a, 0x168, 0x166, 0x164,
    0x162, 0x160, 0x15e, 0x15c, 0x15a, 0x158, 0x157, 0x155,
    0x153, 0x151, 0x150, 0x14e, 0x14c, 0x14a, 0x149, 0x147,
    0x146, 0x144, 0x142, 0x141, 0x13f, 0x13e, 0x13c, 0x13b,
    0x139, 0x138, 0x136, 0x135, 0x133, 0x132, 0x130, 0x12f,
    0x12e, 0x12c, 0x12b, 0x129, 0x128, 0x127, 0x125, 0x124,
    0x123, 0x121, 0x120, 0x11f, 0x11e, 0x11c, 0x11b, 0x11a,
    0x119, 0x118, 0x116, 0x115, 0x114, 0x113, 0x112, 0x111,
    0x10f, 0x10e, 0x10d, 0x10c, 0x10b, 0x10a, 0x109, 0x108,
    0x107, 0x106, 0x105, 0x104, 0x103, 0x102, 0x101, 0x100,
  };

 
# define div_smallq(q, r, u, d)                                          \
  do {                                                                  \
    if ((u) / 0x40 < (d))                                               \
      {                                                                 \
        int _cnt;                                                       \
        uintmax_t _dinv, _mask, _q, _r;                                 \
        count_leading_zeros (_cnt, (d));                                \
        _r = (u);                                                       \
        if (UNLIKELY (_cnt > (W_TYPE_SIZE - 8)))                        \
          {                                                             \
            _dinv = invtab[((d) << (_cnt + 8 - W_TYPE_SIZE)) - 0x80];   \
            _q = _dinv * _r >> (8 + W_TYPE_SIZE - _cnt);                \
          }                                                             \
        else                                                            \
          {                                                             \
            _dinv = invtab[((d) >> (W_TYPE_SIZE - 8 - _cnt)) - 0x7f];   \
            _q = _dinv * (_r >> (W_TYPE_SIZE - 3 - _cnt)) >> 11;        \
          }                                                             \
        _r -= _q * (d);                                                 \
                                                                        \
        _mask = -(uintmax_t) (_r >= (d));                               \
        (r) = _r - (_mask & (d));                                       \
        (q) = _q - _mask;                                               \
        affirm ((q) * (d) + (r) == u);					\
      }                                                                 \
    else                                                                \
      {                                                                 \
        uintmax_t _q = (u) / (d);                                       \
        (r) = (u) - _q * (d);                                           \
        (q) = _q;                                                       \
      }                                                                 \
  } while (0)

 

 
# define QUEUE_SIZE 50
#endif

#if STAT_SQUFOF
# define Q_FREQ_SIZE 50
 
static int q_freq[Q_FREQ_SIZE + 1];
#endif

#if USE_SQUFOF
 
static bool
factor_using_squfof (uintmax_t n1, uintmax_t n0, struct factors *factors)
{
   

  static short const multipliers_1[] =
    {  
      105, 165, 21, 385, 33, 5, 77, 1, 0
    };
  static short const multipliers_3[] =
    {  
      1155, 15, 231, 35, 3, 55, 7, 11, 0
    };

  struct { uintmax_t Q; uintmax_t P; } queue[QUEUE_SIZE];

  if (n1 >= ((uintmax_t) 1 << (W_TYPE_SIZE - 2)))
    return false;

  uintmax_t sqrt_n = isqrt2 (n1, n0);

  if (n0 == sqrt_n * sqrt_n)
    {
      uintmax_t p1, p0;

      umul_ppmm (p1, p0, sqrt_n, sqrt_n);
      affirm (p0 == n0);

      if (n1 == p1)
        {
          if (prime_p (sqrt_n))
            factor_insert_multiplicity (factors, sqrt_n, 2);
          else
            {
              struct factors f;

              f.nfactors = 0;
              if (!factor_using_squfof (0, sqrt_n, &f))
                {
                   
                  factor_using_pollard_rho (sqrt_n, 1, &f);
                }
               
              for (unsigned int i = 0; i < f.nfactors; i++)
                factor_insert_multiplicity (factors, f.p[i], 2 * f.e[i]);
            }
          return true;
        }
    }

   
  for (short const *m = (n0 % 4 == 1) ? multipliers_3 : multipliers_1;
       *m; m++)
    {
      uintmax_t S, Dh, Dl, Q1, Q, P, L, L1, B;
      unsigned int i;
      unsigned int mu = *m;
      int qpos = 0;

      affirm (mu * n0 % 4 == 3);

       
      if (n1 == 0)
        {
          if ((uintmax_t) mu * mu * mu >= n0 / 64)
            continue;
        }
      else
        {
          if (n1 > ((uintmax_t) 1 << (W_TYPE_SIZE - 2)) / mu)
            continue;
        }
      umul_ppmm (Dh, Dl, n0, mu);
      Dh += n1 * mu;

      affirm (Dl % 4 != 1);
      affirm (Dh < (uintmax_t) 1 << (W_TYPE_SIZE - 2));

      S = isqrt2 (Dh, Dl);

      Q1 = 1;
      P = S;

       
      Q = Dl - P * P;
       
      L = isqrt (2 * S);
      B = 2 * L;
      L1 = mu * 2 * L;

       

      for (i = 0; i <= B; i++)
        {
          uintmax_t q, P1, t, rem;

          div_smallq (q, rem, S + P, Q);
          P1 = S - rem;  

          affirm (q > 0 && Q > 0);

# if STAT_SQUFOF
          q_freq[0]++;
          q_freq[MIN (q, Q_FREQ_SIZE)]++;
# endif

          if (Q <= L1)
            {
              uintmax_t g = Q;

              if ((Q & 1) == 0)
                g /= 2;

              g /= gcd_odd (g, mu);

              if (g <= L)
                {
                  if (qpos >= QUEUE_SIZE)
                    error (EXIT_FAILURE, 0, _("squfof queue overflow"));
                  queue[qpos].Q = g;
                  queue[qpos].P = P % g;
                  qpos++;
                }
            }

           
          t = Q1 + q * (P - P1);
          Q1 = Q;
          Q = t;
          P = P1;

          if ((i & 1) == 0)
            {
              uintmax_t r = is_square (Q);
              if (r)
                {
                  for (int j = 0; j < qpos; j++)
                    {
                      if (queue[j].Q == r)
                        {
                          if (r == 1)
                             
                            goto next_multiplier;

                           
                          if (P >= queue[j].P)
                            t = P - queue[j].P;
                          else
                            t = queue[j].P - P;
                          if (t % r == 0)
                            {
                               
                              memmove (queue, queue + j + 1,
                                       (qpos - j - 1) * sizeof (queue[0]));
                              qpos -= (j + 1);
                            }
                          goto next_i;
                        }
                    }

                   
                  Q1 = r;
                  affirm (S >= P);  
                  P += r * ((S - P) / r);

                   
                   
                  uintmax_t hi, lo;
                  umul_ppmm (hi, lo, P, P);
                  sub_ddmmss (hi, lo, Dh, Dl, hi, lo);
                  udiv_qrnnd (Q, rem, hi, lo, Q1);
                  affirm (rem == 0);

                  for (;;)
                    {
                       
                      div_smallq (q, rem, S + P, Q);
                      P1 = S - rem;      

# if STAT_SQUFOF
                      q_freq[0]++;
                      q_freq[MIN (q, Q_FREQ_SIZE)]++;
# endif
                      if (P == P1)
                        break;
                      t = Q1 + q * (P - P1);
                      Q1 = Q;
                      Q = t;
                      P = P1;
                    }

                  if ((Q & 1) == 0)
                    Q /= 2;
                  Q /= gcd_odd (Q, mu);

                  affirm (Q > 1 && (n1 || Q < n0));

                  if (prime_p (Q))
                    factor_insert (factors, Q);
                  else if (!factor_using_squfof (0, Q, factors))
                    factor_using_pollard_rho (Q, 2, factors);

                  divexact_21 (n1, n0, n1, n0, Q);

                  if (prime2_p (n1, n0))
                    factor_insert_large (factors, n1, n0);
                  else
                    {
                      if (!factor_using_squfof (n1, n0, factors))
                        {
                          if (n1 == 0)
                            factor_using_pollard_rho (n0, 1, factors);
                          else
                            factor_using_pollard_rho2 (n1, n0, 1, factors);
                        }
                    }

                  return true;
                }
            }
        next_i:;
        }
    next_multiplier:;
    }
  return false;
}
#endif

 
static void
factor (uintmax_t t1, uintmax_t t0, struct factors *factors)
{
  factors->nfactors = 0;
  factors->plarge[1] = 0;

  if (t1 == 0 && t0 < 2)
    return;

  t0 = factor_using_division (&t1, t1, t0, factors);

  if (t1 == 0 && t0 < 2)
    return;

  if (prime2_p (t1, t0))
    factor_insert_large (factors, t1, t0);
  else
    {
#if USE_SQUFOF
      if (factor_using_squfof (t1, t0, factors))
        return;
#endif

      if (t1 == 0)
        factor_using_pollard_rho (t0, 1, factors);
      else
        factor_using_pollard_rho2 (t1, t0, 1, factors);
    }
}

 
static void
mp_factor (mpz_t t, struct mp_factors *factors)
{
  mp_factor_init (factors);

  if (mpz_sgn (t) != 0)
    {
      mp_factor_using_division (t, factors);

      if (mpz_cmp_ui (t, 1) != 0)
        {
          devmsg ("[is number prime?] ");
          if (mp_prime_p (t))
            mp_factor_insert (factors, t);
          else
            mp_factor_using_pollard_rho (t, 1, factors);
        }
    }
}

static strtol_error
strto2uintmax (uintmax_t *hip, uintmax_t *lop, char const *s)
{
  int lo_carry;
  uintmax_t hi = 0, lo = 0;

  strtol_error err = LONGINT_INVALID;

   
  char const *p = s;
  for (;;)
    {
      unsigned char c = *p++;
      if (c == 0)
        break;

      if (UNLIKELY (!ISDIGIT (c)))
        {
          err = LONGINT_INVALID;
          break;
        }

      err = LONGINT_OK;            
    }

  while (err == LONGINT_OK)
    {
      unsigned char c = *s++;
      if (c == 0)
        break;

      c -= '0';

      if (UNLIKELY (hi > ~(uintmax_t)0 / 10))
        {
          err = LONGINT_OVERFLOW;
          break;
        }
      hi = 10 * hi;

      lo_carry = (lo >> (W_TYPE_SIZE - 3)) + (lo >> (W_TYPE_SIZE - 1));
      lo_carry += 10 * lo < 2 * lo;

      lo = 10 * lo;
      lo += c;

      lo_carry += lo < c;
      hi += lo_carry;
      if (UNLIKELY (hi < lo_carry))
        {
          err = LONGINT_OVERFLOW;
          break;
        }
    }

  *hip = hi;
  *lop = lo;

  return err;
}

 
static struct lbuf_
{
  char *buf;
  char *end;
} lbuf;

 
#define FACTOR_PIPE_BUF 512

static void
lbuf_alloc (void)
{
  if (lbuf.buf)
    return;

   
  lbuf.buf = xmalloc (FACTOR_PIPE_BUF * 2);
  lbuf.end = lbuf.buf;
}

 
static void
lbuf_flush (void)
{
  size_t size = lbuf.end - lbuf.buf;
  if (full_write (STDOUT_FILENO, lbuf.buf, size) != size)
    write_error ();
  lbuf.end = lbuf.buf;
}

 
static void
lbuf_putc (char c)
{
  *lbuf.end++ = c;

  if (c == '\n')
    {
      size_t buffered = lbuf.end - lbuf.buf;

       
      static int line_buffered = -1;
      if (line_buffered == -1)
        line_buffered = isatty (STDIN_FILENO) || isatty (STDOUT_FILENO);
      if (line_buffered)
        lbuf_flush ();
      else if (buffered >= FACTOR_PIPE_BUF)
        {
           
          char const *tend = lbuf.end;

           
          char *tlend = lbuf.buf + FACTOR_PIPE_BUF;
          while (*--tlend != '\n');
          tlend++;

          lbuf.end = tlend;
          lbuf_flush ();

           
          memcpy (lbuf.buf, tlend, tend - tlend);
          lbuf.end = lbuf.buf + (tend - tlend);
        }
    }
}

 
static void
lbuf_putint (uintmax_t i, size_t min_width)
{
  char buf[INT_BUFSIZE_BOUND (uintmax_t)];
  char const *umaxstr = umaxtostr (i, buf);
  size_t width = sizeof (buf) - (umaxstr - buf) - 1;
  size_t z = width;

  for (; z < min_width; z++)
    *lbuf.end++ = '0';

  memcpy (lbuf.end, umaxstr, width);
  lbuf.end += width;
}

static void
print_uintmaxes (uintmax_t t1, uintmax_t t0)
{
  uintmax_t q, r;

  if (t1 == 0)
    lbuf_putint (t0, 0);
  else
    {
       
      q = t1 / 1000000000;
      r = t1 % 1000000000;
      udiv_qrnnd (t0, r, r, t0, 1000000000);
      print_uintmaxes (q, t0);
      lbuf_putint (r, 9);
    }
}

 
static void
print_factors_single (uintmax_t t1, uintmax_t t0)
{
  struct factors factors;

  print_uintmaxes (t1, t0);
  lbuf_putc (':');

  factor (t1, t0, &factors);

  for (int j = 0; j < factors.nfactors; j++)
    for (int k = 0; k < factors.e[j]; k++)
      {
        lbuf_putc (' ');
        print_uintmaxes (0, factors.p[j]);
        if (print_exponents && factors.e[j] > 1)
          {
            lbuf_putc ('^');
            lbuf_putint (factors.e[j], 0);
            break;
          }
      }

  if (factors.plarge[1])
    {
      lbuf_putc (' ');
      print_uintmaxes (factors.plarge[1], factors.plarge[0]);
    }

  lbuf_putc ('\n');
}

 
static bool
print_factors (char const *input)
{
   
  char const *str = input;
  while (*str == ' ')
    str++;
  str += *str == '+';

  uintmax_t t1, t0;

   
  strtol_error err = strto2uintmax (&t1, &t0, str);

  switch (err)
    {
    case LONGINT_OK:
      if (((t1 << 1) >> 1) == t1)
        {
          devmsg ("[using single-precision arithmetic] ");
          print_factors_single (t1, t0);
          return true;
        }
      break;

    case LONGINT_OVERFLOW:
       
      break;

    default:
      error (0, 0, _("%s is not a valid positive integer"), quote (input));
      return false;
    }

  devmsg ("[using arbitrary-precision arithmetic] ");
  mpz_t t;
  struct mp_factors factors;

  mpz_init_set_str (t, str, 10);

  mpz_out_str (stdout, 10, t);
  putchar (':');
  mp_factor (t, &factors);

  for (idx_t j = 0; j < factors.nfactors; j++)
    for (unsigned long int k = 0; k < factors.e[j]; k++)
      {
        putchar (' ');
        mpz_out_str (stdout, 10, factors.p[j]);
        if (print_exponents && factors.e[j] > 1)
          {
            printf ("^%lu", factors.e[j]);
            break;
          }
      }

  mp_factor_clear (&factors);
  mpz_clear (t);
  putchar ('\n');
  fflush (stdout);
  return true;
}

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    emit_try_help ();
  else
    {
      printf (_("\
Usage: %s [OPTION] [NUMBER]...\n\
"),
              program_name);
      fputs (_("\
Print the prime factors of each specified integer NUMBER.  If none\n\
are specified on the command line, read them from standard input.\n\
\n\
"), stdout);
      fputs ("\
  -h, --exponents   print repeated factors in form p^e unless e is 1\n\
", stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}

static bool
do_stdin (void)
{
  bool ok = true;
  token_buffer tokenbuffer;

  init_tokenbuffer (&tokenbuffer);

  while (true)
    {
      size_t token_length = readtoken (stdin, DELIM, sizeof (DELIM) - 1,
                                       &tokenbuffer);
      if (token_length == (size_t) -1)
        {
          if (ferror (stdin))
            error (EXIT_FAILURE, errno, _("error reading input"));
          break;
        }

      ok &= print_factors (tokenbuffer.buffer);
    }
  free (tokenbuffer.buffer);

  return ok;
}

int
main (int argc, char **argv)
{
  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  lbuf_alloc ();
  atexit (close_stdout);
  atexit (lbuf_flush);

  int c;
  while ((c = getopt_long (argc, argv, "h", long_options, nullptr)) != -1)
    {
      switch (c)
        {
        case 'h':  /* NetBSD used -h for this functionality first.  */
          print_exponents = true;
          break;

        case DEV_DEBUG_OPTION:
          dev_debug = true;
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          usage (EXIT_FAILURE);
        }
    }

#if STAT_SQUFOF
  memset (q_freq, 0, sizeof (q_freq));
#endif

  bool ok;
  if (argc <= optind)
    ok = do_stdin ();
  else
    {
      ok = true;
      for (int i = optind; i < argc; i++)
        if (! print_factors (argv[i]))
          ok = false;
    }

#if STAT_SQUFOF
  if (q_freq[0] > 0)
    {
      double acc_f;
      printf ("q  freq.  cum. freq.(total: %d)\n", q_freq[0]);
      for (int i = 1, acc_f = 0.0; i <= Q_FREQ_SIZE; i++)
        {
          double f = (double) q_freq[i] / q_freq[0];
          acc_f += f;
          printf ("%s%d %.2f%% %.2f%%\n", i == Q_FREQ_SIZE ? ">=" : "", i,
                  100.0 * f, 100.0 * acc_f);
        }
    }
#endif

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
