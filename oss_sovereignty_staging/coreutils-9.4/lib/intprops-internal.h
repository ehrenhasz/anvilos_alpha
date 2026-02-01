 
#define _GL_INT_CONVERT(e, v) ((1 ? 0 : (e)) + (v))

 

 
#define _GL_TYPE_SIGNED(t) (! ((t) 0 < (t) -1))

 
#define _GL_EXPR_SIGNED(e) (_GL_INT_NEGATE_CONVERT (e, 1) < 0)


 

 
#define _GL_TYPE_WIDTH(t) (sizeof (t) * CHAR_BIT)

 
#define _GL_INT_MINIMUM(e)                                              \
  (_GL_EXPR_SIGNED (e)                                                  \
   ? ~ _GL_SIGNED_INT_MAXIMUM (e)                                       \
   : _GL_INT_CONVERT (e, 0))
#define _GL_INT_MAXIMUM(e)                                              \
  (_GL_EXPR_SIGNED (e)                                                  \
   ? _GL_SIGNED_INT_MAXIMUM (e)                                         \
   : _GL_INT_NEGATE_CONVERT (e, 1))
#define _GL_SIGNED_INT_MAXIMUM(e)                                       \
  (((_GL_INT_CONVERT (e, 1) << (_GL_TYPE_WIDTH (+ (e)) - 2)) - 1) * 2 + 1)

 
#if !defined LLONG_MAX && defined __INT64_MAX
# define LLONG_MAX __INT64_MAX
# define LLONG_MIN __INT64_MIN
#endif

 

 
#if (2 <= __GNUC__ \
     || (4 <= __clang_major__) \
     || (1210 <= __IBMC__ && defined __IBM__TYPEOF__) \
     || (0x5110 <= __SUNPRO_C && !__STDC__))
# define _GL_HAVE___TYPEOF__ 1
#else
# define _GL_HAVE___TYPEOF__ 0
#endif

 
#if _GL_HAVE___TYPEOF__
# define _GL_SIGNED_TYPE_OR_EXPR(t) _GL_TYPE_SIGNED (__typeof__ (t))
#else
# define _GL_SIGNED_TYPE_OR_EXPR(t) 1
#endif

 
#define _GL_INT_NEGATE_RANGE_OVERFLOW(a, min, max) \
  ((min) < 0 ? (a) < - (max) : 0 < (a))

 
#ifdef __EDG__
 
#if defined __clang_major__ && __clang_major__ < 14
 
#ifdef __EDG__
 
# define _GL_HAS_BUILTIN_OVERFLOW_P 0
#elif defined __has_builtin
# define _GL_HAS_BUILTIN_OVERFLOW_P __has_builtin (__builtin_mul_overflow_p)
#else
# define _GL_HAS_BUILTIN_OVERFLOW_P (7 <= __GNUC__)
#endif

#if (!defined _GL_STDCKDINT_H && 202311 <= __STDC_VERSION__ \
     && ! (_GL_HAS_BUILTIN_ADD_OVERFLOW && _GL_HAS_BUILTIN_MUL_OVERFLOW))
# include <stdckdint.h>
#endif

 
#if _GL_HAS_BUILTIN_ADD_OVERFLOW
# define _GL_INT_ADD_WRAPV(a, b, r) __builtin_add_overflow (a, b, r)
# define _GL_INT_SUBTRACT_WRAPV(a, b, r) __builtin_sub_overflow (a, b, r)
#elif defined ckd_add && defined ckd_sub && !defined _GL_STDCKDINT_H
# define _GL_INT_ADD_WRAPV(a, b, r) ckd_add (r, + (a), + (b))
# define _GL_INT_SUBTRACT_WRAPV(a, b, r) ckd_sub (r, + (a), + (b))
#else
# define _GL_INT_ADD_WRAPV(a, b, r) \
   _GL_INT_OP_WRAPV (a, b, r, +, _GL_INT_ADD_RANGE_OVERFLOW)
# define _GL_INT_SUBTRACT_WRAPV(a, b, r) \
   _GL_INT_OP_WRAPV (a, b, r, -, _GL_INT_SUBTRACT_RANGE_OVERFLOW)
#endif
#if _GL_HAS_BUILTIN_MUL_OVERFLOW
# if ((9 < __GNUC__ + (3 <= __GNUC_MINOR__) \
       || (__GNUC__ == 8 && 4 <= __GNUC_MINOR__)) \
      && !defined __EDG__)
#  define _GL_INT_MULTIPLY_WRAPV(a, b, r) __builtin_mul_overflow (a, b, r)
# else
    
#  define _GL_INT_MULTIPLY_WRAPV(a, b, r) \
    ((!_GL_SIGNED_TYPE_OR_EXPR (*(r)) && _GL_EXPR_SIGNED (a) && _GL_EXPR_SIGNED (b) \
      && _GL_INT_MULTIPLY_RANGE_OVERFLOW (a, b, 0, (__typeof__ (*(r))) -1)) \
     ? ((void) __builtin_mul_overflow (a, b, r), 1) \
     : __builtin_mul_overflow (a, b, r))
# endif
#elif defined ckd_mul && !defined _GL_STDCKDINT_H
# define _GL_INT_MULTIPLY_WRAPV(a, b, r) ckd_mul (r, + (a), + (b))
#else
# define _GL_INT_MULTIPLY_WRAPV(a, b, r) \
   _GL_INT_OP_WRAPV (a, b, r, *, _GL_INT_MULTIPLY_RANGE_OVERFLOW)
#endif

 
#if __GNUC__ || defined __clang__
# define _GL__GENERIC_BOGUS 1
#else
# define _GL__GENERIC_BOGUS 0
#endif

 
#if 201112 <= __STDC_VERSION__ && !_GL__GENERIC_BOGUS
# define _GL_INT_OP_WRAPV(a, b, r, op, overflow) \
   (_Generic \
    (*(r), \
     signed char: \
       _GL_INT_OP_CALC (a, b, r, op, overflow, unsigned int, \
                        signed char, SCHAR_MIN, SCHAR_MAX), \
     unsigned char: \
       _GL_INT_OP_CALC (a, b, r, op, overflow, unsigned int, \
                        unsigned char, 0, UCHAR_MAX), \
     short int: \
       _GL_INT_OP_CALC (a, b, r, op, overflow, unsigned int, \
                        short int, SHRT_MIN, SHRT_MAX), \
     unsigned short int: \
       _GL_INT_OP_CALC (a, b, r, op, overflow, unsigned int, \
                        unsigned short int, 0, USHRT_MAX), \
     int: \
       _GL_INT_OP_CALC (a, b, r, op, overflow, unsigned int, \
                        int, INT_MIN, INT_MAX), \
     unsigned int: \
       _GL_INT_OP_CALC (a, b, r, op, overflow, unsigned int, \
                        unsigned int, 0, UINT_MAX), \
     long int: \
       _GL_INT_OP_CALC (a, b, r, op, overflow, unsigned long int, \
                        long int, LONG_MIN, LONG_MAX), \
     unsigned long int: \
       _GL_INT_OP_CALC (a, b, r, op, overflow, unsigned long int, \
                        unsigned long int, 0, ULONG_MAX), \
     long long int: \
       _GL_INT_OP_CALC (a, b, r, op, overflow, unsigned long long int, \
                        long long int, LLONG_MIN, LLONG_MAX), \
     unsigned long long int: \
       _GL_INT_OP_CALC (a, b, r, op, overflow, unsigned long long int, \
                        unsigned long long int, 0, ULLONG_MAX)))
#else
 
# if _GL_HAVE___TYPEOF__
#  define _GL_INT_OP_WRAPV_SMALLISH(a,b,r,op,overflow,st,smin,smax,ut,umax) \
    (_GL_TYPE_SIGNED (__typeof__ (*(r))) \
     ? _GL_INT_OP_CALC (a, b, r, op, overflow, unsigned int, st, smin, smax) \
     : _GL_INT_OP_CALC (a, b, r, op, overflow, unsigned int, ut, 0, umax))
# else
#  define _GL_INT_OP_WRAPV_SMALLISH(a,b,r,op,overflow,st,smin,smax,ut,umax) \
    (overflow (a, b, smin, smax) \
     ? (overflow (a, b, 0, umax) \
        ? (*(r) = _GL_INT_OP_WRAPV_VIA_UNSIGNED (a,b,op,unsigned,st), 1) \
        : (*(r) = _GL_INT_OP_WRAPV_VIA_UNSIGNED (a,b,op,unsigned,st)) < 0) \
     : (overflow (a, b, 0, umax) \
        ? (*(r) = _GL_INT_OP_WRAPV_VIA_UNSIGNED (a,b,op,unsigned,st)) >= 0 \
        : (*(r) = _GL_INT_OP_WRAPV_VIA_UNSIGNED (a,b,op,unsigned,st), 0)))
# endif

# define _GL_INT_OP_WRAPV(a, b, r, op, overflow) \
   (sizeof *(r) == sizeof (signed char) \
    ? _GL_INT_OP_WRAPV_SMALLISH (a, b, r, op, overflow, \
                                 signed char, SCHAR_MIN, SCHAR_MAX, \
                                 unsigned char, UCHAR_MAX) \
    : sizeof *(r) == sizeof (short int) \
    ? _GL_INT_OP_WRAPV_SMALLISH (a, b, r, op, overflow, \
                                 short int, SHRT_MIN, SHRT_MAX, \
                                 unsigned short int, USHRT_MAX) \
    : sizeof *(r) == sizeof (int) \
    ? (_GL_EXPR_SIGNED (*(r)) \
       ? _GL_INT_OP_CALC (a, b, r, op, overflow, unsigned int, \
                          int, INT_MIN, INT_MAX) \
       : _GL_INT_OP_CALC (a, b, r, op, overflow, unsigned int, \
                          unsigned int, 0, UINT_MAX)) \
    : _GL_INT_OP_WRAPV_LONGISH(a, b, r, op, overflow))
# ifdef LLONG_MAX
#  define _GL_INT_OP_WRAPV_LONGISH(a, b, r, op, overflow) \
    (sizeof *(r) == sizeof (long int) \
     ? (_GL_EXPR_SIGNED (*(r)) \
        ? _GL_INT_OP_CALC (a, b, r, op, overflow, unsigned long int, \
                           long int, LONG_MIN, LONG_MAX) \
        : _GL_INT_OP_CALC (a, b, r, op, overflow, unsigned long int, \
                           unsigned long int, 0, ULONG_MAX)) \
     : (_GL_EXPR_SIGNED (*(r)) \
        ? _GL_INT_OP_CALC (a, b, r, op, overflow, unsigned long long int, \
                           long long int, LLONG_MIN, LLONG_MAX) \
        : _GL_INT_OP_CALC (a, b, r, op, overflow, unsigned long long int, \
                           unsigned long long int, 0, ULLONG_MAX)))
# else
#  define _GL_INT_OP_WRAPV_LONGISH(a, b, r, op, overflow) \
    (_GL_EXPR_SIGNED (*(r)) \
     ? _GL_INT_OP_CALC (a, b, r, op, overflow, unsigned long int, \
                        long int, LONG_MIN, LONG_MAX) \
     : _GL_INT_OP_CALC (a, b, r, op, overflow, unsigned long int, \
                        unsigned long int, 0, ULONG_MAX))
# endif
#endif

 
#define _GL_INT_OP_CALC(a, b, r, op, overflow, ut, t, tmin, tmax) \
  (overflow (a, b, tmin, tmax) \
   ? (*(r) = _GL_INT_OP_WRAPV_VIA_UNSIGNED (a, b, op, ut, t), 1) \
   : (*(r) = _GL_INT_OP_WRAPV_VIA_UNSIGNED (a, b, op, ut, t), 0))

 

#if _GL_HAS_BUILTIN_OVERFLOW_P
# define _GL_INT_NEGATE_OVERFLOW(a) \
   __builtin_sub_overflow_p (0, a, (__typeof__ (- (a))) 0)
#else
# define _GL_INT_NEGATE_OVERFLOW(a) \
   _GL_INT_NEGATE_RANGE_OVERFLOW (a, _GL_INT_MINIMUM (a), _GL_INT_MAXIMUM (a))
#endif

 

#define _GL_INT_OP_WRAPV_VIA_UNSIGNED(a, b, op, ut, t) \
  ((t) ((ut) (a) op (ut) (b)))

 
#define _GL_INT_ADD_RANGE_OVERFLOW(a, b, tmin, tmax) \
  ((b) < 0 \
   ? (((tmin) \
       ? ((_GL_EXPR_SIGNED (_GL_INT_CONVERT (a, (tmin) - (b))) || (b) < (tmin)) \
          && (a) < (tmin) - (b)) \
       : (a) <= -1 - (b)) \
      || ((_GL_EXPR_SIGNED (a) ? 0 <= (a) : (tmax) < (a)) && (tmax) < (a) + (b))) \
   : (a) < 0 \
   ? (((tmin) \
       ? ((_GL_EXPR_SIGNED (_GL_INT_CONVERT (b, (tmin) - (a))) || (a) < (tmin)) \
          && (b) < (tmin) - (a)) \
       : (b) <= -1 - (a)) \
      || ((_GL_EXPR_SIGNED (_GL_INT_CONVERT (a, b)) || (tmax) < (b)) \
          && (tmax) < (a) + (b))) \
   : (tmax) < (b) || (tmax) - (b) < (a))
#define _GL_INT_SUBTRACT_RANGE_OVERFLOW(a, b, tmin, tmax) \
  (((a) < 0) == ((b) < 0) \
   ? ((a) < (b) \
      ? !(tmin) || -1 - (tmin) < (b) - (a) - 1 \
      : (tmax) < (a) - (b)) \
   : (a) < 0 \
   ? ((!_GL_EXPR_SIGNED (_GL_INT_CONVERT ((a) - (tmin), b)) && (a) - (tmin) < 0) \
      || (a) - (tmin) < (b)) \
   : ((! (_GL_EXPR_SIGNED (_GL_INT_CONVERT (tmax, b)) \
          && _GL_EXPR_SIGNED (_GL_INT_CONVERT ((tmax) + (b), a))) \
       && (tmax) <= -1 - (b)) \
      || (tmax) + (b) < (a)))
#define _GL_INT_MULTIPLY_RANGE_OVERFLOW(a, b, tmin, tmax) \
  ((b) < 0 \
   ? ((a) < 0 \
      ? (_GL_EXPR_SIGNED (_GL_INT_CONVERT (tmax, b)) \
         ? (a) < (tmax) / (b) \
         : ((_GL_INT_NEGATE_OVERFLOW (b) \
             ? _GL_INT_CONVERT (b, tmax) >> (_GL_TYPE_WIDTH (+ (b)) - 1) \
             : (tmax) / -(b)) \
            <= -1 - (a))) \
      : _GL_INT_NEGATE_OVERFLOW (_GL_INT_CONVERT (b, tmin)) && (b) == -1 \
      ? (_GL_EXPR_SIGNED (a) \
         ? 0 < (a) + (tmin) \
         : 0 < (a) && -1 - (tmin) < (a) - 1) \
      : (tmin) / (b) < (a)) \
   : (b) == 0 \
   ? 0 \
   : ((a) < 0 \
      ? (_GL_INT_NEGATE_OVERFLOW (_GL_INT_CONVERT (a, tmin)) && (a) == -1 \
         ? (_GL_EXPR_SIGNED (b) ? 0 < (b) + (tmin) : -1 - (tmin) < (b) - 1) \
         : (tmin) / (a) < (b)) \
      : (tmax) / (b) < (a)))

#endif  
