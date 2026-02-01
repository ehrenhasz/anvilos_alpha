 

#include <config.h>

#include "system.h"
#include "quote.h"
#include "set-fields.h"

 
struct field_range_pair *frp;

 
size_t n_frp;

 
static size_t n_frp_allocated;

#define FATAL_ERROR(Message)                                            \
  do                                                                    \
    {                                                                   \
      error (0, 0, (Message));                                          \
      usage (EXIT_FAILURE);                                             \
    }                                                                   \
  while (0)

 
static void
add_range_pair (uintmax_t lo, uintmax_t hi)
{
  if (n_frp == n_frp_allocated)
    frp = X2NREALLOC (frp, &n_frp_allocated);
  frp[n_frp].lo = lo;
  frp[n_frp].hi = hi;
  ++n_frp;
}


 
static int
compare_ranges (const void *a, const void *b)
{
  struct field_range_pair const *ap = a, *bp = b;
  return (ap->lo > bp->lo) - (ap->lo < bp->lo);
}

 

static void
complement_rp (void)
{
  struct field_range_pair *c = frp;
  size_t n = n_frp;

  frp = nullptr;
  n_frp = 0;
  n_frp_allocated = 0;

  if (c[0].lo > 1)
    add_range_pair (1, c[0].lo - 1);

  for (size_t i = 1; i < n; ++i)
    {
      if (c[i - 1].hi + 1 == c[i].lo)
        continue;

      add_range_pair (c[i - 1].hi + 1, c[i].lo - 1);
    }

  if (c[n - 1].hi < UINTMAX_MAX)
    add_range_pair (c[n - 1].hi + 1, UINTMAX_MAX);

  free (c);
}

 
void
set_fields (char const *fieldstr, unsigned int options)
{
  uintmax_t initial = 1;	 
  uintmax_t value = 0;		 
  bool lhs_specified = false;
  bool rhs_specified = false;
  bool dash_found = false;	 

  bool in_digits = false;

   

   
  if ((options & SETFLD_ALLOW_DASH) && STREQ (fieldstr,"-"))
    {
      value = 1;
      lhs_specified = true;
      dash_found = true;
      fieldstr++;
    }

  while (true)
    {
      if (*fieldstr == '-')
        {
          in_digits = false;
           
          if (dash_found)
            FATAL_ERROR ((options & SETFLD_ERRMSG_USE_POS)
                         ? _("invalid byte or character range")
                         : _("invalid field range"));

          dash_found = true;
          fieldstr++;

          if (lhs_specified && !value)
            FATAL_ERROR ((options & SETFLD_ERRMSG_USE_POS)
                         ? _("byte/character positions are numbered from 1")
                         : _("fields are numbered from 1"));

          initial = (lhs_specified ? value : 1);
          value = 0;
        }
      else if (*fieldstr == ','
               || isblank (to_uchar (*fieldstr)) || *fieldstr == '\0')
        {
          in_digits = false;
           
          if (dash_found)
            {
              dash_found = false;

              if (!lhs_specified && !rhs_specified)
                {
                   
                  if (options & SETFLD_ALLOW_DASH)
                    initial = 1;
                  else
                    FATAL_ERROR (_("invalid range with no endpoint: -"));
                }

               
              if (!rhs_specified)
                {
                   
                  add_range_pair (initial, UINTMAX_MAX);
                }
              else
                {
                   
                  if (value < initial)
                    FATAL_ERROR (_("invalid decreasing range"));

                  add_range_pair (initial, value);
                }
              value = 0;
            }
          else
            {
               
              if (value == 0)
                FATAL_ERROR ((options & SETFLD_ERRMSG_USE_POS)
                             ? _("byte/character positions are numbered from 1")
                             : _("fields are numbered from 1"));

              add_range_pair (value, value);
              value = 0;
            }

          if (*fieldstr == '\0')
            break;

          fieldstr++;
          lhs_specified = false;
          rhs_specified = false;
        }
      else if (ISDIGIT (*fieldstr))
        {
           
          static char const *num_start;
          if (!in_digits || !num_start)
            num_start = fieldstr;
          in_digits = true;

          if (dash_found)
            rhs_specified = 1;
          else
            lhs_specified = 1;

           
          if (!DECIMAL_DIGIT_ACCUMULATE (value, *fieldstr - '0', uintmax_t)
              || value == UINTMAX_MAX)
            {
               
               
              size_t len = strspn (num_start, "0123456789");
              char *bad_num = ximemdup0 (num_start, len);
              error (0, 0, (options & SETFLD_ERRMSG_USE_POS)
                           ?_("byte/character offset %s is too large")
                           :_("field number %s is too large"),
                           quote (bad_num));
              free (bad_num);
              usage (EXIT_FAILURE);
            }

          fieldstr++;
        }
      else
        {
          error (0, 0, (options & SETFLD_ERRMSG_USE_POS)
                       ?_("invalid byte/character position %s")
                       :_("invalid field value %s"),
                       quote (fieldstr));
          usage (EXIT_FAILURE);
        }
    }

  if (!n_frp)
    FATAL_ERROR ((options&SETFLD_ERRMSG_USE_POS)
                 ? _("missing list of byte/character positions")
                 : _("missing list of fields"));

  qsort (frp, n_frp, sizeof (frp[0]), compare_ranges);

   
  for (size_t i = 0; i < n_frp; ++i)
    {
      for (size_t j = i + 1; j < n_frp; ++j)
        {
          if (frp[j].lo <= frp[i].hi)
            {
              frp[i].hi = MAX (frp[j].hi, frp[i].hi);
              memmove (frp + j, frp + j + 1, (n_frp - j - 1) * sizeof *frp);
              n_frp--;
              j--;
            }
          else
            break;
        }
    }

  if (options & SETFLD_COMPLEMENT)
    complement_rp ();

   
  ++n_frp;
  frp = xrealloc (frp, n_frp * sizeof (struct field_range_pair));
  frp[n_frp - 1].lo = frp[n_frp - 1].hi = UINTMAX_MAX;
}
