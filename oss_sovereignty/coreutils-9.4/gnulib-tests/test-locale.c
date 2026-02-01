 

#include <config.h>

#include <locale.h>

int a[] =
  {
    LC_ALL,
    LC_COLLATE,
    LC_CTYPE,
    LC_MESSAGES,
    LC_MONETARY,
    LC_NUMERIC,
    LC_TIME
  };

 
struct lconv l;
int ls;

 
static_assert (sizeof NULL == sizeof (void *));

int
main ()
{
#if HAVE_WORKING_NEWLOCALE
   
  locale_t b = LC_GLOBAL_LOCALE;
  (void) b;
#endif

   
  ls += sizeof (*l.decimal_point);
  ls += sizeof (*l.thousands_sep);
  ls += sizeof (*l.grouping);
  ls += sizeof (*l.mon_decimal_point);
  ls += sizeof (*l.mon_thousands_sep);
  ls += sizeof (*l.mon_grouping);
  ls += sizeof (*l.positive_sign);
  ls += sizeof (*l.negative_sign);
  ls += sizeof (*l.currency_symbol);
  ls += sizeof (l.frac_digits);
  ls += sizeof (l.p_cs_precedes);
  ls += sizeof (l.p_sign_posn);
  ls += sizeof (l.p_sep_by_space);
  ls += sizeof (l.n_cs_precedes);
  ls += sizeof (l.n_sign_posn);
  ls += sizeof (l.n_sep_by_space);
  ls += sizeof (*l.int_curr_symbol);
  ls += sizeof (l.int_frac_digits);
  ls += sizeof (l.int_p_cs_precedes);
  ls += sizeof (l.int_p_sign_posn);
  ls += sizeof (l.int_p_sep_by_space);
  ls += sizeof (l.int_n_cs_precedes);
  ls += sizeof (l.int_n_sign_posn);
  ls += sizeof (l.int_n_sep_by_space);

  return 0;
}
