 

 

#define READLINE_LIBRARY

#if defined (__TANDEM)
#  define _XOPEN_SOURCE_EXTENDED 1
#endif

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <sys/types.h>
#if defined (__TANDEM)
#  include <sys/stat.h>
#endif
#include <fcntl.h>
#if defined (HAVE_SYS_FILE_H)
#  include <sys/file.h>
#endif

#include <signal.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif  

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif  

#include <stdio.h>

#include <errno.h>
#if !defined (errno)
extern int errno;
#endif  

#if defined (HAVE_PWD_H)
#include <pwd.h>
#endif

#include "posixdir.h"
#include "posixstat.h"

 
#include "rldefs.h"
#include "rlmbutil.h"

 
#include "readline.h"
#include "xmalloc.h"
#include "rlprivate.h"

#if defined (COLOR_SUPPORT)
#  include "colors.h"
#endif

#ifdef __STDC__
typedef int QSFUNC (const void *, const void *);
#else
typedef int QSFUNC ();
#endif

#ifdef HAVE_LSTAT
#  define LSTAT lstat
#else
#  define LSTAT stat
#endif

 
#define HIDDEN_FILE(fname)	((fname)[0] == '.')

 
#if defined (HAVE_GETPWENT) && (!defined (HAVE_GETPW_DECLS) || defined (_POSIX_SOURCE))
extern struct passwd *getpwent (void);
#endif  

 
rl_compdisp_func_t *rl_completion_display_matches_hook = (rl_compdisp_func_t *)NULL;

#if defined (VISIBLE_STATS) || defined (COLOR_SUPPORT)
#  if !defined (X_OK)
#    define X_OK 1
#  endif
#endif

#if defined (VISIBLE_STATS)
static int stat_char (char *);
#endif

#if defined (COLOR_SUPPORT)
static int colored_stat_start (const char *);
static void colored_stat_end (void);
static int colored_prefix_start (void);
static void colored_prefix_end (void);
#endif

static int path_isdir (const char *);

static char *rl_quote_filename (char *, int, char *);

static void _rl_complete_sigcleanup (int, void *);

static void set_completion_defaults (int);
static int get_y_or_n (int);
static int _rl_internal_pager (int);
static char *printable_part (char *);
static int fnwidth (const char *);
static int fnprint (const char *, int, const char *);
static int print_filename (char *, char *, int);

static char **gen_completion_matches (char *, int, int, rl_compentry_func_t *, int, int);

static char **remove_duplicate_matches (char **);
static void insert_match (char *, int, int, char *);
static int append_to_match (char *, int, int, int);
static void insert_all_matches (char **, int, char *);
static int complete_fncmp (const char *, int, const char *, int);
static void display_matches (char **);
static int compute_lcd_of_matches (char **, int, const char *);
static int postprocess_matches (char ***, int);
static int compare_match (char *, const char *);
static int complete_get_screenwidth (void);

static char *make_quoted_replacement (char *, int, char *);

 
 
 
 
 

 

 
int _rl_complete_show_all = 0;

 
int _rl_complete_show_unmodified = 0;

 
int _rl_complete_mark_directories = 1;

 
int _rl_complete_mark_symlink_dirs = 0;

 
int _rl_print_completions_horizontally;

 
#if (defined (__MSDOS__) && !defined (__DJGPP__)) || (defined (_WIN32) && !defined (__CYGWIN__))
int _rl_completion_case_fold = 1;
#else
int _rl_completion_case_fold = 0;
#endif

 
int _rl_completion_case_map = 0;

 
int _rl_match_hidden_files = 1;

 
int _rl_completion_prefix_display_length = 0;

 
int _rl_completion_columns = -1;

#if defined (COLOR_SUPPORT)
 
int _rl_colored_stats = 0;

 
int _rl_colored_completion_prefix = 0;
#endif

 
int _rl_skip_completed_text = 0;

 
int _rl_menu_complete_prefix_first = 0;

 

#if defined (VISIBLE_STATS)
 
int rl_visible_stats = 0;
#endif  

 
rl_icppfunc_t *rl_directory_completion_hook = (rl_icppfunc_t *)NULL;

rl_icppfunc_t *rl_directory_rewrite_hook = (rl_icppfunc_t *)NULL;

rl_icppfunc_t *rl_filename_stat_hook = (rl_icppfunc_t *)NULL;

 
rl_dequote_func_t *rl_filename_rewrite_hook = (rl_dequote_func_t *)NULL;

 
int rl_complete_with_tilde_expansion = 0;

 
rl_compentry_func_t *rl_completion_entry_function = (rl_compentry_func_t *)NULL;

 
rl_compentry_func_t *rl_menu_completion_entry_function = (rl_compentry_func_t *)NULL;

 
rl_completion_func_t *rl_attempted_completion_function = (rl_completion_func_t *)NULL;

 
int rl_attempted_completion_over = 0;

 
int rl_completion_type = 0;

 
int rl_completion_query_items = 100;

int _rl_page_completions = 1;

 
const char *rl_basic_word_break_characters = " \t\n\"\\'`@$><=;|&{(";  

 
const char *rl_basic_quote_characters = "\"'";

 
const char *rl_completer_word_break_characters = 0;

 
rl_cpvfunc_t *rl_completion_word_break_hook = (rl_cpvfunc_t *)NULL;

 
const char *rl_completer_quote_characters = (const char *)NULL;

 
const char *rl_filename_quote_characters = (const char *)NULL;

 
const char *rl_special_prefixes = (const char *)NULL;

 
int rl_ignore_completion_duplicates = 1;

 
int rl_filename_completion_desired = 0;

 
int rl_filename_quoting_desired = 1;

 
rl_compignore_func_t *rl_ignore_some_completions_function = (rl_compignore_func_t *)NULL;

 
rl_quote_func_t *rl_filename_quoting_function = rl_quote_filename;
         
 
rl_dequote_func_t *rl_filename_dequoting_function = (rl_dequote_func_t *)NULL;

 
rl_linebuf_func_t *rl_char_is_quoted_p = (rl_linebuf_func_t *)NULL;

 
int rl_completion_suppress_append = 0;

 
int rl_completion_append_character = ' ';

 
int rl_completion_suppress_quote = 0;

 
int rl_completion_quote_character;

 
int rl_completion_found_quote;

 
int rl_completion_mark_symlink_dirs;

 
int rl_inhibit_completion;

 
int rl_completion_invoking_key;

 
int rl_sort_completion_matches = 1;

 

 
static int completion_changed_buffer;
static int last_completion_failed = 0;

 
static int completion_y_or_n;

static int _rl_complete_display_matches_interrupt = 0;

 
 
 
 
 

 
int
rl_complete (int ignore, int invoking_key)
{
  rl_completion_invoking_key = invoking_key;

  if (rl_inhibit_completion)
    return (_rl_insert_char (ignore, invoking_key));
#if 0
  else if (rl_last_func == rl_complete && completion_changed_buffer == 0 && last_completion_failed == 0)
#else
  else if (rl_last_func == rl_complete && completion_changed_buffer == 0)
#endif
    return (rl_complete_internal ('?'));
  else if (_rl_complete_show_all)
    return (rl_complete_internal ('!'));
  else if (_rl_complete_show_unmodified)
    return (rl_complete_internal ('@'));
  else
    return (rl_complete_internal (TAB));
}

 
int
rl_possible_completions (int ignore, int invoking_key)
{
  rl_completion_invoking_key = invoking_key;
  return (rl_complete_internal ('?'));
}

int
rl_insert_completions (int ignore, int invoking_key)
{
  rl_completion_invoking_key = invoking_key;
  return (rl_complete_internal ('*'));
}

 
int
rl_completion_mode (rl_command_func_t *cfunc)
{
  if (rl_last_func == cfunc && !completion_changed_buffer)
    return '?';
  else if (_rl_complete_show_all)
    return '!';
  else if (_rl_complete_show_unmodified)
    return '@';
  else
    return TAB;
}

 
 
 
 
 

 
void
_rl_reset_completion_state (void)
{
  rl_completion_found_quote = 0;
  rl_completion_quote_character = 0;
}

static void
_rl_complete_sigcleanup (int sig, void *ptr)
{
  if (sig == SIGINT)	 
    {
      _rl_free_match_list ((char **)ptr);
      _rl_complete_display_matches_interrupt = 1;
    }
}

 
static void
set_completion_defaults (int what_to_do)
{
   
  rl_filename_completion_desired = 0;
  rl_filename_quoting_desired = 1;
  rl_completion_type = what_to_do;
  rl_completion_suppress_append = rl_completion_suppress_quote = 0;
  rl_completion_append_character = ' ';

   
  rl_completion_mark_symlink_dirs = _rl_complete_mark_symlink_dirs;

   
  _rl_complete_display_matches_interrupt = 0;
}

 
static int
get_y_or_n (int for_pager)
{
  int c;

   
#if defined (READLINE_CALLBACKS)
  if (RL_ISSTATE (RL_STATE_CALLBACK))
    return 1;
#endif

  for (;;)
    {
      RL_SETSTATE(RL_STATE_MOREINPUT);
      c = rl_read_key ();
      RL_UNSETSTATE(RL_STATE_MOREINPUT);

      if (c == 'y' || c == 'Y' || c == ' ')
	return (1);
      if (c == 'n' || c == 'N' || c == RUBOUT)
	return (0);
      if (c == ABORT_CHAR || c < 0)
	_rl_abort_internal ();
      if (for_pager && (c == NEWLINE || c == RETURN))
	return (2);
      if (for_pager && (c == 'q' || c == 'Q'))
	return (0);
      rl_ding ();
    }
}

static int
_rl_internal_pager (int lines)
{
  int i;

  fprintf (rl_outstream, "--More--");
  fflush (rl_outstream);
  i = get_y_or_n (1);
  _rl_erase_entire_line ();
  if (i == 0)
    return -1;
  else if (i == 2)
    return (lines - 1);
  else
    return 0;
}

static int
path_isdir (const char *filename)
{
  struct stat finfo;

  return (stat (filename, &finfo) == 0 && S_ISDIR (finfo.st_mode));
}

#if defined (VISIBLE_STATS)
 
static int
stat_char (char *filename)
{
  struct stat finfo;
  int character, r;
  char *f;
  const char *fn;

   
#if __CYGWIN__
  if (filename[0] == '/' && filename[1] == '/' && strchr (filename+2, '/') == 0)
    return '/';
#endif

  f = 0;
  if (rl_filename_stat_hook)
    {
      f = savestring (filename);
      (*rl_filename_stat_hook) (&f);
      fn = f;
    }
  else
    fn = filename;
    
#if defined (HAVE_LSTAT) && defined (S_ISLNK)
  r = lstat (fn, &finfo);
#else
  r = stat (fn, &finfo);
#endif

  if (r == -1)
    {
      xfree (f);
      return (0);
    }

  character = 0;
  if (S_ISDIR (finfo.st_mode))
    character = '/';
#if defined (S_ISCHR)
  else if (S_ISCHR (finfo.st_mode))
    character = '%';
#endif  
#if defined (S_ISBLK)
  else if (S_ISBLK (finfo.st_mode))
    character = '#';
#endif  
#if defined (S_ISLNK)
  else if (S_ISLNK (finfo.st_mode))
    character = '@';
#endif  
#if defined (S_ISSOCK)
  else if (S_ISSOCK (finfo.st_mode))
    character = '=';
#endif  
#if defined (S_ISFIFO)
  else if (S_ISFIFO (finfo.st_mode))
    character = '|';
#endif
  else if (S_ISREG (finfo.st_mode))
    {
#if defined (_WIN32) && !defined (__CYGWIN__)
      char *ext;

       
      ext = strrchr (fn, '.');
      if (ext && (_rl_stricmp (ext, ".exe") == 0 ||
		  _rl_stricmp (ext, ".cmd") == 0 ||
		  _rl_stricmp (ext, ".bat") == 0 ||
		  _rl_stricmp (ext, ".com") == 0))
	character = '*';
#else
      if (access (filename, X_OK) == 0)
	character = '*';
#endif
    }

  xfree (f);
  return (character);
}
#endif  

#if defined (COLOR_SUPPORT)
static int
colored_stat_start (const char *filename)
{
  _rl_set_normal_color ();
  return (_rl_print_color_indicator (filename));
}

static void
colored_stat_end (void)
{
  _rl_prep_non_filename_text ();
  _rl_put_indicator (&_rl_color_indicator[C_CLR_TO_EOL]);
}

static int
colored_prefix_start (void)
{
  _rl_set_normal_color ();
  return (_rl_print_prefix_color ());
}

static void
colored_prefix_end (void)
{
  colored_stat_end ();		 
}
#endif

 
static char *
printable_part (char *pathname)
{
  char *temp, *x;

  if (rl_filename_completion_desired == 0)	 
    return (pathname);

  temp = strrchr (pathname, '/');
#if defined (__MSDOS__) || defined (_WIN32)
  if (temp == 0 && ISALPHA ((unsigned char)pathname[0]) && pathname[1] == ':')
    temp = pathname + 1;
#endif

  if (temp == 0 || *temp == '\0')
    return (pathname);
  else if (temp[1] == 0 && temp == pathname)
    return (pathname);
   
  else if (temp[1] == '\0')
    {
      for (x = temp - 1; x > pathname; x--)
        if (*x == '/')
          break;
      return ((*x == '/') ? x + 1 : pathname);
    }
  else
    return ++temp;
}

 
static int
fnwidth (const char *string)
{
  int width, pos;
#if defined (HANDLE_MULTIBYTE)
  mbstate_t ps;
  int left, w;
  size_t clen;
  WCHAR_T wc;

  left = strlen (string) + 1;
  memset (&ps, 0, sizeof (mbstate_t));
#endif

  width = pos = 0;
  while (string[pos])
    {
      if (CTRL_CHAR (string[pos]) || string[pos] == RUBOUT)
	{
	  width += 2;
	  pos++;
	}
      else
	{
#if defined (HANDLE_MULTIBYTE)
	  clen = MBRTOWC (&wc, string + pos, left - pos, &ps);
	  if (MB_INVALIDCH (clen))
	    {
	      width++;
	      pos++;
	      memset (&ps, 0, sizeof (mbstate_t));
	    }
	  else if (MB_NULLWCH (clen))
	    break;
	  else
	    {
	      pos += clen;
	      w = WCWIDTH (wc);
	      width += (w >= 0) ? w : 1;
	    }
#else
	  width++;
	  pos++;
#endif
	}
    }

  return width;
}

#define ELLIPSIS_LEN	3

static int
fnprint (const char *to_print, int prefix_bytes, const char *real_pathname)
{
  int printed_len, w;
  const char *s;
  int common_prefix_len, print_len;
#if defined (HANDLE_MULTIBYTE)
  mbstate_t ps;
  const char *end;
  size_t tlen;
  int width;
  WCHAR_T wc;

  print_len = strlen (to_print);
  end = to_print + print_len + 1;
  memset (&ps, 0, sizeof (mbstate_t));
#else
  print_len = strlen (to_print);
#endif

  printed_len = common_prefix_len = 0;

   
  if (_rl_completion_prefix_display_length > 0 && prefix_bytes >= print_len)
    prefix_bytes = 0;

#if defined (COLOR_SUPPORT)
  if (_rl_colored_stats && (prefix_bytes == 0 || _rl_colored_completion_prefix <= 0))
    colored_stat_start (real_pathname);
#endif

  if (prefix_bytes && _rl_completion_prefix_display_length > 0 &&
      prefix_bytes > _rl_completion_prefix_display_length)
    {
      char ellipsis;

      ellipsis = (to_print[prefix_bytes] == '.') ? '_' : '.';
      for (w = 0; w < ELLIPSIS_LEN; w++)
	putc (ellipsis, rl_outstream);
      printed_len = ELLIPSIS_LEN;
    }
#if defined (COLOR_SUPPORT)
  else if (prefix_bytes && _rl_colored_completion_prefix > 0)
    {
      common_prefix_len = prefix_bytes;
      prefix_bytes = 0;
       
      colored_prefix_start ();
    }
#endif

  s = to_print + prefix_bytes;
  while (*s)
    {
      if (CTRL_CHAR (*s))
        {
          putc ('^', rl_outstream);
          putc (UNCTRL (*s), rl_outstream);
          printed_len += 2;
          s++;
#if defined (HANDLE_MULTIBYTE)
	  memset (&ps, 0, sizeof (mbstate_t));
#endif
        }
      else if (*s == RUBOUT)
	{
	  putc ('^', rl_outstream);
	  putc ('?', rl_outstream);
	  printed_len += 2;
	  s++;
#if defined (HANDLE_MULTIBYTE)
	  memset (&ps, 0, sizeof (mbstate_t));
#endif
	}
      else
	{
#if defined (HANDLE_MULTIBYTE)
	  tlen = MBRTOWC (&wc, s, end - s, &ps);
	  if (MB_INVALIDCH (tlen))
	    {
	      tlen = 1;
	      width = 1;
	      memset (&ps, 0, sizeof (mbstate_t));
	    }
	  else if (MB_NULLWCH (tlen))
	    break;
	  else
	    {
	      w = WCWIDTH (wc);
	      width = (w >= 0) ? w : 1;
	    }
	  fwrite (s, 1, tlen, rl_outstream);
	  s += tlen;
	  printed_len += width;
#else
	  putc (*s, rl_outstream);
	  s++;
	  printed_len++;
#endif
	}
      if (common_prefix_len > 0 && (s - to_print) >= common_prefix_len)
	{
#if defined (COLOR_SUPPORT)
	   
	   
	  colored_prefix_end ();
	  if (_rl_colored_stats)
	    colored_stat_start (real_pathname);		 
#endif
	  common_prefix_len = 0;
	}
    }

#if defined (COLOR_SUPPORT)
   
  if (_rl_colored_stats)
    colored_stat_end ();
#endif

  return printed_len;
}

 

static int
print_filename (char *to_print, char *full_pathname, int prefix_bytes)
{
  int printed_len, extension_char, slen, tlen;
  char *s, c, *new_full_pathname, *dn;

  extension_char = 0;
#if defined (COLOR_SUPPORT)
   
  if (_rl_colored_stats == 0 || rl_filename_completion_desired == 0)
#endif
    printed_len = fnprint (to_print, prefix_bytes, to_print);

  if (rl_filename_completion_desired && (
#if defined (VISIBLE_STATS)
     rl_visible_stats ||
#endif
#if defined (COLOR_SUPPORT)
     _rl_colored_stats ||
#endif
     _rl_complete_mark_directories))
    {
       
      if (to_print != full_pathname)
	{
	   
	  c = to_print[-1];
	  to_print[-1] = '\0';

	   
	  if (full_pathname == 0 || *full_pathname == 0)
	    dn = "/";
	  else if (full_pathname[0] != '/')
	    dn = full_pathname;
	  else if (full_pathname[1] == 0)
	    dn = "//";		 
	  else
	    dn = full_pathname;
	  s = tilde_expand (dn);
	  if (rl_directory_completion_hook)
	    (*rl_directory_completion_hook) (&s);

	  slen = strlen (s);
	  tlen = strlen (to_print);
	  new_full_pathname = (char *)xmalloc (slen + tlen + 2);
	  strcpy (new_full_pathname, s);
	  if (s[slen - 1] == '/')
	    slen--;
	  else
	    new_full_pathname[slen] = '/';
	  strcpy (new_full_pathname + slen + 1, to_print);

#if defined (VISIBLE_STATS)
	  if (rl_visible_stats)
	    extension_char = stat_char (new_full_pathname);
	  else
#endif
	  if (_rl_complete_mark_directories)
	    {
	      dn = 0;
	      if (rl_directory_completion_hook == 0 && rl_filename_stat_hook)
		{
		  dn = savestring (new_full_pathname);
		  (*rl_filename_stat_hook) (&dn);
		  xfree (new_full_pathname);
		  new_full_pathname = dn;
		}
	      if (path_isdir (new_full_pathname))
		extension_char = '/';
	    }

	   
#if defined (COLOR_SUPPORT)
	  if (_rl_colored_stats)
	    printed_len = fnprint (to_print, prefix_bytes, new_full_pathname);
#endif

	  xfree (new_full_pathname);
	  to_print[-1] = c;
	}
      else
	{
	  s = tilde_expand (full_pathname);
#if defined (VISIBLE_STATS)
	  if (rl_visible_stats)
	    extension_char = stat_char (s);
	  else
#endif
	    if (_rl_complete_mark_directories && path_isdir (s))
	      extension_char = '/';

	   
#if defined (COLOR_SUPPORT)
	  if (_rl_colored_stats)
	    printed_len = fnprint (to_print, prefix_bytes, s);
#endif
	}

      xfree (s);
      if (extension_char)
	{
	  putc (extension_char, rl_outstream);
	  printed_len++;
	}
    }

  return printed_len;
}

static char *
rl_quote_filename (char *s, int rtype, char *qcp)
{
  char *r;

  r = (char *)xmalloc (strlen (s) + 2);
  *r = *rl_completer_quote_characters;
  strcpy (r + 1, s);
  if (qcp)
    *qcp = *rl_completer_quote_characters;
  return r;
}

 

char
_rl_find_completion_word (int *fp, int *dp)
{
  int scan, end, found_quote, delimiter, pass_next, isbrk;
  char quote_char;
  const char *brkchars;

  end = rl_point;
  found_quote = delimiter = 0;
  quote_char = '\0';

  brkchars = 0;
  if (rl_completion_word_break_hook)
    brkchars = (*rl_completion_word_break_hook) ();
  if (brkchars == 0)
    brkchars = rl_completer_word_break_characters;

  if (rl_completer_quote_characters)
    {
       
       
      for (scan = pass_next = 0; scan < end; scan = MB_NEXTCHAR (rl_line_buffer, scan, 1, MB_FIND_ANY))
	{
	  if (pass_next)
	    {
	      pass_next = 0;
	      continue;
	    }

	   
	  if (quote_char != '\'' && rl_line_buffer[scan] == '\\')
	    {
	      pass_next = 1;
	      found_quote |= RL_QF_BACKSLASH;
	      continue;
	    }

	  if (quote_char != '\0')
	    {
	       
	      if (rl_line_buffer[scan] == quote_char)
		{
		   
		  quote_char = '\0';
		  rl_point = end;
		}
	    }
	  else if (strchr (rl_completer_quote_characters, rl_line_buffer[scan]))
	    {
	       
	      quote_char = rl_line_buffer[scan];
	      rl_point = scan + 1;
	       
	      if (quote_char == '\'')
		found_quote |= RL_QF_SINGLE_QUOTE;
	      else if (quote_char == '"')
		found_quote |= RL_QF_DOUBLE_QUOTE;
	      else
		found_quote |= RL_QF_OTHER_QUOTE;      
	    }
	}
    }

  if (rl_point == end && quote_char == '\0')
    {
       
      while (rl_point = MB_PREVCHAR (rl_line_buffer, rl_point, MB_FIND_ANY))
	{
	  scan = rl_line_buffer[rl_point];

	  if (strchr (brkchars, scan) == 0)
	    continue;

	   
	  if (rl_char_is_quoted_p && found_quote &&
	      (*rl_char_is_quoted_p) (rl_line_buffer, rl_point))
	    continue;

	   
	  break;
	}
    }

   
  scan = rl_line_buffer[rl_point];

   
  if (scan)
    {
      if (rl_char_is_quoted_p)
	isbrk = (found_quote == 0 ||
		(*rl_char_is_quoted_p) (rl_line_buffer, rl_point) == 0) &&
		strchr (brkchars, scan) != 0;
      else
	isbrk = strchr (brkchars, scan) != 0;

      if (isbrk)
	{
	   
	  if (rl_basic_quote_characters &&
	      strchr (rl_basic_quote_characters, scan) &&
	      (end - rl_point) > 1)
	    delimiter = scan;

	   
	  if (rl_special_prefixes == 0 || strchr (rl_special_prefixes, scan) == 0)
	    rl_point++;
	}
    }

  if (fp)
    *fp = found_quote;
  if (dp)
    *dp = delimiter;

  return (quote_char);
}

static char **
gen_completion_matches (char *text, int start, int end, rl_compentry_func_t *our_func, int found_quote, int quote_char)
{
  char **matches;

  rl_completion_found_quote = found_quote;
  rl_completion_quote_character = quote_char;

   
  if (rl_attempted_completion_function)
    {
      matches = (*rl_attempted_completion_function) (text, start, end);
      if (RL_SIG_RECEIVED())
	{
	  _rl_free_match_list (matches);
	  matches = 0;
	  RL_CHECK_SIGNALS ();
	}

      if (matches || rl_attempted_completion_over)
	{
	  rl_attempted_completion_over = 0;
	  return (matches);
	}
    }

   

   
  matches = rl_completion_matches (text, our_func);
  if (RL_SIG_RECEIVED())
    {
      _rl_free_match_list (matches);
      matches = 0;
      RL_CHECK_SIGNALS ();
    }
  return matches;  
}

 
static char **
remove_duplicate_matches (char **matches)
{
  char *lowest_common;
  int i, j, newlen;
  char dead_slot;
  char **temp_array;

   
  for (i = 0; matches[i]; i++)
    ;

   
  if (i && rl_sort_completion_matches)
    qsort (matches+1, i-1, sizeof (char *), (QSFUNC *)_rl_qsort_string_compare);

   
  lowest_common = savestring (matches[0]);

  for (i = newlen = 0; matches[i + 1]; i++)
    {
      if (strcmp (matches[i], matches[i + 1]) == 0)
	{
	  xfree (matches[i]);
	  matches[i] = (char *)&dead_slot;
	}
      else
	newlen++;
    }

   
  temp_array = (char **)xmalloc ((3 + newlen) * sizeof (char *));
  for (i = j = 1; matches[i]; i++)
    {
      if (matches[i] != (char *)&dead_slot)
	temp_array[j++] = matches[i];
    }
  temp_array[j] = (char *)NULL;

  if (matches[0] != (char *)&dead_slot)
    xfree (matches[0]);

   
  temp_array[0] = lowest_common;

   
  if (j == 2 && strcmp (temp_array[0], temp_array[1]) == 0)
    {
      xfree (temp_array[1]);
      temp_array[1] = (char *)NULL;
    }
  return (temp_array);
}

 
static int
compute_lcd_of_matches (char **match_list, int matches, const char *text)
{
  register int i, c1, c2, si;
  int low;		 
  int lx;
  char *dtext;		 
#if defined (HANDLE_MULTIBYTE)
  int v;
  size_t v1, v2;
  mbstate_t ps1, ps2;
  WCHAR_T wc1, wc2;
#endif

   
  if (matches == 1)
    {
      match_list[0] = match_list[1];
      match_list[1] = (char *)NULL;
      return 1;
    }

  for (i = 1, low = 100000; i < matches; i++)
    {
#if defined (HANDLE_MULTIBYTE)
      if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
	{
	  memset (&ps1, 0, sizeof (mbstate_t));
	  memset (&ps2, 0, sizeof (mbstate_t));
	}
#endif
      for (si = 0; (c1 = match_list[i][si]) && (c2 = match_list[i + 1][si]); si++)
	{
	    if (_rl_completion_case_fold)
	      {
	        c1 = _rl_to_lower (c1);
	        c2 = _rl_to_lower (c2);
	      }
#if defined (HANDLE_MULTIBYTE)
	    if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
	      {
		v1 = MBRTOWC (&wc1, match_list[i]+si, strlen (match_list[i]+si), &ps1);
		v2 = MBRTOWC (&wc2, match_list[i+1]+si, strlen (match_list[i+1]+si), &ps2);
		if (MB_INVALIDCH (v1) || MB_INVALIDCH (v2))
		  {
		    if (c1 != c2)	 
		      break;
		    continue;
		  }
		if (_rl_completion_case_fold)
		  {
		    wc1 = towlower (wc1);
		    wc2 = towlower (wc2);
		  }
		if (wc1 != wc2)
		  break;
		else if (v1 > 1)
		  si += v1 - 1;
	      }
	    else
#endif
	    if (c1 != c2)
	      break;
	}

      if (low > si)
	low = si;
    }

   
  if (low == 0 && text && *text)
    {
      match_list[0] = (char *)xmalloc (strlen (text) + 1);
      strcpy (match_list[0], text);
    }
  else
    {
      match_list[0] = (char *)xmalloc (low + 1);

       

       
      if (_rl_completion_case_fold)
	{
	   
	  dtext = (char *)NULL;
	  if (rl_filename_completion_desired &&
	      rl_filename_dequoting_function &&
	      rl_completion_found_quote &&
	      rl_filename_quoting_desired)
	    {
	      dtext = (*rl_filename_dequoting_function) ((char *)text, rl_completion_quote_character);
	      text = dtext;
	    }

	   
	  if (rl_sort_completion_matches)
	    qsort (match_list+1, matches, sizeof(char *), (QSFUNC *)_rl_qsort_string_compare);

	  si = strlen (text);
	  lx = (si <= low) ? si : low;	 
	   
	  for (i = 1; i <= matches; i++)
	    if (strncmp (match_list[i], text, lx) == 0)
	      {
		strncpy (match_list[0], match_list[i], low);
		break;
	      }
	   
	  if (i > matches)
	    strncpy (match_list[0], match_list[1], low);

	  FREE (dtext);
	}
      else
        strncpy (match_list[0], match_list[1], low);

      match_list[0][low] = '\0';
    }

  return matches;
}

static int
postprocess_matches (char ***matchesp, int matching_filenames)
{
  char *t, **matches, **temp_matches;
  int nmatch, i;

  matches = *matchesp;

  if (matches == 0)
    return 0;

   
  if (rl_ignore_completion_duplicates)
    {
      temp_matches = remove_duplicate_matches (matches);
      xfree (matches);
      matches = temp_matches;
    }

   
  if (rl_ignore_some_completions_function && matching_filenames)
    {
      for (nmatch = 1; matches[nmatch]; nmatch++)
	;
      (void)(*rl_ignore_some_completions_function) (matches);
      if (matches == 0 || matches[0] == 0)
	{
	  FREE (matches);
	  *matchesp = (char **)0;
	  return 0;
        }
      else
	{
	   
	  for (i = 1; matches[i]; i++)
	    ;
	  if (i > 1 && i < nmatch)
	    {
	      t = matches[0];
	      compute_lcd_of_matches (matches, i - 1, t);
	      FREE (t);
	    }
	}
    }

  *matchesp = matches;
  return (1);
}

static int
complete_get_screenwidth (void)
{
  int cols;
  char *envcols;

  cols = _rl_completion_columns;
  if (cols >= 0 && cols <= _rl_screenwidth)
    return cols;
  envcols = getenv ("COLUMNS");
  if (envcols && *envcols)
    cols = atoi (envcols);
  if (cols >= 0 && cols <= _rl_screenwidth)
    return cols;
  return _rl_screenwidth;
}

 
void
rl_display_match_list (char **matches, int len, int max)
{
  int count, limit, printed_len, lines, cols;
  int i, j, k, l, common_length, sind;
  char *temp, *t;

   
  common_length = sind = 0;
  if (_rl_completion_prefix_display_length > 0)
    {
      t = printable_part (matches[0]);
       
      temp = rl_filename_completion_desired ? strrchr (t, '/') : 0;
      common_length = temp ? fnwidth (temp) : fnwidth (t);
      sind = temp ? strlen (temp) : strlen (t);
      if (common_length > max || sind > max)
	common_length = sind = 0;

      if (common_length > _rl_completion_prefix_display_length && common_length > ELLIPSIS_LEN)
	max -= common_length - ELLIPSIS_LEN;
      else if (_rl_colored_completion_prefix <= 0)
	common_length = sind = 0;
    }
#if defined (COLOR_SUPPORT)
  else if (_rl_colored_completion_prefix > 0)
    {
      t = printable_part (matches[0]);
      temp = rl_filename_completion_desired ? strrchr (t, '/') : 0;
      common_length = temp ? fnwidth (temp) : fnwidth (t);
      sind = temp ? RL_STRLEN (temp+1) : RL_STRLEN (t);		 
      if (common_length > max || sind > max)
	common_length = sind = 0;
    }
#endif

   
  cols = complete_get_screenwidth ();
  max += 2;
  limit = cols / max;
  if (limit != 1 && (limit * max == cols))
    limit--;

   
  if (cols < _rl_screenwidth && limit < 0)
    limit = 1;

   
  if (limit == 0)
    limit = 1;

   
  count = (len + (limit - 1)) / limit;

   

   
  if (rl_ignore_completion_duplicates == 0 && rl_sort_completion_matches)
    qsort (matches + 1, len, sizeof (char *), (QSFUNC *)_rl_qsort_string_compare);

  rl_crlf ();

  lines = 0;
  if (_rl_print_completions_horizontally == 0)
    {
       
      for (i = 1; i <= count; i++)
	{
	  for (j = 0, l = i; j < limit; j++)
	    {
	      if (l > len || matches[l] == 0)
		break;
	      else
		{
		  temp = printable_part (matches[l]);
		  printed_len = print_filename (temp, matches[l], sind);

		  if (j + 1 < limit)
		    {
		      if (max <= printed_len)
			putc (' ', rl_outstream);
		      else
			for (k = 0; k < max - printed_len; k++)
			  putc (' ', rl_outstream);
		    }
		}
	      l += count;
	    }
	  rl_crlf ();
#if defined (SIGWINCH)
	  if (RL_SIG_RECEIVED () && RL_SIGWINCH_RECEIVED() == 0)
#else
	  if (RL_SIG_RECEIVED ())
#endif
	    return;
	  lines++;
	  if (_rl_page_completions && lines >= (_rl_screenheight - 1) && i < count)
	    {
	      lines = _rl_internal_pager (lines);
	      if (lines < 0)
		return;
	    }
	}
    }
  else
    {
       
      for (i = 1; matches[i]; i++)
	{
	  temp = printable_part (matches[i]);
	  printed_len = print_filename (temp, matches[i], sind);
	   
#if defined (SIGWINCH)
	  if (RL_SIG_RECEIVED () && RL_SIGWINCH_RECEIVED() == 0)
#else
	  if (RL_SIG_RECEIVED ())
#endif
	    return;
	  if (matches[i+1])
	    {
	      if (limit == 1 || (i && (limit > 1) && (i % limit) == 0))
		{
		  rl_crlf ();
		  lines++;
		  if (_rl_page_completions && lines >= _rl_screenheight - 1)
		    {
		      lines = _rl_internal_pager (lines);
		      if (lines < 0)
			return;
		    }
		}
	      else if (max <= printed_len)
		putc (' ', rl_outstream);
	      else
		for (k = 0; k < max - printed_len; k++)
		  putc (' ', rl_outstream);
	    }
	}
      rl_crlf ();
    }
}

 
static void
display_matches (char **matches)
{
  int len, max, i;
  char *temp;

   
  _rl_move_vert (_rl_vis_botlin);

   
  if (matches[1] == 0)
    {
      temp = printable_part (matches[0]);
      rl_crlf ();
      print_filename (temp, matches[0], 0);
      rl_crlf ();

      rl_forced_update_display ();
      rl_display_fixed = 1;

      return;
    }

   
  for (max = 0, i = 1; matches[i]; i++)
    {
      temp = printable_part (matches[i]);
      len = fnwidth (temp);

      if (len > max)
	max = len;
    }

  len = i - 1;

   
  if (rl_completion_display_matches_hook)
    {
      (*rl_completion_display_matches_hook) (matches, len, max);
      return;
    }
	
   
  if (rl_completion_query_items > 0 && len >= rl_completion_query_items)
    {
      rl_crlf ();
      fprintf (rl_outstream, "Display all %d possibilities? (y or n)", len);
      fflush (rl_outstream);
      if ((completion_y_or_n = get_y_or_n (0)) == 0)
	{
	  rl_crlf ();

	  rl_forced_update_display ();
	  rl_display_fixed = 1;

	  return;
	}
    }

  rl_display_match_list (matches, len, max);

  rl_forced_update_display ();
  rl_display_fixed = 1;
}

 
static char *
make_quoted_replacement (char *match, int mtype, char *qc)
{
  int should_quote, do_replace;
  char *replacement;

   
  replacement = match;

  should_quote = match && rl_completer_quote_characters &&
			rl_filename_completion_desired &&
			rl_filename_quoting_desired;

  if (should_quote)
    should_quote = should_quote && (!qc || !*qc ||
		     (rl_completer_quote_characters && strchr (rl_completer_quote_characters, *qc)));

  if (should_quote)
    {
       
      should_quote = rl_filename_quote_characters
			? (_rl_strpbrk (match, rl_filename_quote_characters) != 0)
			: 0;

      do_replace = should_quote ? mtype : NO_MATCH;
       
      if (do_replace != NO_MATCH && rl_filename_quoting_function)
	replacement = (*rl_filename_quoting_function) (match, do_replace, qc);
    }
  return (replacement);
}

static void
insert_match (char *match, int start, int mtype, char *qc)
{
  char *replacement, *r;
  char oqc;
  int end, rlen;

  oqc = qc ? *qc : '\0';
  replacement = make_quoted_replacement (match, mtype, qc);

   
  if (replacement)
    {
      rlen = strlen (replacement);
       
      if (qc && *qc && start && rl_line_buffer[start - 1] == *qc &&
	    replacement[0] == *qc)
	start--;
       
      else if (qc && (*qc != oqc) && start && rl_line_buffer[start - 1] == oqc &&
	    replacement[0] != oqc)
	start--;
      end = rl_point - 1;
       
      if (qc && *qc && end && rl_line_buffer[rl_point] == *qc && replacement[rlen - 1] == *qc)
        end++;
      if (_rl_skip_completed_text)
	{
	  r = replacement;
	  while (start < rl_end && *r && rl_line_buffer[start] == *r)
	    {
	      start++;
	      r++;
	    }
	  if (start <= end || *r)
	    _rl_replace_text (r, start, end);
	  rl_point = start + strlen (r);
	}
      else
	_rl_replace_text (replacement, start, end);
      if (replacement != match)
        xfree (replacement);
    }
}

 
static int
append_to_match (char *text, int delimiter, int quote_char, int nontrivial_match)
{
  char temp_string[4], *filename, *fn;
  int temp_string_index, s;
  struct stat finfo;

  temp_string_index = 0;
  if (quote_char && rl_point && rl_completion_suppress_quote == 0 &&
      rl_line_buffer[rl_point - 1] != quote_char)
    temp_string[temp_string_index++] = quote_char;

  if (delimiter)
    temp_string[temp_string_index++] = delimiter;
  else if (rl_completion_suppress_append == 0 && rl_completion_append_character)
    temp_string[temp_string_index++] = rl_completion_append_character;

  temp_string[temp_string_index++] = '\0';

  if (rl_filename_completion_desired)
    {
      filename = tilde_expand (text);
      if (rl_filename_stat_hook)
        {
          fn = savestring (filename);
	  (*rl_filename_stat_hook) (&fn);
	  xfree (filename);
	  filename = fn;
        }
      s = (nontrivial_match && rl_completion_mark_symlink_dirs == 0)
		? LSTAT (filename, &finfo)
		: stat (filename, &finfo);
      if (s == 0 && S_ISDIR (finfo.st_mode))
	{
	  if (_rl_complete_mark_directories  )
	    {
	       
	      if (rl_point && rl_line_buffer[rl_point] == '\0' && rl_line_buffer[rl_point - 1] == '/')
		;
	      else if (rl_line_buffer[rl_point] != '/')
		rl_insert_text ("/");
	    }
	}
#ifdef S_ISLNK
       
      else if (s == 0 && S_ISLNK (finfo.st_mode) && path_isdir (filename))
	;
#endif
      else
	{
	  if (rl_point == rl_end && temp_string_index)
	    rl_insert_text (temp_string);
	}
      xfree (filename);
    }
  else
    {
      if (rl_point == rl_end && temp_string_index)
	rl_insert_text (temp_string);
    }

  return (temp_string_index);
}

static void
insert_all_matches (char **matches, int point, char *qc)
{
  int i;
  char *rp;

  rl_begin_undo_group ();
   
  if (qc && *qc && point && rl_line_buffer[point - 1] == *qc)
    point--;
  rl_delete_text (point, rl_point);
  rl_point = point;

  if (matches[1])
    {
      for (i = 1; matches[i]; i++)
	{
	  rp = make_quoted_replacement (matches[i], SINGLE_MATCH, qc);
	  rl_insert_text (rp);
	  rl_insert_text (" ");
	  if (rp != matches[i])
	    xfree (rp);
	}
    }
  else
    {
      rp = make_quoted_replacement (matches[0], SINGLE_MATCH, qc);
      rl_insert_text (rp);
      rl_insert_text (" ");
      if (rp != matches[0])
	xfree (rp);
    }
  rl_end_undo_group ();
}

void
_rl_free_match_list (char **matches)
{
  register int i;

  if (matches == 0)
    return;

  for (i = 0; matches[i]; i++)
    xfree (matches[i]);
  xfree (matches);
}

 
static int
compare_match (char *text, const char *match)
{
  char *temp;
  int r;

  if (rl_filename_completion_desired && rl_filename_quoting_desired && 
      rl_completion_found_quote && rl_filename_dequoting_function)
    {
      temp = (*rl_filename_dequoting_function) (text, rl_completion_quote_character);
      r = strcmp (temp, match);
      xfree (temp);
      return r;
    }      
  return (strcmp (text, match));
}

 
int
rl_complete_internal (int what_to_do)
{
  char **matches;
  rl_compentry_func_t *our_func;
  int start, end, delimiter, found_quote, i, nontrivial_lcd;
  char *text, *saved_line_buffer;
  char quote_char;
  int tlen, mlen, saved_last_completion_failed;

  RL_SETSTATE(RL_STATE_COMPLETING);

  saved_last_completion_failed = last_completion_failed;

  set_completion_defaults (what_to_do);

  saved_line_buffer = rl_line_buffer ? savestring (rl_line_buffer) : (char *)NULL;
  our_func = rl_completion_entry_function
		? rl_completion_entry_function
		: rl_filename_completion_function;
   
  end = rl_point;
  found_quote = delimiter = 0;
  quote_char = '\0';

  if (rl_point)
     
    quote_char = _rl_find_completion_word (&found_quote, &delimiter);

  start = rl_point;
  rl_point = end;

  text = rl_copy_text (start, end);
  matches = gen_completion_matches (text, start, end, our_func, found_quote, quote_char);
   
  nontrivial_lcd = matches && compare_match (text, matches[0]) != 0;
  if (what_to_do == '!' || what_to_do == '@')
    tlen = strlen (text);
  xfree (text);

  if (matches == 0)
    {
      rl_ding ();
      FREE (saved_line_buffer);
      completion_changed_buffer = 0;
      last_completion_failed = 1;
      RL_UNSETSTATE(RL_STATE_COMPLETING);
      _rl_reset_completion_state ();
      return (0);
    }

   
  i = rl_filename_completion_desired;

  if (postprocess_matches (&matches, i) == 0)
    {
      rl_ding ();
      FREE (saved_line_buffer);
      completion_changed_buffer = 0;
      last_completion_failed = 1;
      RL_UNSETSTATE(RL_STATE_COMPLETING);
      _rl_reset_completion_state ();
      return (0);
    }

  if (matches && matches[0] && *matches[0])
    last_completion_failed = 0;

  switch (what_to_do)
    {
    case TAB:
    case '!':
    case '@':
       
      if (what_to_do == TAB)
        {
          if (*matches[0])
	    insert_match (matches[0], start, matches[1] ? MULT_MATCH : SINGLE_MATCH, &quote_char);
        }
      else if (*matches[0] && matches[1] == 0)
	 
	insert_match (matches[0], start, matches[1] ? MULT_MATCH : SINGLE_MATCH, &quote_char);
      else if (*matches[0])	 
	{
	  mlen = *matches[0] ? strlen (matches[0]) : 0;
	  if (mlen >= tlen)
	    insert_match (matches[0], start, matches[1] ? MULT_MATCH : SINGLE_MATCH, &quote_char);
	}

       
      if (matches[1])
	{
	  if (what_to_do == '!')
	    {
	      display_matches (matches);
	      break;
	    }
	  else if (what_to_do == '@')
	    {
	      if (nontrivial_lcd == 0)
		display_matches (matches);
	      break;
	    }
	  else if (rl_editing_mode != vi_mode)
	    rl_ding ();	 
	}
      else
	append_to_match (matches[0], delimiter, quote_char, nontrivial_lcd);

      break;

    case '*':
      insert_all_matches (matches, start, &quote_char);
      break;

    case '?':
       
      if (saved_last_completion_failed && matches[0] && *matches[0] && matches[1] == 0)
	{
	  insert_match (matches[0], start, matches[1] ? MULT_MATCH : SINGLE_MATCH, &quote_char);
	  append_to_match (matches[0], delimiter, quote_char, nontrivial_lcd);
	  break;
	}
      
      if (rl_completion_display_matches_hook == 0)
	{
	  _rl_sigcleanup = _rl_complete_sigcleanup;
	  _rl_sigcleanarg = matches;
	  _rl_complete_display_matches_interrupt = 0;
	}
      display_matches (matches);
      if (_rl_complete_display_matches_interrupt)
        {
          matches = 0;		 
          _rl_complete_display_matches_interrupt = 0;
	  if (rl_signal_event_hook)
	    (*rl_signal_event_hook) ();		 
        }
      _rl_sigcleanup = 0;
      _rl_sigcleanarg = 0;
      break;

    default:
      _rl_ttymsg ("bad value %d for what_to_do in rl_complete", what_to_do);
      rl_ding ();
      FREE (saved_line_buffer);
      RL_UNSETSTATE(RL_STATE_COMPLETING);
      _rl_free_match_list (matches);
      _rl_reset_completion_state ();
      return 1;
    }

  _rl_free_match_list (matches);

   
  if (saved_line_buffer)
    {
      completion_changed_buffer = strcmp (rl_line_buffer, saved_line_buffer) != 0;
      xfree (saved_line_buffer);
    }

  RL_UNSETSTATE(RL_STATE_COMPLETING);
  _rl_reset_completion_state ();

  RL_CHECK_SIGNALS ();
  return 0;
}

 
 
 
 
 

 
char **
rl_completion_matches (const char *text, rl_compentry_func_t *entry_function)
{
  register int i;

   
  int match_list_size;

   
  char **match_list;

   
  int matches;

   
  char *string;

  matches = 0;
  match_list_size = 10;
  match_list = (char **)xmalloc ((match_list_size + 1) * sizeof (char *));
  match_list[1] = (char *)NULL;

  while (string = (*entry_function) (text, matches))
    {
      if (RL_SIG_RECEIVED ())
	{
	   
	  if (entry_function == rl_filename_completion_function)
	    {
	      for (i = 1; match_list[i]; i++)
		xfree (match_list[i]);
	    }
	  xfree (match_list);
	  match_list = 0;
	  match_list_size = 0;
	  matches = 0;
	  RL_CHECK_SIGNALS ();
	}

      if (matches + 1 >= match_list_size)
	match_list = (char **)xrealloc
	  (match_list, ((match_list_size += 10) + 1) * sizeof (char *));

      if (match_list == 0)
	return (match_list);

      match_list[++matches] = string;
      match_list[matches + 1] = (char *)NULL;
    }

   
  if (matches)
    compute_lcd_of_matches (match_list, matches, text);
  else				 
    {
      xfree (match_list);
      match_list = (char **)NULL;
    }
  return (match_list);
}

 
char *
rl_username_completion_function (const char *text, int state)
{
#if defined (__WIN32__) || defined (__OPENNT)
  return (char *)NULL;
#else  
  static char *username = (char *)NULL;
  static struct passwd *entry;
  static int namelen, first_char, first_char_loc;
  char *value;

  if (state == 0)
    {
      FREE (username);

      first_char = *text;
      first_char_loc = first_char == '~';

      username = savestring (&text[first_char_loc]);
      namelen = strlen (username);
#if defined (HAVE_GETPWENT)
      setpwent ();
#endif
    }

#if defined (HAVE_GETPWENT)
  while (entry = getpwent ())
    {
       
      if (namelen == 0 || (STREQN (username, entry->pw_name, namelen)))
	break;
    }
#endif

  if (entry == 0)
    {
#if defined (HAVE_GETPWENT)
      endpwent ();
#endif
      return ((char *)NULL);
    }
  else
    {
      value = (char *)xmalloc (2 + strlen (entry->pw_name));

      *value = *text;

      strcpy (value + first_char_loc, entry->pw_name);

      if (first_char == '~')
	rl_filename_completion_desired = 1;

      return (value);
    }
#endif  
}

 
static int
complete_fncmp (const char *convfn, int convlen, const char *filename, int filename_len)
{
  register char *s1, *s2;
  int d, len;
#if defined (HANDLE_MULTIBYTE)
  size_t v1, v2;
  mbstate_t ps1, ps2;
  WCHAR_T wc1, wc2;
#endif

#if defined (HANDLE_MULTIBYTE)
  memset (&ps1, 0, sizeof (mbstate_t));
  memset (&ps2, 0, sizeof (mbstate_t));
#endif

  if (filename_len == 0)
    return 1;
  if (convlen < filename_len)
    return 0;

  len = filename_len;
  s1 = (char *)convfn;
  s2 = (char *)filename;

   
  if (_rl_completion_case_fold && _rl_completion_case_map)
    {
       
#if defined (HANDLE_MULTIBYTE)
      if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
	{
	  do
	    {
	      v1 = MBRTOWC (&wc1, s1, convlen, &ps1);
	      v2 = MBRTOWC (&wc2, s2, filename_len, &ps2);
	      if (v1 == 0 && v2 == 0)
		return 1;
	      else if (MB_INVALIDCH (v1) || MB_INVALIDCH (v2))
		{
		  if (*s1 != *s2)		 
		    return 0;
		  else if ((*s1 == '-' || *s1 == '_') && (*s2 == '-' || *s2 == '_'))
		    return 0;
		  s1++; s2++; len--;
		  continue;
		}
	      wc1 = towlower (wc1);
	      wc2 = towlower (wc2);
	      s1 += v1;
	      s2 += v1;
	      len -= v1;
	      if ((wc1 == L'-' || wc1 == L'_') && (wc2 == L'-' || wc2 == L'_'))
	        continue;
	      if (wc1 != wc2)
		return 0;
	    }
	  while (len != 0);
	}
      else
#endif
	{
	do
	  {
	    d = _rl_to_lower (*s1) - _rl_to_lower (*s2);
	     
	    if ((*s1 == '-' || *s1 == '_') && (*s2 == '-' || *s2 == '_'))
	      d = 0;
	    if (d != 0)
	      return 0;
	    s1++; s2++;	 
	  }
	while (--len != 0);
	}

      return 1;
    }
  else if (_rl_completion_case_fold)
    {
#if defined (HANDLE_MULTIBYTE)
      if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
	{
	  do
	    {
	      v1 = MBRTOWC (&wc1, s1, convlen, &ps1);
	      v2 = MBRTOWC (&wc2, s2, filename_len, &ps2);
	      if (v1 == 0 && v2 == 0)
		return 1;
	      else if (MB_INVALIDCH (v1) || MB_INVALIDCH (v2))
		{
		  if (*s1 != *s2)		 
		    return 0;
		  s1++; s2++; len--;
		  continue;
		}
	      wc1 = towlower (wc1);
	      wc2 = towlower (wc2);
	      if (wc1 != wc2)
		return 0;
	      s1 += v1;
	      s2 += v1;
	      len -= v1;
	    }
	  while (len != 0);
	  return 1;
	}
      else
#endif
      if ((_rl_to_lower (convfn[0]) == _rl_to_lower (filename[0])) &&
	  (convlen >= filename_len) &&
	  (_rl_strnicmp (filename, convfn, filename_len) == 0))
	return 1;
    }
  else
    {
      if ((convfn[0] == filename[0]) &&
	  (convlen >= filename_len) &&
	  (strncmp (filename, convfn, filename_len) == 0))
	return 1;
    }
  return 0;
}

 
char *
rl_filename_completion_function (const char *text, int state)
{
  static DIR *directory = (DIR *)NULL;
  static char *filename = (char *)NULL;
  static char *dirname = (char *)NULL;
  static char *users_dirname = (char *)NULL;
  static int filename_len;
  char *temp, *dentry, *convfn;
  int dirlen, dentlen, convlen;
  int tilde_dirname;
  struct dirent *entry;

   
  if (state == 0)
    {
       
      if (directory)
	{
	  closedir (directory);
	  directory = (DIR *)NULL;
	}
      FREE (dirname);
      FREE (filename);
      FREE (users_dirname);

      filename = savestring (text);
      if (*text == 0)
	text = ".";
      dirname = savestring (text);

      temp = strrchr (dirname, '/');

#if defined (__MSDOS__) || defined (_WIN32)
       
      if (dirname[0] == '/' && dirname[1] == '/' && ISALPHA ((unsigned char)dirname[2]) && dirname[3] == '/')
        temp = strrchr (dirname + 3, '/');
#endif

      if (temp)
	{
	  strcpy (filename, ++temp);
	  *temp = '\0';
	}
#if defined (__MSDOS__) || (defined (_WIN32) && !defined (__CYGWIN__))
       
      else if (ISALPHA ((unsigned char)dirname[0]) && dirname[1] == ':')
        {
          strcpy (filename, dirname + 2);
          dirname[2] = '\0';
        }
#endif
      else
	{
	  dirname[0] = '.';
	  dirname[1] = '\0';
	}

       

       
      if (rl_completion_found_quote && rl_filename_dequoting_function)
	users_dirname = (*rl_filename_dequoting_function) (dirname, rl_completion_quote_character);
      else
	users_dirname = savestring (dirname);

      tilde_dirname = 0;
      if (*dirname == '~')
	{
	  temp = tilde_expand (dirname);
	  xfree (dirname);
	  dirname = temp;
	  tilde_dirname = 1;
	}

       
      if (rl_directory_rewrite_hook)
	(*rl_directory_rewrite_hook) (&dirname);
      else if (rl_directory_completion_hook && (*rl_directory_completion_hook) (&dirname))
	{
	  xfree (users_dirname);
	  users_dirname = savestring (dirname);
	}
      else if (tilde_dirname == 0 && rl_completion_found_quote && rl_filename_dequoting_function)
	{
	   
	  xfree (dirname);
	  dirname = savestring (users_dirname);
	}
      directory = opendir (dirname);

       
      if (*filename && rl_completion_found_quote && rl_filename_dequoting_function)
	{
	   
	  temp = (*rl_filename_dequoting_function) (filename, rl_completion_quote_character);
	  xfree (filename);
	  filename = temp;
	}
      filename_len = strlen (filename);

      rl_filename_completion_desired = 1;
    }

   
   

   

  entry = (struct dirent *)NULL;
  while (directory && (entry = readdir (directory)))
    {
      convfn = dentry = entry->d_name;
      convlen = dentlen = D_NAMLEN (entry);

      if (rl_filename_rewrite_hook)
	{
	  convfn = (*rl_filename_rewrite_hook) (dentry, dentlen);
	  convlen = (convfn == dentry) ? dentlen : strlen (convfn);
	}

       
      if (filename_len == 0)
	{
	  if (_rl_match_hidden_files == 0 && HIDDEN_FILE (convfn))
	    continue;

	  if (convfn[0] != '.' ||
	       (convfn[1] && (convfn[1] != '.' || convfn[2])))
	    break;
	}
      else
	{
	  if (complete_fncmp (convfn, convlen, filename, filename_len))
	    break;
	}
    }

  if (entry == 0)
    {
      if (directory)
	{
	  closedir (directory);
	  directory = (DIR *)NULL;
	}
      if (dirname)
	{
	  xfree (dirname);
	  dirname = (char *)NULL;
	}
      if (filename)
	{
	  xfree (filename);
	  filename = (char *)NULL;
	}
      if (users_dirname)
	{
	  xfree (users_dirname);
	  users_dirname = (char *)NULL;
	}

      return (char *)NULL;
    }
  else
    {
       
      if (dirname && (dirname[0] != '.' || dirname[1]))
	{
	  if (rl_complete_with_tilde_expansion && *users_dirname == '~')
	    {
	      dirlen = strlen (dirname);
	      temp = (char *)xmalloc (2 + dirlen + D_NAMLEN (entry));
	      strcpy (temp, dirname);
	       
	      if (dirname[dirlen - 1] != '/')
	        {
	          temp[dirlen++] = '/';
	          temp[dirlen] = '\0';
	        }
	    }
	  else
	    {
	      dirlen = strlen (users_dirname);
	      temp = (char *)xmalloc (2 + dirlen + D_NAMLEN (entry));
	      strcpy (temp, users_dirname);
	       
	      if (users_dirname[dirlen - 1] != '/')
		temp[dirlen++] = '/';
	    }

	  strcpy (temp + dirlen, convfn);
	}
      else
	temp = savestring (convfn);

      if (convfn != dentry)
	xfree (convfn);

      return (temp);
    }
}

 
int
rl_old_menu_complete (int count, int invoking_key)
{
  rl_compentry_func_t *our_func;
  int matching_filenames, found_quote;

  static char *orig_text;
  static char **matches = (char **)0;
  static int match_list_index = 0;
  static int match_list_size = 0;
  static int orig_start, orig_end;
  static char quote_char;
  static int delimiter;

   
  if (rl_last_func != rl_old_menu_complete)
    {
       
      FREE (orig_text);
      if (matches)
	_rl_free_match_list (matches);

      match_list_index = match_list_size = 0;
      matches = (char **)NULL;

      rl_completion_invoking_key = invoking_key;

      RL_SETSTATE(RL_STATE_COMPLETING);

       
      set_completion_defaults ('%');

      our_func = rl_menu_completion_entry_function;
      if (our_func == 0)
	our_func = rl_completion_entry_function
			? rl_completion_entry_function
			: rl_filename_completion_function;

       
      orig_end = rl_point;
      found_quote = delimiter = 0;
      quote_char = '\0';

      if (rl_point)
	 
	quote_char = _rl_find_completion_word (&found_quote, &delimiter);

      orig_start = rl_point;
      rl_point = orig_end;

      orig_text = rl_copy_text (orig_start, orig_end);
      matches = gen_completion_matches (orig_text, orig_start, orig_end,
					our_func, found_quote, quote_char);

       
      matching_filenames = rl_filename_completion_desired;

      if (matches == 0 || postprocess_matches (&matches, matching_filenames) == 0)
	{
	  rl_ding ();
	  FREE (matches);
	  matches = (char **)0;
	  FREE (orig_text);
	  orig_text = (char *)0;
	  completion_changed_buffer = 0;
	  RL_UNSETSTATE(RL_STATE_COMPLETING);
	  return (0);
	}

      RL_UNSETSTATE(RL_STATE_COMPLETING);

      for (match_list_size = 0; matches[match_list_size]; match_list_size++)
        ;
       

      if (match_list_size > 1 && _rl_complete_show_all)
	display_matches (matches);
    }

   

  if (matches == 0 || match_list_size == 0) 
    {
      rl_ding ();
      FREE (matches);
      matches = (char **)0;
      completion_changed_buffer = 0;
      return (0);
    }

  match_list_index += count;
  if (match_list_index < 0)
    {
      while (match_list_index < 0)
	match_list_index += match_list_size;
    }
  else
    match_list_index %= match_list_size;

  if (match_list_index == 0 && match_list_size > 1)
    {
      rl_ding ();
      insert_match (orig_text, orig_start, MULT_MATCH, &quote_char);
    }
  else
    {
      insert_match (matches[match_list_index], orig_start, SINGLE_MATCH, &quote_char);
      append_to_match (matches[match_list_index], delimiter, quote_char,
		       compare_match (orig_text, matches[match_list_index]));
    }

  completion_changed_buffer = 1;
  return (0);
}

 
 
int
rl_menu_complete (int count, int ignore)
{
  rl_compentry_func_t *our_func;
  int matching_filenames, found_quote;

  static char *orig_text;
  static char **matches = (char **)0;
  static int match_list_index = 0;
  static int match_list_size = 0;
  static int nontrivial_lcd = 0;
  static int full_completion = 0;	 
  static int orig_start, orig_end;
  static char quote_char;
  static int delimiter, cstate;

   
  if ((rl_last_func != rl_menu_complete && rl_last_func != rl_backward_menu_complete) || full_completion)
    {
       
      FREE (orig_text);
      if (matches)
	_rl_free_match_list (matches);

      match_list_index = match_list_size = 0;
      matches = (char **)NULL;

      full_completion = 0;

      RL_SETSTATE(RL_STATE_COMPLETING);

       
      set_completion_defaults ('%');

      our_func = rl_menu_completion_entry_function;
      if (our_func == 0)
	our_func = rl_completion_entry_function
			? rl_completion_entry_function
			: rl_filename_completion_function;

       
      orig_end = rl_point;
      found_quote = delimiter = 0;
      quote_char = '\0';

      if (rl_point)
	 
	quote_char = _rl_find_completion_word (&found_quote, &delimiter);

      orig_start = rl_point;
      rl_point = orig_end;

      orig_text = rl_copy_text (orig_start, orig_end);
      matches = gen_completion_matches (orig_text, orig_start, orig_end,
					our_func, found_quote, quote_char);

      nontrivial_lcd = matches && compare_match (orig_text, matches[0]) != 0;

       
      matching_filenames = rl_filename_completion_desired;

      if (matches == 0 || postprocess_matches (&matches, matching_filenames) == 0)
	{
	  rl_ding ();
	  FREE (matches);
	  matches = (char **)0;
	  FREE (orig_text);
	  orig_text = (char *)0;
	  completion_changed_buffer = 0;
	  RL_UNSETSTATE(RL_STATE_COMPLETING);
	  return (0);
	}

      RL_UNSETSTATE(RL_STATE_COMPLETING);

      for (match_list_size = 0; matches[match_list_size]; match_list_size++)
        ;

      if (match_list_size == 0) 
	{
	  rl_ding ();
	  FREE (matches);
	  matches = (char **)0;
	  match_list_index = 0;
	  completion_changed_buffer = 0;
	  return (0);
        }

       
      if (*matches[0])
	{
	  insert_match (matches[0], orig_start, matches[1] ? MULT_MATCH : SINGLE_MATCH, &quote_char);
	  orig_end = orig_start + strlen (matches[0]);
	  completion_changed_buffer = STREQ (orig_text, matches[0]) == 0;
	}

      if (match_list_size > 1 && _rl_complete_show_all)
	{
	  display_matches (matches);
	   
	  if (rl_completion_query_items > 0 && match_list_size >= rl_completion_query_items)
	    {
	      rl_ding ();
	      FREE (matches);
	      matches = (char **)0;
	      full_completion = 1;
	      return (0);
	    }
	  else if (_rl_menu_complete_prefix_first)
	    {
	      rl_ding ();
	      return (0);
	    }
	}
      else if (match_list_size <= 1)
	{
	  append_to_match (matches[0], delimiter, quote_char, nontrivial_lcd);
	  full_completion = 1;
	  return (0);
	}
      else if (_rl_menu_complete_prefix_first && match_list_size > 1)
	{
	  rl_ding ();
	  return (0);
	}
    }

   

  if (matches == 0 || match_list_size == 0) 
    {
      rl_ding ();
      FREE (matches);
      matches = (char **)0;
      completion_changed_buffer = 0;
      return (0);
    }

  match_list_index += count;
  if (match_list_index < 0)
    {
      while (match_list_index < 0)
	match_list_index += match_list_size;
    }
  else
    match_list_index %= match_list_size;

  if (match_list_index == 0 && match_list_size > 1)
    {
      rl_ding ();
      insert_match (matches[0], orig_start, MULT_MATCH, &quote_char);
    }
  else
    {
      insert_match (matches[match_list_index], orig_start, SINGLE_MATCH, &quote_char);
      append_to_match (matches[match_list_index], delimiter, quote_char,
		       compare_match (orig_text, matches[match_list_index]));
    }

  completion_changed_buffer = 1;
  return (0);
}

int
rl_backward_menu_complete (int count, int key)
{
   
  return (rl_menu_complete (-count, key));
}
