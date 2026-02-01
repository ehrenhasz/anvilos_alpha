 

#include <config.h>

#include <locale.h>

#include "signature.h"
SIGNATURE_CHECK (localeconv, struct lconv *, (void));

#include <limits.h>
#include <string.h>

#include "macros.h"

int
main ()
{
   
  {
    struct lconv *l = localeconv ();

    ASSERT (STREQ (l->decimal_point, "."));
    ASSERT (STREQ (l->thousands_sep, ""));
#if !((defined __FreeBSD__ || defined __DragonFly__) || defined __sun || defined __CYGWIN__)
    ASSERT (STREQ (l->grouping, ""));
#endif

    ASSERT (STREQ (l->mon_decimal_point, ""));
    ASSERT (STREQ (l->mon_thousands_sep, ""));
#if !((defined __FreeBSD__ || defined __DragonFly__) || defined __sun || defined __CYGWIN__)
    ASSERT (STREQ (l->mon_grouping, ""));
#endif
    ASSERT (STREQ (l->positive_sign, ""));
    ASSERT (STREQ (l->negative_sign, ""));

    ASSERT (STREQ (l->currency_symbol, ""));
    ASSERT (l->frac_digits == CHAR_MAX);
    ASSERT (l->p_cs_precedes == CHAR_MAX);
    ASSERT (l->p_sign_posn == CHAR_MAX);
    ASSERT (l->p_sep_by_space == CHAR_MAX);
    ASSERT (l->n_cs_precedes == CHAR_MAX);
    ASSERT (l->n_sign_posn == CHAR_MAX);
    ASSERT (l->n_sep_by_space == CHAR_MAX);

    ASSERT (STREQ (l->int_curr_symbol, ""));
    ASSERT (l->int_frac_digits == CHAR_MAX);
    ASSERT (l->int_p_cs_precedes == CHAR_MAX);
    ASSERT (l->int_p_sign_posn == CHAR_MAX);
    ASSERT (l->int_p_sep_by_space == CHAR_MAX);
    ASSERT (l->int_n_cs_precedes == CHAR_MAX);
    ASSERT (l->int_n_sign_posn == CHAR_MAX);
    ASSERT (l->int_n_sep_by_space == CHAR_MAX);
  }

  return 0;
}
