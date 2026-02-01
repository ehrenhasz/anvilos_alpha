 
#include <locale.h>

#include <limits.h>

#if HAVE_STRUCT_LCONV_DECIMAL_POINT

# define FIX_CHAR_VALUE(x) ((x) >= 0 ? (x) : CHAR_MAX)

 

struct lconv *
localeconv (void)
{
  static struct lconv result;
# undef lconv
# undef localeconv
  struct lconv *sys_result = localeconv ();

  result.decimal_point = sys_result->decimal_point;
  result.thousands_sep = sys_result->thousands_sep;
  result.grouping = sys_result->grouping;
  result.mon_decimal_point = sys_result->mon_decimal_point;
  result.mon_thousands_sep = sys_result->mon_thousands_sep;
  result.mon_grouping = sys_result->mon_grouping;
  result.positive_sign = sys_result->positive_sign;
  result.negative_sign = sys_result->negative_sign;
  result.currency_symbol = sys_result->currency_symbol;
  result.frac_digits = FIX_CHAR_VALUE (sys_result->frac_digits);
  result.p_cs_precedes = FIX_CHAR_VALUE (sys_result->p_cs_precedes);
  result.p_sign_posn = FIX_CHAR_VALUE (sys_result->p_sign_posn);
  result.p_sep_by_space = FIX_CHAR_VALUE (sys_result->p_sep_by_space);
  result.n_cs_precedes = FIX_CHAR_VALUE (sys_result->n_cs_precedes);
  result.n_sign_posn = FIX_CHAR_VALUE (sys_result->n_sign_posn);
  result.n_sep_by_space = FIX_CHAR_VALUE (sys_result->n_sep_by_space);
  result.int_curr_symbol = sys_result->int_curr_symbol;
  result.int_frac_digits = FIX_CHAR_VALUE (sys_result->int_frac_digits);
# if HAVE_STRUCT_LCONV_INT_P_CS_PRECEDES
  result.int_p_cs_precedes = FIX_CHAR_VALUE (sys_result->int_p_cs_precedes);
  result.int_p_sign_posn = FIX_CHAR_VALUE (sys_result->int_p_sign_posn);
  result.int_p_sep_by_space = FIX_CHAR_VALUE (sys_result->int_p_sep_by_space);
  result.int_n_cs_precedes = FIX_CHAR_VALUE (sys_result->int_n_cs_precedes);
  result.int_n_sign_posn = FIX_CHAR_VALUE (sys_result->int_n_sign_posn);
  result.int_n_sep_by_space = FIX_CHAR_VALUE (sys_result->int_n_sep_by_space);
# else
  result.int_p_cs_precedes = FIX_CHAR_VALUE (sys_result->p_cs_precedes);
  result.int_p_sign_posn = FIX_CHAR_VALUE (sys_result->p_sign_posn);
  result.int_p_sep_by_space = FIX_CHAR_VALUE (sys_result->p_sep_by_space);
  result.int_n_cs_precedes = FIX_CHAR_VALUE (sys_result->n_cs_precedes);
  result.int_n_sign_posn = FIX_CHAR_VALUE (sys_result->n_sign_posn);
  result.int_n_sep_by_space = FIX_CHAR_VALUE (sys_result->n_sep_by_space);
# endif

  return &result;
}

#else

 

struct lconv *
localeconv (void)
{
  static   struct lconv result =
    {
        ".",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        CHAR_MAX,
        CHAR_MAX,
        CHAR_MAX,
        CHAR_MAX,
        CHAR_MAX,
        CHAR_MAX,
        CHAR_MAX,
        "",
        CHAR_MAX,
        CHAR_MAX,
        CHAR_MAX,
        CHAR_MAX,
        CHAR_MAX,
        CHAR_MAX,
        CHAR_MAX
    };

  return &result;
}

#endif
