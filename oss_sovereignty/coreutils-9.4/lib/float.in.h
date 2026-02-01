 
#@INCLUDE_NEXT@ @NEXT_FLOAT_H@

#ifndef _@GUARD_PREFIX@_FLOAT_H
#define _@GUARD_PREFIX@_FLOAT_H

 

#if defined __i386__ && (defined __BEOS__ || defined __OpenBSD__)
 
# undef LDBL_MANT_DIG
# define LDBL_MANT_DIG   64
 
# undef LDBL_DIG
# define LDBL_DIG        18
 
# undef LDBL_EPSILON
# define LDBL_EPSILON    1.0842021724855044340E-19L
 
# undef LDBL_MIN_EXP
# define LDBL_MIN_EXP    (-16381)
 
# undef LDBL_MAX_EXP
# define LDBL_MAX_EXP    16384
 
# undef LDBL_MIN
# define LDBL_MIN        3.3621031431120935063E-4932L
 
# undef LDBL_MAX
# define LDBL_MAX        1.1897314953572317650E+4932L
 
# undef LDBL_MIN_10_EXP
# define LDBL_MIN_10_EXP (-4931)
 
# undef LDBL_MAX_10_EXP
# define LDBL_MAX_10_EXP 4932
#endif

 
# undef LDBL_MANT_DIG
# define LDBL_MANT_DIG   64
 
# undef LDBL_DIG
# define LDBL_DIG        18
 
# undef LDBL_EPSILON
# define LDBL_EPSILON 1.084202172485504434007452800869941711426e-19L  
 
# undef LDBL_MIN_EXP
# define LDBL_MIN_EXP    (-16381)
 
# undef LDBL_MAX_EXP
# define LDBL_MAX_EXP    16384
 
# undef LDBL_MIN
# define LDBL_MIN        3.362103143112093506262677817321752E-4932L  
 
# undef LDBL_MAX
 
# if !GNULIB_defined_long_double_union
union gl_long_double_union
  {
    struct { unsigned int lo; unsigned int hi; unsigned int exponent; } xd;
    long double ld;
  };
#  define GNULIB_defined_long_double_union 1
# endif
extern const union gl_long_double_union gl_LDBL_MAX;
# define LDBL_MAX (gl_LDBL_MAX.ld)
 
# undef LDBL_MIN_10_EXP
# define LDBL_MIN_10_EXP (-4931)
 
# undef LDBL_MAX_10_EXP
# define LDBL_MAX_10_EXP 4932
#endif

 
#if (defined _ARCH_PPC || defined _POWER) && defined _AIX && (LDBL_MANT_DIG == 106) && defined __GNUC__
# undef LDBL_MIN_EXP
# define LDBL_MIN_EXP DBL_MIN_EXP
# undef LDBL_MIN_10_EXP
# define LDBL_MIN_10_EXP DBL_MIN_10_EXP
# undef LDBL_MIN
# define LDBL_MIN 2.22507385850720138309023271733240406422e-308L  
#endif
#if (defined _ARCH_PPC || defined _POWER) && (defined _AIX || defined __linux__) && (LDBL_MANT_DIG == 106) && defined __GNUC__
# undef LDBL_MAX
 
# if !GNULIB_defined_long_double_union
union gl_long_double_union
  {
    struct { double hi; double lo; } dd;
    long double ld;
  };
#  define GNULIB_defined_long_double_union 1
# endif
extern const union gl_long_double_union gl_LDBL_MAX;
# define LDBL_MAX (gl_LDBL_MAX.ld)
#endif

 
#if defined __sgi && (LDBL_MANT_DIG >= 106)
# undef LDBL_MANT_DIG
# define LDBL_MANT_DIG 106
# if defined __GNUC__
#  undef LDBL_MIN_EXP
#  define LDBL_MIN_EXP DBL_MIN_EXP
#  undef LDBL_MIN_10_EXP
#  define LDBL_MIN_10_EXP DBL_MIN_10_EXP
#  undef LDBL_MIN
#  define LDBL_MIN 2.22507385850720138309023271733240406422e-308L  
#  undef LDBL_EPSILON
#  define LDBL_EPSILON 2.46519032881566189191165176650870696773e-32L  
# endif
#endif

#if @REPLACE_ITOLD@
 
extern
# ifdef __cplusplus
"C"
# endif
void _Qp_itoq (long double *, int);
static void (*_gl_float_fix_itold) (long double *, int) = _Qp_itoq;
#endif

#endif  
#endif  
