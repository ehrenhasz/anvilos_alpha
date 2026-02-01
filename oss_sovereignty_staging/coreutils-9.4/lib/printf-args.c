 

#ifndef PRINTF_FETCHARGS
# include <config.h>
#endif

 
#ifndef PRINTF_FETCHARGS
# include "printf-args.h"
#endif

 
#include <limits.h>

#ifdef STATIC
STATIC
#endif
int
PRINTF_FETCHARGS (va_list args, arguments *a)
{
  size_t i;
  argument *ap;

  for (i = 0, ap = &a->arg[0]; i < a->count; i++, ap++)
    switch (ap->type)
      {
      case TYPE_SCHAR:
        ap->a.a_schar = va_arg (args,   int);
        break;
      case TYPE_UCHAR:
        ap->a.a_uchar = va_arg (args,   int);
        break;
      case TYPE_SHORT:
        ap->a.a_short = va_arg (args,   int);
        break;
      case TYPE_USHORT:
        ap->a.a_ushort = va_arg (args,   int);
        break;
      case TYPE_INT:
        ap->a.a_int = va_arg (args, int);
        break;
      case TYPE_UINT:
        ap->a.a_uint = va_arg (args, unsigned int);
        break;
      case TYPE_LONGINT:
        ap->a.a_longint = va_arg (args, long int);
        break;
      case TYPE_ULONGINT:
        ap->a.a_ulongint = va_arg (args, unsigned long int);
        break;
      case TYPE_LONGLONGINT:
        ap->a.a_longlongint = va_arg (args, long long int);
        break;
      case TYPE_ULONGLONGINT:
        ap->a.a_ulonglongint = va_arg (args, unsigned long long int);
        break;
      case TYPE_INT8_T:
        #if INT8_WIDTH < INT_WIDTH
        ap->a.a_int8_t = va_arg (args,   int);
        #else
        ap->a.a_int8_t = va_arg (args, int8_t);
        #endif
        break;
      case TYPE_UINT8_T:
        #if UINT8_WIDTH < INT_WIDTH
        ap->a.a_uint8_t = va_arg (args,   int);
        #else
        ap->a.a_uint8_t = va_arg (args, uint8_t);
        #endif
        break;
      case TYPE_INT16_T:
        #if INT16_WIDTH < INT_WIDTH
        ap->a.a_int16_t = va_arg (args,   int);
        #else
        ap->a.a_int16_t = va_arg (args, int16_t);
        #endif
        break;
      case TYPE_UINT16_T:
        #if UINT16_WIDTH < INT_WIDTH
        ap->a.a_uint16_t = va_arg (args,   int);
        #else
        ap->a.a_uint16_t = va_arg (args, uint16_t);
        #endif
        break;
      case TYPE_INT32_T:
        #if INT32_WIDTH < INT_WIDTH
        ap->a.a_int32_t = va_arg (args,   int);
        #else
        ap->a.a_int32_t = va_arg (args, int32_t);
        #endif
        break;
      case TYPE_UINT32_T:
        #if UINT32_WIDTH < INT_WIDTH
        ap->a.a_uint32_t = va_arg (args,   int);
        #else
        ap->a.a_uint32_t = va_arg (args, uint32_t);
        #endif
        break;
      case TYPE_INT64_T:
        ap->a.a_int64_t = va_arg (args, int64_t);
        break;
      case TYPE_UINT64_T:
        ap->a.a_uint64_t = va_arg (args, uint64_t);
        break;
      case TYPE_INT_FAST8_T:
        #if INT_FAST8_WIDTH < INT_WIDTH
        ap->a.a_int_fast8_t = va_arg (args,   int);
        #else
        ap->a.a_int_fast8_t = va_arg (args, int_fast8_t);
        #endif
        break;
      case TYPE_UINT_FAST8_T:
        #if UINT_FAST8_WIDTH < INT_WIDTH
        ap->a.a_uint_fast8_t = va_arg (args,   int);
        #else
        ap->a.a_uint_fast8_t = va_arg (args, uint_fast8_t);
        #endif
        break;
      case TYPE_INT_FAST16_T:
        #if INT_FAST16_WIDTH < INT_WIDTH
        ap->a.a_int_fast16_t = va_arg (args,   int);
        #else
        ap->a.a_int_fast16_t = va_arg (args, int_fast16_t);
        #endif
        break;
      case TYPE_UINT_FAST16_T:
        #if UINT_FAST16_WIDTH < INT_WIDTH
        ap->a.a_uint_fast16_t = va_arg (args,   int);
        #else
        ap->a.a_uint_fast16_t = va_arg (args, uint_fast16_t);
        #endif
        break;
      case TYPE_INT_FAST32_T:
        #if INT_FAST32_WIDTH < INT_WIDTH
        ap->a.a_int_fast32_t = va_arg (args,   int);
        #else
        ap->a.a_int_fast32_t = va_arg (args, int_fast32_t);
        #endif
        break;
      case TYPE_UINT_FAST32_T:
        #if UINT_FAST32_WIDTH < INT_WIDTH
        ap->a.a_uint_fast32_t = va_arg (args,   int);
        #else
        ap->a.a_uint_fast32_t = va_arg (args, uint_fast32_t);
        #endif
        break;
      case TYPE_INT_FAST64_T:
        ap->a.a_int_fast64_t = va_arg (args, int_fast64_t);
        break;
      case TYPE_UINT_FAST64_T:
        ap->a.a_uint_fast64_t = va_arg (args, uint_fast64_t);
        break;
      case TYPE_DOUBLE:
        ap->a.a_double = va_arg (args, double);
        break;
      case TYPE_LONGDOUBLE:
        ap->a.a_longdouble = va_arg (args, long double);
        break;
      case TYPE_CHAR:
        ap->a.a_char = va_arg (args, int);
        break;
#if HAVE_WINT_T
      case TYPE_WIDE_CHAR:
         
        ap->a.a_wide_char =
          (sizeof (wint_t) < sizeof (int)
           ? (wint_t) va_arg (args, int)
           : va_arg (args, wint_t));
        break;
#endif
      case TYPE_STRING:
        ap->a.a_string = va_arg (args, const char *);
         
        if (ap->a.a_string == NULL)
          ap->a.a_string = "(NULL)";
        break;
#if HAVE_WCHAR_T
      case TYPE_WIDE_STRING:
        ap->a.a_wide_string = va_arg (args, const wchar_t *);
         
        if (ap->a.a_wide_string == NULL)
          {
            static const wchar_t wide_null_string[] =
              {
                (wchar_t)'(',
                (wchar_t)'N', (wchar_t)'U', (wchar_t)'L', (wchar_t)'L',
                (wchar_t)')',
                (wchar_t)0
              };
            ap->a.a_wide_string = wide_null_string;
          }
        break;
#endif
      case TYPE_POINTER:
        ap->a.a_pointer = va_arg (args, void *);
        break;
      case TYPE_COUNT_SCHAR_POINTER:
        ap->a.a_count_schar_pointer = va_arg (args, signed char *);
        break;
      case TYPE_COUNT_SHORT_POINTER:
        ap->a.a_count_short_pointer = va_arg (args, short *);
        break;
      case TYPE_COUNT_INT_POINTER:
        ap->a.a_count_int_pointer = va_arg (args, int *);
        break;
      case TYPE_COUNT_LONGINT_POINTER:
        ap->a.a_count_longint_pointer = va_arg (args, long int *);
        break;
      case TYPE_COUNT_LONGLONGINT_POINTER:
        ap->a.a_count_longlongint_pointer = va_arg (args, long long int *);
        break;
      case TYPE_COUNT_INT8_T_POINTER:
        ap->a.a_count_int8_t_pointer = va_arg (args, int8_t *);
        break;
      case TYPE_COUNT_INT16_T_POINTER:
        ap->a.a_count_int16_t_pointer = va_arg (args, int16_t *);
        break;
      case TYPE_COUNT_INT32_T_POINTER:
        ap->a.a_count_int32_t_pointer = va_arg (args, int32_t *);
        break;
      case TYPE_COUNT_INT64_T_POINTER:
        ap->a.a_count_int64_t_pointer = va_arg (args, int64_t *);
        break;
      case TYPE_COUNT_INT_FAST8_T_POINTER:
        ap->a.a_count_int_fast8_t_pointer = va_arg (args, int_fast8_t *);
        break;
      case TYPE_COUNT_INT_FAST16_T_POINTER:
        ap->a.a_count_int_fast16_t_pointer = va_arg (args, int_fast16_t *);
        break;
      case TYPE_COUNT_INT_FAST32_T_POINTER:
        ap->a.a_count_int_fast32_t_pointer = va_arg (args, int_fast32_t *);
        break;
      case TYPE_COUNT_INT_FAST64_T_POINTER:
        ap->a.a_count_int_fast64_t_pointer = va_arg (args, int_fast64_t *);
        break;
#if ENABLE_UNISTDIO
       
      case TYPE_U8_STRING:
        ap->a.a_u8_string = va_arg (args, const uint8_t *);
         
        if (ap->a.a_u8_string == NULL)
          {
            static const uint8_t u8_null_string[] =
              { '(', 'N', 'U', 'L', 'L', ')', 0 };
            ap->a.a_u8_string = u8_null_string;
          }
        break;
      case TYPE_U16_STRING:
        ap->a.a_u16_string = va_arg (args, const uint16_t *);
         
        if (ap->a.a_u16_string == NULL)
          {
            static const uint16_t u16_null_string[] =
              { '(', 'N', 'U', 'L', 'L', ')', 0 };
            ap->a.a_u16_string = u16_null_string;
          }
        break;
      case TYPE_U32_STRING:
        ap->a.a_u32_string = va_arg (args, const uint32_t *);
         
        if (ap->a.a_u32_string == NULL)
          {
            static const uint32_t u32_null_string[] =
              { '(', 'N', 'U', 'L', 'L', ')', 0 };
            ap->a.a_u32_string = u32_null_string;
          }
        break;
#endif
      default:
         
        return -1;
      }
  return 0;
}
