 

#include <config.h>

 
#include "argmatch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _(msgid) gettext (msgid)

#include "error.h"
#include "quotearg.h"

#if USE_UNLOCKED_IO
# include "unlocked-io.h"
#endif

 
#ifndef ARGMATCH_QUOTING_STYLE
# define ARGMATCH_QUOTING_STYLE locale_quoting_style
#endif

 
#ifndef ARGMATCH_DIE
# include "exitfail.h"
# define ARGMATCH_DIE exit (exit_failure)
#endif

#ifdef ARGMATCH_DIE_DECL
ARGMATCH_DIE_DECL;
#endif

static void
__argmatch_die (void)
{
  ARGMATCH_DIE;
}

 
argmatch_exit_fn argmatch_die = __argmatch_die;


 

ptrdiff_t
argmatch (const char *arg, const char *const *arglist,
          const void *vallist, size_t valsize)
{
  size_t i;                      
  size_t arglen;                 
  ptrdiff_t matchind = -1;       
  bool ambiguous = false;        

  arglen = strlen (arg);

   
  for (i = 0; arglist[i]; i++)
    {
      if (!strncmp (arglist[i], arg, arglen))
        {
          if (strlen (arglist[i]) == arglen)
             
            return i;
          else if (matchind == -1)
             
            matchind = i;
          else
            {
               
              if (vallist == NULL
                  || memcmp ((char const *) vallist + valsize * matchind,
                             (char const *) vallist + valsize * i, valsize))
                {
                   
                  ambiguous = true;
                }
            }
        }
    }
  if (ambiguous)
    return -2;
  else
    return matchind;
}

ptrdiff_t
argmatch_exact (const char *arg, const char *const *arglist)
{
  size_t i;

   
  for (i = 0; arglist[i]; i++)
    {
      if (!strcmp (arglist[i], arg))
        return i;
    }

  return -1;
}

 

void
argmatch_invalid (const char *context, const char *value, ptrdiff_t problem)
{
  char const *format = (problem == -1
                        ? _("invalid argument %s for %s")
                        : _("ambiguous argument %s for %s"));

  error (0, 0, format, quotearg_n_style (0, ARGMATCH_QUOTING_STYLE, value),
         quote_n (1, context));
}

 
void
argmatch_valid (const char *const *arglist,
                const void *vallist, size_t valsize)
{
  size_t i;
  const char *last_val = NULL;

   
  fputs (_("Valid arguments are:"), stderr);
  for (i = 0; arglist[i]; i++)
    if ((i == 0)
        || memcmp (last_val, (char const *) vallist + valsize * i, valsize))
      {
        fprintf (stderr, "\n  - %s", quote (arglist[i]));
        last_val = (char const *) vallist + valsize * i;
      }
    else
      {
        fprintf (stderr, ", %s", quote (arglist[i]));
      }
  putc ('\n', stderr);
}

 

ptrdiff_t
__xargmatch_internal (const char *context,
                      const char *arg, const char *const *arglist,
                      const void *vallist, size_t valsize,
                      argmatch_exit_fn exit_fn,
                      bool allow_abbreviation)
{
  ptrdiff_t res;

  if (allow_abbreviation)
    res = argmatch (arg, arglist, vallist, valsize);
  else
    res = argmatch_exact (arg, arglist);

  if (res >= 0)
     
    return res;

   
  argmatch_invalid (context, arg, res);
  argmatch_valid (arglist, vallist, valsize);
  (*exit_fn) ();

  return -1;  
}

 
const char *
argmatch_to_argument (const void *value,
                      const char *const *arglist,
                      const void *vallist, size_t valsize)
{
  size_t i;

  for (i = 0; arglist[i]; i++)
    if (!memcmp (value, (char const *) vallist + valsize * i, valsize))
      return arglist[i];
  return NULL;
}

#ifdef TEST
 

 
enum backup_type
{
   
  no_backups,

   
  simple_backups,

   
  numbered_existing_backups,

   
  numbered_backups
};

 
static const char *const backup_args[] =
{
  "no", "none", "off",
  "simple", "never",
  "existing", "nil",
  "numbered", "t",
  0
};

static const enum backup_type backup_vals[] =
{
  no_backups, no_backups, no_backups,
  simple_backups, simple_backups,
  numbered_existing_backups, numbered_existing_backups,
  numbered_backups, numbered_backups
};

int
main (int argc, const char *const *argv)
{
  const char *cp;
  enum backup_type backup_type = no_backups;

  if (argc > 2)
    {
      fprintf (stderr, "Usage: %s [VERSION_CONTROL]\n", getprogname ());
      exit (1);
    }

  if ((cp = getenv ("VERSION_CONTROL")))
    backup_type = XARGMATCH ("$VERSION_CONTROL", cp,
                             backup_args, backup_vals);

  if (argc == 2)
    backup_type = XARGMATCH (getprogname (), argv[1],
                             backup_args, backup_vals);

  printf ("The version control is '%s'\n",
          ARGMATCH_TO_ARGUMENT (&backup_type, backup_args, backup_vals));

  return 0;
}
#endif
