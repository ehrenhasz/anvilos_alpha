 

 

#include "config.h"

#include "bashtypes.h"
#if defined (HAVE_SYS_PARAM_H)
#  include <sys/param.h>
#endif
#include "posixstat.h"

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include "filecntl.h"
#include "bashansi.h"
#include <stdio.h>
#include "chartypes.h"
#include <errno.h>

#include "bashintl.h"

#include "shell.h"
#include "parser.h"
#include "flags.h"
#include "findcmd.h"
#include "test.h"
#include "trap.h"
#include "pathexp.h"

#include "builtins/common.h"

#if defined (HAVE_MBSTR_H) && defined (HAVE_MBSCHR)
#  include <mbstr.h>		 
#endif

#include <tilde/tilde.h>

#if !defined (errno)
extern int errno;
#endif  

#ifdef __CYGWIN__
#  include <sys/cygwin.h>
#endif

static char *bash_special_tilde_expansions PARAMS((char *));
static int unquoted_tilde_word PARAMS((const char *));
static void initialize_group_array PARAMS((void));

 
const char * const bash_getcwd_errstr = N_("getcwd: cannot access parent directories");

 

static struct {
  int *posix_mode_var;
} posix_vars[] = 
{
  &interactive_comments,
  &source_uses_path,
  &expaliases_flag,
  &inherit_errexit,
  &print_shift_error,
  0
};

static char *saved_posix_vars = 0;

void
posix_initialize (on)
     int on;
{
   
  if (on != 0)
    {
      interactive_comments = source_uses_path = 1;
      expand_aliases = expaliases_flag = 1;
      inherit_errexit = 1;
      source_searches_cwd = 0;
      print_shift_error = 1;
    }

   
  else if (saved_posix_vars)		 
    {
      set_posix_options (saved_posix_vars);
      expand_aliases = expaliases_flag;
      free (saved_posix_vars);
      saved_posix_vars = 0;
    }
  else	 
    {
      source_searches_cwd = 1;
      expand_aliases = expaliases_flag = interactive_shell;	 
      print_shift_error = 0;
    }
}

int
num_posix_options ()
{
  return ((sizeof (posix_vars) / sizeof (posix_vars[0])) - 1);
}

char *
get_posix_options (bitmap)
     char *bitmap;
{
  register int i;

  if (bitmap == 0)
    bitmap = (char *)xmalloc (num_posix_options ());	 
  for (i = 0; posix_vars[i].posix_mode_var; i++)
    bitmap[i] = *(posix_vars[i].posix_mode_var);
  return bitmap;
}

#undef save_posix_options
void
save_posix_options ()
{
  saved_posix_vars = get_posix_options (saved_posix_vars);
}

void
set_posix_options (bitmap)
     const char *bitmap;
{
  register int i;

  for (i = 0; posix_vars[i].posix_mode_var; i++)
    *(posix_vars[i].posix_mode_var) = bitmap[i];
}

 
 
 
 
 

#if defined (RLIMTYPE)
RLIMTYPE
string_to_rlimtype (s)
     char *s;
{
  RLIMTYPE ret;
  int neg;

  ret = 0;
  neg = 0;
  while (s && *s && whitespace (*s))
    s++;
  if (s && (*s == '-' || *s == '+'))
    {
      neg = *s == '-';
      s++;
    }
  for ( ; s && *s && DIGIT (*s); s++)
    ret = (ret * 10) + TODIGIT (*s);
  return (neg ? -ret : ret);
}

void
print_rlimtype (n, addnl)
     RLIMTYPE n;
     int addnl;
{
  char s[INT_STRLEN_BOUND (RLIMTYPE) + 1], *p;

  p = s + sizeof(s);
  *--p = '\0';

  if (n < 0)
    {
      do
	*--p = '0' - n % 10;
      while ((n /= 10) != 0);

      *--p = '-';
    }
  else
    {
      do
	*--p = '0' + n % 10;
      while ((n /= 10) != 0);
    }

  printf ("%s%s", p, addnl ? "\n" : "");
}
#endif  

 
 
 
 
 

 
int
all_digits (string)
     const char *string;
{
  register const char *s;

  for (s = string; *s; s++)
    if (DIGIT (*s) == 0)
      return (0);

  return (1);
}

 
int
legal_number (string, result)
     const char *string;
     intmax_t *result;
{
  intmax_t value;
  char *ep;

  if (result)
    *result = 0;

  if (string == 0)
    return 0;

  errno = 0;
  value = strtoimax (string, &ep, 10);
  if (errno || ep == string)
    return 0;	 

   
  while (whitespace (*ep))
    ep++;

   
  if (*string && *ep == '\0')
    {
      if (result)
	*result = value;
       
      return 1;
    }
    
  return (0);
}

 
int
legal_identifier (name)
     const char *name;
{
  register const char *s;
  unsigned char c;

  if (!name || !(c = *name) || (legal_variable_starter (c) == 0))
    return (0);

  for (s = name + 1; (c = *s) != 0; s++)
    {
      if (legal_variable_char (c) == 0)
	return (0);
    }
  return (1);
}

 
int
valid_nameref_value (name, flags)
     const char *name;
     int flags;
{
  if (name == 0 || *name == 0)
    return 0;

   
#if defined (ARRAY_VARS)  
  if (legal_identifier (name) || (flags != 2 && valid_array_reference (name, 0)))
#else
  if (legal_identifier (name))
#endif
    return 1;

  return 0;
}

int
check_selfref (name, value, flags)
     const char *name;
     char *value;
     int flags;
{
  char *t;

  if (STREQ (name, value))
    return 1;

#if defined (ARRAY_VARS)
  if (valid_array_reference (value, 0))
    {
      t = array_variable_name (value, 0, (char **)NULL, (int *)NULL);
      if (t && STREQ (name, t))
	{
	  free (t);
	  return 1;
	}
      free (t);
    }
#endif

  return 0;	 
}

 
int
check_identifier (word, check_word)
     WORD_DESC *word;
     int check_word;
{
  if (word->flags & (W_HASDOLLAR|W_QUOTED))	 
    {
      internal_error (_("`%s': not a valid identifier"), word->word);
      return (0);
    }
  else if (check_word && (all_digits (word->word) || legal_identifier (word->word) == 0))
    {
      internal_error (_("`%s': not a valid identifier"), word->word);
      return (0);
    }
  else
    return (1);
}

 
int
importable_function_name (string, len)
     const char *string;
     size_t len;
{
  if (absolute_program (string))	 
    return 0;
  if (*string == '\n')			 
    return 0;
  if (shellblank (*string) || shellblank(string[len-1]))
    return 0;
  return (posixly_correct ? legal_identifier (string) : 1);
}

int
exportable_function_name (string)
     const char *string;
{
  if (absolute_program (string))
    return 0;
  if (mbschr (string, '=') != 0)
    return 0;
  return 1;
}

 
int
legal_alias_name (string, flags)
     const char *string;
     int flags;
{
  register const char *s;

  for (s = string; *s; s++)
    if (shellbreak (*s) || shellxquote (*s) || shellexp (*s) || (*s == '/'))
      return 0;
  return 1;
}

 
int
assignment (string, flags)
     const char *string;
     int flags;
{
  register unsigned char c;
  register int newi, indx;

  c = string[indx = 0];

#if defined (ARRAY_VARS)
   
  if ((flags & 1) && c != '[')		 
    return (0);
  else if ((flags & 1) == 0 && legal_variable_starter (c) == 0)
#else
  if (legal_variable_starter (c) == 0)
#endif
    return (0);

  while (c = string[indx])
    {
       
      if (c == '=')
	return (indx);

#if defined (ARRAY_VARS)
      if (c == '[')
	{
	  newi = skipsubscript (string, indx, (flags & 2) ? 1 : 0);
	   
	  if (string[newi++] != ']')
	    return (0);
	  if (string[newi] == '+' && string[newi+1] == '=')
	    return (newi + 1);
	  return ((string[newi] == '=') ? newi : 0);
	}
#endif  

       
      if (c == '+' && string[indx+1] == '=')
	return (indx + 1);

       
      if (legal_variable_char (c) == 0)
	return (0);

      indx++;
    }
  return (0);
}

int
line_isblank (line)
     const char *line;
{
  register int i;

  if (line == 0)
    return 0;		 
  for (i = 0; line[i]; i++)
    if (isblank ((unsigned char)line[i]) == 0)
      break;
  return (line[i] == '\0');  
}

 
 
 
 
 

 

#if !defined (O_NDELAY)
#  if defined (FNDELAY)
#    define O_NDELAY FNDELAY
#  endif
#endif  

 
int
sh_unset_nodelay_mode (fd)
     int fd;
{
  int flags, bflags;

  if ((flags = fcntl (fd, F_GETFL, 0)) < 0)
    return -1;

  bflags = 0;

   
#ifdef O_NONBLOCK
  bflags |= O_NONBLOCK;
#endif

#ifdef O_NDELAY
  bflags |= O_NDELAY;
#endif

  if (flags & bflags)
    {
      flags &= ~bflags;
      return (fcntl (fd, F_SETFL, flags));
    }

  return 0;
}

 
int
sh_setclexec (fd)
     int fd;
{
  return (SET_CLOSE_ON_EXEC (fd));
}

 
int
sh_validfd (fd)
     int fd;
{
  return (fcntl (fd, F_GETFD, 0) >= 0);
}

int
fd_ispipe (fd)
     int fd;
{
  errno = 0;
  return ((lseek (fd, 0L, SEEK_CUR) < 0) && (errno == ESPIPE));
}

 

#if defined (__BEOS__)
 
#  undef O_NONBLOCK
#  define O_NONBLOCK 0
#endif  

void
check_dev_tty ()
{
  int tty_fd;
  char *tty;

  tty_fd = open ("/dev/tty", O_RDWR|O_NONBLOCK);

  if (tty_fd < 0)
    {
      tty = (char *)ttyname (fileno (stdin));
      if (tty == 0)
	return;
      tty_fd = open (tty, O_RDWR|O_NONBLOCK);
    }
  if (tty_fd >= 0)
    close (tty_fd);
}

 
int
same_file (path1, path2, stp1, stp2)
     const char *path1, *path2;
     struct stat *stp1, *stp2;
{
  struct stat st1, st2;

  if (stp1 == NULL)
    {
      if (stat (path1, &st1) != 0)
	return (0);
      stp1 = &st1;
    }

  if (stp2 == NULL)
    {
      if (stat (path2, &st2) != 0)
	return (0);
      stp2 = &st2;
    }

  return ((stp1->st_dev == stp2->st_dev) && (stp1->st_ino == stp2->st_ino));
}

 
int
move_to_high_fd (fd, check_new, maxfd)
     int fd, check_new, maxfd;
{
  int script_fd, nfds, ignore;

  if (maxfd < 20)
    {
      nfds = getdtablesize ();
      if (nfds <= 0)
	nfds = 20;
      if (nfds > HIGH_FD_MAX)
	nfds = HIGH_FD_MAX;		 
    }
  else
    nfds = maxfd;

  for (nfds--; check_new && nfds > 3; nfds--)
    if (fcntl (nfds, F_GETFD, &ignore) == -1)
      break;

  if (nfds > 3 && fd != nfds && (script_fd = dup2 (fd, nfds)) != -1)
    {
      if (check_new == 0 || fd != fileno (stderr))	 
	close (fd);
      return (script_fd);
    }

   
  return (fd);
}
 
 

int
check_binary_file (sample, sample_len)
     const char *sample;
     int sample_len;
{
  register int i;
  int nline;
  unsigned char c;

  if (sample_len >= 4 && sample[0] == 0x7f && sample[1] == 'E' && sample[2] == 'L' && sample[3] == 'F')
    return 1;

   
  nline = (sample[0] == '#' && sample[1] == '!') ? 2 : 1;

  for (i = 0; i < sample_len; i++)
    {
      c = sample[i];
      if (c == '\n' && --nline == 0)
	return (0);
      if (c == '\0')
	return (1);
    }

  return (0);
}

 
 
 
 
 

int
sh_openpipe (pv)
     int *pv;
{
  int r;

  if ((r = pipe (pv)) < 0)
    return r;

  pv[0] = move_to_high_fd (pv[0], 1, 64);
  pv[1] = move_to_high_fd (pv[1], 1, 64);

  return 0;  
}

int
sh_closepipe (pv)
     int *pv;
{
  if (pv[0] >= 0)
    close (pv[0]);

  if (pv[1] >= 0)
    close (pv[1]);

  pv[0] = pv[1] = -1;
  return 0;
}

 
 
 
 
 

int
file_exists (fn)
     const char *fn;
{
  struct stat sb;

  return (stat (fn, &sb) == 0);
}

int
file_isdir (fn)
     const char *fn;
{
  struct stat sb;

  return ((stat (fn, &sb) == 0) && S_ISDIR (sb.st_mode));
}

int
file_iswdir (fn)
     const char *fn;
{
  return (file_isdir (fn) && sh_eaccess (fn, W_OK) == 0);
}

 
int
path_dot_or_dotdot (string)
     const char *string;
{
  if (string == 0 || *string == '\0' || *string != '.')
    return (0);

   
  if (PATHSEP(string[1]) || (string[1] == '.' && PATHSEP(string[2])))
    return (1);

  return (0);
}

 
int
absolute_pathname (string)
     const char *string;
{
  if (string == 0 || *string == '\0')
    return (0);

  if (ABSPATH(string))
    return (1);

  if (string[0] == '.' && PATHSEP(string[1]))	 
    return (1);

  if (string[0] == '.' && string[1] == '.' && PATHSEP(string[2]))	 
    return (1);

  return (0);
}

 
int
absolute_program (string)
     const char *string;
{
  return ((char *)mbschr (string, '/') != (char *)NULL);
}

 
 
 
 
 

 
char *
make_absolute (string, dot_path)
     const char *string, *dot_path;
{
  char *result;

  if (dot_path == 0 || ABSPATH(string))
#ifdef __CYGWIN__
    {
      char pathbuf[PATH_MAX + 1];

       
      cygwin_conv_path (CCP_WIN_A_TO_POSIX, string, pathbuf, PATH_MAX);
      result = savestring (pathbuf);
    }
#else
    result = savestring (string);
#endif
  else
    result = sh_makepath (dot_path, string, 0);

  return (result);
}

 
char *
base_pathname (string)
     char *string;
{
  char *p;

#if 0
  if (absolute_pathname (string) == 0)
    return (string);
#endif

  if (string[0] == '/' && string[1] == 0)
    return (string);

  p = (char *)strrchr (string, '/');
  return (p ? ++p : string);
}

 
char *
full_pathname (file)
     char *file;
{
  char *ret;

  file = (*file == '~') ? bash_tilde_expand (file, 0) : savestring (file);

  if (ABSPATH(file))
    return (file);

  ret = sh_makepath ((char *)NULL, file, (MP_DOCWD|MP_RMDOT));
  free (file);

  return (ret);
}

 
static char tdir[PATH_MAX];

 
char *
polite_directory_format (name)
     char *name;
{
  char *home;
  int l;

  home = get_string_value ("HOME");
  l = home ? strlen (home) : 0;
  if (l > 1 && strncmp (home, name, l) == 0 && (!name[l] || name[l] == '/'))
    {
      strncpy (tdir + 1, name + l, sizeof(tdir) - 2);
      tdir[0] = '~';
      tdir[sizeof(tdir) - 1] = '\0';
      return (tdir);
    }
  else
    return (name);
}

 
char *
trim_pathname (name, maxlen)
     char *name;
     int maxlen;
{
  int nlen, ndirs;
  intmax_t nskip;
  char *nbeg, *nend, *ntail, *v;

  if (name == 0 || (nlen = strlen (name)) == 0)
    return name;
  nend = name + nlen;

  v = get_string_value ("PROMPT_DIRTRIM");
  if (v == 0 || *v == 0)
    return name;
  if (legal_number (v, &nskip) == 0 || nskip <= 0)
    return name;

   
  nbeg = name;
  if (name[0] == '~')
    for (nbeg = name; *nbeg; nbeg++)
      if (*nbeg == '/')
	{
	  nbeg++;
	  break;
	}
  if (*nbeg == 0)
    return name;

  for (ndirs = 0, ntail = nbeg; *ntail; ntail++)
    if (*ntail == '/')
      ndirs++;
  if (ndirs < nskip)
    return name;

  for (ntail = (*nend == '/') ? nend : nend - 1; ntail > nbeg; ntail--)
    {
      if (*ntail == '/')
	nskip--;
      if (nskip == 0)
	break;
    }
  if (ntail == nbeg)
    return name;

   
  nlen = ntail - nbeg;
  if (nlen <= 3)
    return name;

  *nbeg++ = '.';
  *nbeg++ = '.';
  *nbeg++ = '.';

  nlen = nend - ntail;
  memmove (nbeg, ntail, nlen);
  nbeg[nlen] = '\0';

  return name;
}

 
char *
printable_filename (fn, flags)
     char *fn;
     int flags;
{
  char *newf;

  if (ansic_shouldquote (fn))
    newf = ansic_quote (fn, 0, NULL);
  else if (flags && sh_contains_shell_metas (fn))
    newf = sh_single_quote (fn);
  else
    newf = fn;

  return newf;
}

 
char *
extract_colon_unit (string, p_index)
     char *string;
     int *p_index;
{
  int i, start, len;
  char *value;

  if (string == 0)
    return (string);

  len = strlen (string);
  if (*p_index >= len)
    return ((char *)NULL);

  i = *p_index;

   
  if (i && string[i] == ':')
    i++;

  for (start = i; string[i] && string[i] != ':'; i++)
    ;

  *p_index = i;

  if (i == start)
    {
      if (string[i])
	(*p_index)++;
       
      value = (char *)xmalloc (1);
      value[0] = '\0';
    }
  else
    value = substring (string, start, i);

  return (value);
}

 
 
 
 
 

#if defined (PUSHD_AND_POPD)
extern char *get_dirstack_from_string PARAMS((char *));
#endif

static char **bash_tilde_prefixes;
static char **bash_tilde_prefixes2;
static char **bash_tilde_suffixes;
static char **bash_tilde_suffixes2;

 
static char *
bash_special_tilde_expansions (text)
     char *text;
{
  char *result;

  result = (char *)NULL;

  if (text[0] == '+' && text[1] == '\0')
    result = get_string_value ("PWD");
  else if (text[0] == '-' && text[1] == '\0')
    result = get_string_value ("OLDPWD");
#if defined (PUSHD_AND_POPD)
  else if (DIGIT (*text) || ((*text == '+' || *text == '-') && DIGIT (text[1])))
    result = get_dirstack_from_string (text);
#endif

  return (result ? savestring (result) : (char *)NULL);
}

 
void
tilde_initialize ()
{
  static int times_called = 0;

   
  tilde_expansion_preexpansion_hook = bash_special_tilde_expansions;

   
  if (times_called++ == 0)
    {
      bash_tilde_prefixes = strvec_create (3);
      bash_tilde_prefixes[0] = "=~";
      bash_tilde_prefixes[1] = ":~";
      bash_tilde_prefixes[2] = (char *)NULL;

      bash_tilde_prefixes2 = strvec_create (2);
      bash_tilde_prefixes2[0] = ":~";
      bash_tilde_prefixes2[1] = (char *)NULL;

      tilde_additional_prefixes = bash_tilde_prefixes;

      bash_tilde_suffixes = strvec_create (3);
      bash_tilde_suffixes[0] = ":";
      bash_tilde_suffixes[1] = "=~";	 
      bash_tilde_suffixes[2] = (char *)NULL;

      tilde_additional_suffixes = bash_tilde_suffixes;

      bash_tilde_suffixes2 = strvec_create (2);
      bash_tilde_suffixes2[0] = ":";
      bash_tilde_suffixes2[1] = (char *)NULL;
    }
}

 

#define TILDE_END(c)	((c) == '\0' || (c) == '/' || (c) == ':')

static int
unquoted_tilde_word (s)
     const char *s;
{
  const char *r;

  for (r = s; TILDE_END(*r) == 0; r++)
    {
      switch (*r)
	{
	case '\\':
	case '\'':
	case '"':
	  return 0;
	}
    }
  return 1;
}

 
char *
bash_tilde_find_word (s, flags, lenp)
     const char *s;
     int flags, *lenp;
{
  const char *r;
  char *ret;
  int l;

  for (r = s; *r && *r != '/'; r++)
    {
       
      if (*r == '\\' || *r == '\'' || *r == '"')  
	{
	  ret = savestring (s);
	  if (lenp)
	    *lenp = 0;
	  return ret;
	}
      else if (flags && *r == ':')
	break;
    }
  l = r - s;
  ret = xmalloc (l + 1);
  strncpy (ret, s, l);
  ret[l] = '\0';
  if (lenp)
    *lenp = l;
  return ret;
}
    
 
char *
bash_tilde_expand (s, assign_p)
     const char *s;
     int assign_p;
{
  int r;
  char *ret;

  tilde_additional_prefixes = assign_p == 0 ? (char **)0
  					    : (assign_p == 2 ? bash_tilde_prefixes2 : bash_tilde_prefixes);
  if (assign_p == 2)
    tilde_additional_suffixes = bash_tilde_suffixes2;

  r = (*s == '~') ? unquoted_tilde_word (s) : 1;
  ret = r ? tilde_expand (s) : savestring (s);

  QUIT;

  return (ret);
}

 
 
 
 
 

static int ngroups, maxgroups;

 
static GETGROUPS_T *group_array = (GETGROUPS_T *)NULL;

#if !defined (NOGROUP)
#  define NOGROUP (gid_t) -1
#endif

static void
initialize_group_array ()
{
  register int i;

  if (maxgroups == 0)
    maxgroups = getmaxgroups ();

  ngroups = 0;
  group_array = (GETGROUPS_T *)xrealloc (group_array, maxgroups * sizeof (GETGROUPS_T));

#if defined (HAVE_GETGROUPS)
  ngroups = getgroups (maxgroups, group_array);
#endif

   
  if (ngroups == 0)
    {
      group_array[0] = current_user.gid;
      ngroups = 1;
    }

   
  for (i = 0; i < ngroups; i++)
    if (current_user.gid == (gid_t)group_array[i])
      break;
  if (i == ngroups && ngroups < maxgroups)
    {
      for (i = ngroups; i > 0; i--)
	group_array[i] = group_array[i - 1];
      group_array[0] = current_user.gid;
      ngroups++;
    }

   
  if (group_array[0] != current_user.gid)
    {
      for (i = 0; i < ngroups; i++)
	if (group_array[i] == current_user.gid)
	  break;
      if (i < ngroups)
	{
	  group_array[i] = group_array[0];
	  group_array[0] = current_user.gid;
	}
    }
}

 
int
#if defined (__STDC__) || defined ( _MINIX)
group_member (gid_t gid)
#else
group_member (gid)
     gid_t gid;
#endif  
{
#if defined (HAVE_GETGROUPS)
  register int i;
#endif

   
  if (gid == current_user.gid || gid == current_user.egid)
    return (1);

#if defined (HAVE_GETGROUPS)
  if (ngroups == 0)
    initialize_group_array ();

   
  if (ngroups <= 0)
    return (0);

   
  for (i = 0; i < ngroups; i++)
    if (gid == (gid_t)group_array[i])
      return (1);
#endif

  return (0);
}

char **
get_group_list (ngp)
     int *ngp;
{
  static char **group_vector = (char **)NULL;
  register int i;

  if (group_vector)
    {
      if (ngp)
	*ngp = ngroups;
      return group_vector;
    }

  if (ngroups == 0)
    initialize_group_array ();

  if (ngroups <= 0)
    {
      if (ngp)
	*ngp = 0;
      return (char **)NULL;
    }

  group_vector = strvec_create (ngroups);
  for (i = 0; i < ngroups; i++)
    group_vector[i] = itos (group_array[i]);

  if (ngp)
    *ngp = ngroups;
  return group_vector;
}

int *
get_group_array (ngp)
     int *ngp;
{
  int i;
  static int *group_iarray = (int *)NULL;

  if (group_iarray)
    {
      if (ngp)
	*ngp = ngroups;
      return (group_iarray);
    }

  if (ngroups == 0)
    initialize_group_array ();    

  if (ngroups <= 0)
    {
      if (ngp)
	*ngp = 0;
      return (int *)NULL;
    }

  group_iarray = (int *)xmalloc (ngroups * sizeof (int));
  for (i = 0; i < ngroups; i++)
    group_iarray[i] = (int)group_array[i];

  if (ngp)
    *ngp = ngroups;
  return group_iarray;
}

 
 
 
 
 

 
char *
conf_standard_path ()
{
#if defined (_CS_PATH) && defined (HAVE_CONFSTR)
  char *p;
  size_t len;

  len = (size_t)confstr (_CS_PATH, (char *)NULL, (size_t)0);
  if (len > 0)
    {
      p = (char *)xmalloc (len + 2);
      *p = '\0';
      confstr (_CS_PATH, p, len);
      return (p);
    }
  else
    return (savestring (STANDARD_UTILS_PATH));
#else  
#  if defined (CS_PATH)
  return (savestring (CS_PATH));
#  else
  return (savestring (STANDARD_UTILS_PATH));
#  endif  
#endif  
}

int
default_columns ()
{
  char *v;
  int c;

  c = -1;
  v = get_string_value ("COLUMNS");
  if (v && *v)
    {
      c = atoi (v);
      if (c > 0)
	return c;
    }

  if (check_window_size)
    get_new_window_size (0, (int *)0, &c);

  return (c > 0 ? c : 80);
}

  
