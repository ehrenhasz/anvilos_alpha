 

 

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#if defined (HAVE_STRING_H)
#  include <string.h>
#else  
#  include <strings.h>
#endif    

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif  

#include <sys/types.h>
#if defined (HAVE_PWD_H)
#include <pwd.h>
#endif

#include "tilde.h"

#if defined (TEST) || defined (STATIC_MALLOC)
static void *xmalloc (), *xrealloc ();
#else
#  include "xmalloc.h"
#endif  

#if !defined (HAVE_GETPW_DECLS)
#  if defined (HAVE_GETPWUID)
extern struct passwd *getpwuid (uid_t);
#  endif
#  if defined (HAVE_GETPWNAM)
extern struct passwd *getpwnam (const char *);
#  endif
#endif  

#if !defined (savestring)
#define savestring(x) strcpy ((char *)xmalloc (1 + strlen (x)), (x))
#endif  

#if !defined (NULL)
#  if defined (__STDC__)
#    define NULL ((void *) 0)
#  else
#    define NULL 0x0
#  endif  
#endif  

 
extern char *sh_get_home_dir (void);
extern char *sh_get_env_value (const char *);

 
static const char *default_prefixes[] =
  { " ~", "\t~", (const char *)NULL };

 
static const char *default_suffixes[] =
  { " ", "\n", (const char *)NULL };

 
tilde_hook_func_t *tilde_expansion_preexpansion_hook = (tilde_hook_func_t *)NULL;

 
tilde_hook_func_t *tilde_expansion_failure_hook = (tilde_hook_func_t *)NULL;

 
char **tilde_additional_prefixes = (char **)default_prefixes;

 
char **tilde_additional_suffixes = (char **)default_suffixes;

static int tilde_find_prefix (const char *, int *);
static int tilde_find_suffix (const char *);
static char *isolate_tilde_prefix (const char *, int *);
static char *glue_prefix_and_suffix (char *, const char *, int);

 
static int
tilde_find_prefix (const char *string, int *len)
{
  register int i, j, string_len;
  register char **prefixes;

  prefixes = tilde_additional_prefixes;

  string_len = strlen (string);
  *len = 0;

  if (*string == '\0' || *string == '~')
    return (0);

  if (prefixes)
    {
      for (i = 0; i < string_len; i++)
	{
	  for (j = 0; prefixes[j]; j++)
	    {
	      if (strncmp (string + i, prefixes[j], strlen (prefixes[j])) == 0)
		{
		  *len = strlen (prefixes[j]) - 1;
		  return (i + *len);
		}
	    }
	}
    }
  return (string_len);
}

 
static int
tilde_find_suffix (const char *string)
{
  register int i, j, string_len;
  register char **suffixes;

  suffixes = tilde_additional_suffixes;
  string_len = strlen (string);

  for (i = 0; i < string_len; i++)
    {
#if defined (__MSDOS__)
      if (string[i] == '/' || string[i] == '\\'  )
#else
      if (string[i] == '/'  )
#endif
	break;

      for (j = 0; suffixes && suffixes[j]; j++)
	{
	  if (strncmp (string + i, suffixes[j], strlen (suffixes[j])) == 0)
	    return (i);
	}
    }
  return (i);
}

 
char *
tilde_expand (const char *string)
{
  char *result;
  int result_size, result_index;

  result_index = result_size = 0;
  if (result = strchr (string, '~'))
    result = (char *)xmalloc (result_size = (strlen (string) + 16));
  else
    result = (char *)xmalloc (result_size = (strlen (string) + 1));

   
  while (1)
    {
      register int start, end;
      char *tilde_word, *expansion;
      int len;

       
      start = tilde_find_prefix (string, &len);

       
      if ((result_index + start + 1) > result_size)
	result = (char *)xrealloc (result, 1 + (result_size += (start + 20)));

      strncpy (result + result_index, string, start);
      result_index += start;

       
      string += start;

       
      end = tilde_find_suffix (string);

       
      if (!start && !end)
	break;

       
      tilde_word = (char *)xmalloc (1 + end);
      strncpy (tilde_word, string, end);
      tilde_word[end] = '\0';
      string += end;

      expansion = tilde_expand_word (tilde_word);

      if (expansion == 0)
	expansion = tilde_word;
      else
	xfree (tilde_word);	

      len = strlen (expansion);
#ifdef __CYGWIN__
       
      if (len > 1 || *expansion != '/' || *string != '/')
#endif
	{
	  if ((result_index + len + 1) > result_size)
	    result = (char *)xrealloc (result, 1 + (result_size += (len + 20)));

	  strcpy (result + result_index, expansion);
	  result_index += len;
	}
      xfree (expansion);
    }

  result[result_index] = '\0';

  return (result);
}

 
static char *
isolate_tilde_prefix (const char *fname, int *lenp)
{
  char *ret;
  int i;

  ret = (char *)xmalloc (strlen (fname));
#if defined (__MSDOS__)
  for (i = 1; fname[i] && fname[i] != '/' && fname[i] != '\\'; i++)
#else
  for (i = 1; fname[i] && fname[i] != '/'; i++)
#endif
    ret[i - 1] = fname[i];
  ret[i - 1] = '\0';
  if (lenp)
    *lenp = i;
  return ret;
}

#if 0
 
char *
tilde_find_word (const char *fname, int flags, int *lenp)
{
  int x;
  char *r;

  x = tilde_find_suffix (fname);
  if (x == 0)
    {
      r = savestring (fname);
      if (lenp)
	*lenp = 0;
    }
  else
    {
      r = (char *)xmalloc (1 + x);
      strncpy (r, fname, x);
      r[x] = '\0';
      if (lenp)
	*lenp = x;
    }

  return r;
}
#endif

 
static char *
glue_prefix_and_suffix (char *prefix, const char *suffix, int suffind)
{
  char *ret;
  int plen, slen;

  plen = (prefix && *prefix) ? strlen (prefix) : 0;
  slen = strlen (suffix + suffind);
  ret = (char *)xmalloc (plen + slen + 1);
  if (plen)
    strcpy (ret, prefix);
  strcpy (ret + plen, suffix + suffind);
  return ret;
}

 
char *
tilde_expand_word (const char *filename)
{
  char *dirname, *expansion, *username;
  int user_len;
  struct passwd *user_entry;

  if (filename == 0)
    return ((char *)NULL);

  if (*filename != '~')
    return (savestring (filename));

   
  if (filename[1] == '\0' || filename[1] == '/')
    {
       
      expansion = sh_get_env_value ("HOME");
#if defined (_WIN32)
      if (expansion == 0)
	expansion = sh_get_env_value ("APPDATA");
#endif

       
      if (expansion == 0)
	expansion = sh_get_home_dir ();

      return (glue_prefix_and_suffix (expansion, filename, 1));
    }

  username = isolate_tilde_prefix (filename, &user_len);

  if (tilde_expansion_preexpansion_hook)
    {
      expansion = (*tilde_expansion_preexpansion_hook) (username);
      if (expansion)
	{
	  dirname = glue_prefix_and_suffix (expansion, filename, user_len);
	  xfree (username);
	  xfree (expansion);
	  return (dirname);
	}
    }

   
  dirname = (char *)NULL;
#if defined (HAVE_GETPWNAM)
  user_entry = getpwnam (username);
#else
  user_entry = 0;
#endif
  if (user_entry == 0)
    {
       
      if (tilde_expansion_failure_hook)
	{
	  expansion = (*tilde_expansion_failure_hook) (username);
	  if (expansion)
	    {
	      dirname = glue_prefix_and_suffix (expansion, filename, user_len);
	      xfree (expansion);
	    }
	}
       
      if (dirname == 0)
	dirname = savestring (filename);
    }
#if defined (HAVE_GETPWENT)
  else
    dirname = glue_prefix_and_suffix (user_entry->pw_dir, filename, user_len);
#endif

  xfree (username);
#if defined (HAVE_GETPWENT)
  endpwent ();
#endif
  return (dirname);
}


#if defined (TEST)
#undef NULL
#include <stdio.h>

main (int argc, char **argv)
{
  char *result, line[512];
  int done = 0;

  while (!done)
    {
      printf ("~expand: ");
      fflush (stdout);

      if (!gets (line))
	strcpy (line, "done");

      if ((strcmp (line, "done") == 0) ||
	  (strcmp (line, "quit") == 0) ||
	  (strcmp (line, "exit") == 0))
	{
	  done = 1;
	  break;
	}

      result = tilde_expand (line);
      printf ("  --> %s\n", result);
      free (result);
    }
  exit (0);
}

static void memory_error_and_abort (void);

static void *
xmalloc (size_t bytes)
{
  void *temp = (char *)malloc (bytes);

  if (!temp)
    memory_error_and_abort ();
  return (temp);
}

static void *
xrealloc (void *pointer, int bytes)
{
  void *temp;

  if (!pointer)
    temp = malloc (bytes);
  else
    temp = realloc (pointer, bytes);

  if (!temp)
    memory_error_and_abort ();

  return (temp);
}

static void
memory_error_and_abort (void)
{
  fprintf (stderr, "readline: out of virtual memory\n");
  abort ();
}

 
#endif  
