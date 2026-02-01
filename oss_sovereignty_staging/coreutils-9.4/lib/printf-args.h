 

 
#ifndef PRINTF_FETCHARGS
# define PRINTF_FETCHARGS printf_fetchargs
#endif

 
#include <stddef.h>

 
#if HAVE_WCHAR_T
# include <stddef.h>
#endif

 
#if HAVE_WINT_T
# include <wchar.h>
#endif

 
#include <stdint.h>

 
#include <stdarg.h>


 
typedef enum
{
  TYPE_NONE,
  TYPE_SCHAR,
  TYPE_UCHAR,
  TYPE_SHORT,
  TYPE_USHORT,
  TYPE_INT,
  TYPE_UINT,
  TYPE_LONGINT,
  TYPE_ULONGINT,
  TYPE_LONGLONGINT,
  TYPE_ULONGLONGINT,
   
  TYPE_INT8_T,
  TYPE_UINT8_T,
  TYPE_INT16_T,
  TYPE_UINT16_T,
  TYPE_INT32_T,
  TYPE_UINT32_T,
  TYPE_INT64_T,
  TYPE_UINT64_T,
  TYPE_INT_FAST8_T,
  TYPE_UINT_FAST8_T,
  TYPE_INT_FAST16_T,
  TYPE_UINT_FAST16_T,
  TYPE_INT_FAST32_T,
  TYPE_UINT_FAST32_T,
  TYPE_INT_FAST64_T,
  TYPE_UINT_FAST64_T,
  TYPE_DOUBLE,
  TYPE_LONGDOUBLE,
  TYPE_CHAR,
#if HAVE_WINT_T
  TYPE_WIDE_CHAR,
#endif
  TYPE_STRING,
#if HAVE_WCHAR_T
  TYPE_WIDE_STRING,
#endif
  TYPE_POINTER,
  TYPE_COUNT_SCHAR_POINTER,
  TYPE_COUNT_SHORT_POINTER,
  TYPE_COUNT_INT_POINTER,
  TYPE_COUNT_LONGINT_POINTER,
  TYPE_COUNT_LONGLONGINT_POINTER,
  TYPE_COUNT_INT8_T_POINTER,
  TYPE_COUNT_INT16_T_POINTER,
  TYPE_COUNT_INT32_T_POINTER,
  TYPE_COUNT_INT64_T_POINTER,
  TYPE_COUNT_INT_FAST8_T_POINTER,
  TYPE_COUNT_INT_FAST16_T_POINTER,
  TYPE_COUNT_INT_FAST32_T_POINTER,
  TYPE_COUNT_INT_FAST64_T_POINTER
#if ENABLE_UNISTDIO
   
, TYPE_U8_STRING
, TYPE_U16_STRING
, TYPE_U32_STRING
#endif
} arg_type;

 
typedef struct
{
  arg_type type;
  union
  {
    signed char                 a_schar;
    unsigned char               a_uchar;
    short                       a_short;
    unsigned short              a_ushort;
    int                         a_int;
    unsigned int                a_uint;
    long int                    a_longint;
    unsigned long int           a_ulongint;
    long long int               a_longlongint;
    unsigned long long int      a_ulonglongint;
    int8_t                      a_int8_t;
    uint8_t                     a_uint8_t;
    int16_t                     a_int16_t;
    uint16_t                    a_uint16_t;
    int32_t                     a_int32_t;
    uint32_t                    a_uint32_t;
    int64_t                     a_int64_t;
    uint64_t                    a_uint64_t;
    int_fast8_t                 a_int_fast8_t;
    uint_fast8_t                a_uint_fast8_t;
    int_fast16_t                a_int_fast16_t;
    uint_fast16_t               a_uint_fast16_t;
    int_fast32_t                a_int_fast32_t;
    uint_fast32_t               a_uint_fast32_t;
    int_fast64_t                a_int_fast64_t;
    uint_fast64_t               a_uint_fast64_t;
    float                       a_float;                      
    double                      a_double;
    long double                 a_longdouble;
    int                         a_char;
#if HAVE_WINT_T
    wint_t                      a_wide_char;
#endif
    const char*                 a_string;
#if HAVE_WCHAR_T
    const wchar_t*              a_wide_string;
#endif
    void*                       a_pointer;
    signed char *               a_count_schar_pointer;
    short *                     a_count_short_pointer;
    int *                       a_count_int_pointer;
    long int *                  a_count_longint_pointer;
    long long int *             a_count_longlongint_pointer;
    int8_t *                    a_count_int8_t_pointer;
    int16_t *                   a_count_int16_t_pointer;
    int32_t *                   a_count_int32_t_pointer;
    int64_t *                   a_count_int64_t_pointer;
    int_fast8_t *               a_count_int_fast8_t_pointer;
    int_fast16_t *              a_count_int_fast16_t_pointer;
    int_fast32_t *              a_count_int_fast32_t_pointer;
    int_fast64_t *              a_count_int_fast64_t_pointer;
#if ENABLE_UNISTDIO
     
    const uint8_t *             a_u8_string;
    const uint16_t *            a_u16_string;
    const uint32_t *            a_u32_string;
#endif
  }
  a;
}
argument;

 
#define N_DIRECT_ALLOC_ARGUMENTS 7

typedef struct
{
  size_t count;
  argument *arg;
  argument direct_alloc_arg[N_DIRECT_ALLOC_ARGUMENTS];
}
arguments;


 
#ifdef STATIC
STATIC
#else
extern
#endif
int PRINTF_FETCHARGS (va_list args, arguments *a);

#endif  
