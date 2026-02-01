 

 

#include "config.h"

#include "bashtypes.h"

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#if HAVE_LANGINFO_CODESET
#  include <langinfo.h>
#endif

#include "bashintl.h"
#include "bashansi.h"
#include <stdio.h>
#include "chartypes.h"
#include <errno.h>

#include "shell.h"
#include "input.h"	 

#ifndef errno
extern int errno;
#endif

int locale_utf8locale;
int locale_mb_cur_max;	 
int locale_shiftstates = 0;

int singlequote_translations = 0;	 

extern int dump_translatable_strings, dump_po_strings;

 
static char *default_locale;

 
static char *default_domain;
static char *default_dir;

 
static char *lc_all;

 
static char *lang;

 
static int reset_locale_vars PARAMS((void));

static void locale_setblanks PARAMS((void));
static int locale_isutf8 PARAMS((char *));

 
void
set_default_locale ()
{
#if defined (HAVE_SETLOCALE)
  default_locale = setlocale (LC_ALL, "");
  if (default_locale)
    default_locale = savestring (default_locale);
#else
  default_locale = savestring ("C");
#endif  
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  locale_mb_cur_max = MB_CUR_MAX;
  locale_utf8locale = locale_isutf8 (default_locale);
#if defined (HANDLE_MULTIBYTE)
  locale_shiftstates = mblen ((char *)NULL, 0);
#else
  locale_shiftstates = 0;
#endif
}

 
void
set_default_locale_vars ()
{
  char *val;

#if defined (HAVE_SETLOCALE)

#  if defined (LC_CTYPE)
  val = get_string_value ("LC_CTYPE");
  if (val == 0 && lc_all && *lc_all)
    {
      setlocale (LC_CTYPE, lc_all);
      locale_setblanks ();
      locale_mb_cur_max = MB_CUR_MAX;
      locale_utf8locale = locale_isutf8 (lc_all);

#    if defined (HANDLE_MULTIBYTE)
      locale_shiftstates = mblen ((char *)NULL, 0);
#    else
      locale_shiftstates = 0;
#    endif

      u32reset ();
    }
#  endif

#  if defined (LC_COLLATE)
  val = get_string_value ("LC_COLLATE");
  if (val == 0 && lc_all && *lc_all)
    setlocale (LC_COLLATE, lc_all);
#  endif  

#  if defined (LC_MESSAGES)
  val = get_string_value ("LC_MESSAGES");
  if (val == 0 && lc_all && *lc_all)
    setlocale (LC_MESSAGES, lc_all);
#  endif  

#  if defined (LC_NUMERIC)
  val = get_string_value ("LC_NUMERIC");
  if (val == 0 && lc_all && *lc_all)
    setlocale (LC_NUMERIC, lc_all);
#  endif  

#  if defined (LC_TIME)
  val = get_string_value ("LC_TIME");
  if (val == 0 && lc_all && *lc_all)
    setlocale (LC_TIME, lc_all);
#  endif  

#endif  

  val = get_string_value ("TEXTDOMAIN");
  if (val && *val)
    {
      FREE (default_domain);
      default_domain = savestring (val);
      if (default_dir && *default_dir)
	bindtextdomain (default_domain, default_dir);
    }

  val = get_string_value ("TEXTDOMAINDIR");
  if (val && *val)
    {
      FREE (default_dir);
      default_dir = savestring (val);
      if (default_domain && *default_domain)
	bindtextdomain (default_domain, default_dir);
    }
}

 
int
set_locale_var (var, value)
     char *var, *value;
{
  int r;
  char *x;

  x = "";
  errno = 0;
  if (var[0] == 'T' && var[10] == 0)		 
    {
      FREE (default_domain);
      default_domain = value ? savestring (value) : (char *)NULL;
      if (default_dir && *default_dir)
	bindtextdomain (default_domain, default_dir);
      return (1);
    }
  else if (var[0] == 'T')			 
    {
      FREE (default_dir);
      default_dir = value ? savestring (value) : (char *)NULL;
      if (default_domain && *default_domain)
	bindtextdomain (default_domain, default_dir);
      return (1);
    }

   

  else if (var[3] == 'A')			 
    {
      FREE (lc_all);
      if (value)
	lc_all = savestring (value);
      else
	{
	  lc_all = (char *)xmalloc (1);
	  lc_all[0] = '\0';
	}
#if defined (HAVE_SETLOCALE)
      r = *lc_all ? ((x = setlocale (LC_ALL, lc_all)) != 0) : reset_locale_vars ();
      if (x == 0)
	{
	  if (errno == 0)
	    internal_warning(_("setlocale: LC_ALL: cannot change locale (%s)"), lc_all);
	  else
	    internal_warning(_("setlocale: LC_ALL: cannot change locale (%s): %s"), lc_all, strerror (errno));
	}
      locale_setblanks ();
      locale_mb_cur_max = MB_CUR_MAX;
       
      if (*lc_all && x)
	locale_utf8locale = locale_isutf8 (lc_all);
#  if defined (HANDLE_MULTIBYTE)
      locale_shiftstates = mblen ((char *)NULL, 0);
#  else
      locale_shiftstates = 0;
#  endif
      u32reset ();
      return r;
#else
      return (1);
#endif
    }

#if defined (HAVE_SETLOCALE)
  else if (var[3] == 'C' && var[4] == 'T')	 
    {
#  if defined (LC_CTYPE)
      if (lc_all == 0 || *lc_all == '\0')
	{
	  x = setlocale (LC_CTYPE, get_locale_var ("LC_CTYPE"));
	  locale_setblanks ();
	  locale_mb_cur_max = MB_CUR_MAX;
	   
	  if (x)
	    locale_utf8locale = locale_isutf8 (x);
#if defined (HANDLE_MULTIBYTE)
	  locale_shiftstates = mblen ((char *)NULL, 0);
#else
	  locale_shiftstates = 0;
#endif
	  u32reset ();
	}
#  endif
    }
  else if (var[3] == 'C' && var[4] == 'O')	 
    {
#  if defined (LC_COLLATE)
      if (lc_all == 0 || *lc_all == '\0')
	x = setlocale (LC_COLLATE, get_locale_var ("LC_COLLATE"));
#  endif  
    }
  else if (var[3] == 'M' && var[4] == 'E')	 
    {
#  if defined (LC_MESSAGES)
      if (lc_all == 0 || *lc_all == '\0')
	x = setlocale (LC_MESSAGES, get_locale_var ("LC_MESSAGES"));
#  endif  
    }
  else if (var[3] == 'N' && var[4] == 'U')	 
    {
#  if defined (LC_NUMERIC)
      if (lc_all == 0 || *lc_all == '\0')
	x = setlocale (LC_NUMERIC, get_locale_var ("LC_NUMERIC"));
#  endif  
    }
  else if (var[3] == 'T' && var[4] == 'I')	 
    {
#  if defined (LC_TIME)
      if (lc_all == 0 || *lc_all == '\0')
	x = setlocale (LC_TIME, get_locale_var ("LC_TIME"));
#  endif  
    }
#endif  
  
  if (x == 0)
    {
      if (errno == 0)
	internal_warning(_("setlocale: %s: cannot change locale (%s)"), var, get_locale_var (var));
      else
	internal_warning(_("setlocale: %s: cannot change locale (%s): %s"), var, get_locale_var (var), strerror (errno));
    }

  return (x != 0);
}

 
int
set_lang (var, value)
     char *var, *value;
{
  FREE (lang);
  if (value)
    lang = savestring (value);
  else
    {
      lang = (char *)xmalloc (1);
      lang[0] = '\0';
    }

  return ((lc_all == 0 || *lc_all == 0) ? reset_locale_vars () : 0);
}

 
void
set_default_lang ()
{
  char *v;

  v = get_string_value ("LC_ALL");
  set_locale_var ("LC_ALL", v);

  v = get_string_value ("LANG");
  set_lang ("LANG", v);
}

 
char *
get_locale_var (var)
     char *var;
{
  char *locale;

  locale = lc_all;

  if (locale == 0 || *locale == 0)
    locale = get_string_value (var);	 
  if (locale == 0 || *locale == 0)
    locale = lang;
  if (locale == 0 || *locale == 0)
#if 0
    locale = default_locale;	 
#else
    locale = "";
#endif
  return (locale);
}

 
static int
reset_locale_vars ()
{
  char *t, *x;
#if defined (HAVE_SETLOCALE)
  if (lang == 0 || *lang == '\0')
    maybe_make_export_env ();		 
  if (setlocale (LC_ALL, lang ? lang : "") == 0)
    return 0;

  x = 0;
#  if defined (LC_CTYPE)
  x = setlocale (LC_CTYPE, get_locale_var ("LC_CTYPE"));
#  endif
#  if defined (LC_COLLATE)
  t = setlocale (LC_COLLATE, get_locale_var ("LC_COLLATE"));
#  endif
#  if defined (LC_MESSAGES)
  t = setlocale (LC_MESSAGES, get_locale_var ("LC_MESSAGES"));
#  endif
#  if defined (LC_NUMERIC)
  t = setlocale (LC_NUMERIC, get_locale_var ("LC_NUMERIC"));
#  endif
#  if defined (LC_TIME)
  t = setlocale (LC_TIME, get_locale_var ("LC_TIME"));
#  endif

  locale_setblanks ();  
  locale_mb_cur_max = MB_CUR_MAX;
  if (x)
    locale_utf8locale = locale_isutf8 (x);
#  if defined (HANDLE_MULTIBYTE)
  locale_shiftstates = mblen ((char *)NULL, 0);
#  else
  locale_shiftstates = 0;
#  endif
  u32reset ();
#endif
  return 1;
}

#if defined (TRANSLATABLE_STRINGS)
 
char *
localetrans (string, len, lenp)
     char *string;
     int len, *lenp;
{
  char *locale, *t;
  char *translated;
  int tlen;

   
  if (string == 0 || *string == 0)
    {
      if (lenp)
	*lenp = 0;
      return ((char *)NULL);
    }

  locale = get_locale_var ("LC_MESSAGES");

   
  if (locale == 0 || locale[0] == '\0' ||
      (locale[0] == 'C' && locale[1] == '\0') || STREQ (locale, "POSIX"))
    {
      t = (char *)xmalloc (len + 1);
      strcpy (t, string);
      if (lenp)
	*lenp = len;
      return (t);
    }

   
  if (default_domain && *default_domain)
    translated = dgettext (default_domain, string);
  else
    translated = string;

  if (translated == string)	 
    {
      t = (char *)xmalloc (len + 1);
      strcpy (t, string);
      if (lenp)
	*lenp = len;
    }
  else
    {
      tlen = strlen (translated);
      t = (char *)xmalloc (tlen + 1);
      strcpy (t, translated);
      if (lenp)
	*lenp = tlen;
    }
  return (t);
}

 
char *
mk_msgstr (string, foundnlp)
     char *string;
     int *foundnlp;
{
  register int c, len;
  char *result, *r, *s;

  for (len = 0, s = string; s && *s; s++)
    {
      len++;
      if (*s == '"' || *s == '\\')
	len++;
      else if (*s == '\n')
	len += 5;
    }
  
  r = result = (char *)xmalloc (len + 3);
  *r++ = '"';

  for (s = string; s && (c = *s); s++)
    {
      if (c == '\n')	 
	{
	  *r++ = '\\';
	  *r++ = 'n';
	  *r++ = '"';
	  *r++ = '\n';
	  *r++ = '"';
	  if (foundnlp)
	    *foundnlp = 1;
	  continue;
	}
      if (c == '"' || c == '\\')
	*r++ = '\\';
      *r++ = c;
    }

  *r++ = '"';
  *r++ = '\0';

  return result;
}

 
char *
locale_expand (string, start, end, lineno, lenp)
     char *string;
     int start, end, lineno, *lenp;
{
  int len, tlen, foundnl;
  char *temp, *t, *t2;

  temp = (char *)xmalloc (end - start + 1);
  for (tlen = 0, len = start; len < end; )
    temp[tlen++] = string[len++];
  temp[tlen] = '\0';

   
  if (dump_translatable_strings)
    {
      if (dump_po_strings)
	{
	  foundnl = 0;
	  t = mk_msgstr (temp, &foundnl);
	  t2 = foundnl ? "\"\"\n" : "";

	  printf ("#: %s:%d\nmsgid %s%s\nmsgstr \"\"\n",
			yy_input_name (), lineno, t2, t);
	  free (t);
	}
      else
	printf ("\"%s\"\n", temp);

      if (lenp)
	*lenp = tlen;
      return (temp);
    }
  else if (*temp)
    {
      t = localetrans (temp, tlen, &len);
      free (temp);
      if (lenp)
	*lenp = len;
      return (t);
    }
  else
    {
      if (lenp)
	*lenp = 0;
      return (temp);
    }
}
#endif

 
static void
locale_setblanks ()
{
  int x;

  for (x = 0; x < sh_syntabsiz; x++)
    {
      if (isblank ((unsigned char)x))
	sh_syntaxtab[x] |= CSHBRK|CBLANK;
      else if (member (x, shell_break_chars))
	{
	  sh_syntaxtab[x] |= CSHBRK;
	  sh_syntaxtab[x] &= ~CBLANK;
	}
      else
	sh_syntaxtab[x] &= ~(CSHBRK|CBLANK);
    }
}

 
static int
locale_isutf8 (lspec)
     char *lspec;
{
  char *cp, *encoding;

#if HAVE_LANGINFO_CODESET
  cp = nl_langinfo (CODESET);
  return (STREQ (cp, "UTF-8") || STREQ (cp, "utf8"));
#elif HAVE_LOCALE_CHARSET
  cp = locale_charset ();
  return (STREQ (cp, "UTF-8") || STREQ (cp, "utf8"));
#else
   
  for (cp = lspec; *cp && *cp != '@' && *cp != '+' && *cp != ','; cp++)
    {
      if (*cp == '.')
	{
	  for (encoding = ++cp; *cp && *cp != '@' && *cp != '+' && *cp != ','; cp++)
	    ;
	   
	  if ((cp - encoding == 5 && STREQN (encoding, "UTF-8", 5)) ||
	      (cp - encoding == 4 && STREQN (encoding, "utf8", 4)))
	    return 1;
	  else
	    return 0;
	}
    }
  return 0;
#endif
}

#if defined (HAVE_LOCALECONV)
int
locale_decpoint ()
{
  struct lconv *lv;

  lv = localeconv ();
  return (lv && lv->decimal_point && lv->decimal_point[0]) ? lv->decimal_point[0] : '.';
}
#else
#  undef locale_decpoint
int
locale_decpoint ()
{
  return '.';
}
#endif
