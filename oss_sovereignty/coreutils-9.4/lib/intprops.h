 

 
#define TYPE_IS_INTEGER(t) ((t) 1.5 == 1)

 
#define TYPE_SIGNED(t) _GL_TYPE_SIGNED (t)

 
#define EXPR_SIGNED(e) _GL_EXPR_SIGNED (e)


 

 
#define TYPE_WIDTH(t) _GL_TYPE_WIDTH (t)

 
#define TYPE_MINIMUM(t) ((t) ~ TYPE_MAXIMUM (t))
#define TYPE_MAXIMUM(t)                                                 \
  ((t) (! TYPE_SIGNED (t)                                               \
        ? (t) -1                                                        \
        : ((((t) 1 << (TYPE_WIDTH (t) - 2)) - 1) * 2 + 1)))

 
#define INT_BITS_STRLEN_BOUND(b) (((b) * 146 + 484) / 485)

 
#define INT_STRLEN_BOUND(t)                                     \
  (INT_BITS_STRLEN_BOUND (TYPE_WIDTH (t) - _GL_SIGNED_TYPE_OR_EXPR (t)) \
   + _GL_SIGNED_TYPE_OR_EXPR (t))

 
#define INT_BUFSIZE_BOUND(t) (INT_STRLEN_BOUND (t) + 1)


 

 
#define INT_ADD_RANGE_OVERFLOW(a, b, min, max)          \
  ((b) < 0                                              \
   ? (a) < (min) - (b)                                  \
   : (max) - (b) < (a))

 
#define INT_SUBTRACT_RANGE_OVERFLOW(a, b, min, max)     \
  ((b) < 0                                              \
   ? (max) + (b) < (a)                                  \
   : (a) < (min) + (b))

 
#define INT_NEGATE_RANGE_OVERFLOW(a, min, max)          \
  _GL_INT_NEGATE_RANGE_OVERFLOW (a, min, max)

 
#define INT_DIVIDE_RANGE_OVERFLOW(a, b, min, max)       \
  ((min) < 0 && (b) == -1 && (a) < - (max))

 
#define INT_REMAINDER_RANGE_OVERFLOW(a, b, min, max)    \
  INT_DIVIDE_RANGE_OVERFLOW (a, b, min, max)

 
#define INT_LEFT_SHIFT_RANGE_OVERFLOW(a, b, min, max)   \
  ((a) < 0                                              \
   ? (a) < (min) >> (b)                                 \
   : (max) >> (b) < (a))

 
#if _GL_HAS_BUILTIN_OVERFLOW_P
# define _GL_ADD_OVERFLOW(a, b, min, max)                               \
   __builtin_add_overflow_p (a, b, (__typeof__ ((a) + (b))) 0)
# define _GL_SUBTRACT_OVERFLOW(a, b, min, max)                          \
   __builtin_sub_overflow_p (a, b, (__typeof__ ((a) - (b))) 0)
# define _GL_MULTIPLY_OVERFLOW(a, b, min, max)                          \
   __builtin_mul_overflow_p (a, b, (__typeof__ ((a) * (b))) 0)
#else
# define _GL_ADD_OVERFLOW(a, b, min, max)                                \
   ((min) < 0 ? INT_ADD_RANGE_OVERFLOW (a, b, min, max)                  \
    : (a) < 0 ? (b) <= (a) + (b)                                         \
    : (b) < 0 ? (a) <= (a) + (b)                                         \
    : (a) + (b) < (b))
# define _GL_SUBTRACT_OVERFLOW(a, b, min, max)                           \
   ((min) < 0 ? INT_SUBTRACT_RANGE_OVERFLOW (a, b, min, max)             \
    : (a) < 0 ? 1                                                        \
    : (b) < 0 ? (a) - (b) <= (a)                                         \
    : (a) < (b))
# define _GL_MULTIPLY_OVERFLOW(a, b, min, max)                           \
   (((min) == 0 && (((a) < 0 && 0 < (b)) || ((b) < 0 && 0 < (a))))       \
    || INT_MULTIPLY_RANGE_OVERFLOW (a, b, min, max))
#endif
#define _GL_DIVIDE_OVERFLOW(a, b, min, max)                             \
  ((min) < 0 ? (b) == _GL_INT_NEGATE_CONVERT (min, 1) && (a) < - (max)  \
   : (a) < 0 ? (b) <= (a) + (b) - 1                                     \
   : (b) < 0 && (a) + (b) <= (a))
#define _GL_REMAINDER_OVERFLOW(a, b, min, max)                          \
  ((min) < 0 ? (b) == _GL_INT_NEGATE_CONVERT (min, 1) && (a) < - (max)  \
   : (a) < 0 ? (a) % (b) != ((max) - (b) + 1) % (b)                     \
   : (b) < 0 && ! _GL_UNSIGNED_NEG_MULTIPLE (a, b, max))

 
#define _GL_UNSIGNED_NEG_MULTIPLE(a, b, max)                            \
  (((b) < -_GL_SIGNED_INT_MAXIMUM (b)                                   \
    ? (_GL_SIGNED_INT_MAXIMUM (b) == (max)                              \
       ? (a)                                                            \
       : (a) % (_GL_INT_CONVERT (a, _GL_SIGNED_INT_MAXIMUM (b)) + 1))   \
    : (a) % - (b))                                                      \
   == 0)

 

#define INT_ADD_OVERFLOW(a, b) \
  _GL_BINARY_OP_OVERFLOW (a, b, _GL_ADD_OVERFLOW)
#define INT_SUBTRACT_OVERFLOW(a, b) \
  _GL_BINARY_OP_OVERFLOW (a, b, _GL_SUBTRACT_OVERFLOW)
#define INT_NEGATE_OVERFLOW(a) _GL_INT_NEGATE_OVERFLOW (a)
#define INT_MULTIPLY_OVERFLOW(a, b) \
  _GL_BINARY_OP_OVERFLOW (a, b, _GL_MULTIPLY_OVERFLOW)
#define INT_DIVIDE_OVERFLOW(a, b) \
  _GL_BINARY_OP_OVERFLOW (a, b, _GL_DIVIDE_OVERFLOW)
#define INT_REMAINDER_OVERFLOW(a, b) \
  _GL_BINARY_OP_OVERFLOW (a, b, _GL_REMAINDER_OVERFLOW)
#define INT_LEFT_SHIFT_OVERFLOW(a, b) \
  INT_LEFT_SHIFT_RANGE_OVERFLOW (a, b, \
                                 _GL_INT_MINIMUM (a), _GL_INT_MAXIMUM (a))

 
#define _GL_BINARY_OP_OVERFLOW(a, b, op_result_overflow)        \
  op_result_overflow (a, b,                                     \
                      _GL_INT_MINIMUM (_GL_INT_CONVERT (a, b)), \
                      _GL_INT_MAXIMUM (_GL_INT_CONVERT (a, b)))

 
#define INT_ADD_WRAPV(a, b, r) _GL_INT_ADD_WRAPV (a, b, r)
#define INT_SUBTRACT_WRAPV(a, b, r) _GL_INT_SUBTRACT_WRAPV (a, b, r)
#define INT_MULTIPLY_WRAPV(a, b, r) _GL_INT_MULTIPLY_WRAPV (a, b, r)

 

#define INT_ADD_OK(a, b, r) (! INT_ADD_WRAPV (a, b, r))
#define INT_SUBTRACT_OK(a, b, r) (! INT_SUBTRACT_WRAPV (a, b, r))
#define INT_MULTIPLY_OK(a, b, r) (! INT_MULTIPLY_WRAPV (a, b, r))

#endif  
