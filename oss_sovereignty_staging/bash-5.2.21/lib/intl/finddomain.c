 

 

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#if defined HAVE_UNISTD_H || defined _LIBC
# include <unistd.h>
#endif

#include "gettextP.h"
#ifdef _LIBC
# include <libintl.h>
#else
# include "libgnuintl.h"
#endif

 
 
static struct loaded_l10nfile *_nl_loaded_domains;


 
struct loaded_l10nfile *
internal_function
_nl_find_domain (dirname, locale, domainname, domainbinding)
     const char *dirname;
     char *locale;
     const char *domainname;
     struct binding *domainbinding;
{
  struct loaded_l10nfile *retval;
  const char *language;
  const char *modifier;
  const char *territory;
  const char *codeset;
  const char *normalized_codeset;
  const char *special;
  const char *sponsor;
  const char *revision;
  const char *alias_value;
  int mask;

   

   
  retval = _nl_make_l10nflist (&_nl_loaded_domains, dirname,
			       strlen (dirname) + 1, 0, locale, NULL, NULL,
			       NULL, NULL, NULL, NULL, NULL, domainname, 0);
  if (retval != NULL)
    {
       
      int cnt;

      if (retval->decided == 0)
	_nl_load_domain (retval, domainbinding);

      if (retval->data != NULL)
	return retval;

      for (cnt = 0; retval->successor[cnt] != NULL; ++cnt)
	{
	  if (retval->successor[cnt]->decided == 0)
	    _nl_load_domain (retval->successor[cnt], domainbinding);

	  if (retval->successor[cnt]->data != NULL)
	    break;
	}
      return cnt >= 0 ? retval : NULL;
       
    }

   
  alias_value = _nl_expand_alias (locale);
  if (alias_value != NULL)
    {
#if defined _LIBC || defined HAVE_STRDUP
      locale = strdup (alias_value);
      if (locale == NULL)
	return NULL;
#else
      size_t len = strlen (alias_value) + 1;
      locale = (char *) malloc (len);
      if (locale == NULL)
	return NULL;

      memcpy (locale, alias_value, len);
#endif
    }

   
  mask = _nl_explode_name (locale, &language, &modifier, &territory,
			   &codeset, &normalized_codeset, &special,
			   &sponsor, &revision);

   
  retval = _nl_make_l10nflist (&_nl_loaded_domains, dirname,
			       strlen (dirname) + 1, mask, language, territory,
			       codeset, normalized_codeset, modifier, special,
			       sponsor, revision, domainname, 1);
  if (retval == NULL)
     
    return NULL;

  if (retval->decided == 0)
    _nl_load_domain (retval, domainbinding);
  if (retval->data == NULL)
    {
      int cnt;
      for (cnt = 0; retval->successor[cnt] != NULL; ++cnt)
	{
	  if (retval->successor[cnt]->decided == 0)
	    _nl_load_domain (retval->successor[cnt], domainbinding);
	  if (retval->successor[cnt]->data != NULL)
	    break;
	}
    }

   
  if (alias_value != NULL)
    free (locale);

   
  if (mask & XPG_NORM_CODESET)
    free ((void *) normalized_codeset);

  return retval;
}


#ifdef _LIBC
libc_freeres_fn (free_mem)
{
  struct loaded_l10nfile *runp = _nl_loaded_domains;

  while (runp != NULL)
    {
      struct loaded_l10nfile *here = runp;
      if (runp->data != NULL)
	_nl_unload_domain ((struct loaded_domain *) runp->data);
      runp = runp->next;
      free ((char *) here->filename);
      free (here);
    }
}
#endif
