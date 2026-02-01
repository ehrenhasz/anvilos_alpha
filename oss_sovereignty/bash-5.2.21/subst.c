 

 

 

#include "config.h"

#include "bashtypes.h"
#include <stdio.h>
#include "chartypes.h"
#if defined (HAVE_PWD_H)
#  include <pwd.h>
#endif
#include <signal.h>
#include <errno.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#define NEED_FPURGE_DECL

#include "bashansi.h"
#include "posixstat.h"
#include "bashintl.h"

#include "shell.h"
#include "parser.h"
#include "redir.h"
#include "flags.h"
#include "jobs.h"
#include "execute_cmd.h"
#include "filecntl.h"
#include "trap.h"
#include "pathexp.h"
#include "mailcheck.h"

#include "shmbutil.h"
#if defined (HAVE_MBSTR_H) && defined (HAVE_MBSCHR)
#  include <mbstr.h>		 
#endif
#include "typemax.h"

#include "builtins/getopt.h"
#include "builtins/common.h"

#include "builtins/builtext.h"

#include <tilde/tilde.h>
#include <glob/strmatch.h>

#if !defined (errno)
extern int errno;
#endif  

 
#define DEFAULT_INITIAL_ARRAY_SIZE 112
#define DEFAULT_ARRAY_SIZE 128

 
#define VT_VARIABLE	0
#define VT_POSPARMS	1
#define VT_ARRAYVAR	2
#define VT_ARRAYMEMBER	3
#define VT_ASSOCVAR	4

#define VT_STARSUB	128	 

 
#define ST_BACKSL	0x01
#define ST_CTLESC	0x02
#define ST_SQUOTE	0x04	 
#define ST_DQUOTE	0x08	 

 
#define LBRACE		'{'
#define RBRACE		'}'
#define LPAREN		'('
#define RPAREN		')'
#define LBRACK		'['
#define RBRACK		']'

#if defined (HANDLE_MULTIBYTE)
#define WLPAREN		L'('
#define WRPAREN		L')'
#endif

#define DOLLAR_AT_STAR(c)	((c) == '@' || (c) == '*')
#define STR_DOLLAR_AT_STAR(s)	(DOLLAR_AT_STAR ((s)[0]) && (s)[1] == '\0')

 
#define VALID_SPECIAL_LENGTH_PARAM(c) \
  ((c) == '-' || (c) == '?' || (c) == '#' || (c) == '@')

 
#define VALID_INDIR_PARAM(c) \
  ((posixly_correct == 0 && (c) == '#') || (posixly_correct == 0 && (c) == '?') || (c) == '@' || (c) == '*')

 
#define VALID_PARAM_EXPAND_CHAR(c) (sh_syntaxtab[(unsigned char)c] & CSUBSTOP)

 
#define SPECIAL_VAR(name, wi) \
 (*name && ((DIGIT (*name) && all_digits (name)) || \
      (name[1] == '\0' && (sh_syntaxtab[(unsigned char)*name] & CSPECVAR)) || \
      (wi && name[2] == '\0' && VALID_INDIR_PARAM (name[1]))))

 

#define CHECK_STRING_OVERRUN(oind, ind, len, ch) \
  if (ind >= len) \
    { \
      oind = len; \
      ch = 0; \
      break; \
    } \
  else \

 
typedef WORD_LIST *EXPFUNC PARAMS((char *, int));

 
pid_t last_command_subst_pid = NO_PID;
pid_t current_command_subst_pid = NO_PID;

 
SHELL_VAR *ifs_var;
char *ifs_value;
unsigned char ifs_cmap[UCHAR_MAX + 1];
int ifs_is_set, ifs_is_null;

#if defined (HANDLE_MULTIBYTE)
unsigned char ifs_firstc[MB_LEN_MAX];
size_t ifs_firstc_len;
#else
unsigned char ifs_firstc;
#endif

 
int inherit_errexit = 0;

 
int assigning_in_environment;

 
WORD_LIST *subst_assign_varlist = (WORD_LIST *)NULL;

 
int no_longjmp_on_fatal_error = 0;

 
int allow_null_glob_expansion;

 
int fail_glob_expansion;

 
int patsub_replacement = 1;

 
extern struct fd_bitmap *current_fds_to_close;
extern int wordexp_only;
extern int singlequote_translations;
extern int extended_quote;

#if defined (JOB_CONTROL) && defined (PROCESS_SUBSTITUTION)
extern PROCESS *last_procsub_child;
#endif

#if !defined (HAVE_WCSDUP) && defined (HANDLE_MULTIBYTE)
extern wchar_t *wcsdup PARAMS((const wchar_t *));
#endif

#if 0
 
char *glob_argv_flags;
static int glob_argv_flags_size;
#endif

static WORD_LIST *cached_quoted_dollar_at = 0;

 
static WORD_LIST expand_word_error, expand_word_fatal;
static WORD_DESC expand_wdesc_error, expand_wdesc_fatal;
static char expand_param_error, expand_param_fatal, expand_param_unset;
static char extract_string_error, extract_string_fatal;

 
static int expand_no_split_dollar_star = 0;

 
static WORD_LIST *garglist = (WORD_LIST *)NULL;

static char *quoted_substring PARAMS((char *, int, int));
static int quoted_strlen PARAMS((char *));
static char *quoted_strchr PARAMS((char *, int, int));

static char *expand_string_if_necessary PARAMS((char *, int, EXPFUNC *));
static inline char *expand_string_to_string_internal PARAMS((char *, int, EXPFUNC *));
static WORD_LIST *call_expand_word_internal PARAMS((WORD_DESC *, int, int, int *, int *));
static WORD_LIST *expand_string_internal PARAMS((char *, int));
static WORD_LIST *expand_string_leave_quoted PARAMS((char *, int));
static WORD_LIST *expand_string_for_rhs PARAMS((char *, int, int, int, int *, int *));
static WORD_LIST *expand_string_for_pat PARAMS((char *, int, int *, int *));

static char *quote_escapes_internal PARAMS((const char *, int));

static WORD_LIST *list_quote_escapes PARAMS((WORD_LIST *));
static WORD_LIST *list_dequote_escapes PARAMS((WORD_LIST *));

static char *make_quoted_char PARAMS((int));
static WORD_LIST *quote_list PARAMS((WORD_LIST *));

static int unquoted_substring PARAMS((char *, char *));
static int unquoted_member PARAMS((int, char *));

#if defined (ARRAY_VARS)
static SHELL_VAR *do_compound_assignment PARAMS((char *, char *, int));
#endif
static int do_assignment_internal PARAMS((const WORD_DESC *, int));

static char *string_extract_verbatim PARAMS((char *, size_t, int *, char *, int));
static char *string_extract PARAMS((char *, int *, char *, int));
static char *string_extract_double_quoted PARAMS((char *, int *, int));
static inline char *string_extract_single_quoted PARAMS((char *, int *, int));
static inline int skip_single_quoted PARAMS((const char *, size_t, int, int));
static int skip_double_quoted PARAMS((char *, size_t, int, int));
static char *extract_delimited_string PARAMS((char *, int *, char *, char *, char *, int));
static char *extract_heredoc_dolbrace_string PARAMS((char *, int *, int, int));
static char *extract_dollar_brace_string PARAMS((char *, int *, int, int));
static int skip_matched_pair PARAMS((const char *, int, int, int, int));

static char *pos_params PARAMS((char *, int, int, int, int));

static unsigned char *mb_getcharlens PARAMS((char *, int));

static char *remove_upattern PARAMS((char *, char *, int));
#if defined (HANDLE_MULTIBYTE) 
static wchar_t *remove_wpattern PARAMS((wchar_t *, size_t, wchar_t *, int));
#endif
static char *remove_pattern PARAMS((char *, char *, int));

static int match_upattern PARAMS((char *, char *, int, char **, char **));
#if defined (HANDLE_MULTIBYTE)
static int match_wpattern PARAMS((wchar_t *, char **, size_t, wchar_t *, int, char **, char **));
#endif
static int match_pattern PARAMS((char *, char *, int, char **, char **));
static int getpatspec PARAMS((int, char *));
static char *getpattern PARAMS((char *, int, int));
static char *variable_remove_pattern PARAMS((char *, char *, int, int));
static char *list_remove_pattern PARAMS((WORD_LIST *, char *, int, int, int));
static char *parameter_list_remove_pattern PARAMS((int, char *, int, int));
#ifdef ARRAY_VARS
static char *array_remove_pattern PARAMS((SHELL_VAR *, char *, int, int, int));
#endif
static char *parameter_brace_remove_pattern PARAMS((char *, char *, array_eltstate_t *, char *, int, int, int));

static char *string_var_assignment PARAMS((SHELL_VAR *, char *));
#if defined (ARRAY_VARS)
static char *array_var_assignment PARAMS((SHELL_VAR *, int, int, int));
#endif
static char *pos_params_assignment PARAMS((WORD_LIST *, int, int));
static char *string_transform PARAMS((int, SHELL_VAR *, char *));
static char *list_transform PARAMS((int, SHELL_VAR *, WORD_LIST *, int, int));
static char *parameter_list_transform PARAMS((int, int, int));
#if defined ARRAY_VARS
static char *array_transform PARAMS((int, SHELL_VAR *, int, int));
#endif
static char *parameter_brace_transform PARAMS((char *, char *, array_eltstate_t *, char *, int, int, int, int));
static int valid_parameter_transform PARAMS((char *));

static char *process_substitute PARAMS((char *, int));

static char *optimize_cat_file PARAMS((REDIRECT *, int, int, int *));
static char *read_comsub PARAMS((int, int, int, int *));

#ifdef ARRAY_VARS
static arrayind_t array_length_reference PARAMS((char *));
#endif

static int valid_brace_expansion_word PARAMS((char *, int));
static int chk_atstar PARAMS((char *, int, int, int *, int *));
static int chk_arithsub PARAMS((const char *, int));

static WORD_DESC *parameter_brace_expand_word PARAMS((char *, int, int, int, array_eltstate_t *));
static char *parameter_brace_find_indir PARAMS((char *, int, int, int));
static WORD_DESC *parameter_brace_expand_indir PARAMS((char *, int, int, int, int *, int *));
static WORD_DESC *parameter_brace_expand_rhs PARAMS((char *, char *, int, int, int, int *, int *));
static void parameter_brace_expand_error PARAMS((char *, char *, int));

static int valid_length_expression PARAMS((char *));
static intmax_t parameter_brace_expand_length PARAMS((char *));

static char *skiparith PARAMS((char *, int));
static int verify_substring_values PARAMS((SHELL_VAR *, char *, char *, int, intmax_t *, intmax_t *));
static int get_var_and_type PARAMS((char *, char *, array_eltstate_t *, int, int, SHELL_VAR **, char **));
static char *mb_substring PARAMS((char *, int, int));
static char *parameter_brace_substring PARAMS((char *, char *, array_eltstate_t *, char *, int, int, int));

static int shouldexp_replacement PARAMS((char *));

static char *pos_params_pat_subst PARAMS((char *, char *, char *, int));

static char *expand_string_for_patsub PARAMS((char *, int));
static char *parameter_brace_patsub PARAMS((char *, char *, array_eltstate_t *, char *, int, int, int));

static char *pos_params_casemod PARAMS((char *, char *, int, int));
static char *parameter_brace_casemod PARAMS((char *, char *, array_eltstate_t *, int, char *, int, int, int));

static WORD_DESC *parameter_brace_expand PARAMS((char *, int *, int, int, int *, int *));
static WORD_DESC *param_expand PARAMS((char *, int *, int, int *, int *, int *, int *, int));

static WORD_LIST *expand_word_internal PARAMS((WORD_DESC *, int, int, int *, int *));

static WORD_LIST *word_list_split PARAMS((WORD_LIST *));

static void exp_jump_to_top_level PARAMS((int));

static WORD_LIST *separate_out_assignments PARAMS((WORD_LIST *));
static WORD_LIST *glob_expand_word_list PARAMS((WORD_LIST *, int));
#ifdef BRACE_EXPANSION
static WORD_LIST *brace_expand_word_list PARAMS((WORD_LIST *, int));
#endif
#if defined (ARRAY_VARS)
static int make_internal_declare PARAMS((char *, char *, char *));
static void expand_compound_assignment_word PARAMS((WORD_LIST *, int));
static WORD_LIST *expand_declaration_argument PARAMS((WORD_LIST *, WORD_LIST *));
#endif
static WORD_LIST *shell_expand_word_list PARAMS((WORD_LIST *, int));
static WORD_LIST *expand_word_list_internal PARAMS((WORD_LIST *, int));

static int do_assignment_statements PARAMS((WORD_LIST *, char *, int));

 
 
 
 
 

#if defined (DEBUG)
void
dump_word_flags (flags)
     int flags;
{
  int f;

  f = flags;
  fprintf (stderr, "%d -> ", f);
  if (f & W_ARRAYIND)
    {
      f &= ~W_ARRAYIND;
      fprintf (stderr, "W_ARRAYIND%s", f ? "|" : "");
    }
  if (f & W_ASSIGNASSOC)
    {
      f &= ~W_ASSIGNASSOC;
      fprintf (stderr, "W_ASSIGNASSOC%s", f ? "|" : "");
    }
  if (f & W_ASSIGNARRAY)
    {
      f &= ~W_ASSIGNARRAY;
      fprintf (stderr, "W_ASSIGNARRAY%s", f ? "|" : "");
    }
  if (f & W_SAWQUOTEDNULL)
    {
      f &= ~W_SAWQUOTEDNULL;
      fprintf (stderr, "W_SAWQUOTEDNULL%s", f ? "|" : "");
    }
  if (f & W_NOPROCSUB)
    {
      f &= ~W_NOPROCSUB;
      fprintf (stderr, "W_NOPROCSUB%s", f ? "|" : "");
    }
  if (f & W_DQUOTE)
    {
      f &= ~W_DQUOTE;
      fprintf (stderr, "W_DQUOTE%s", f ? "|" : "");
    }
  if (f & W_HASQUOTEDNULL)
    {
      f &= ~W_HASQUOTEDNULL;
      fprintf (stderr, "W_HASQUOTEDNULL%s", f ? "|" : "");
    }
  if (f & W_ASSIGNARG)
    {
      f &= ~W_ASSIGNARG;
      fprintf (stderr, "W_ASSIGNARG%s", f ? "|" : "");
    }
  if (f & W_ASSNBLTIN)
    {
      f &= ~W_ASSNBLTIN;
      fprintf (stderr, "W_ASSNBLTIN%s", f ? "|" : "");
    }
  if (f & W_ASSNGLOBAL)
    {
      f &= ~W_ASSNGLOBAL;
      fprintf (stderr, "W_ASSNGLOBAL%s", f ? "|" : "");
    }
  if (f & W_COMPASSIGN)
    {
      f &= ~W_COMPASSIGN;
      fprintf (stderr, "W_COMPASSIGN%s", f ? "|" : "");
    }
  if (f & W_EXPANDRHS)
    {
      f &= ~W_EXPANDRHS;
      fprintf (stderr, "W_EXPANDRHS%s", f ? "|" : "");
    }
  if (f & W_NOTILDE)
    {
      f &= ~W_NOTILDE;
      fprintf (stderr, "W_NOTILDE%s", f ? "|" : "");
    }
  if (f & W_ASSIGNRHS)
    {
      f &= ~W_ASSIGNRHS;
      fprintf (stderr, "W_ASSIGNRHS%s", f ? "|" : "");
    }
  if (f & W_NOASSNTILDE)
    {
      f &= ~W_NOASSNTILDE;
      fprintf (stderr, "W_NOASSNTILDE%s", f ? "|" : "");
    }
  if (f & W_NOCOMSUB)
    {
      f &= ~W_NOCOMSUB;
      fprintf (stderr, "W_NOCOMSUB%s", f ? "|" : "");
    }
  if (f & W_ARRAYREF)
    {
      f &= ~W_ARRAYREF;
      fprintf (stderr, "W_ARRAYREF%s", f ? "|" : "");
    }
  if (f & W_DOLLARAT)
    {
      f &= ~W_DOLLARAT;
      fprintf (stderr, "W_DOLLARAT%s", f ? "|" : "");
    }
  if (f & W_TILDEEXP)
    {
      f &= ~W_TILDEEXP;
      fprintf (stderr, "W_TILDEEXP%s", f ? "|" : "");
    }
  if (f & W_NOSPLIT2)
    {
      f &= ~W_NOSPLIT2;
      fprintf (stderr, "W_NOSPLIT2%s", f ? "|" : "");
    }
  if (f & W_NOSPLIT)
    {
      f &= ~W_NOSPLIT;
      fprintf (stderr, "W_NOSPLIT%s", f ? "|" : "");
    }
  if (f & W_NOBRACE)
    {
      f &= ~W_NOBRACE;
      fprintf (stderr, "W_NOBRACE%s", f ? "|" : "");
    }
  if (f & W_NOGLOB)
    {
      f &= ~W_NOGLOB;
      fprintf (stderr, "W_NOGLOB%s", f ? "|" : "");
    }
  if (f & W_SPLITSPACE)
    {
      f &= ~W_SPLITSPACE;
      fprintf (stderr, "W_SPLITSPACE%s", f ? "|" : "");
    }
  if (f & W_ASSIGNMENT)
    {
      f &= ~W_ASSIGNMENT;
      fprintf (stderr, "W_ASSIGNMENT%s", f ? "|" : "");
    }
  if (f & W_QUOTED)
    {
      f &= ~W_QUOTED;
      fprintf (stderr, "W_QUOTED%s", f ? "|" : "");
    }
  if (f & W_HASDOLLAR)
    {
      f &= ~W_HASDOLLAR;
      fprintf (stderr, "W_HASDOLLAR%s", f ? "|" : "");
    }
  if (f & W_COMPLETE)
    {
      f &= ~W_COMPLETE;
      fprintf (stderr, "W_COMPLETE%s", f ? "|" : "");
    }
  if (f & W_CHKLOCAL)
    {
      f &= ~W_CHKLOCAL;
      fprintf (stderr, "W_CHKLOCAL%s", f ? "|" : "");
    }
  if (f & W_FORCELOCAL)
    {
      f &= ~W_FORCELOCAL;
      fprintf (stderr, "W_FORCELOCAL%s", f ? "|" : "");
    }

  fprintf (stderr, "\n");
  fflush (stderr);
}
#endif

#ifdef INCLUDE_UNUSED
static char *
quoted_substring (string, start, end)
     char *string;
     int start, end;
{
  register int len, l;
  register char *result, *s, *r;

  len = end - start;

   
  for (s = string, l = 0; *s && l < start; )
    {
      if (*s == CTLESC)
	{
	  s++;
	  continue;
	}
      l++;
      if (*s == 0)
	break;
    }

  r = result = (char *)xmalloc (2*len + 1);       

   
  s = string + l;
  for (l = 0; l < len; s++)
    {
      if (*s == CTLESC)
	*r++ = *s++;
      *r++ = *s;
      l++;
      if (*s == 0)
	break;
    }
  *r = '\0';
  return result;
}
#endif

#ifdef INCLUDE_UNUSED
 
static int
quoted_strlen (s)
     char *s;
{
  register char *p;
  int i;

  i = 0;
  for (p = s; *p; p++)
    {
      if (*p == CTLESC)
	{
	  p++;
	  if (*p == 0)
	    return (i + 1);
	}
      i++;
    }

  return i;
}
#endif

#ifdef INCLUDE_UNUSED
 
static char *
quoted_strchr (s, c, flags)
     char *s;
     int c, flags;
{
  register char *p;

  for (p = s; *p; p++)
    {
      if (((flags & ST_BACKSL) && *p == '\\')
	    || ((flags & ST_CTLESC) && *p == CTLESC))
	{
	  p++;
	  if (*p == '\0')
	    return ((char *)NULL);
	  continue;
	}
      else if (*p == c)
	return p;
    }
  return ((char *)NULL);
}

 
static int
unquoted_member (character, string)
     int character;
     char *string;
{
  size_t slen;
  int sindex, c;
  DECLARE_MBSTATE;

  slen = strlen (string);
  sindex = 0;
  while (c = string[sindex])
    {
      if (c == character)
	return (1);

      switch (c)
	{
	default:
	  ADVANCE_CHAR (string, slen, sindex);
	  break;

	case '\\':
	  sindex++;
	  if (string[sindex])
	    ADVANCE_CHAR (string, slen, sindex);
	  break;

	case '\'':
	  sindex = skip_single_quoted (string, slen, ++sindex, 0);
	  break;

	case '"':
	  sindex = skip_double_quoted (string, slen, ++sindex, 0);
	  break;
	}
    }
  return (0);
}

 
static int
unquoted_substring (substr, string)
     char *substr, *string;
{
  size_t slen;
  int sindex, c, sublen;
  DECLARE_MBSTATE;

  if (substr == 0 || *substr == '\0')
    return (0);

  slen = strlen (string);
  sublen = strlen (substr);
  for (sindex = 0; c = string[sindex]; )
    {
      if (STREQN (string + sindex, substr, sublen))
	return (1);

      switch (c)
	{
	case '\\':
	  sindex++;
	  if (string[sindex])
	    ADVANCE_CHAR (string, slen, sindex);
	  break;

	case '\'':
	  sindex = skip_single_quoted (string, slen, ++sindex, 0);
	  break;

	case '"':
	  sindex = skip_double_quoted (string, slen, ++sindex, 0);
	  break;

	default:
	  ADVANCE_CHAR (string, slen, sindex);
	  break;
	}
    }
  return (0);
}
#endif

 

 
INLINE char *
sub_append_string (source, target, indx, size)
     char *source, *target;
     size_t *indx;
     size_t *size;
{
  if (source)
    {
      size_t n, srclen;

      srclen = STRLEN (source);
      if (srclen >= (*size - *indx))
	{
	  n = srclen + *indx;
	  n = (n + DEFAULT_ARRAY_SIZE) - (n % DEFAULT_ARRAY_SIZE);
	  target = (char *)xrealloc (target, (*size = n));
	}

      FASTCOPY (source, target + *indx, srclen);
      *indx += srclen;
      target[*indx] = '\0';

      free (source);
    }
  return (target);
}

#if 0
 
 
char *
sub_append_number (number, target, indx, size)
     intmax_t number;
     char *target;
     size_t *indx;
     size_t *size;
{
  char *temp;

  temp = itos (number);
  return (sub_append_string (temp, target, indx, size));
}
#endif

 
static char *
string_extract (string, sindex, charlist, flags)
     char *string;
     int *sindex;
     char *charlist;
     int flags;
{
  register int c, i;
  int found;
  size_t slen;
  char *temp;
  DECLARE_MBSTATE;

  slen = (MB_CUR_MAX > 1) ? strlen (string + *sindex) + *sindex : 0;
  i = *sindex;
  found = 0;
  while (c = string[i])
    {
      if (c == '\\')
	{
	  if (string[i + 1])
	    i++;
	  else
	    break;
	}
#if defined (ARRAY_VARS)
      else if ((flags & SX_VARNAME) && c == LBRACK)
	{
	  int ni;
	   
	  ni = skipsubscript (string, i, 0);
	  if (string[ni] == RBRACK)
	    i = ni;
	}
#endif
      else if (MEMBER (c, charlist))
	{
	  found = 1;
	  break;
	}

      ADVANCE_CHAR (string, slen, i);
    }

   
  if ((flags & SX_REQMATCH) && found == 0)
    {
      *sindex = i;
      return (&extract_string_error);
    }
  
  temp = (flags & SX_NOALLOC) ? (char *)NULL : substring (string, *sindex, i);
  *sindex = i;
  
  return (temp);
}

 
static char *
string_extract_double_quoted (string, sindex, flags)
     char *string;
     int *sindex, flags;
{
  size_t slen;
  char *send;
  int j, i, t;
  unsigned char c;
  char *temp, *ret;		 
  int pass_next, backquote, si;	 
  int dquote;
  int stripdq;
  DECLARE_MBSTATE;

  slen = strlen (string + *sindex) + *sindex;
  send = string + slen;

  stripdq = (flags & SX_STRIPDQ);

  pass_next = backquote = dquote = 0;
  temp = (char *)xmalloc (1 + slen - *sindex);

  j = 0;
  i = *sindex;
  while (c = string[i])
    {
       
      if (pass_next)
	{
	   
	   

	   
	  if ((stripdq == 0 && c != '"') ||
	      (stripdq && ((dquote && (sh_syntaxtab[c] & CBSDQUOTE)) || dquote == 0)))
	    temp[j++] = '\\';
	  pass_next = 0;

add_one_character:
	  COPY_CHAR_I (temp, j, string, send, i);
	  continue;
	}

       
      if (c == '\\')
	{
	  pass_next++;
	  i++;
	  continue;
	}

       
      if (backquote)
	{
	  if (c == '`')
	    backquote = 0;
	  temp[j++] = c;	 
	  i++;
	  continue;
	}

      if (c == '`')
	{
	  temp[j++] = c;
	  backquote++;
	  i++;
	  continue;
	}

       
      if (c == '$' && ((string[i + 1] == LPAREN) || (string[i + 1] == LBRACE)))
	{
	  int free_ret = 1;

	  si = i + 2;
	  if (string[i + 1] == LPAREN)
	    ret = extract_command_subst (string, &si, (flags & SX_COMPLETE));
	  else
	    ret = extract_dollar_brace_string (string, &si, Q_DOUBLE_QUOTES, 0);

	  temp[j++] = '$';
	  temp[j++] = string[i + 1];

	   
	  if (ret == 0 && no_longjmp_on_fatal_error)
	    {
	      free_ret = 0;
	      ret = string + i + 2;
	    }

	   
	  for (t = 0; ret[t]; t++, j++)
	    temp[j] = ret[t];
	  temp[j] = string[si];

	  if (si < i + 2)	 
	    i += 2;
	  else if (string[si])
	    {
	      j++;
	      i = si + 1;
	    }
	  else
	    i = si;

	  if (free_ret)
	    free (ret);
	  continue;
	}

       
      if (c != '"')
	goto add_one_character;

       
      if (stripdq)
	{
	  dquote ^= 1;
	  i++;
	  continue;
	}

      break;
    }
  temp[j] = '\0';

   
  if (c)
    i++;
  *sindex = i;

  return (temp);
}

 
static int
skip_double_quoted (string, slen, sind, flags)
     char *string;
     size_t slen;
     int sind;
     int flags;
{
  int c, i;
  char *ret;
  int pass_next, backquote, si;
  DECLARE_MBSTATE;

  pass_next = backquote = 0;
  i = sind;
  while (c = string[i])
    {
      if (pass_next)
	{
	  pass_next = 0;
	  ADVANCE_CHAR (string, slen, i);
	  continue;
	}
      else if (c == '\\')
	{
	  pass_next++;
	  i++;
	  continue;
	}
      else if (backquote)
	{
	  if (c == '`')
	    backquote = 0;
	  ADVANCE_CHAR (string, slen, i);
	  continue;
	}
      else if (c == '`')
	{
	  backquote++;
	  i++;
	  continue;
	}
      else if (c == '$' && ((string[i + 1] == LPAREN) || (string[i + 1] == LBRACE)))
	{
	  si = i + 2;
	  if (string[i + 1] == LPAREN)
	    ret = extract_command_subst (string, &si, SX_NOALLOC|(flags&SX_COMPLETE));
	  else
	    ret = extract_dollar_brace_string (string, &si, Q_DOUBLE_QUOTES, SX_NOALLOC);

	   
	  CHECK_STRING_OVERRUN (i, si, slen, c);

	  i = si + 1;
	  continue;
	}
      else if (c != '"')
	{
	  ADVANCE_CHAR (string, slen, i);
	  continue;
	}
      else
	break;
    }

  if (c)
    i++;

  return (i);
}

 
static inline char *
string_extract_single_quoted (string, sindex, allowesc)
     char *string;
     int *sindex;
     int allowesc;
{
  register int i;
  size_t slen;
  char *t;
  int pass_next;
  DECLARE_MBSTATE;

   
  slen = (MB_CUR_MAX > 1) ? strlen (string + *sindex) + *sindex : 0;
  i = *sindex;
  pass_next = 0;
  while (string[i])
    {
      if (pass_next)
	{
	  pass_next = 0;
	  ADVANCE_CHAR (string, slen, i);
	  continue;
	}
      if (allowesc && string[i] == '\\')
	pass_next++;
      else if (string[i] == '\'')
        break;
      ADVANCE_CHAR (string, slen, i);
    }

  t = substring (string, *sindex, i);

  if (string[i])
    i++;
  *sindex = i;

  return (t);
}

 
static inline int
skip_single_quoted (string, slen, sind, flags)
     const char *string;
     size_t slen;
     int sind;
     int flags;
{
  register int c;
  DECLARE_MBSTATE;

  c = sind;
  while (string[c] && string[c] != '\'')
    {
      if ((flags & SX_COMPLETE) && string[c] == '\\' && string[c+1] == '\'' && string[c+2])
	ADVANCE_CHAR (string, slen, c);
      ADVANCE_CHAR (string, slen, c);
    }

  if (string[c])
    c++;
  return c;
}

 
static char *
string_extract_verbatim (string, slen, sindex, charlist, flags)
     char *string;
     size_t slen;
     int *sindex;
     char *charlist;
     int flags;
{
  register int i;
#if defined (HANDLE_MULTIBYTE)
  wchar_t *wcharlist;
#endif
  int c;
  char *temp;
  DECLARE_MBSTATE;

  if ((flags & SX_NOCTLESC) && charlist[0] == '\'' && charlist[1] == '\0')
    {
      temp = string_extract_single_quoted (string, sindex, 0);
      --*sindex;	 
      return temp;
    }

   
  if (*charlist == 0)
    {
      temp = string + *sindex;
      c = (*sindex == 0) ? slen : STRLEN (temp);
      temp = savestring (temp);
      *sindex += c;
      return temp;
    }

  i = *sindex;
#if defined (HANDLE_MULTIBYTE)
  wcharlist = 0;
#endif
  while (c = string[i])
    {
#if defined (HANDLE_MULTIBYTE)
      size_t mblength;
#endif
      if ((flags & SX_NOCTLESC) == 0 && c == CTLESC)
	{
	  i += 2;
	  CHECK_STRING_OVERRUN (i, i, slen, c);
	  continue;
	}
       
      else if ((flags & SX_NOESCCTLNUL) == 0 && c == CTLESC && string[i+1] == CTLNUL)
	{
	  i += 2;
	  CHECK_STRING_OVERRUN (i, i, slen, c);
	  continue;
	}

#if defined (HANDLE_MULTIBYTE)
      if (locale_utf8locale && slen > i && UTF8_SINGLEBYTE (string[i]))
	mblength = (string[i] != 0) ? 1 : 0;
      else
	mblength = MBLEN (string + i, slen - i);
      if (mblength > 1)
	{
	  wchar_t wc;
	  mblength = mbtowc (&wc, string + i, slen - i);
	  if (MB_INVALIDCH (mblength))
	    {
	      if (MEMBER (c, charlist))
		break;
	    }
	  else
	    {
	      if (wcharlist == 0)
		{
		  size_t len;
		  len = mbstowcs (wcharlist, charlist, 0);
		  if (len == -1)
		    len = 0;
		  wcharlist = (wchar_t *)xmalloc (sizeof (wchar_t) * (len + 1));
		  mbstowcs (wcharlist, charlist, len + 1);
		}

	      if (wcschr (wcharlist, wc))
		break;
	    }
	}
      else		
#endif
      if (MEMBER (c, charlist))
	break;

      ADVANCE_CHAR (string, slen, i);
    }

#if defined (HANDLE_MULTIBYTE)
  FREE (wcharlist);
#endif

  temp = substring (string, *sindex, i);
  *sindex = i;

  return (temp);
}

 
char *
extract_command_subst (string, sindex, xflags)
     char *string;
     int *sindex;
     int xflags;
{
  char *ret;

  if (string[*sindex] == LPAREN || (xflags & SX_COMPLETE))
    return (extract_delimited_string (string, sindex, "$(", "(", ")", xflags|SX_COMMAND));  
  else
    {
      xflags |= (no_longjmp_on_fatal_error ? SX_NOLONGJMP : 0);
      ret = xparse_dolparen (string, string+*sindex, sindex, xflags);
      return ret;
    }
}

 
char *
extract_arithmetic_subst (string, sindex)
     char *string;
     int *sindex;
{
  return (extract_delimited_string (string, sindex, "$[", "[", "]", 0));  
}

#if defined (PROCESS_SUBSTITUTION)
   
char *
extract_process_subst (string, starter, sindex, xflags)
     char *string;
     char *starter;
     int *sindex;
     int xflags;
{
#if 0
   
  return (extract_delimited_string (string, sindex, starter, "(", ")", SX_COMMAND));
#else
  xflags |= (no_longjmp_on_fatal_error ? SX_NOLONGJMP : 0);
  return (xparse_dolparen (string, string+*sindex, sindex, xflags));
#endif
}
#endif  

#if defined (ARRAY_VARS)
 
char *
extract_array_assignment_list (string, sindex)
     char *string;
     int *sindex;
{
  int slen;
  char *ret;

  slen = strlen (string);
  if (string[slen - 1] == RPAREN)
   {
      ret = substring (string, *sindex, slen - 1);
      *sindex = slen - 1;
      return ret;
    }
  return 0;  
}
#endif

 
static char *
extract_delimited_string (string, sindex, opener, alt_opener, closer, flags)
     char *string;
     int *sindex;
     char *opener, *alt_opener, *closer;
     int flags;
{
  int i, c, si;
  size_t slen;
  char *t, *result;
  int pass_character, nesting_level, in_comment;
  int len_closer, len_opener, len_alt_opener;
  DECLARE_MBSTATE;

  slen = strlen (string + *sindex) + *sindex;
  len_opener = STRLEN (opener);
  len_alt_opener = STRLEN (alt_opener);
  len_closer = STRLEN (closer);

  pass_character = in_comment = 0;

  nesting_level = 1;
  i = *sindex;

  while (nesting_level)
    {
      c = string[i];

       
      if (i > slen)
	{
	  i = slen;
	  c = string[i = slen];
	  break;
	}

      if (c == 0)
	break;

      if (in_comment)
	{
	  if (c == '\n')
	    in_comment = 0;
	  ADVANCE_CHAR (string, slen, i);
	  continue;
	}

      if (pass_character)	 
	{
	  pass_character = 0;
	  ADVANCE_CHAR (string, slen, i);
	  continue;
	}

       
      if ((flags & SX_COMMAND) && c == '#' && (i == 0 || string[i - 1] == '\n' || shellblank (string[i - 1])))
	{
          in_comment = 1;
          ADVANCE_CHAR (string, slen, i);
          continue;
	}
        
      if (c == CTLESC || c == '\\')
	{
	  pass_character++;
	  i++;
	  continue;
	}

       
      if ((flags & SX_COMMAND) && string[i] == '$' && string[i+1] == LPAREN)
        {
          si = i + 2;
          t = extract_command_subst (string, &si, flags|SX_NOALLOC);
          CHECK_STRING_OVERRUN (i, si, slen, c);
          i = si + 1;
          continue;
        }

       
      if (STREQN (string + i, opener, len_opener))
	{
	  si = i + len_opener;
	  t = extract_delimited_string (string, &si, opener, alt_opener, closer, flags|SX_NOALLOC);
	  CHECK_STRING_OVERRUN (i, si, slen, c);
	  i = si + 1;
	  continue;
	}

       
      if (len_alt_opener && STREQN (string + i, alt_opener, len_alt_opener))
	{
	  si = i + len_alt_opener;
	  t = extract_delimited_string (string, &si, alt_opener, alt_opener, closer, flags|SX_NOALLOC);
	  CHECK_STRING_OVERRUN (i, si, slen, c);
	  i = si + 1;
	  continue;
	}

       
      if (STREQN (string + i, closer, len_closer))
	{
	  i += len_closer - 1;	 
	  nesting_level--;
	  if (nesting_level == 0)
	    break;
	}

       
      if (c == '`')
	{
	  si = i + 1;
	  t = string_extract (string, &si, "`", flags|SX_NOALLOC);
	  CHECK_STRING_OVERRUN (i, si, slen, c);
	  i = si + 1;
	  continue;
	}

       
      if (c == '\'' || c == '"')
	{
	  si = i + 1;
	  i = (c == '\'') ? skip_single_quoted (string, slen, si, 0)
			  : skip_double_quoted (string, slen, si, 0);
	  continue;
	}

       
      ADVANCE_CHAR (string, slen, i);
    }

  if (c == 0 && nesting_level)
    {
      if (no_longjmp_on_fatal_error == 0)
	{
	  last_command_exit_value = EXECUTION_FAILURE;
	  report_error (_("bad substitution: no closing `%s' in %s"), closer, string);
	  exp_jump_to_top_level (DISCARD);
	}
      else
	{
	  *sindex = i;
	  return (char *)NULL;
	}
    }

  si = i - *sindex - len_closer + 1;
  if (flags & SX_NOALLOC)
    result = (char *)NULL;
  else    
    {
      result = (char *)xmalloc (1 + si);
      strncpy (result, string + *sindex, si);
      result[si] = '\0';
    }
  *sindex = i;

  return (result);
}

 
static char *
extract_heredoc_dolbrace_string (string, sindex, quoted, flags)
     char *string;
     int *sindex, quoted, flags;
{
  register int i, c;
  size_t slen, tlen, result_index, result_size;
  int pass_character, nesting_level, si, dolbrace_state;
  char *result, *t, *send;
  DECLARE_MBSTATE;

  pass_character = 0;
  nesting_level = 1;
  slen = strlen (string + *sindex) + *sindex;
  send = string + slen;

  result_size = slen;
  result_index = 0;
  result = xmalloc (result_size + 1);

   
  dolbrace_state = DOLBRACE_QUOTE;

  i = *sindex;
  while (c = string[i])
    {
      if (pass_character)
	{
	  pass_character = 0;
	  RESIZE_MALLOCED_BUFFER (result, result_index, locale_mb_cur_max + 1, result_size, 64);
	  COPY_CHAR_I (result, result_index, string, send, i);
	  continue;
	}

       
      if (c == CTLESC || c == '\\')
	{
	  pass_character++;
	  RESIZE_MALLOCED_BUFFER (result, result_index, 2, result_size, 64);
	  result[result_index++] = c;
	  i++;
	  continue;
	}

       
      if (c == '$' && string[i+1] == '\'')
	{
	  char *ttrans;
	  int ttranslen;

	  if ((posixly_correct || extended_quote == 0) && dolbrace_state != DOLBRACE_QUOTE && dolbrace_state != DOLBRACE_QUOTE2)
	    {
	      RESIZE_MALLOCED_BUFFER (result, result_index, 3, result_size, 64);
	      result[result_index++] = '$';
	      result[result_index++] = '\'';
	      i += 2;
	      continue;
	    }

	  si = i + 2;
	  t = string_extract_single_quoted (string, &si, 1);	 
	  CHECK_STRING_OVERRUN (i, si, slen, c);

	  tlen = si - i - 2;	 
	  ttrans = ansiexpand (t, 0, tlen, &ttranslen);
	  free (t);

	   
	  if (dolbrace_state == DOLBRACE_QUOTE || dolbrace_state == DOLBRACE_QUOTE2)
	    {
	      t = sh_single_quote (ttrans);
	      tlen = strlen (t);
	      free (ttrans);
	    }
	  else if (extended_quote)  
	    {
	       
	      t = ttrans;
	      tlen = strlen (t);
	    }

	  RESIZE_MALLOCED_BUFFER (result, result_index, tlen + 1, result_size, 64);
	  strncpy (result + result_index, t, tlen);
	  result_index += tlen;
	  free (t);
	  i = si;
	  continue;
	}

#if defined (TRANSLATABLE_STRINGS)
      if (c == '$' && string[i+1] == '"')
	{
	  char *ttrans;
	  int ttranslen;

	  si = i + 2;
	  t = string_extract_double_quoted (string, &si, flags);	 
	  CHECK_STRING_OVERRUN (i, si, slen, c);

	  tlen = si - i - 2;	 
	  ttrans = locale_expand (t, 0, tlen, line_number, &ttranslen);
	  free (t);

	  t = singlequote_translations ? sh_single_quote (ttrans) : sh_mkdoublequoted (ttrans, ttranslen, 0);
	  tlen = strlen (t);
	  free (ttrans);

	  RESIZE_MALLOCED_BUFFER (result, result_index, tlen + 1, result_size, 64);
	  strncpy (result + result_index, t, tlen);
	  result_index += tlen;
	  free (t);
	  i = si;
	  continue;
	}
#endif  

      if (c == '$' && string[i+1] == LBRACE)
	{
	  nesting_level++;
	  RESIZE_MALLOCED_BUFFER (result, result_index, 3, result_size, 64);
	  result[result_index++] = c;
	  result[result_index++] = string[i+1];
	  i += 2;
	  if (dolbrace_state == DOLBRACE_QUOTE || dolbrace_state == DOLBRACE_QUOTE2 || dolbrace_state == DOLBRACE_WORD)
	    dolbrace_state = DOLBRACE_PARAM;
	  continue;
	}

      if (c == RBRACE)
	{
	  nesting_level--;
	  if (nesting_level == 0)
	    break;
	  RESIZE_MALLOCED_BUFFER (result, result_index, 2, result_size, 64);
	  result[result_index++] = c;
	  i++;
	  continue;
	}

       
      if (c == '`')
	{
	  si = i + 1;
	  t = string_extract (string, &si, "`", flags);	 
	  CHECK_STRING_OVERRUN (i, si, slen, c);

	  tlen = si - i - 1;
	  RESIZE_MALLOCED_BUFFER (result, result_index, tlen + 3, result_size, 64);
	  result[result_index++] = c;
	  strncpy (result + result_index, t, tlen);
	  result_index += tlen;
	  result[result_index++] = string[si];
	  free (t);
	  i = si + 1;
	  continue;
	}

       
      if (string[i] == '$' && string[i+1] == LPAREN)
	{
	  si = i + 2;
	  t = extract_command_subst (string, &si, flags);
	  CHECK_STRING_OVERRUN (i, si, slen, c);

	  tlen = si - i - 2;
	  RESIZE_MALLOCED_BUFFER (result, result_index, tlen + 4, result_size, 64);
	  result[result_index++] = c;
	  result[result_index++] = LPAREN;
	  strncpy (result + result_index, t, tlen);
	  result_index += tlen;
	  result[result_index++] = string[si];
	  free (t);
	  i = si + 1;
	  continue;
	}

#if defined (PROCESS_SUBSTITUTION)
       
      if ((string[i] == '<' || string[i] == '>') && string[i+1] == LPAREN)
	{
	  si = i + 2;
	  t = extract_process_subst (string, (string[i] == '<' ? "<(" : ">)"), &si, flags);
	  CHECK_STRING_OVERRUN (i, si, slen, c);

	  tlen = si - i - 2;
	  RESIZE_MALLOCED_BUFFER (result, result_index, tlen + 4, result_size, 64);
	  result[result_index++] = c;
	  result[result_index++] = LPAREN;
	  strncpy (result + result_index, t, tlen);
	  result_index += tlen;
	  result[result_index++] = string[si];
	  free (t);
	  i = si + 1;
	  continue;
	}
#endif

      if (c == '\'' && posixly_correct && shell_compatibility_level > 42 && dolbrace_state != DOLBRACE_QUOTE)
	{
	  COPY_CHAR_I (result, result_index, string, send, i);
	  continue;
	}

       
      if (c == '"' || c == '\'')
	{
	  si = i + 1;
	  if (c == '"')
	    t = string_extract_double_quoted (string, &si, flags);
	  else
	    t = string_extract_single_quoted (string, &si, 0);
	  CHECK_STRING_OVERRUN (i, si, slen, c);

	  tlen = si - i - 2;	 
	  RESIZE_MALLOCED_BUFFER (result, result_index, tlen + 3, result_size, 64);
	  result[result_index++] = c;
	  strncpy (result + result_index, t, tlen);
	  result_index += tlen;
	  result[result_index++] = string[si - 1];
	  free (t);
	  i = si;
	  continue;
	}

       
      COPY_CHAR_I (result, result_index, string, send, i);

       
      if (dolbrace_state == DOLBRACE_PARAM && c == '%' && (i - *sindex) > 1)
	dolbrace_state = DOLBRACE_QUOTE;
      else if (dolbrace_state == DOLBRACE_PARAM && c == '#' && (i - *sindex) > 1)
        dolbrace_state = DOLBRACE_QUOTE;
      else if (dolbrace_state == DOLBRACE_PARAM && c == '/' && (i - *sindex) > 1)
        dolbrace_state = DOLBRACE_QUOTE2;	 
      else if (dolbrace_state == DOLBRACE_PARAM && c == '^' && (i - *sindex) > 1)
        dolbrace_state = DOLBRACE_QUOTE;
      else if (dolbrace_state == DOLBRACE_PARAM && c == ',' && (i - *sindex) > 1)
        dolbrace_state = DOLBRACE_QUOTE;
       
      else if (dolbrace_state == DOLBRACE_PARAM && strchr ("#%^,~:-=?+/", c) != 0)
	dolbrace_state = DOLBRACE_OP;
      else if (dolbrace_state == DOLBRACE_OP && strchr ("#%^,~:-=?+/", c) == 0)
	dolbrace_state = DOLBRACE_WORD;
    }

  if (c == 0 && nesting_level)
    {
      free (result);
      if (no_longjmp_on_fatal_error == 0)
	{			 
	  last_command_exit_value = EXECUTION_FAILURE;
	  report_error (_("bad substitution: no closing `%s' in %s"), "}", string);
	  exp_jump_to_top_level (DISCARD);
	}
      else
	{
	  *sindex = i;
	  return ((char *)NULL);
	}
    }

  *sindex = i;
  result[result_index] = '\0';

  return (result);
}

#define PARAMEXPNEST_MAX	32	
static int dbstate[PARAMEXPNEST_MAX];

 
 
static char *
extract_dollar_brace_string (string, sindex, quoted, flags)
     char *string;
     int *sindex, quoted, flags;
{
  register int i, c;
  size_t slen;
  int pass_character, nesting_level, si, dolbrace_state;
  char *result, *t;
  DECLARE_MBSTATE;

   
  dolbrace_state = (flags & SX_WORD) ? DOLBRACE_WORD : DOLBRACE_PARAM;
  if ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) && (flags & SX_POSIXEXP))
    dolbrace_state = DOLBRACE_QUOTE;

  if (quoted == Q_HERE_DOCUMENT && dolbrace_state == DOLBRACE_QUOTE && (flags & SX_NOALLOC) == 0)
    return (extract_heredoc_dolbrace_string (string, sindex, quoted, flags));

  dbstate[0] = dolbrace_state;

  pass_character = 0;
  nesting_level = 1;
  slen = strlen (string + *sindex) + *sindex;

  i = *sindex;
  while (c = string[i])
    {
      if (pass_character)
	{
	  pass_character = 0;
	  ADVANCE_CHAR (string, slen, i);
	  continue;
	}

       
      if (c == CTLESC || c == '\\')
	{
	  pass_character++;
	  i++;
	  continue;
	}

      if (string[i] == '$' && string[i+1] == LBRACE)
	{
	  if (nesting_level < PARAMEXPNEST_MAX)
	    dbstate[nesting_level] = dolbrace_state;
	  nesting_level++;
	  i += 2;
	  if (dolbrace_state == DOLBRACE_QUOTE || dolbrace_state == DOLBRACE_WORD)
	    dolbrace_state = DOLBRACE_PARAM;
	  continue;
	}

      if (c == RBRACE)
	{
	  nesting_level--;
	  if (nesting_level == 0)
	    break;
	  dolbrace_state = (nesting_level < PARAMEXPNEST_MAX) ? dbstate[nesting_level] : dbstate[0];	 
	  i++;
	  continue;
	}

       
      if (c == '`')
	{
	  si = i + 1;
	  t = string_extract (string, &si, "`", flags|SX_NOALLOC);

	  CHECK_STRING_OVERRUN (i, si, slen, c);

	  i = si + 1;
	  continue;
	}

       
      if (string[i] == '$' && string[i+1] == LPAREN)
	{
	  si = i + 2;
	  t = extract_command_subst (string, &si, flags|SX_NOALLOC);

	  CHECK_STRING_OVERRUN (i, si, slen, c);

	  i = si + 1;
	  continue;
	}

#if defined (PROCESS_SUBSTITUTION)
       
      if ((string[i] == '<' || string[i] == '>') && string[i+1] == LPAREN)
	{
	  si = i + 2;
	  t = extract_process_subst (string, (string[i] == '<' ? "<(" : ">)"), &si, flags|SX_NOALLOC);

	  CHECK_STRING_OVERRUN (i, si, slen, c);

	  i = si + 1;
	  continue;
	}
#endif

       
      if (c == '"')
	{
	  si = i + 1;
	  i = skip_double_quoted (string, slen, si, 0);
	   
	  continue;
	}

      if (c == '\'')
	{
 
	  if (posixly_correct && shell_compatibility_level > 42 && dolbrace_state != DOLBRACE_QUOTE && (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)))
	    ADVANCE_CHAR (string, slen, i);
	  else
	    {
	      si = i + 1;
	      i = skip_single_quoted (string, slen, si, 0);
	    }

          continue;
	}

#if defined (ARRAY_VARS)
      if (c == LBRACK && dolbrace_state == DOLBRACE_PARAM)
	{
	  si = skipsubscript (string, i, 0);
	  CHECK_STRING_OVERRUN (i, si, slen, c);
	  if (string[si] == RBRACK)
	    c = string[i = si];
	}
#endif

       
      ADVANCE_CHAR (string, slen, i);

       
      if (dolbrace_state == DOLBRACE_PARAM && c == '%' && (i - *sindex) > 1)
	dolbrace_state = DOLBRACE_QUOTE;
      else if (dolbrace_state == DOLBRACE_PARAM && c == '#' && (i - *sindex) > 1)
        dolbrace_state = DOLBRACE_QUOTE;
      else if (dolbrace_state == DOLBRACE_PARAM && c == '/' && (i - *sindex) > 1)
        dolbrace_state = DOLBRACE_QUOTE2;	 
      else if (dolbrace_state == DOLBRACE_PARAM && c == '^' && (i - *sindex) > 1)
        dolbrace_state = DOLBRACE_QUOTE;
      else if (dolbrace_state == DOLBRACE_PARAM && c == ',' && (i - *sindex) > 1)
        dolbrace_state = DOLBRACE_QUOTE;
       
      else if (dolbrace_state == DOLBRACE_PARAM && strchr ("#%^,~:-=?+/", c) != 0)
	dolbrace_state = DOLBRACE_OP;
      else if (dolbrace_state == DOLBRACE_OP && strchr ("#%^,~:-=?+/", c) == 0)
	dolbrace_state = DOLBRACE_WORD;
    }

  if (c == 0 && nesting_level)
    {
      if (no_longjmp_on_fatal_error == 0)
	{			 
	  last_command_exit_value = EXECUTION_FAILURE;
	  report_error (_("bad substitution: no closing `%s' in %s"), "}", string);
	  exp_jump_to_top_level (DISCARD);
	}
      else
	{
	  *sindex = i;
	  return ((char *)NULL);
	}
    }

  result = (flags & SX_NOALLOC) ? (char *)NULL : substring (string, *sindex, i);
  *sindex = i;

  return (result);
}

 
char *
de_backslash (string)
     char *string;
{
  register size_t slen;
  register int i, j, prev_i;
  DECLARE_MBSTATE;

  slen = strlen (string);
  i = j = 0;

   
  while (i < slen)
    {
      if (string[i] == '\\' && (string[i + 1] == '`' || string[i + 1] == '\\' ||
			      string[i + 1] == '$'))
	i++;
      prev_i = i;
      ADVANCE_CHAR (string, slen, i);
      if (j < prev_i)
	do string[j++] = string[prev_i++]; while (prev_i < i);
      else
	j = i;
    }
  string[j] = '\0';

  return (string);
}

#if 0
 
 
void
unquote_bang (string)
     char *string;
{
  register int i, j;
  register char *temp;

  temp = (char *)xmalloc (1 + strlen (string));

  for (i = 0, j = 0; (temp[j] = string[i]); i++, j++)
    {
      if (string[i] == '\\' && string[i + 1] == '!')
	{
	  temp[j] = '!';
	  i++;
	}
    }
  strcpy (string, temp);
  free (temp);
}
#endif

#define CQ_RETURN(x) do { no_longjmp_on_fatal_error = oldjmp; return (x); } while (0)

 
static int
skip_matched_pair (string, start, open, close, flags)
     const char *string;
     int start, open, close, flags;
{
  int i, pass_next, backq, si, c, count, oldjmp;
  size_t slen;
  char *temp, *ss;
  DECLARE_MBSTATE;

  slen = strlen (string + start) + start;
  oldjmp = no_longjmp_on_fatal_error;
  no_longjmp_on_fatal_error = 1;

   
  i = (flags & 2) ? start : start + 1;
  count = 1;
  pass_next = backq = 0;
  ss = (char *)string;
  while (c = string[i])
    {
      if (pass_next)
	{
	  pass_next = 0;
	  if (c == 0)
	    CQ_RETURN(i);
	  ADVANCE_CHAR (string, slen, i);
	  continue;
	}
      else if ((flags & 1) == 0 && c == '\\')
	{
	  pass_next = 1;
	  i++;
	  continue;
	}
      else if (backq)
	{
	  if (c == '`')
	    backq = 0;
	  ADVANCE_CHAR (string, slen, i);
	  continue;
	}
      else if ((flags & 1) == 0 && c == '`')
	{
	  backq = 1;
	  i++;
	  continue;
	}
      else if ((flags & 1) == 0 && c == open)
	{
	  count++;
	  i++;
	  continue;
	}
      else if (c == close)
	{
	  count--;
	  if (count == 0)
	    break;
	  i++;
	  continue;
	}
      else if ((flags & 1) == 0 && (c == '\'' || c == '"'))
	{
	  i = (c == '\'') ? skip_single_quoted (ss, slen, ++i, 0)
			  : skip_double_quoted (ss, slen, ++i, 0);
	   
	}
      else if ((flags & 1) == 0 && c == '$' && (string[i+1] == LPAREN || string[i+1] == LBRACE))
	{
	  si = i + 2;
	  if (string[si] == '\0')
	    CQ_RETURN(si);

	   
	  if (string[i+1] == LPAREN)
	    temp = extract_delimited_string (ss, &si, "$(", "(", ")", SX_NOALLOC|SX_COMMAND);  
	  else
	    temp = extract_dollar_brace_string (ss, &si, 0, SX_NOALLOC);

	  CHECK_STRING_OVERRUN (i, si, slen, c);

	  i = si;
	  if (string[i] == '\0')	 
	    break;
	  i++;
	  continue;
	}
      else
	ADVANCE_CHAR (string, slen, i);
    }

  CQ_RETURN(i);
}

#if defined (ARRAY_VARS)
 
int
skipsubscript (string, start, flags)
     const char *string;
     int start, flags;
{
  return (skip_matched_pair (string, start, '[', ']', flags));
}
#endif

 
int
skip_to_delim (string, start, delims, flags)
     char *string;
     int start;
     char *delims;
     int flags;
{
  int i, pass_next, backq, dquote, si, c, oldjmp;
  int invert, skipquote, skipcmd, noprocsub, completeflag;
  int arithexp, skipcol;
  size_t slen;
  char *temp, open[3];
  DECLARE_MBSTATE;

  slen = strlen (string + start) + start;
  oldjmp = no_longjmp_on_fatal_error;
  if (flags & SD_NOJMP)
    no_longjmp_on_fatal_error = 1;
  invert = (flags & SD_INVERT);
  skipcmd = (flags & SD_NOSKIPCMD) == 0;
  noprocsub = (flags & SD_NOPROCSUB);
  completeflag = (flags & SD_COMPLETE) ? SX_COMPLETE : 0;

  arithexp = (flags & SD_ARITHEXP);
  skipcol = 0;

  i = start;
  pass_next = backq = dquote = 0;
  while (c = string[i])
    {
       
      skipquote = ((flags & SD_NOQUOTEDELIM) && (c == '\'' || c =='"'));
      if (pass_next)
	{
	  pass_next = 0;
	  if (c == 0)
	    CQ_RETURN(i);
	  ADVANCE_CHAR (string, slen, i);
	  continue;
	}
      else if (c == '\\')
	{
	  pass_next = 1;
	  i++;
	  continue;
	}
      else if (backq)
	{
	  if (c == '`')
	    backq = 0;
	  ADVANCE_CHAR (string, slen, i);
	  continue;
	}
      else if (c == '`')
	{
	  backq = 1;
	  i++;
	  continue;
	}
      else if (arithexp && skipcol && c == ':')
	{
	  skipcol--;
	  i++;
	  continue;
	}
      else if (arithexp && c == '?')
	{
	  skipcol++;
	  i++;
	  continue;
	}
      else if (skipquote == 0 && invert == 0 && member (c, delims))
	break;
       
       
      else if (completeflag && i > 0 && string[i-1] == '$' && c == '\'')
	i = skip_single_quoted (string, slen, ++i, SX_COMPLETE);
      else if (c == '\'')
	i = skip_single_quoted (string, slen, ++i, 0);
      else if (c == '"')
	i = skip_double_quoted (string, slen, ++i, completeflag);
      else if (c == LPAREN && arithexp)
        {
          si = i + 1;
          if (string[si] == '\0')
	    CQ_RETURN(si);

	  temp = extract_delimited_string (string, &si, "(", "(", ")", SX_NOALLOC);  
	  i = si;
	  if (string[i] == '\0')	 
	    break;
	  i++;
	  continue;         
        }
      else if (c == '$' && ((skipcmd && string[i+1] == LPAREN) || string[i+1] == LBRACE))
	{
	  si = i + 2;
	  if (string[si] == '\0')
	    CQ_RETURN(si);

	  if (string[i+1] == LPAREN)
	    temp = extract_delimited_string (string, &si, "$(", "(", ")", SX_NOALLOC|SX_COMMAND|completeflag);  
	  else
	    temp = extract_dollar_brace_string (string, &si, 0, SX_NOALLOC);
	  CHECK_STRING_OVERRUN (i, si, slen, c);
	  i = si;
	  if (string[i] == '\0')	 
	    break;
	  i++;
	  continue;
	}
#if defined (PROCESS_SUBSTITUTION)
      else if (skipcmd && noprocsub == 0 && (c == '<' || c == '>') && string[i+1] == LPAREN)
	{
	  si = i + 2;
	  if (string[si] == '\0')
	    CQ_RETURN(si);

	  temp = extract_delimited_string (string, &si, (c == '<') ? "<(" : ">(", "(", ")", SX_COMMAND|SX_NOALLOC);  
	  CHECK_STRING_OVERRUN (i, si, slen, c);
	  i = si;
	  if (string[i] == '\0')
	    break;
	  i++;
	  continue;
	}
#endif  
#if defined (EXTENDED_GLOB)
      else if ((flags & SD_EXTGLOB) && extended_glob && string[i+1] == LPAREN && member (c, "?*+!@"))
	{
	  si = i + 2;
	  if (string[si] == '\0')
	    CQ_RETURN(si);

	  open[0] = c;
	  open[1] = LPAREN;
	  open[2] = '\0';
	  temp = extract_delimited_string (string, &si, open, "(", ")", SX_NOALLOC);  

	  CHECK_STRING_OVERRUN (i, si, slen, c);
	  i = si;
	  if (string[i] == '\0')	 
	    break;
	  i++;
	  continue;
	}
#endif
      else if ((flags & SD_GLOB) && c == LBRACK)
	{
	  si = i + 1;
	  if (string[si] == '\0')
	    CQ_RETURN(si);

	  temp = extract_delimited_string (string, &si, "[", "[", "]", SX_NOALLOC);  

	  i = si;
	  if (string[i] == '\0')	 
	    break;
	  i++;
	  continue;
	}
      else if ((skipquote || invert) && (member (c, delims) == 0))
	break;
      else
	ADVANCE_CHAR (string, slen, i);
    }

  CQ_RETURN(i);
}

#if defined (BANG_HISTORY)
 
int
skip_to_histexp (string, start, delims, flags)
     char *string;
     int start;
     char *delims;
     int flags;
{
  int i, pass_next, backq, dquote, c, oldjmp;
  int histexp_comsub, histexp_backq, old_dquote;
  size_t slen;
  DECLARE_MBSTATE;

  slen = strlen (string + start) + start;
  oldjmp = no_longjmp_on_fatal_error;
  if (flags & SD_NOJMP)
    no_longjmp_on_fatal_error = 1;

  histexp_comsub = histexp_backq = old_dquote = 0;

  i = start;
  pass_next = backq = dquote = 0;
  while (c = string[i])
    {
      if (pass_next)
	{
	  pass_next = 0;
	  if (c == 0)
	    CQ_RETURN(i);
	  ADVANCE_CHAR (string, slen, i);
	  continue;
	}
      else if (c == '\\')
	{
	  pass_next = 1;
	  i++;
	  continue;
	}
      else if (backq && c == '`')
	{
	  backq = 0;
	  histexp_backq--;
	  dquote = old_dquote;
	  i++;
	  continue;
	}
      else if (c == '`')
	{
	  backq = 1;
	  histexp_backq++;
	  old_dquote = dquote;		 
	  dquote = 0;
	  i++;
	  continue;
	}
       
      else if (dquote && c == delims[0] && string[i+1] == '"')
	{
	  i++;
	  continue;
	}
      else if (c == delims[0])
	break;
       
      else if (dquote && c == '\'')
        {
          i++;
          continue;
        }
      else if (c == '\'')
	i = skip_single_quoted (string, slen, ++i, 0);
       
      else if (posixly_correct == 0 && c == '"')
	{
	  dquote = 1 - dquote;
	  i++;
	  continue;
	}     
      else if (c == '"')
	i = skip_double_quoted (string, slen, ++i, 0);
#if defined (PROCESS_SUBSTITUTION)
      else if ((c == '$' || c == '<' || c == '>') && string[i+1] == LPAREN && string[i+2] != LPAREN)
#else
      else if (c == '$' && string[i+1] == LPAREN && string[i+2] != LPAREN)
#endif
        {
	  if (string[i+2] == '\0')
	    CQ_RETURN(i+2);
	  i += 2;
	  histexp_comsub++;
	  old_dquote = dquote;
	  dquote = 0;
        }
      else if (histexp_comsub && c == RPAREN)
	{
	  histexp_comsub--;
	  dquote = old_dquote;
	  i++;
	  continue;
	}
      else if (backq)		 
	{
	  ADVANCE_CHAR (string, slen, i);
	  continue;
	}
      else
	ADVANCE_CHAR (string, slen, i);
    }

  CQ_RETURN(i);
}
#endif  

#if defined (READLINE)
 

int
char_is_quoted (string, eindex)
     char *string;
     int eindex;
{
  int i, pass_next, c, oldjmp;
  size_t slen;
  DECLARE_MBSTATE;

  slen = strlen (string);
  oldjmp = no_longjmp_on_fatal_error;
  no_longjmp_on_fatal_error = 1;
  i = pass_next = 0;

   
  if (current_command_line_count > 0 && dstack.delimiter_depth > 0)
    {
      c = dstack.delimiters[dstack.delimiter_depth - 1];
      if (c == '\'')
	i = skip_single_quoted (string, slen, 0, 0);
      else if (c == '"')
	i = skip_double_quoted (string, slen, 0, SX_COMPLETE);
      if (i > eindex)
	CQ_RETURN (1);
    }

  while (i <= eindex)
    {
      c = string[i];

      if (pass_next)
	{
	  pass_next = 0;
	  if (i >= eindex)	 
	    CQ_RETURN(1);
	  ADVANCE_CHAR (string, slen, i);
	  continue;
	}
      else if (c == '\\')
	{
	  pass_next = 1;
	  i++;
	  continue;
	}
      else if (c == '$' && string[i+1] == '\'' && string[i+2])
	{
	  i += 2;
	  i = skip_single_quoted (string, slen, i, SX_COMPLETE);
	  if (i > eindex)
	    CQ_RETURN (i);
	}
      else if (c == '\'' || c == '"')
	{
	  i = (c == '\'') ? skip_single_quoted (string, slen, ++i, 0)
			  : skip_double_quoted (string, slen, ++i, SX_COMPLETE);
	  if (i > eindex)
	    CQ_RETURN(1);
	   
	}
      else
	ADVANCE_CHAR (string, slen, i);
    }

  CQ_RETURN(0);
}

int
unclosed_pair (string, eindex, openstr)
     char *string;
     int eindex;
     char *openstr;
{
  int i, pass_next, openc, olen;
  size_t slen;
  DECLARE_MBSTATE;

  slen = strlen (string);
  olen = strlen (openstr);
  i = pass_next = openc = 0;
  while (i <= eindex)
    {
      if (pass_next)
	{
	  pass_next = 0;
	  if (i >= eindex)	 
	    return 0;
	  ADVANCE_CHAR (string, slen, i);
	  continue;
	}
      else if (string[i] == '\\')
	{
	  pass_next = 1;
	  i++;
	  continue;
	}
      else if (STREQN (string + i, openstr, olen))
	{
	  openc = 1 - openc;
	  i += olen;
	}
       
      else if (string[i] == '\'' || string[i] == '"')
	{
	  i = (string[i] == '\'') ? skip_single_quoted (string, slen, i, 0)
				  : skip_double_quoted (string, slen, i, SX_COMPLETE);
	  if (i > eindex)
	    return 0;
	}
      else
	ADVANCE_CHAR (string, slen, i);
    }
  return (openc);
}

 
WORD_LIST *
split_at_delims (string, slen, delims, sentinel, flags, nwp, cwp)
     char *string;
     int slen;
     const char *delims;
     int sentinel, flags;
     int *nwp, *cwp;
{
  int ts, te, i, nw, cw, ifs_split, dflags;
  char *token, *d, *d2;
  WORD_LIST *ret, *tl;

  if (string == 0 || *string == '\0')
    {
      if (nwp)
	*nwp = 0;
      if (cwp)
	*cwp = 0;	
      return ((WORD_LIST *)NULL);
    }

  d = (delims == 0) ? ifs_value : (char *)delims;
  ifs_split = delims == 0;

   
  d2 = 0;
  if (delims)
    {
      size_t slength;
#if defined (HANDLE_MULTIBYTE)
      size_t mblength = 1;
#endif
      DECLARE_MBSTATE;

      slength = strlen (delims);
      d2 = (char *)xmalloc (slength + 1);
      i = ts = 0;
      while (delims[i])
	{
#if defined (HANDLE_MULTIBYTE)
	  mbstate_t state_bak;
	  state_bak = state;
	  mblength = MBRLEN (delims + i, slength, &state);
	  if (MB_INVALIDCH (mblength))
	    state = state_bak;
	  else if (mblength > 1)
	    {
	      memcpy (d2 + ts, delims + i, mblength);
	      ts += mblength;
	      i += mblength;
	      slength -= mblength;
	      continue;
	    }
#endif
	  if (whitespace (delims[i]) == 0)
	    d2[ts++] = delims[i];

	  i++;
	  slength--;
	}
      d2[ts] = '\0';
    }

  ret = (WORD_LIST *)NULL;

   
  for (i = 0; member (string[i], d) && spctabnl (string[i]); i++)
    ;
  if (string[i] == '\0')
    {
      FREE (d2);
      return (ret);
    }

  ts = i;
  nw = 0;
  cw = -1;
  dflags = flags|SD_NOJMP;
  while (1)
    {
      te = skip_to_delim (string, ts, d, dflags);

       
      if (ts == te && d2 && member (string[ts], d2))
	{
	  te = ts + 1;
	   
	  if (ifs_split)
	    while (member (string[te], d) && spctabnl (string[te]) && ((flags&SD_NOQUOTEDELIM) == 0 || (string[te] != '\'' && string[te] != '"')))
	      te++;
	  else
	    while (member (string[te], d2) && ((flags&SD_NOQUOTEDELIM) == 0 || (string[te] != '\'' && string[te] != '"')))
	      te++;
	}

      token = substring (string, ts, te);

      ret = add_string_to_list (token, ret);	 
      free (token);
      nw++;

      if (sentinel >= ts && sentinel <= te)
	cw = nw;

       
      if (cwp && cw == -1 && sentinel == ts-1)
	cw = nw;

       
      if (cwp && cw == -1 && sentinel < ts)
	{
	  tl = make_word_list (make_word (""), ret->next);
	  ret->next = tl;
	  cw = nw;
	  nw++;
	}

      if (string[te] == 0)
	break;

      i = te;
       
      while (member (string[i], d) && (ifs_split || spctabnl(string[i])) && ((flags&SD_NOQUOTEDELIM) == 0 || (string[te] != '\'' && string[te] != '"')))
	i++;

      if (string[i])
	ts = i;
      else
	break;
    }

   
  if (cwp && cw == -1 && (sentinel >= slen || sentinel >= te))
    {
      if (whitespace (string[sentinel - 1]))
	{
	  token = "";
	  ret = add_string_to_list (token, ret);
	  nw++;
	}
      cw = nw;
    }

  if (nwp)
    *nwp = nw;
  if (cwp)
    *cwp = cw;

  FREE (d2);

  return (REVERSE_LIST (ret, WORD_LIST *));
}
#endif  

#if 0
 
 
char *
assignment_name (string)
     char *string;
{
  int offset;
  char *temp;

  offset = assignment (string, 0);
  if (offset == 0)
    return (char *)NULL;
  temp = substring (string, 0, offset);
  return (temp);
}
#endif

 
 
 
 
 

 
char *
string_list_internal (list, sep)
     WORD_LIST *list;
     char *sep;
{
  register WORD_LIST *t;
  char *result, *r;
  size_t word_len, sep_len, result_size;

  if (list == 0)
    return ((char *)NULL);

   
  if (list->next == 0)
    return (savestring (list->word->word));

   
  sep_len = STRLEN (sep);
  result_size = 0;

  for (t = list; t; t = t->next)
    {
      if (t != list)
	result_size += sep_len;
      result_size += strlen (t->word->word);
    }

  r = result = (char *)xmalloc (result_size + 1);

  for (t = list; t; t = t->next)
    {
      if (t != list && sep_len)
	{
	  if (sep_len > 1)
	    {
	      FASTCOPY (sep, r, sep_len);
	      r += sep_len;
	    }
	  else
	    *r++ = sep[0];
	}

      word_len = strlen (t->word->word);
      FASTCOPY (t->word->word, r, word_len);
      r += word_len;
    }

  *r = '\0';
  return (result);
}

 
char *
string_list (list)
     WORD_LIST *list;
{
  return (string_list_internal (list, " "));
}

 
char *
ifs_firstchar (lenp)
     int *lenp;
{
  char *ret;
  int len;

  ret = xmalloc (MB_LEN_MAX + 1);
#if defined (HANDLE_MULTIBYTE)
  if (ifs_firstc_len == 1)
    {
      ret[0] = ifs_firstc[0];
      ret[1] = '\0';
      len = ret[0] ? 1 : 0;
    }
  else
    {
      memcpy (ret, ifs_firstc, ifs_firstc_len);
      ret[len = ifs_firstc_len] = '\0';
    }
#else
  ret[0] = ifs_firstc;
  ret[1] = '\0';
  len = ret[0] ? 0 : 1;
#endif

  if (lenp)
    *lenp = len;

  return ret;
}

 
 
char *
string_list_dollar_star (list, quoted, flags)
     WORD_LIST *list;
     int quoted, flags;
{
  char *ret;
#if defined (HANDLE_MULTIBYTE)
#  if defined (__GNUC__)
  char sep[MB_CUR_MAX + 1];
#  else
  char *sep = 0;
#  endif
#else
  char sep[2];
#endif

#if defined (HANDLE_MULTIBYTE)
#  if !defined (__GNUC__)
  sep = (char *)xmalloc (MB_CUR_MAX + 1);
#  endif  
  if (ifs_firstc_len == 1)
    {
      sep[0] = ifs_firstc[0];
      sep[1] = '\0';
    }
  else
    {
      memcpy (sep, ifs_firstc, ifs_firstc_len);
      sep[ifs_firstc_len] = '\0';
    }
#else
  sep[0] = ifs_firstc;
  sep[1] = '\0';
#endif

  ret = string_list_internal (list, sep);
#if defined (HANDLE_MULTIBYTE) && !defined (__GNUC__)
  free (sep);
#endif
  return ret;
}

 
char *
string_list_dollar_at (list, quoted, flags)
     WORD_LIST *list;
     int quoted;
     int flags;
{
  char *ifs, *ret;
#if defined (HANDLE_MULTIBYTE)
#  if defined (__GNUC__)
  char sep[MB_CUR_MAX + 1];
#  else
  char *sep = 0;
#  endif  
#else
  char sep[2];
#endif
  WORD_LIST *tlist;

   
  ifs = ifs_var ? value_cell (ifs_var) : (char *)0;

#if defined (HANDLE_MULTIBYTE)
#  if !defined (__GNUC__)
  sep = (char *)xmalloc (MB_CUR_MAX + 1);
#  endif  
   
  if (flags & PF_ASSIGNRHS)
    {
      sep[0] = ' ';
      sep[1] = '\0';
    }
  else if (ifs && *ifs)
    {
      if (ifs_firstc_len == 1)
	{
	  sep[0] = ifs_firstc[0];
	  sep[1] = '\0';
	}
      else
	{
	  memcpy (sep, ifs_firstc, ifs_firstc_len);
	  sep[ifs_firstc_len] = '\0';
	}
    }
  else
    {
      sep[0] = ' ';
      sep[1] = '\0';
    }
#else	 
   
  sep[0] = ((flags & PF_ASSIGNRHS) || ifs == 0 || *ifs == 0) ? ' ' : *ifs;
  sep[1] = '\0';
#endif	 

   
  tlist = (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES|Q_PATQUOTE))
		? quote_list (list)
		: list_quote_escapes (list);

  ret = string_list_internal (tlist, sep);
#if defined (HANDLE_MULTIBYTE) && !defined (__GNUC__)
  free (sep);
#endif
  return ret;
}

 
 
char *
string_list_pos_params (pchar, list, quoted, pflags)
     int pchar;
     WORD_LIST *list;
     int quoted, pflags;
{
  char *ret;
  WORD_LIST *tlist;

  if (pchar == '*' && (quoted & Q_DOUBLE_QUOTES))
    {
      tlist = quote_list (list);
      word_list_remove_quoted_nulls (tlist);
      ret = string_list_dollar_star (tlist, 0, 0);
    }
  else if (pchar == '*' && (quoted & Q_HERE_DOCUMENT))
    {
      tlist = quote_list (list);
      word_list_remove_quoted_nulls (tlist);
      ret = string_list (tlist);
    }
  else if (pchar == '*' && quoted == 0 && ifs_is_null)	 
    ret = expand_no_split_dollar_star ? string_list_dollar_star (list, quoted, 0) : string_list_dollar_at (list, quoted, 0);	 
  else if (pchar == '*' && quoted == 0 && (pflags & PF_ASSIGNRHS))	 
    ret = expand_no_split_dollar_star ? string_list_dollar_star (list, quoted, 0) : string_list_dollar_at (list, quoted, 0);	 
  else if (pchar == '*')
    {
       
      ret = string_list_dollar_star (list, quoted, 0);
    }
  else if (pchar == '@' && (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)))
     
    ret = string_list_dollar_at (list, quoted, 0);
  else if (pchar == '@' && quoted == 0 && ifs_is_null)	 
    ret = string_list_dollar_at (list, quoted, 0);	 
  else if (pchar == '@' && quoted == 0 && (pflags & PF_ASSIGNRHS))
    ret = string_list_dollar_at (list, quoted, pflags);	 
  else if (pchar == '@')
    ret = string_list_dollar_star (list, quoted, 0);
  else
    ret = string_list ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) ? quote_list (list) : list);

  return ret;
}

 

 

 
#define issep(c) \
	(((separators)[0]) ? ((separators)[1] ? isifs(c) \
					      : (c) == (separators)[0]) \
			   : 0)

 
#define ifs_whitespace(c)	ISSPACE(c)

 
#define ifs_whitesep(c)	((sh_style_split || separators == 0) ? spctabnl (c) \
							     : ifs_whitespace (c))

WORD_LIST *
list_string (string, separators, quoted)
     register char *string, *separators;
     int quoted;
{
  WORD_LIST *result;
  WORD_DESC *t;
  char *current_word, *s;
  int sindex, sh_style_split, whitesep, xflags, free_word;
  size_t slen;

  if (!string || !*string)
    return ((WORD_LIST *)NULL);

  sh_style_split = separators && separators[0] == ' ' &&
				 separators[1] == '\t' &&
				 separators[2] == '\n' &&
				 separators[3] == '\0';
  for (xflags = 0, s = ifs_value; s && *s; s++)
    {
      if (*s == CTLESC) xflags |= SX_NOCTLESC;
      else if (*s == CTLNUL) xflags |= SX_NOESCCTLNUL;
    }

  slen = 0;
   
#if 0
  if (!quoted || !separators || !*separators)
#else
   
  if (!quoted && separators && *separators)
#endif
    {
      for (s = string; *s && issep (*s) && ifs_whitespace (*s); s++);

      if (!*s)
	return ((WORD_LIST *)NULL);

      string = s;
    }

   
  slen = STRLEN (string);
  for (result = (WORD_LIST *)NULL, sindex = 0; string[sindex]; )
    {
       
      current_word = string_extract_verbatim (string, slen, &sindex, separators, xflags);
      if (current_word == 0)
	break;

      free_word = 1;	 

       
      if (QUOTED_NULL (current_word))
	{
	  t = alloc_word_desc ();
	  t->word = make_quoted_char ('\0');
	  t->flags |= W_QUOTED|W_HASQUOTEDNULL;
	  result = make_word_list (t, result);
	}
      else if (current_word[0] != '\0')
	{
	   
	  remove_quoted_nulls (current_word);

	   
	  t = alloc_word_desc ();
	  t->word = current_word;
	  result = make_word_list (t, result);
	  free_word = 0;
	  result->word->flags &= ~W_HASQUOTEDNULL;	 
	  if (quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT))
	    result->word->flags |= W_QUOTED;
	   
	  if (current_word == 0 || current_word[0] == '\0')
	    result->word->flags |= W_SAWQUOTEDNULL;
	}

       
      else if (!sh_style_split && !ifs_whitespace (string[sindex]))
	{
	  t = alloc_word_desc ();
	  t->word = make_quoted_char ('\0');
	  t->flags |= W_QUOTED|W_HASQUOTEDNULL;
	  result = make_word_list (t, result);
	}

      if (free_word)
	free (current_word);

       
      whitesep = string[sindex] && ifs_whitesep (string[sindex]);

       
      if (string[sindex])
	{
	  DECLARE_MBSTATE;
	  ADVANCE_CHAR (string, slen, sindex);
	}

       
      while (string[sindex] && ifs_whitesep (string[sindex]) && issep (string[sindex]))
	sindex++;

       
      if (string[sindex] && whitesep && issep (string[sindex]) && !ifs_whitesep (string[sindex]))
	{
	  sindex++;
	   
	  while (string[sindex] && ifs_whitesep (string[sindex]) && isifs (string[sindex]))
	    sindex++;
	}
    }
  return (REVERSE_LIST (result, WORD_LIST *));
}

 

 
#define islocalsep(c)	(local_cmap[(unsigned char)(c)] != 0)

char *
get_word_from_string (stringp, separators, endptr)
     char **stringp, *separators, **endptr;
{
  register char *s;
  char *current_word;
  int sindex, sh_style_split, whitesep, xflags;
  unsigned char local_cmap[UCHAR_MAX+1];	 
  size_t slen;

  if (!stringp || !*stringp || !**stringp)
    return ((char *)NULL);

  sh_style_split = separators && separators[0] == ' ' &&
				 separators[1] == '\t' &&
				 separators[2] == '\n' &&
				 separators[3] == '\0';
  memset (local_cmap, '\0', sizeof (local_cmap));
  for (xflags = 0, s = separators; s && *s; s++)
    {
      if (*s == CTLESC) xflags |= SX_NOCTLESC;
      if (*s == CTLNUL) xflags |= SX_NOESCCTLNUL;
      local_cmap[(unsigned char)*s] = 1;	 
    }

  s = *stringp;
  slen = 0;

   
  if (sh_style_split || separators == 0)
    for (; *s && spctabnl (*s) && islocalsep (*s); s++);
  else
    for (; *s && ifs_whitespace (*s) && islocalsep (*s); s++);

   
  if (!*s)
    {
      *stringp = s;
      if (endptr)
	*endptr = s;
      return ((char *)NULL);
    }

   
  sindex = 0;
   
  slen = STRLEN (s);
  current_word = string_extract_verbatim (s, slen, &sindex, separators, xflags);

   
  if (endptr)
    *endptr = s + sindex;

   
  whitesep = s[sindex] && ifs_whitesep (s[sindex]);

   
  if (s[sindex])
    {
      DECLARE_MBSTATE;
      ADVANCE_CHAR (s, slen, sindex);
    }

   
  while (s[sindex] && spctabnl (s[sindex]) && islocalsep (s[sindex]))
    sindex++;

   
  if (s[sindex] && whitesep && islocalsep (s[sindex]) && !ifs_whitesep (s[sindex]))
    {
      sindex++;
       
      while (s[sindex] && ifs_whitesep (s[sindex]) && islocalsep(s[sindex]))
	sindex++;
    }

   
  *stringp = s + sindex;
  return (current_word);
}

 
char *
strip_trailing_ifs_whitespace (string, separators, saw_escape)
     char *string, *separators;
     int saw_escape;
{
  char *s;

  s = string + STRLEN (string) - 1;
  while (s > string && ((spctabnl (*s) && isifs (*s)) ||
			(saw_escape && *s == CTLESC && spctabnl (s[1]))))
    s--;
  *++s = '\0';
  return string;
}

#if 0
 
 
WORD_LIST *
list_string_with_quotes (string)
     char *string;
{
  WORD_LIST *list;
  char *token, *s;
  size_t s_len;
  int c, i, tokstart, len;

  for (s = string; s && *s && spctabnl (*s); s++)
    ;
  if (s == 0 || *s == 0)
    return ((WORD_LIST *)NULL);

  s_len = strlen (s);
  tokstart = i = 0;
  list = (WORD_LIST *)NULL;
  while (1)
    {
      c = s[i];
      if (c == '\\')
	{
	  i++;
	  if (s[i])
	    i++;
	}
      else if (c == '\'')
	i = skip_single_quoted (s, s_len, ++i, 0);
      else if (c == '"')
	i = skip_double_quoted (s, s_len, ++i, 0);
      else if (c == 0 || spctabnl (c))
	{
	   
	  token = substring (s, tokstart, i);
	  list = add_string_to_list (token, list);
	  free (token);
	  while (spctabnl (s[i]))
	    i++;
	  if (s[i])
	    tokstart = i;
	  else
	    break;
	}
      else
	i++;	 
    }
  return (REVERSE_LIST (list, WORD_LIST *));
}
#endif

 
 
 
 
 

#if defined (ARRAY_VARS)
static SHELL_VAR *
do_compound_assignment (name, value, flags)
     char *name, *value;
     int flags;
{
  SHELL_VAR *v;
  int mklocal, mkassoc, mkglobal, chklocal;
  WORD_LIST *list;
  char *newname;	 

  mklocal = flags & ASS_MKLOCAL;
  mkassoc = flags & ASS_MKASSOC;
  mkglobal = flags & ASS_MKGLOBAL;
  chklocal = flags & ASS_CHKLOCAL;

  if (mklocal && variable_context)
    {
      v = find_variable (name);		 
      newname = (v == 0) ? nameref_transform_name (name, flags) : v->name;
      if (v && ((readonly_p (v) && (flags & ASS_FORCE) == 0) || noassign_p (v)))
	{
	  if (readonly_p (v))
	    err_readonly (name);
	  return (v);	 
	}
      list = expand_compound_array_assignment (v, value, flags);
      if (mkassoc)
	v = make_local_assoc_variable (newname, 0);
      else if (v == 0 || (array_p (v) == 0 && assoc_p (v) == 0) || v->context != variable_context)
        v = make_local_array_variable (newname, 0);
      if (v)
	assign_compound_array_list (v, list, flags);
      if (list)
	dispose_words (list);
    }
   
  else if (mkglobal && variable_context)
    {
      v = chklocal ? find_variable (name) : 0;
      if (v && (local_p (v) == 0 || v->context != variable_context))
	v = 0;
      if (v == 0)
        v = find_global_variable (name);
      if (v && ((readonly_p (v) && (flags & ASS_FORCE) == 0) || noassign_p (v)))
	{
	  if (readonly_p (v))
	    err_readonly (name);
	  return (v);	 
	}
       
      newname = (v == 0) ? nameref_transform_name (name, flags) : name;
      list = expand_compound_array_assignment (v, value, flags);
      if (v == 0 && mkassoc)
	v = make_new_assoc_variable (newname);
      else if (v && mkassoc && assoc_p (v) == 0)
	v = convert_var_to_assoc (v);
      else if (v == 0)
	v = make_new_array_variable (newname);
      else if (v && mkassoc == 0 && array_p (v) == 0)
	v = convert_var_to_array (v);
      if (v)
	assign_compound_array_list (v, list, flags);
      if (list)
	dispose_words (list);
    }
  else
    {
      v = assign_array_from_string (name, value, flags);
      if (v && ((readonly_p (v) && (flags & ASS_FORCE) == 0) || noassign_p (v)))
	{
	  if (readonly_p (v))
	    err_readonly (name);
	  return (v);	 
	}
    }

  return (v);
}
#endif

 
static int
do_assignment_internal (word, expand)
     const WORD_DESC *word;
     int expand;
{
  int offset, appendop, assign_list, aflags, retval;
  char *name, *value, *temp;
  SHELL_VAR *entry;
#if defined (ARRAY_VARS)
  char *t;
  int ni;
#endif
  const char *string;

  if (word == 0 || word->word == 0)
    return 0;

  appendop = assign_list = aflags = 0;
  string = word->word;
  offset = assignment (string, 0);
  name = savestring (string);
  value = (char *)NULL;

  if (name[offset] == '=')
    {
      if (name[offset - 1] == '+')
	{
	  appendop = 1;
	  name[offset - 1] = '\0';
	}

      name[offset] = 0;		 
      temp = name + offset + 1;

#if defined (ARRAY_VARS)
      if (expand && (word->flags & W_COMPASSIGN))
	{
	  assign_list = ni = 1;
	  value = extract_array_assignment_list (temp, &ni);
	}
      else
#endif
      if (expand && temp[0])
	value = expand_string_if_necessary (temp, 0, expand_string_assignment);
      else
	value = savestring (temp);
    }

  if (value == 0)
    {
      value = (char *)xmalloc (1);
      value[0] = '\0';
    }

  if (echo_command_at_execute)
    {
      if (appendop)
	name[offset - 1] = '+';
      xtrace_print_assignment (name, value, assign_list, 1);
      if (appendop)
	name[offset - 1] = '\0';
    }

#define ASSIGN_RETURN(r)	do { FREE (value); free (name); return (r); } while (0)

  if (appendop)
    aflags |= ASS_APPEND;

#if defined (ARRAY_VARS)
  if (t = mbschr (name, LBRACK))
    {
      if (assign_list)
	{
	  report_error (_("%s: cannot assign list to array member"), name);
	  ASSIGN_RETURN (0);
	}
      aflags |= ASS_ALLOWALLSUB;	 
      entry = assign_array_element (name, value, aflags, (array_eltstate_t *)0);
      if (entry == 0)
	ASSIGN_RETURN (0);
    }
  else if (assign_list)
    {
      if ((word->flags & W_ASSIGNARG) && (word->flags & W_CHKLOCAL))
	aflags |= ASS_CHKLOCAL;
      if ((word->flags & W_ASSIGNARG) && (word->flags & W_ASSNGLOBAL) == 0)
	aflags |= ASS_MKLOCAL;
      if ((word->flags & W_ASSIGNARG) && (word->flags & W_ASSNGLOBAL))
	aflags |= ASS_MKGLOBAL;
      if (word->flags & W_ASSIGNASSOC)
	aflags |= ASS_MKASSOC;
      entry = do_compound_assignment (name, value, aflags);
    }
  else
#endif  
  entry = bind_variable (name, value, aflags);

  if (entry)
    stupidly_hack_special_variables (entry->name);	 
  else
    stupidly_hack_special_variables (name);

   
  if (entry == 0 || readonly_p (entry))
    retval = 0;		 
  else if (noassign_p (entry))
    {
      set_exit_status (EXECUTION_FAILURE);
      retval = 1;	 
    }
  else
    retval = 1;

  if (entry && retval != 0 && noassign_p (entry) == 0)
    VUNSETATTR (entry, att_invisible);

  ASSIGN_RETURN (retval);
}

 
int
do_assignment (string)
     char *string;
{
  WORD_DESC td;

  td.flags = W_ASSIGNMENT;
  td.word = string;

  return do_assignment_internal (&td, 1);
}

int
do_word_assignment (word, flags)
     WORD_DESC *word;
     int flags;
{
  return do_assignment_internal (word, 1);
}

 
int
do_assignment_no_expand (string)
     char *string;
{
  WORD_DESC td;

  td.flags = W_ASSIGNMENT;
  td.word = string;

  return (do_assignment_internal (&td, 0));
}

 

 
WORD_LIST *
list_rest_of_args ()
{
  register WORD_LIST *list, *args;
  int i;

   
  for (i = 1, list = (WORD_LIST *)NULL; i < 10 && dollar_vars[i]; i++)
    list = make_word_list (make_bare_word (dollar_vars[i]), list);

  for (args = rest_of_args; args; args = args->next)
    list = make_word_list (make_bare_word (args->word->word), list);

  return (REVERSE_LIST (list, WORD_LIST *));
}

 
char *
get_dollar_var_value (ind)
     intmax_t ind;
{
  char *temp;
  WORD_LIST *p;

  if (ind < 10)
    temp = dollar_vars[ind] ? savestring (dollar_vars[ind]) : (char *)NULL;
  else	 
    {
      ind -= 10;
      for (p = rest_of_args; p && ind--; p = p->next)
	;
      temp = p ? savestring (p->word->word) : (char *)NULL;
    }
  return (temp);
}

 
char *
string_rest_of_args (dollar_star)
     int dollar_star;
{
  register WORD_LIST *list;
  char *string;

  list = list_rest_of_args ();
  string = dollar_star ? string_list_dollar_star (list, 0, 0) : string_list (list);
  dispose_words (list);
  return (string);
}

 
static char *
pos_params (string, start, end, quoted, pflags)
     char *string;
     int start, end, quoted, pflags;
{
  WORD_LIST *save, *params, *h, *t;
  char *ret;
  int i;

   
  if (start == end)
    return ((char *)NULL);

  save = params = list_rest_of_args ();
  if (save == 0 && start > 0)
    return ((char *)NULL);

  if (start == 0)		 
    {
      t = make_word_list (make_word (dollar_vars[0]), params);
      save = params = t;
    }

  for (i = start ? 1 : 0; params && i < start; i++)
    params = params->next;
  if (params == 0)
    {
      dispose_words (save);
      return ((char *)NULL);
    }
  for (h = t = params; params && i < end; i++)
    {
      t = params;
      params = params->next;
    }
  t->next = (WORD_LIST *)NULL;

  ret = string_list_pos_params (string[0], h, quoted, pflags);

  if (t != params)
    t->next = params;

  dispose_words (save);
  return (ret);
}

 
 
 
 
 

#if defined (PROCESS_SUBSTITUTION)
#define EXP_CHAR(s) (s == '$' || s == '`' || s == '<' || s == '>' || s == CTLESC || s == '~')
#else
#define EXP_CHAR(s) (s == '$' || s == '`' || s == CTLESC || s == '~')
#endif

 
#define ARITH_EXP_CHAR(s) (s == '$' || s == '`' || s == CTLESC || s == '~')

 
static char *
expand_string_if_necessary (string, quoted, func)
     char *string;
     int quoted;
     EXPFUNC *func;
{
  WORD_LIST *list;
  size_t slen;
  int i, saw_quote;
  char *ret;
  DECLARE_MBSTATE;

   
  slen = (MB_CUR_MAX > 1) ? strlen (string) : 0;
  i = saw_quote = 0;
  while (string[i])
    {
      if (EXP_CHAR (string[i]))
	break;
      else if (string[i] == '\'' || string[i] == '\\' || string[i] == '"')
	saw_quote = 1;
      ADVANCE_CHAR (string, slen, i);
    }

  if (string[i])
    {
      list = (*func) (string, quoted);
      if (list)
	{
	  ret = string_list (list);
	  dispose_words (list);
	}
      else
	ret = (char *)NULL;
    }
  else if (saw_quote && ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) == 0))
    ret = string_quote_removal (string, quoted);
  else
    ret = savestring (string);

  return ret;
}

static inline char *
expand_string_to_string_internal (string, quoted, func)
     char *string;
     int quoted;
     EXPFUNC *func;
{
  WORD_LIST *list;
  char *ret;

  if (string == 0 || *string == '\0')
    return ((char *)NULL);

  list = (*func) (string, quoted);
  if (list)
    {
      ret = string_list (list);
      dispose_words (list);
    }
  else
    ret = (char *)NULL;

  return (ret);
}

char *
expand_string_to_string (string, quoted)
     char *string;
     int quoted;
{
  return (expand_string_to_string_internal (string, quoted, expand_string));
}

char *
expand_string_unsplit_to_string (string, quoted)
     char *string;
     int quoted;
{
  return (expand_string_to_string_internal (string, quoted, expand_string_unsplit));
}

char *
expand_assignment_string_to_string (string, quoted)
     char *string;
     int quoted;
{
  return (expand_string_to_string_internal (string, quoted, expand_string_assignment));
}

 
static char *
quote_string_for_repl (string, flags)
     char *string;
     int flags;
{
  size_t slen;
  char *result, *t;
  const char *s, *send;
  DECLARE_MBSTATE;

  slen = strlen (string);
  send = string + slen;

  result = (char *)xmalloc (slen * 2 + 1);

  if (string[0] == CTLESC && string[1] == 0)
    {
      result[0] = CTLESC;
      result[1] = '\0';
      return (result);
    }

   

  for (s = string, t = result; *s; )
    {
       
      if (*s == CTLESC && (s[1] == '&' || s[1] == '\\'))
        {
          *t++ = '\\';
          s++;
          *t++ = *s++;
          continue;
        }
       
      if (*s == CTLESC)
        {
	  s++;
	  if (*s == '\0')
	    break;
        }
      COPY_CHAR_P (t, s, send);
    }

  *t = '\0';
  return (result);
}
	
 
static char *
expand_string_for_patsub (string, quoted)
     char *string;
     int quoted;
{
  WORD_LIST *value;
  char *ret, *t;

  if (string == 0 || *string == '\0')
    return (char *)NULL;

  value = expand_string_for_pat (string, quoted, (int *)0, (int *)0);

  if (value && value->word)
    {
      remove_quoted_nulls (value->word->word);	 
      value->word->flags &= ~W_HASQUOTEDNULL;
    }

  if (value)
    {
      t = (value->next) ? string_list (value) : value->word->word;
      ret = quote_string_for_repl (t, quoted);
      if (t != value->word->word)
	free (t);
      dispose_words (value);
    }
  else
    ret = (char *)NULL;

  return (ret);
}

char *
expand_arith_string (string, quoted)
     char *string;
     int quoted;
{
  WORD_DESC td;
  WORD_LIST *list, *tlist;
  size_t slen;
  int i, saw_quote;
  char *ret;
  DECLARE_MBSTATE;

   
  slen = (MB_CUR_MAX > 1) ? strlen (string) : 0;
  i = saw_quote = 0;
  while (string[i])
    {
      if (ARITH_EXP_CHAR (string[i]))
	break;
      else if (string[i] == '\'' || string[i] == '\\' || string[i] == '"')
	saw_quote = string[i];
      ADVANCE_CHAR (string, slen, i);
    }

  if (string[i])
    {
       
      td.flags = W_NOPROCSUB|W_NOTILDE;	 
#if 0	 
      if (quoted & Q_ARRAYSUB)
	td.flags |= W_NOCOMSUB;
#endif
      td.word = savestring (string);
      list = call_expand_word_internal (&td, quoted, 0, (int *)NULL, (int *)NULL);
       
      if (list)
	{
	  tlist = word_list_split (list);
	  dispose_words (list);
	  list = tlist;
	  if (list)
	    dequote_list (list);
	}
       
      if (list)
	{
	  ret = string_list (list);
	  dispose_words (list);
	}
      else
	ret = (char *)NULL;
      FREE (td.word);
    }
  else if (saw_quote && (quoted & Q_ARITH))
    ret = string_quote_removal (string, quoted);
  else if (saw_quote && ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) == 0))
    ret = string_quote_removal (string, quoted);
  else
    ret = savestring (string);

  return ret;
}

#if defined (COND_COMMAND)
 
char *
remove_backslashes (string)
     char *string;
{
  char *r, *ret, *s;

  r = ret = (char *)xmalloc (strlen (string) + 1);
  for (s = string; s && *s; )
    {
      if (*s == '\\')
	s++;
      if (*s == 0)
	break;
      *r++ = *s++;
    }
  *r = '\0';
  return ret;
}

 
 
char *
cond_expand_word (w, special)
     WORD_DESC *w;
     int special;
{
  char *r, *p;
  WORD_LIST *l;
  int qflags;

  if (w->word == 0 || w->word[0] == '\0')
    return ((char *)NULL);

  expand_no_split_dollar_star = 1;
  w->flags |= W_NOSPLIT2;
  qflags = (special == 3) ? Q_ARITH : 0;
  l = call_expand_word_internal (w, qflags, 0, (int *)0, (int *)0);
  expand_no_split_dollar_star = 0;
  if (l)
    {
      if (special == 0)			 
	{
	  if (l->word)
	    word_list_remove_quoted_nulls (l);
	  dequote_list (l);
	  r = string_list (l);
	}
      else if (special == 3)		 
	{
	  if (l->word)
	    word_list_remove_quoted_nulls (l);	 
	  dequote_list (l);
	  r = string_list (l);
	}
      else
	{
	   
	  qflags = QGLOB_CVTNULL|QGLOB_CTLESC;
	  if (special == 2)
	    qflags |= QGLOB_REGEXP;
	  word_list_remove_quoted_nulls (l);
	  p = string_list (l);
	  r = quote_string_for_globbing (p, qflags);
	  free (p);
	}
      dispose_words (l);
    }
  else
    r = (char *)NULL;

  return r;
}
#endif

 
char *
expand_string_dollar_quote (string, flags)
     char *string;
     int flags;
{
  size_t slen, retind, retsize;
  int sindex, c, translen, peekc, news;
  char *ret, *trans, *send, *t;
  DECLARE_MBSTATE;

  slen = strlen (string);
  send = string + slen;
  sindex = 0;

  retsize = slen + 1;
  ret = xmalloc (retsize);
  retind = 0;

  while (c = string[sindex])
    {
      switch (c)
	{
	default:
	  RESIZE_MALLOCED_BUFFER (ret, retind, locale_mb_cur_max + 1, retsize, 64);
	  COPY_CHAR_I (ret, retind, string, send, sindex);
	  break;

	case '\\':
	  RESIZE_MALLOCED_BUFFER (ret, retind, locale_mb_cur_max + 2, retsize, 64);
	  ret[retind++] = string[sindex++];

	  if (string[sindex])
	    COPY_CHAR_I (ret, retind, string, send, sindex);
	  break;

	case '\'':
	case '"':
	  if (c == '\'')
	    news = skip_single_quoted (string, slen, ++sindex, SX_COMPLETE);
	  else
	    news = skip_double_quoted (string, slen, ++sindex, SX_COMPLETE);
	  translen = news - sindex - 1;
	  RESIZE_MALLOCED_BUFFER (ret, retind, translen + 3, retsize, 64);
	  ret[retind++] = c;
	  if (translen > 0)
	    {
	      strncpy (ret + retind, string + sindex, translen);
	      retind += translen;
	    }
	  if (news > sindex && string[news - 1] == c)
	    ret[retind++] = c;
	  sindex = news;
	  break;

	case CTLESC:
	  RESIZE_MALLOCED_BUFFER (ret, retind, locale_mb_cur_max + 2, retsize, 64);
	  if (flags)
	    ret[retind++] = string[sindex++];
	  if (string[sindex])
	    COPY_CHAR_I (ret, retind, string, send, sindex);
	  break;

	case '$':
	  peekc = string[++sindex];
#if defined (TRANSLATABLE_STRINGS)
	  if (peekc != '\'' && peekc != '"')
#else
	  if (peekc != '\'')
#endif
	    {
	      RESIZE_MALLOCED_BUFFER (ret, retind, 2, retsize, 16);
	      ret[retind++] = c;
	      break;
	    }
	  if (string[sindex + 1] == '\0')	 	
	    {
	      RESIZE_MALLOCED_BUFFER (ret, retind, 3, retsize, 16);
	      ret[retind++] = c;
	      ret[retind++] = peekc;
	      sindex++;
	      break;
	    }
	  if (peekc == '\'')
	    {
	       
	       
	      news = skip_single_quoted (string, slen, ++sindex, SX_COMPLETE);
	       
	      if (news > sindex && string[news] == '\0' && string[news-1] != peekc)
		{
		  RESIZE_MALLOCED_BUFFER (ret, retind, 3, retsize, 16);
		  ret[retind++] = c;
		  ret[retind++] = peekc;
		  continue;
		}
	      t = substring (string, sindex, news - 1);
	      trans = ansiexpand (t, 0, news-sindex-1, &translen);
	      free (t);
	      t = sh_single_quote (trans);
	      sindex = news;
	    }
#if defined (TRANSLATABLE_STRINGS)
	  else
	    {
	      news = ++sindex;
	      t = string_extract_double_quoted (string, &news, SX_COMPLETE);
	       
	      if (news > sindex && string[news] == '\0' && string[news-1] != peekc)
		{
		  RESIZE_MALLOCED_BUFFER (ret, retind, 3, retsize, 16);
		  ret[retind++] = c;
		  ret[retind++] = peekc;
		  free (t);
		  continue;
		}
	      trans = locale_expand (t, 0, news-sindex, 0, &translen);
	      free (t);
	      if (singlequote_translations &&
		    ((news-sindex-1) != translen || STREQN (t, trans, translen) == 0))
		t = sh_single_quote (trans);
	      else
		t = sh_mkdoublequoted (trans, translen, 0);
	      sindex = news;
	    }
#endif  
	  free (trans);
	  trans = t;
	  translen = strlen (trans);

	  RESIZE_MALLOCED_BUFFER (ret, retind, translen + 1, retsize, 128);
	  strcpy (ret + retind, trans);
	  retind += translen;
	  FREE (trans);
	  break;
	}
    }

  ret[retind] = 0;
  return ret;
}

 
static WORD_LIST *
call_expand_word_internal (w, q, i, c, e)
     WORD_DESC *w;
     int q, i, *c, *e;
{
  WORD_LIST *result;

  result = expand_word_internal (w, q, i, c, e);
  if (result == &expand_word_error || result == &expand_word_fatal)
    {
       
      w->word = (char *)NULL;
      last_command_exit_value = EXECUTION_FAILURE;
      exp_jump_to_top_level ((result == &expand_word_error) ? DISCARD : FORCE_EOF);
       
      return (NULL);
    }
  else
    return (result);
}

 
static WORD_LIST *
expand_string_internal (string, quoted)
     char *string;
     int quoted;
{
  WORD_DESC td;
  WORD_LIST *tresult;

  if (string == 0 || *string == 0)
    return ((WORD_LIST *)NULL);

  td.flags = 0;
  td.word = savestring (string);

  tresult = call_expand_word_internal (&td, quoted, 0, (int *)NULL, (int *)NULL);

  FREE (td.word);
  return (tresult);
}

 
WORD_LIST *
expand_string_unsplit (string, quoted)
     char *string;
     int quoted;
{
  WORD_LIST *value;

  if (string == 0 || *string == '\0')
    return ((WORD_LIST *)NULL);

  expand_no_split_dollar_star = 1;
  value = expand_string_internal (string, quoted);
  expand_no_split_dollar_star = 0;

  if (value)
    {
      if (value->word)
	{
	  remove_quoted_nulls (value->word->word);	 
	  value->word->flags &= ~W_HASQUOTEDNULL;
	}
      dequote_list (value);
    }
  return (value);
}

 
WORD_LIST *
expand_string_assignment (string, quoted)
     char *string;
     int quoted;
{
  WORD_DESC td;
  WORD_LIST *value;

  if (string == 0 || *string == '\0')
    return ((WORD_LIST *)NULL);

  expand_no_split_dollar_star = 1;

#if 0
   
  td.flags = W_ASSIGNRHS|W_NOSPLIT2;		 
#else
  td.flags = W_ASSIGNRHS;
#endif
  td.flags |= (W_NOGLOB|W_TILDEEXP);
  td.word = savestring (string);
  value = call_expand_word_internal (&td, quoted, 0, (int *)NULL, (int *)NULL);
  FREE (td.word);

  expand_no_split_dollar_star = 0;

  if (value)
    {
      if (value->word)
	{
	  remove_quoted_nulls (value->word->word);	 
	  value->word->flags &= ~W_HASQUOTEDNULL;
	}
      dequote_list (value);
    }
  return (value);
}

 
WORD_LIST *
expand_prompt_string (string, quoted, wflags)
     char *string;
     int quoted;
     int wflags;
{
  WORD_LIST *value;
  WORD_DESC td;

  if (string == 0 || *string == 0)
    return ((WORD_LIST *)NULL);

  td.flags = wflags;
  td.word = savestring (string);

  no_longjmp_on_fatal_error = 1;
  value = expand_word_internal (&td, quoted, 0, (int *)NULL, (int *)NULL);
  no_longjmp_on_fatal_error = 0;

  if (value == &expand_word_error || value == &expand_word_fatal)
    {
      value = make_word_list (make_bare_word (string), (WORD_LIST *)NULL);
      return value;
    }
  FREE (td.word);
  if (value)
    {
      if (value->word)
	{
	  remove_quoted_nulls (value->word->word);	 
	  value->word->flags &= ~W_HASQUOTEDNULL;
	}
      dequote_list (value);
    }
  return (value);
}

 
static WORD_LIST *
expand_string_leave_quoted (string, quoted)
     char *string;
     int quoted;
{
  WORD_LIST *tlist;
  WORD_LIST *tresult;

  if (string == 0 || *string == '\0')
    return ((WORD_LIST *)NULL);

  tlist = expand_string_internal (string, quoted);

  if (tlist)
    {
      tresult = word_list_split (tlist);
      dispose_words (tlist);
      return (tresult);
    }
  return ((WORD_LIST *)NULL);
}

 
static WORD_LIST *
expand_string_for_rhs (string, quoted, op, pflags, dollar_at_p, expanded_p)
     char *string;
     int quoted, op, pflags;
     int *dollar_at_p, *expanded_p;
{
  WORD_DESC td;
  WORD_LIST *tresult;
  int old_nosplit;

  if (string == 0 || *string == '\0')
    return (WORD_LIST *)NULL;

   
   
   
  old_nosplit = expand_no_split_dollar_star;
  expand_no_split_dollar_star = (quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT)) || op == '=' || ifs_is_null == 0;	 
  td.flags = W_EXPANDRHS;		 
  td.flags |= W_NOSPLIT2;		 
  if (pflags & PF_ASSIGNRHS)		 
    td.flags |= W_ASSIGNRHS;
  if (op == '=')
#if 0
    td.flags |= W_ASSIGNRHS;		 
#else
    td.flags |= W_ASSIGNRHS|W_NOASSNTILDE;		 
#endif
  td.word = savestring (string);
  tresult = call_expand_word_internal (&td, quoted, 1, dollar_at_p, expanded_p);
  expand_no_split_dollar_star = old_nosplit;
  free (td.word);

  return (tresult);
}

 
static WORD_LIST *
expand_string_for_pat (string, quoted, dollar_at_p, expanded_p)
     char *string;
     int quoted, *dollar_at_p, *expanded_p;
{
  WORD_DESC td;
  WORD_LIST *tresult;
  int oexp;

  if (string == 0 || *string == '\0')
    return (WORD_LIST *)NULL;

  oexp = expand_no_split_dollar_star;
  expand_no_split_dollar_star = 1;
  td.flags = W_NOSPLIT2;		 
  td.word = savestring (string);
  tresult = call_expand_word_internal (&td, quoted, 1, dollar_at_p, expanded_p);
  expand_no_split_dollar_star = oexp;
  free (td.word);

  return (tresult);
}

 
WORD_LIST *
expand_string (string, quoted)
     char *string;
     int quoted;
{
  WORD_LIST *result;

  if (string == 0 || *string == '\0')
    return ((WORD_LIST *)NULL);

  result = expand_string_leave_quoted (string, quoted);
  return (result ? dequote_list (result) : result);
}

 

 

WORD_LIST *
expand_word (word, quoted)
     WORD_DESC *word;
     int quoted;
{
  WORD_LIST *result, *tresult;

  tresult = call_expand_word_internal (word, quoted, 0, (int *)NULL, (int *)NULL);
  result = word_list_split (tresult);
  dispose_words (tresult);
  return (result ? dequote_list (result) : result);
}

 
WORD_LIST *
expand_word_unsplit (word, quoted)
     WORD_DESC *word;
     int quoted;
{
  WORD_LIST *result;

  result = expand_word_leave_quoted (word, quoted);
  return (result ? dequote_list (result) : result);
}

 
WORD_LIST *
expand_word_leave_quoted (word, quoted)
     WORD_DESC *word;
     int quoted;
{
  WORD_LIST *result;

  expand_no_split_dollar_star = 1;
  if (ifs_is_null)
    word->flags |= W_NOSPLIT;
  word->flags |= W_NOSPLIT2;
  result = call_expand_word_internal (word, quoted, 0, (int *)NULL, (int *)NULL);
  expand_no_split_dollar_star = 0;

  return result;
}

 

 

 
static char *
quote_escapes_internal (string, flags)
     const char *string;
     int flags;
{
  const char *s, *send;
  char *t, *result;
  size_t slen;
  int quote_spaces, skip_ctlesc, skip_ctlnul, nosplit;
  DECLARE_MBSTATE; 

  slen = strlen (string);
  send = string + slen;

  quote_spaces = (ifs_value && *ifs_value == 0);
  nosplit = (flags & PF_NOSPLIT2);

  for (skip_ctlesc = skip_ctlnul = 0, s = ifs_value; s && *s; s++)
    {
      skip_ctlesc |= (nosplit == 0 && *s == CTLESC);
      skip_ctlnul |= (nosplit == 0 && *s == CTLNUL);
    }

  t = result = (char *)xmalloc ((slen * 2) + 1);
  s = string;

  while (*s)
    {
      if ((skip_ctlesc == 0 && *s == CTLESC) || (skip_ctlnul == 0 && *s == CTLNUL) || (quote_spaces && *s == ' '))
	*t++ = CTLESC;
      COPY_CHAR_P (t, s, send);
    }
  *t = '\0';

  return (result);
}

char *
quote_escapes (string)
     const char *string;
{
  return (quote_escapes_internal (string, 0));
}

char *
quote_rhs (string)
     const char *string;
{
  return (quote_escapes_internal (string, PF_NOSPLIT2));
}

static WORD_LIST *
list_quote_escapes (list)
     WORD_LIST *list;
{
  register WORD_LIST *w;
  char *t;

  for (w = list; w; w = w->next)
    {
      t = w->word->word;
      w->word->word = quote_escapes (t);
      free (t);
    }
  return list;
}

 
char *
dequote_escapes (string)
     const char *string;
{
  const char *s, *send;
  char *t, *result;
  size_t slen;
  int quote_spaces;
  DECLARE_MBSTATE;

  if (string == 0)
    return (char *)0;

  slen = strlen (string);
  send = string + slen;

  t = result = (char *)xmalloc (slen + 1);

  if (strchr (string, CTLESC) == 0)
    return (strcpy (result, string));

  quote_spaces = (ifs_value && *ifs_value == 0);

  s = string;
  while (*s)
    {
      if (*s == CTLESC && (s[1] == CTLESC || s[1] == CTLNUL || (quote_spaces && s[1] == ' ')))
	{
	  s++;
	  if (*s == '\0')
	    break;
	}
      COPY_CHAR_P (t, s, send);
    }
  *t = '\0';

  return result;
}

#if defined (INCLUDE_UNUSED)
static WORD_LIST *
list_dequote_escapes (list)
     WORD_LIST *list;
{
  register WORD_LIST *w;
  char *t;

  for (w = list; w; w = w->next)
    {
      t = w->word->word;
      w->word->word = dequote_escapes (t);
      free (t);
    }
  return list;
}
#endif

 
static char *
make_quoted_char (c)
     int c;
{
  char *temp;

  temp = (char *)xmalloc (3);
  if (c == 0)
    {
      temp[0] = CTLNUL;
      temp[1] = '\0';
    }
  else
    {
      temp[0] = CTLESC;
      temp[1] = c;
      temp[2] = '\0';
    }
  return (temp);
}

 
char *
quote_string (string)
     char *string;
{
  register char *t;
  size_t slen;
  char *result, *send;

  if (*string == 0)
    {
      result = (char *)xmalloc (2);
      result[0] = CTLNUL;
      result[1] = '\0';
    }
  else
    {
      DECLARE_MBSTATE;

      slen = strlen (string);
      send = string + slen;

      result = (char *)xmalloc ((slen * 2) + 1);

      for (t = result; string < send; )
	{
	  *t++ = CTLESC;
	  COPY_CHAR_P (t, string, send);
	}
      *t = '\0';
    }
  return (result);
}

 
char *
dequote_string (string)
     char *string;
{
  register char *s, *t;
  size_t slen;
  char *result, *send;
  DECLARE_MBSTATE;

  if (string[0] == CTLESC && string[1] == 0)
    internal_debug ("dequote_string: string with bare CTLESC");

  slen = STRLEN (string);

  t = result = (char *)xmalloc (slen + 1);

  if (QUOTED_NULL (string))
    {
      result[0] = '\0';
      return (result);
    }

   
  if (string[0] == CTLESC && string[1] == 0)
    {
      result[0] = CTLESC;
      result[1] = '\0';
      return (result);
    }

   
  if (strchr (string, CTLESC) == NULL)
    return (strcpy (result, string));

  send = string + slen;
  s = string;
  while (*s)
    {
      if (*s == CTLESC)
	{
	  s++;
	  if (*s == '\0')
	    break;
	}
      COPY_CHAR_P (t, s, send);
    }

  *t = '\0';
  return (result);
}

 
static WORD_LIST *
quote_list (list)
     WORD_LIST *list;
{
  register WORD_LIST *w;
  char *t;

  for (w = list; w; w = w->next)
    {
      t = w->word->word;
      w->word->word = quote_string (t);
      if (*t == 0)
	w->word->flags |= W_HASQUOTEDNULL;	 
      w->word->flags |= W_QUOTED;
      free (t);
    }
  return list;
}

WORD_DESC *
dequote_word (word)
     WORD_DESC *word;
{
  register char *s;

  s = dequote_string (word->word);
  if (QUOTED_NULL (word->word))
    word->flags &= ~W_HASQUOTEDNULL;
  free (word->word);
  word->word = s;

  return word;
}

 
WORD_LIST *
dequote_list (list)
     WORD_LIST *list;
{
  register char *s;
  register WORD_LIST *tlist;

  for (tlist = list; tlist; tlist = tlist->next)
    {
      s = dequote_string (tlist->word->word);
      if (QUOTED_NULL (tlist->word->word))
	tlist->word->flags &= ~W_HASQUOTEDNULL;
      free (tlist->word->word);
      tlist->word->word = s;
    }
  return list;
}

 
char *
remove_quoted_escapes (string)
     char *string;
{
  char *t;

  if (string)
    {
      t = dequote_escapes (string);
      strcpy (string, t);
      free (t);
    }

  return (string);
}

 
char *
remove_quoted_ifs (string)
     char *string;
{
  register size_t slen;
  register int i, j;
  char *ret, *send;
  DECLARE_MBSTATE;

  slen = strlen (string);
  send = string + slen;

  i = j = 0;
  ret = (char *)xmalloc (slen + 1);

  while (i < slen)
    {
      if (string[i] == CTLESC)
	{
	  i++;
	  if (string[i] == 0 || isifs (string[i]) == 0)
	    ret[j++] = CTLESC;
	  if (i == slen)
	    break;
	}

      COPY_CHAR_I (ret, j, string, send, i);
    }
  ret[j] = '\0';

  return (ret);
}

char *
remove_quoted_nulls (string)
     char *string;
{
  register size_t slen;
  register int i, j, prev_i;
  DECLARE_MBSTATE;

  if (strchr (string, CTLNUL) == 0)		 
    return string;				 

  slen = strlen (string);
  i = j = 0;

  while (i < slen)
    {
      if (string[i] == CTLESC)
	{
	   
	  i++;
	  string[j++] = CTLESC;
	  if (i == slen)
	    break;
	}
      else if (string[i] == CTLNUL)
	{
	  i++;
	  continue;
	}

      prev_i = i;
      ADVANCE_CHAR (string, slen, i);		 
      if (j < prev_i)
	{
	  do string[j++] = string[prev_i++]; while (prev_i < i);
	}
      else
	j = i;
    }
  string[j] = '\0';

  return (string);
}

 
void
word_list_remove_quoted_nulls (list)
     WORD_LIST *list;
{
  register WORD_LIST *t;

  for (t = list; t; t = t->next)
    {
      remove_quoted_nulls (t->word->word);
      t->word->flags &= ~W_HASQUOTEDNULL;
    }
}

 
 
 
 
 

#if defined (HANDLE_MULTIBYTE)
# ifdef INCLUDE_UNUSED
static unsigned char *
mb_getcharlens (string, len)
     char *string;
     int len;
{
  int i, offset, last;
  unsigned char *ret;
  char *p;
  DECLARE_MBSTATE;

  i = offset = 0;
  last = 0;
  ret = (unsigned char *)xmalloc (len);
  memset (ret, 0, len);
  while (string[last])
    {
      ADVANCE_CHAR (string, len, offset);
      ret[last] = offset - last;
      last = offset;
    }
  return ret;
}
#  endif
#endif

 

#define RP_LONG_LEFT	1
#define RP_SHORT_LEFT	2
#define RP_LONG_RIGHT	3
#define RP_SHORT_RIGHT	4

 
static char *
remove_upattern (param, pattern, op)
     char *param, *pattern;
     int op;
{
  register size_t len;
  register char *end;
  register char *p, *ret, c;

  len = STRLEN (param);
  end = param + len;

  switch (op)
    {
      case RP_LONG_LEFT:	 
	for (p = end; p >= param; p--)
	  {
	    c = *p; *p = '\0';
	    if (strmatch (pattern, param, FNMATCH_EXTFLAG) != FNM_NOMATCH)
	      {
		*p = c;
		return (savestring (p));
	      }
	    *p = c;

	  }
	break;

      case RP_SHORT_LEFT:	 
	for (p = param; p <= end; p++)
	  {
	    c = *p; *p = '\0';
	    if (strmatch (pattern, param, FNMATCH_EXTFLAG) != FNM_NOMATCH)
	      {
		*p = c;
		return (savestring (p));
	      }
	    *p = c;
	  }
	break;

      case RP_LONG_RIGHT:	 
	for (p = param; p <= end; p++)
	  {
	    if (strmatch (pattern, p, FNMATCH_EXTFLAG) != FNM_NOMATCH)
	      {
		c = *p; *p = '\0';
		ret = savestring (param);
		*p = c;
		return (ret);
	      }
	  }
	break;

      case RP_SHORT_RIGHT:	 
	for (p = end; p >= param; p--)
	  {
	    if (strmatch (pattern, p, FNMATCH_EXTFLAG) != FNM_NOMATCH)
	      {
		c = *p; *p = '\0';
		ret = savestring (param);
		*p = c;
		return (ret);
	      }
	  }
	break;
    }

  return (param);	 
}

#if defined (HANDLE_MULTIBYTE)
 
static wchar_t *
remove_wpattern (wparam, wstrlen, wpattern, op)
     wchar_t *wparam;
     size_t wstrlen;
     wchar_t *wpattern;
     int op;
{
  wchar_t wc, *ret;
  int n;

  switch (op)
    {
      case RP_LONG_LEFT:	 
        for (n = wstrlen; n >= 0; n--)
	  {
	    wc = wparam[n]; wparam[n] = L'\0';
	    if (wcsmatch (wpattern, wparam, FNMATCH_EXTFLAG) != FNM_NOMATCH)
	      {
		wparam[n] = wc;
		return (wcsdup (wparam + n));
	      }
	    wparam[n] = wc;
	  }
	break;

      case RP_SHORT_LEFT:	 
	for (n = 0; n <= wstrlen; n++)
	  {
	    wc = wparam[n]; wparam[n] = L'\0';
	    if (wcsmatch (wpattern, wparam, FNMATCH_EXTFLAG) != FNM_NOMATCH)
	      {
		wparam[n] = wc;
		return (wcsdup (wparam + n));
	      }
	    wparam[n] = wc;
	  }
	break;

      case RP_LONG_RIGHT:	 
        for (n = 0; n <= wstrlen; n++)
	  {
	    if (wcsmatch (wpattern, wparam + n, FNMATCH_EXTFLAG) != FNM_NOMATCH)
	      {
		wc = wparam[n]; wparam[n] = L'\0';
		ret = wcsdup (wparam);
		wparam[n] = wc;
		return (ret);
	      }
	  }
	break;

      case RP_SHORT_RIGHT:	 
	for (n = wstrlen; n >= 0; n--)
	  {
	    if (wcsmatch (wpattern, wparam + n, FNMATCH_EXTFLAG) != FNM_NOMATCH)
	      {
		wc = wparam[n]; wparam[n] = L'\0';
		ret = wcsdup (wparam);
		wparam[n] = wc;
		return (ret);
	      }
	  }
	break;
    }

  return (wparam);	 
}
#endif  

static char *
remove_pattern (param, pattern, op)
     char *param, *pattern;
     int op;
{
  char *xret;

  if (param == NULL)
    return (param);
  if (*param == '\0' || pattern == NULL || *pattern == '\0')	 
    return (savestring (param));

#if defined (HANDLE_MULTIBYTE)
  if (MB_CUR_MAX > 1)
    {
      wchar_t *ret, *oret;
      size_t n;
      wchar_t *wparam, *wpattern;
      mbstate_t ps;

       

      n = xdupmbstowcs (&wpattern, NULL, pattern);
      if (n == (size_t)-1)
	{
	  xret = remove_upattern (param, pattern, op);
	  return ((xret == param) ? savestring (param) : xret);
	}
      n = xdupmbstowcs (&wparam, NULL, param);

      if (n == (size_t)-1)
	{
	  free (wpattern);
	  xret = remove_upattern (param, pattern, op);
	  return ((xret == param) ? savestring (param) : xret);
	}
      oret = ret = remove_wpattern (wparam, n, wpattern, op);
       
      if (ret == wparam)
        {
          free (wparam);
          free (wpattern);
          return (savestring (param));
        }

      free (wparam);
      free (wpattern);

      n = strlen (param);
      xret = (char *)xmalloc (n + 1);
      memset (&ps, '\0', sizeof (mbstate_t));
      n = wcsrtombs (xret, (const wchar_t **)&ret, n, &ps);
      xret[n] = '\0';		 
      free (oret);
      return xret;      
    }
  else
#endif
    {
      xret = remove_upattern (param, pattern, op);
      return ((xret == param) ? savestring (param) : xret);
    }
}

 
static int
match_upattern (string, pat, mtype, sp, ep)
     char *string, *pat;
     int mtype;
     char **sp, **ep;
{
  int c, mlen;
  size_t len;
  register char *p, *p1, *npat;
  char *end;

   
   
  len = STRLEN (pat);
  if (pat[0] != '*' || (pat[0] == '*' && pat[1] == LPAREN && extended_glob) || pat[len - 1] != '*')
    {
      int unescaped_backslash;
      char *pp;

      p = npat = (char *)xmalloc (len + 3);
      p1 = pat;
      if ((mtype != MATCH_BEG) && (*p1 != '*' || (*p1 == '*' && p1[1] == LPAREN && extended_glob)))
	*p++ = '*';
      while (*p1)
	*p++ = *p1++;
#if 1
       
       
      if ((mtype != MATCH_END) && (p1[-1] == '*' && (unescaped_backslash = p1[-2] == '\\')))
	{
	  pp = p1 - 3;
	  while (pp >= pat && *pp-- == '\\')
	    unescaped_backslash = 1 - unescaped_backslash;
	  if (unescaped_backslash)
	    *p++ = '*';
	}
      else if (mtype != MATCH_END && p1[-1] != '*')
	*p++ = '*';
#else 
      if (p1[-1] != '*' || p1[-2] == '\\')
	*p++ = '*';
#endif
      *p = '\0';
    }
  else
    npat = pat;
  c = strmatch (npat, string, FNMATCH_EXTFLAG | FNMATCH_IGNCASE);
  if (npat != pat)
    free (npat);
  if (c == FNM_NOMATCH)
    return (0);

  len = STRLEN (string);
  end = string + len;

  mlen = umatchlen (pat, len);
  if (mlen > (int)len)
    return (0);

  switch (mtype)
    {
    case MATCH_ANY:
      for (p = string; p <= end; p++)
	{
	  if (match_pattern_char (pat, p, FNMATCH_IGNCASE))
	    {
	      p1 = (mlen == -1) ? end : p + mlen;
	       
	      if (p1 > end)
		break;
	      for ( ; p1 >= p; p1--)
		{
		  c = *p1; *p1 = '\0';
		  if (strmatch (pat, p, FNMATCH_EXTFLAG | FNMATCH_IGNCASE) == 0)
		    {
		      *p1 = c;
		      *sp = p;
		      *ep = p1;
		      return 1;
		    }
		  *p1 = c;
#if 1
		   
		  if (mlen != -1)
		    break;
#endif
		}
	    }
	}

      return (0);

    case MATCH_BEG:
      if (match_pattern_char (pat, string, FNMATCH_IGNCASE) == 0)
	return (0);

      for (p = (mlen == -1) ? end : string + mlen; p >= string; p--)
	{
	  c = *p; *p = '\0';
	  if (strmatch (pat, string, FNMATCH_EXTFLAG | FNMATCH_IGNCASE) == 0)
	    {
	      *p = c;
	      *sp = string;
	      *ep = p;
	      return 1;
	    }
	  *p = c;
	   
	  if (mlen != -1)
	    break;
	}

      return (0);

    case MATCH_END:
      for (p = end - ((mlen == -1) ? len : mlen); p <= end; p++)
	{
	  if (strmatch (pat, p, FNMATCH_EXTFLAG | FNMATCH_IGNCASE) == 0)
	    {
	      *sp = p;
	      *ep = end;
	      return 1;
	    }
	   
	  if (mlen != -1)
	    break;
	}

      return (0);
    }

  return (0);
}

#if defined (HANDLE_MULTIBYTE)

#define WFOLD(c) (match_ignore_case && iswupper (c) ? towlower (c) : (c))

 
static int
match_wpattern (wstring, indices, wstrlen, wpat, mtype, sp, ep)
     wchar_t *wstring;
     char **indices;
     size_t wstrlen;
     wchar_t *wpat;
     int mtype;
     char **sp, **ep;
{
  wchar_t wc, *wp, *nwpat, *wp1;
  size_t len;
  int mlen;
  int n, n1, n2, simple;

  simple = (wpat[0] != L'\\' && wpat[0] != L'*' && wpat[0] != L'?' && wpat[0] != L'[');
#if defined (EXTENDED_GLOB)
  if (extended_glob)
    simple &= (wpat[1] != L'(' || (wpat[0] != L'*' && wpat[0] != L'?' && wpat[0] != L'+' && wpat[0] != L'!' && wpat[0] != L'@'));  
#endif

   
  len = wcslen (wpat);
  if (wpat[0] != L'*' || (wpat[0] == L'*' && wpat[1] == WLPAREN && extended_glob) || wpat[len - 1] != L'*')
    {
      int unescaped_backslash;
      wchar_t *wpp;

      wp = nwpat = (wchar_t *)xmalloc ((len + 3) * sizeof (wchar_t));
      wp1 = wpat;
      if (*wp1 != L'*' || (*wp1 == '*' && wp1[1] == WLPAREN && extended_glob))
	*wp++ = L'*';
      while (*wp1 != L'\0')
	*wp++ = *wp1++;
#if 1
       
      if (wp1[-1] == L'*' && (unescaped_backslash = wp1[-2] == L'\\'))
        {
          wpp = wp1 - 3;
          while (wpp >= wpat && *wpp-- == L'\\')
            unescaped_backslash = 1 - unescaped_backslash;
          if (unescaped_backslash)
            *wp++ = L'*';
        }
      else if (wp1[-1] != L'*')
        *wp++ = L'*';
#else      
      if (wp1[-1] != L'*' || wp1[-2] == L'\\')
        *wp++ = L'*';
#endif
      *wp = '\0';
    }
  else
    nwpat = wpat;
  len = wcsmatch (nwpat, wstring, FNMATCH_EXTFLAG | FNMATCH_IGNCASE);
  if (nwpat != wpat)
    free (nwpat);
  if (len == FNM_NOMATCH)
    return (0);

  mlen = wmatchlen (wpat, wstrlen);
  if (mlen > (int)wstrlen)
    return (0);

 
  switch (mtype)
    {
    case MATCH_ANY:
      for (n = 0; n <= wstrlen; n++)
	{
	  n2 = simple ? (WFOLD(*wpat) == WFOLD(wstring[n])) : match_pattern_wchar (wpat, wstring + n, FNMATCH_IGNCASE);
	  if (n2)
	    {
	      n1 = (mlen == -1) ? wstrlen : n + mlen;
	      if (n1 > wstrlen)
	        break;

	      for ( ; n1 >= n; n1--)
		{
		  wc = wstring[n1]; wstring[n1] = L'\0';
		  if (wcsmatch (wpat, wstring + n, FNMATCH_EXTFLAG | FNMATCH_IGNCASE) == 0)
		    {
		      wstring[n1] = wc;
		      *sp = indices[n];
		      *ep = indices[n1];
		      return 1;
		    }
		  wstring[n1] = wc;
		   
		  if (mlen != -1)
		    break;
		}
	    }
	}

      return (0);

    case MATCH_BEG:
      if (match_pattern_wchar (wpat, wstring, FNMATCH_IGNCASE) == 0)
	return (0);

      for (n = (mlen == -1) ? wstrlen : mlen; n >= 0; n--)
	{
	  wc = wstring[n]; wstring[n] = L'\0';
	  if (wcsmatch (wpat, wstring, FNMATCH_EXTFLAG | FNMATCH_IGNCASE) == 0)
	    {
	      wstring[n] = wc;
	      *sp = indices[0];
	      *ep = indices[n];
	      return 1;
	    }
	  wstring[n] = wc;
	   
	  if (mlen != -1)
	    break;
	}

      return (0);

    case MATCH_END:
      for (n = wstrlen - ((mlen == -1) ? wstrlen : mlen); n <= wstrlen; n++)
	{
	  if (wcsmatch (wpat, wstring + n, FNMATCH_EXTFLAG | FNMATCH_IGNCASE) == 0)
	    {
	      *sp = indices[n];
	      *ep = indices[wstrlen];
	      return 1;
	    }
	   
	  if (mlen != -1)
	    break;
	}

      return (0);
    }

  return (0);
}
#undef WFOLD
#endif  

static int
match_pattern (string, pat, mtype, sp, ep)
     char *string, *pat;
     int mtype;
     char **sp, **ep;
{
#if defined (HANDLE_MULTIBYTE)
  int ret;
  size_t n;
  wchar_t *wstring, *wpat;
  char **indices;
#endif

  if (string == 0 || pat == 0 || *pat == 0)
    return (0);

#if defined (HANDLE_MULTIBYTE)
  if (MB_CUR_MAX > 1)
    {
      if (mbsmbchar (string) == 0 && mbsmbchar (pat) == 0)
        return (match_upattern (string, pat, mtype, sp, ep));

      n = xdupmbstowcs (&wpat, NULL, pat);
      if (n == (size_t)-1)
	return (match_upattern (string, pat, mtype, sp, ep));
      n = xdupmbstowcs (&wstring, &indices, string);
      if (n == (size_t)-1)
	{
	  free (wpat);
	  return (match_upattern (string, pat, mtype, sp, ep));
	}
      ret = match_wpattern (wstring, indices, n, wpat, mtype, sp, ep);

      free (wpat);
      free (wstring);
      free (indices);

      return (ret);
    }
  else
#endif
    return (match_upattern (string, pat, mtype, sp, ep));
}

static int
getpatspec (c, value)
     int c;
     char *value;
{
  if (c == '#')
    return ((*value == '#') ? RP_LONG_LEFT : RP_SHORT_LEFT);
  else	 
    return ((*value == '%') ? RP_LONG_RIGHT : RP_SHORT_RIGHT);
}

 
static char *
getpattern (value, quoted, expandpat)
     char *value;
     int quoted, expandpat;
{
  char *pat, *tword;
  WORD_LIST *l;
#if 0
  int i;
#endif
   
#if 0
  if (expandpat && (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) && *tword)
    {
      i = 0;
      pat = string_extract_double_quoted (tword, &i, SX_STRIPDQ);
      free (tword);
      tword = pat;
    }
#endif

   
  l = *value ? expand_string_for_pat (value,
				      (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) ? Q_PATQUOTE : quoted,
				      (int *)NULL, (int *)NULL)
	     : (WORD_LIST *)0;
  if (l)
    word_list_remove_quoted_nulls (l);
  pat = string_list (l);
  dispose_words (l);
  if (pat)
    {
      tword = quote_string_for_globbing (pat, QGLOB_CVTNULL);
      free (pat);
      pat = tword;
    }
  return (pat);
}

#if 0
 
static char *
variable_remove_pattern (value, pattern, patspec, quoted)
     char *value, *pattern;
     int patspec, quoted;
{
  char *tword;

  tword = remove_pattern (value, pattern, patspec);

  return (tword);
}
#endif

static char *
list_remove_pattern (list, pattern, patspec, itype, quoted)
     WORD_LIST *list;
     char *pattern;
     int patspec, itype, quoted;
{
  WORD_LIST *new, *l;
  WORD_DESC *w;
  char *tword;

  for (new = (WORD_LIST *)NULL, l = list; l; l = l->next)
    {
      tword = remove_pattern (l->word->word, pattern, patspec);
      w = alloc_word_desc ();
      w->word = tword ? tword : savestring ("");
      new = make_word_list (w, new);
    }

  l = REVERSE_LIST (new, WORD_LIST *);
  tword = string_list_pos_params (itype, l, quoted, 0);
  dispose_words (l);

  return (tword);
}

static char *
parameter_list_remove_pattern (itype, pattern, patspec, quoted)
     int itype;
     char *pattern;
     int patspec, quoted;
{
  char *ret;
  WORD_LIST *list;

  list = list_rest_of_args ();
  if (list == 0)
    return ((char *)NULL);
  ret = list_remove_pattern (list, pattern, patspec, itype, quoted);
  dispose_words (list);
  return (ret);
}

#if defined (ARRAY_VARS)
static char *
array_remove_pattern (var, pattern, patspec, starsub, quoted)
     SHELL_VAR *var;
     char *pattern;
     int patspec;
     int starsub;	 
     int quoted;
{
  ARRAY *a;
  HASH_TABLE *h;
  int itype;
  char *ret;
  WORD_LIST *list;
  SHELL_VAR *v;

  v = var;		 

  itype = starsub ? '*' : '@';

  a = (v && array_p (v)) ? array_cell (v) : 0;
  h = (v && assoc_p (v)) ? assoc_cell (v) : 0;
  
  list = a ? array_to_word_list (a) : (h ? assoc_to_word_list (h) : 0);
  if (list == 0)
   return ((char *)NULL);
  ret = list_remove_pattern (list, pattern, patspec, itype, quoted);
  dispose_words (list);

  return ret;
}
#endif  

static char *
parameter_brace_remove_pattern (varname, value, estatep, patstr, rtype, quoted, flags)
     char *varname, *value;
     array_eltstate_t *estatep;
     char *patstr;
     int rtype, quoted, flags;
{
  int vtype, patspec, starsub;
  char *temp1, *val, *pattern, *oname;
  SHELL_VAR *v;

  if (value == 0)
    return ((char *)NULL);

  oname = this_command_name;
  this_command_name = varname;

  vtype = get_var_and_type (varname, value, estatep, quoted, flags, &v, &val);
  if (vtype == -1)
    {
      this_command_name = oname;
      return ((char *)NULL);
    }

  starsub = vtype & VT_STARSUB;
  vtype &= ~VT_STARSUB;

  patspec = getpatspec (rtype, patstr);
  if (patspec == RP_LONG_LEFT || patspec == RP_LONG_RIGHT)
    patstr++;

   
  temp1 = savestring (patstr);
  pattern = getpattern (temp1, quoted, 1);
  free (temp1);

  temp1 = (char *)NULL;		 
  switch (vtype)
    {
    case VT_VARIABLE:
    case VT_ARRAYMEMBER:
      temp1 = remove_pattern (val, pattern, patspec);
      if (vtype == VT_VARIABLE)
	FREE (val);
      if (temp1)
	{
	  val = (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES))
			? quote_string (temp1)
			: quote_escapes (temp1);
	  free (temp1);
	  temp1 = val;
	}
      break;
#if defined (ARRAY_VARS)
    case VT_ARRAYVAR:
      temp1 = array_remove_pattern (v, pattern, patspec, starsub, quoted);
      if (temp1 && ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) == 0))
	{
	  val = quote_escapes (temp1);
	  free (temp1);
	  temp1 = val;
	}
      break;
#endif
    case VT_POSPARMS:
      temp1 = parameter_list_remove_pattern (varname[0], pattern, patspec, quoted);
      if (temp1 && quoted == 0 && ifs_is_null)
	{
	   
	}
      else if (temp1 && ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) == 0))
	{
	  val = quote_escapes (temp1);
	  free (temp1);
	  temp1 = val;
	}
      break;
    }

  this_command_name = oname;

  FREE (pattern);
  return temp1;
}    

#if defined (PROCESS_SUBSTITUTION)

static void reap_some_procsubs PARAMS((int));

 
 
 
 
 

#if !defined (HAVE_DEV_FD)
 
#define FIFO_INCR 20

 
struct temp_fifo {
  char *file;
  pid_t proc;
};

static struct temp_fifo *fifo_list = (struct temp_fifo *)NULL;
static int nfifo;
static int fifo_list_size;

void
clear_fifo_list ()
{
  int i;

  for (i = 0; i < fifo_list_size; i++)
    {
      if (fifo_list[i].file)
	free (fifo_list[i].file);
      fifo_list[i].file = NULL;
      fifo_list[i].proc = 0;
    }
  nfifo = 0;
}

void *
copy_fifo_list (sizep)
     int *sizep;
{
  if (sizep)
    *sizep = 0;
  return (void *)NULL;
}

static void
add_fifo_list (pathname)
     char *pathname;
{
  int osize, i;

  if (nfifo >= fifo_list_size - 1)
    {
      osize = fifo_list_size;
      fifo_list_size += FIFO_INCR;
      fifo_list = (struct temp_fifo *)xrealloc (fifo_list,
				fifo_list_size * sizeof (struct temp_fifo));
      for (i = osize; i < fifo_list_size; i++)
	{
	  fifo_list[i].file = (char *)NULL;
	  fifo_list[i].proc = 0;	 
	}
    }

  fifo_list[nfifo].file = savestring (pathname);
  nfifo++;
}

void
unlink_fifo (i)
     int i;
{
  if ((fifo_list[i].proc == (pid_t)-1) || (fifo_list[i].proc > 0 && (kill(fifo_list[i].proc, 0) == -1)))
    {
      unlink (fifo_list[i].file);
      free (fifo_list[i].file);
      fifo_list[i].file = (char *)NULL;
      fifo_list[i].proc = 0;
    }
}

void
unlink_fifo_list ()
{
  int saved, i, j;

  if (nfifo == 0)
    return;

  for (i = saved = 0; i < nfifo; i++)
    {
      if ((fifo_list[i].proc == (pid_t)-1) || (fifo_list[i].proc > 0 && (kill(fifo_list[i].proc, 0) == -1)))
	{
	  unlink (fifo_list[i].file);
	  free (fifo_list[i].file);
	  fifo_list[i].file = (char *)NULL;
	  fifo_list[i].proc = 0;
	}
      else
	saved++;
    }

   
  if (saved)
    {
      for (i = j = 0; i < nfifo; i++)
	if (fifo_list[i].file)
	  {
	    if (i != j)
	      {
		fifo_list[j].file = fifo_list[i].file;
		fifo_list[j].proc = fifo_list[i].proc;
		fifo_list[i].file = (char *)NULL;
		fifo_list[i].proc = 0;
	      }
	    j++;
	  }
      nfifo = j;
    }
  else
    nfifo = 0;
}

void
unlink_all_fifos ()
{
  int i, fd;

  if (nfifo == 0)
    return;

  for (i = 0; i < nfifo; i++)
    {
      fifo_list[i].proc = (pid_t)-1;
#if defined (O_NONBLOCK)
      fd = open (fifo_list[i].file, O_RDWR|O_NONBLOCK);
#else
      fd = -1;
#endif
      unlink_fifo (i);
      if (fd >= 0)
	close (fd);
    }

  nfifo = 0;
}

 
void
close_new_fifos (list, lsize)
     void *list;
     int lsize;
{
  int i;
  char *plist;

  if (list == 0)
    {
      unlink_fifo_list ();
      return;
    }

  for (plist = (char *)list, i = 0; i < lsize; i++)
    if (plist[i] == 0 && i < fifo_list_size && fifo_list[i].proc != -1)
      unlink_fifo (i);

  for (i = lsize; i < fifo_list_size; i++)
    unlink_fifo (i);  
}

int
find_procsub_child (pid)
     pid_t pid;
{
  int i;

  for (i = 0; i < nfifo; i++)
    if (fifo_list[i].proc == pid)
      return i;
  return -1;
}

void
set_procsub_status (ind, pid, status)
     int ind;
     pid_t pid;
     int status;
{
  if (ind >= 0 && ind < nfifo)
    fifo_list[ind].proc = (pid_t)-1;		 
}

 
static void
reap_some_procsubs (max)
     int max;
{
  int i;

  for (i = 0; i < max; i++)
    if (fifo_list[i].proc == (pid_t)-1)	 
      unlink_fifo (i);
}

void
reap_procsubs ()
{
  reap_some_procsubs (nfifo);
}

#if 0
 
void
wait_procsubs ()
{
  int i, r;

  for (i = 0; i < nfifo; i++)
    {
      if (fifo_list[i].proc != (pid_t)-1 && fifo_list[i].proc > 0)
	{
	  r = wait_for (fifo_list[i].proc, 0);
	  save_proc_status (fifo_list[i].proc, r);
	  fifo_list[i].proc = (pid_t)-1;
	}
    }
}
#endif

int
fifos_pending ()
{
  return nfifo;
}

int
num_fifos ()
{
  return nfifo;
}

static char *
make_named_pipe ()
{
  char *tname;

  tname = sh_mktmpname ("sh-np", MT_USERANDOM|MT_USETMPDIR);
  if (mkfifo (tname, 0600) < 0)
    {
      free (tname);
      return ((char *)NULL);
    }

  add_fifo_list (tname);
  return (tname);
}

#else  

 
 

static pid_t *dev_fd_list = (pid_t *)NULL;
static int nfds;
static int totfds;	 

void
clear_fifo (i)
     int i;
{
  if (dev_fd_list[i])
    {
      dev_fd_list[i] = 0;
      nfds--;
    }
}

void
clear_fifo_list ()
{
  register int i;

  if (nfds == 0)
    return;

  for (i = 0; nfds && i < totfds; i++)
    clear_fifo (i);

  nfds = 0;
}

void *
copy_fifo_list (sizep)
     int *sizep;
{
  void *ret;

  if (nfds == 0 || totfds == 0)
    {
      if (sizep)
	*sizep = 0;
      return (void *)NULL;
    }

  if (sizep)
    *sizep = totfds;
  ret = xmalloc (totfds * sizeof (pid_t));
  return (memcpy (ret, dev_fd_list, totfds * sizeof (pid_t)));
}

static void
add_fifo_list (fd)
     int fd;
{
  if (dev_fd_list == 0 || fd >= totfds)
    {
      int ofds;

      ofds = totfds;
      totfds = getdtablesize ();
      if (totfds < 0 || totfds > 256)
	totfds = 256;
      if (fd >= totfds)
	totfds = fd + 2;

      dev_fd_list = (pid_t *)xrealloc (dev_fd_list, totfds * sizeof (dev_fd_list[0]));
       
      memset (dev_fd_list + ofds, '\0', (totfds - ofds) * sizeof (pid_t));
    }

  dev_fd_list[fd] = 1;		 
  nfds++;
}

int
fifos_pending ()
{
  return 0;	 
}

int
num_fifos ()
{
  return nfds;
}

void
unlink_fifo (fd)
     int fd;
{
  if (dev_fd_list[fd])
    {
      close (fd);
      dev_fd_list[fd] = 0;
      nfds--;
    }
}

void
unlink_fifo_list ()
{
  register int i;

  if (nfds == 0)
    return;

  for (i = totfds-1; nfds && i >= 0; i--)
    unlink_fifo (i);

  nfds = 0;
}

void
unlink_all_fifos ()
{
  unlink_fifo_list ();
}

 
void
close_new_fifos (list, lsize)
     void *list;
     int lsize;
{
  int i;
  pid_t *plist;

  if (list == 0)
    {
      unlink_fifo_list ();
      return;
    }

  for (plist = (pid_t *)list, i = 0; i < lsize; i++)
    if (plist[i] == 0 && i < totfds && dev_fd_list[i])
      unlink_fifo (i);

  for (i = lsize; i < totfds; i++)
    unlink_fifo (i);  
}

int
find_procsub_child (pid)
     pid_t pid;
{
  int i;

  if (nfds == 0)
    return -1;

  for (i = 0; i < totfds; i++)
    if (dev_fd_list[i] == pid)
      return i;

  return -1;
}

void
set_procsub_status (ind, pid, status)
     int ind;
     pid_t pid;
     int status;
{
  if (ind >= 0 && ind < totfds)
    dev_fd_list[ind] = (pid_t)-1;		 
}

 
static void
reap_some_procsubs (max)
     int max;
{
  int i;

  for (i = 0; nfds > 0 && i < max; i++)
    if (dev_fd_list[i] == (pid_t)-1)
      unlink_fifo (i);
}

void
reap_procsubs ()
{
  reap_some_procsubs (totfds);
}

#if 0
 
void
wait_procsubs ()
{
  int i, r;

  for (i = 0; nfds > 0 && i < totfds; i++)
    {
      if (dev_fd_list[i] != (pid_t)-1 && dev_fd_list[i] > 0)
	{
	  r = wait_for (dev_fd_list[i], 0);
	  save_proc_status (dev_fd_list[i], r);
	  dev_fd_list[i] = (pid_t)-1;
	}
    }
}
#endif

#if defined (NOTDEF)
print_dev_fd_list ()
{
  register int i;

  fprintf (stderr, "pid %ld: dev_fd_list:", (long)getpid ());
  fflush (stderr);

  for (i = 0; i < totfds; i++)
    {
      if (dev_fd_list[i])
	fprintf (stderr, " %d", i);
    }
  fprintf (stderr, "\n");
}
#endif  

static char *
make_dev_fd_filename (fd)
     int fd;
{
  char *ret, intbuf[INT_STRLEN_BOUND (int) + 1], *p;

  ret = (char *)xmalloc (sizeof (DEV_FD_PREFIX) + 8);

  strcpy (ret, DEV_FD_PREFIX);
  p = inttostr (fd, intbuf, sizeof (intbuf));
  strcpy (ret + sizeof (DEV_FD_PREFIX) - 1, p);

  add_fifo_list (fd);
  return (ret);
}

#endif  

 

static char *
process_substitute (string, open_for_read_in_child)
     char *string;
     int open_for_read_in_child;
{
  char *pathname;
  int fd, result, rc, function_value;
  pid_t old_pid, pid;
#if defined (HAVE_DEV_FD)
  int parent_pipe_fd, child_pipe_fd;
  int fildes[2];
#endif  
#if defined (JOB_CONTROL)
  pid_t old_pipeline_pgrp;
#endif

  if (!string || !*string || wordexp_only)
    return ((char *)NULL);

#if !defined (HAVE_DEV_FD)
  pathname = make_named_pipe ();
#else  
  if (pipe (fildes) < 0)
    {
      sys_error ("%s", _("cannot make pipe for process substitution"));
      return ((char *)NULL);
    }
   
  parent_pipe_fd = fildes[open_for_read_in_child];
  child_pipe_fd = fildes[1 - open_for_read_in_child];
   
  parent_pipe_fd = move_to_high_fd (parent_pipe_fd, 1, 64);

  pathname = make_dev_fd_filename (parent_pipe_fd);
#endif  

  if (pathname == 0)
    {
      sys_error ("%s", _("cannot make pipe for process substitution"));
      return ((char *)NULL);
    }

  old_pid = last_made_pid;

#if defined (JOB_CONTROL)
  old_pipeline_pgrp = pipeline_pgrp;
  if (pipeline_pgrp == 0 || (subshell_environment & (SUBSHELL_PIPE|SUBSHELL_FORK|SUBSHELL_ASYNC)) == 0)
    pipeline_pgrp = shell_pgrp;
  save_pipeline (1);
#endif  

  pid = make_child ((char *)NULL, FORK_ASYNC);
  if (pid == 0)
    {
#if 0
      int old_interactive;

      old_interactive = interactive;
#endif
       
      interactive = 0;

      reset_terminating_signals ();	 
      free_pushed_string_input ();
       
      restore_original_signals ();	 
      subshell_environment &= ~SUBSHELL_IGNTRAP;
      QUIT;	 
      setup_async_signals ();
#if 0
      if (open_for_read_in_child == 0 && old_interactive && (bash_input.type == st_stdin || bash_input.type == st_stream))
	async_redirect_stdin ();
#endif

      subshell_environment |= SUBSHELL_COMSUB|SUBSHELL_PROCSUB|SUBSHELL_ASYNC;

       
      change_flag ('v', FLAG_OFF);

       
      if (expanding_redir)
        flush_temporary_env ();
    }

#if defined (JOB_CONTROL)
  set_sigchld_handler ();
  stop_making_children ();
   
  pipeline_pgrp = old_pipeline_pgrp;
#else
  stop_making_children ();
#endif  

  if (pid < 0)
    {
      sys_error ("%s", _("cannot make child for process substitution"));
      free (pathname);
#if defined (HAVE_DEV_FD)
      close (parent_pipe_fd);
      close (child_pipe_fd);
#endif  
#if defined (JOB_CONTROL)
      restore_pipeline (1);
#endif
      return ((char *)NULL);
    }

  if (pid > 0)
    {
#if defined (JOB_CONTROL)
      last_procsub_child = restore_pipeline (0);
       
      last_procsub_child->next = 0;
      procsub_add (last_procsub_child);
#endif

#if defined (HAVE_DEV_FD)
      dev_fd_list[parent_pipe_fd] = pid;
#else
      fifo_list[nfifo-1].proc = pid;
#endif

      last_made_pid = old_pid;

#if defined (JOB_CONTROL) && defined (PGRP_PIPE)
      close_pgrp_pipe ();
#endif  

#if defined (HAVE_DEV_FD)
      close (child_pipe_fd);
#endif  

      return (pathname);
    }

  set_sigint_handler ();

#if defined (JOB_CONTROL)
   
  set_job_control (0);

   
  procsub_clear ();

   

  if (pipeline_pgrp != shell_pgrp)
    pipeline_pgrp = getpid ();
#endif  

#if !defined (HAVE_DEV_FD)
   
  fd = open (pathname, open_for_read_in_child ? O_RDONLY : O_WRONLY);
  if (fd < 0)
    {
       
      if (open_for_read_in_child)
	sys_error (_("cannot open named pipe %s for reading"), pathname);
      else
	sys_error (_("cannot open named pipe %s for writing"), pathname);

      exit (127);
    }
  if (open_for_read_in_child)
    {
      if (sh_unset_nodelay_mode (fd) < 0)
	{
	  sys_error (_("cannot reset nodelay mode for fd %d"), fd);
	  exit (127);
	}
    }
#else  
  fd = child_pipe_fd;
#endif  

   
  if (open_for_read_in_child == 0)
    fpurge (stdout);

  if (dup2 (fd, open_for_read_in_child ? 0 : 1) < 0)
    {
      sys_error (_("cannot duplicate named pipe %s as fd %d"), pathname,
	open_for_read_in_child ? 0 : 1);
      exit (127);
    }

  if (fd != (open_for_read_in_child ? 0 : 1))
    close (fd);

   
  if (current_fds_to_close)
    {
      close_fd_bitmap (current_fds_to_close);
      current_fds_to_close = (struct fd_bitmap *)NULL;
    }

#if defined (HAVE_DEV_FD)
   
  close (parent_pipe_fd);
  dev_fd_list[parent_pipe_fd] = 0;
#endif  

   
  expanding_redir = 0;

  remove_quoted_escapes (string);

  startup_state = 2;	 
  parse_and_execute_level = 0;

   
  result = setjmp_nosigs (top_level);

   
  if (result == 0 && return_catch_flag)
    function_value = setjmp_nosigs (return_catch);
  else
    function_value = 0;

  if (result == ERREXIT)
    rc = last_command_exit_value;
  else if (result == EXITPROG || result == EXITBLTIN)
    rc = last_command_exit_value;
  else if (result)
    rc = EXECUTION_FAILURE;
  else if (function_value)
    rc = return_catch_value;
  else
    {
      subshell_level++;
      rc = parse_and_execute (string, "process substitution", (SEVAL_NONINT|SEVAL_NOHIST));
       
    }

#if !defined (HAVE_DEV_FD)
   
  close (open_for_read_in_child ? 0 : 1);
#endif  

  last_command_exit_value = rc;
  rc = run_exit_trap ();
  exit (rc);
   
}
#endif  

 
 
 
 
 

#define COMSUB_PIPEBUF	4096

static char *
optimize_cat_file (r, quoted, flags, flagp)
     REDIRECT *r;
     int quoted, flags, *flagp;
{
  char *ret;
  int fd;

  fd = open_redir_file (r, (char **)0);
  if (fd < 0)
    return &expand_param_error;

  ret = read_comsub (fd, quoted, flags, flagp);
  close (fd);

  return ret;
}

static char *
read_comsub (fd, quoted, flags, rflag)
     int fd, quoted, flags;
     int *rflag;
{
  char *istring, buf[COMSUB_PIPEBUF], *bufp;
  int c, tflag, skip_ctlesc, skip_ctlnul;
  int mb_cur_max;
  size_t istring_index;
  size_t istring_size;
  ssize_t bufn;
  int nullbyte;
#if defined (HANDLE_MULTIBYTE)
  mbstate_t ps;
  wchar_t wc;
  size_t mblen;
  int i;
#endif

  istring = (char *)NULL;
  istring_index = istring_size = bufn = tflag = 0;

  skip_ctlesc = ifs_cmap[CTLESC];
  skip_ctlnul = ifs_cmap[CTLNUL];

  mb_cur_max = MB_CUR_MAX;
  nullbyte = 0;

   
  while (1)
    {
      if (fd < 0)
	break;
      if (--bufn <= 0)
	{
	  bufn = zread (fd, buf, sizeof (buf));
	  if (bufn <= 0) 
	    break;
	  bufp = buf;
	}
      c = *bufp++;

      if (c == 0)
	{
#if 1
	  if (nullbyte == 0)
	    {
	      internal_warning ("%s", _("command substitution: ignored null byte in input"));
	      nullbyte = 1;
	    }
#endif
	  continue;
	}

       
      RESIZE_MALLOCED_BUFFER (istring, istring_index, mb_cur_max+1, istring_size, 512);

       
      if ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES))  )
	istring[istring_index++] = CTLESC;
      else if ((flags & PF_ASSIGNRHS) && skip_ctlesc && c == CTLESC)
	istring[istring_index++] = CTLESC;
       
      else if (skip_ctlesc == 0 && c == CTLESC)
	istring[istring_index++] = CTLESC;
      else if ((skip_ctlnul == 0 && c == CTLNUL) || (c == ' ' && (ifs_value && *ifs_value == 0)))
	istring[istring_index++] = CTLESC;

#if defined (HANDLE_MULTIBYTE)
      if ((locale_utf8locale && (c & 0x80)) ||
	  (locale_utf8locale == 0 && mb_cur_max > 1 && (unsigned char)c > 127))
	{
	   
	   
	  memset (&ps, '\0', sizeof (mbstate_t));
	  mblen = mbrtowc (&wc, bufp-1, bufn, &ps);
	  if (MB_INVALIDCH (mblen) || mblen == 0 || mblen == 1)
	    istring[istring_index++] = c;
	  else
	    {
	      istring[istring_index++] = c;
	      for (i = 0; i < mblen-1; i++)
		istring[istring_index++] = *bufp++;
	      bufn -= mblen - 1;
	    }
	  continue;
	}
#endif

      istring[istring_index++] = c;
    }

  if (istring)
    istring[istring_index] = '\0';

   
  if (istring_index == 0)
    {
      FREE (istring);
      if (rflag)
	*rflag = tflag;
      return (char *)NULL;
    }

   
  if (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES))
    {
      while (istring_index > 0)
	{
	  if (istring[istring_index - 1] == '\n')
	    {
	      --istring_index;

	       
	      if (istring[istring_index - 1] == CTLESC)
		--istring_index;
	    }
	  else
	    break;
	}
      istring[istring_index] = '\0';
    }
  else
    strip_trailing (istring, istring_index - 1, 1);

  if (rflag)
    *rflag = tflag;
  return istring;
}

 
WORD_DESC *
command_substitute (string, quoted, flags)
     char *string;
     int quoted;
     int flags;
{
  pid_t pid, old_pid, old_pipeline_pgrp, old_async_pid;
  char *istring, *s;
  int result, fildes[2], function_value, pflags, rc, tflag, fork_flags;
  WORD_DESC *ret;
  sigset_t set, oset;

  istring = (char *)NULL;

   
  for (s = string; s && *s && (shellblank (*s) || *s == '\n'); s++)
    ;
  if (s == 0 || *s == 0)
    return ((WORD_DESC *)NULL);

  if (*s == '<' && (s[1] != '<' && s[1] != '>' && s[1] != '&'))
    {
      COMMAND *cmd;

      cmd = parse_string_to_command (string, 0);	 
      if (cmd && can_optimize_cat_file (cmd))
	{
	  tflag = 0;
	  istring = optimize_cat_file (cmd->value.Simple->redirects, quoted, flags, &tflag);
	  if (istring == &expand_param_error)
	    {
	      last_command_exit_value = EXECUTION_FAILURE;
	      istring = 0;
	    }
	  else
	    last_command_exit_value = EXECUTION_SUCCESS;	 
	  last_command_subst_pid = dollar_dollar_pid;

	  dispose_command (cmd);	  
	  ret = alloc_word_desc ();
	  ret->word = istring;
	  ret->flags = tflag;

	  return ret;
	}
      dispose_command (cmd);
    }

  if (wordexp_only && read_but_dont_execute)
    {
      last_command_exit_value = EX_WEXPCOMSUB;
      jump_to_top_level (EXITPROG);
    }

   
  if (subst_assign_varlist == 0 || garglist == 0)
    maybe_make_export_env ();	 

   
  pflags = (interactive && sourcelevel == 0) ? SEVAL_RESETLINE : 0;

  old_pid = last_made_pid;

   
  if (pipe (fildes) < 0)
    {
      sys_error ("%s", _("cannot make pipe for command substitution"));
      goto error_exit;
    }

#if defined (JOB_CONTROL)
  old_pipeline_pgrp = pipeline_pgrp;
   
  if ((subshell_environment & (SUBSHELL_FORK|SUBSHELL_PIPE)) == 0)
    pipeline_pgrp = shell_pgrp;
  cleanup_the_pipeline ();
#endif  

  old_async_pid = last_asynchronous_pid;
  fork_flags = (subshell_environment&SUBSHELL_ASYNC) ? FORK_ASYNC : 0;
  pid = make_child ((char *)NULL, fork_flags|FORK_NOTERM);
  last_asynchronous_pid = old_async_pid;

  if (pid == 0)
    {
       
      reset_signal_handlers ();
      if (ISINTERRUPT)
	{
	  kill (getpid (), SIGINT);
	  CLRINTERRUPT;		 
	}	
      QUIT;	 
      subshell_environment |= SUBSHELL_RESETTRAP;
      subshell_environment &= ~SUBSHELL_IGNTRAP;
    }

#if defined (JOB_CONTROL)
   
  set_sigchld_handler ();
  stop_making_children ();
  if (pid != 0)
    pipeline_pgrp = old_pipeline_pgrp;
#else
  stop_making_children ();
#endif  

  if (pid < 0)
    {
      sys_error (_("cannot make child for command substitution"));
    error_exit:

      last_made_pid = old_pid;

      FREE (istring);
      close (fildes[0]);
      close (fildes[1]);
      return ((WORD_DESC *)NULL);
    }

  if (pid == 0)
    {
       
      interactive = 0;

#if defined (JOB_CONTROL)
       
      if (pipeline_pgrp > 0 && pipeline_pgrp != shell_pgrp)
	shell_pgrp = pipeline_pgrp;
#endif

      set_sigint_handler ();	 

      free_pushed_string_input ();

       
      fpurge (stdout);

      if (dup2 (fildes[1], 1) < 0)
	{
	  sys_error ("%s", _("command_substitute: cannot duplicate pipe as fd 1"));
	  exit (EXECUTION_FAILURE);
	}

       
      if ((fildes[1] != fileno (stdin)) &&
	  (fildes[1] != fileno (stdout)) &&
	  (fildes[1] != fileno (stderr)))
	close (fildes[1]);

      if ((fildes[0] != fileno (stdin)) &&
	  (fildes[0] != fileno (stdout)) &&
	  (fildes[0] != fileno (stderr)))
	close (fildes[0]);

#ifdef __CYGWIN__
       
      freopen (NULL, "w", stdout);
      sh_setlinebuf (stdout);
#endif  

       
      subshell_environment |= SUBSHELL_COMSUB;

       
      change_flag ('v', FLAG_OFF);

       
      if (inherit_errexit == 0)
        {
          builtin_ignoring_errexit = 0;
	  change_flag ('e', FLAG_OFF);
        }
      set_shellopts ();

       
      if (expanding_redir)
	{
	  flush_temporary_env ();
	  expanding_redir = 0;
	}

      remove_quoted_escapes (string);

       
      if (expand_aliases && (flags & PF_BACKQUOTE) == 0)
        expand_aliases = posixly_correct == 0;

      startup_state = 2;	 
      parse_and_execute_level = 0;

       
      result = setjmp_nosigs (top_level);

       
      if (result == 0 && return_catch_flag)
	function_value = setjmp_nosigs (return_catch);
      else
	function_value = 0;

      if (result == ERREXIT)
	rc = last_command_exit_value;
      else if (result == EXITPROG || result == EXITBLTIN)
	rc = last_command_exit_value;
      else if (result)
	rc = EXECUTION_FAILURE;
      else if (function_value)
	rc = return_catch_value;
      else
	{
	  subshell_level++;
	  rc = parse_and_execute (string, "command substitution", pflags|SEVAL_NOHIST);
	   
	}

      last_command_exit_value = rc;
      rc = run_exit_trap ();
#if defined (PROCESS_SUBSTITUTION)
      unlink_fifo_list ();
#endif
      exit (rc);
    }
  else
    {
      int dummyfd;

#if defined (JOB_CONTROL) && defined (PGRP_PIPE)
      close_pgrp_pipe ();
#endif  

      close (fildes[1]);

      begin_unwind_frame ("read-comsub");
      dummyfd = fildes[0];
      add_unwind_protect (close, dummyfd);

       
      BLOCK_SIGNAL (SIGINT, set, oset);
      tflag = 0;
      istring = read_comsub (fildes[0], quoted, flags, &tflag);

      close (fildes[0]);
      discard_unwind_frame ("read-comsub");
      UNBLOCK_SIGNAL (oset);

      current_command_subst_pid = pid;
      last_command_exit_value = wait_for (pid, JWAIT_NOTERM);
      last_command_subst_pid = pid;
      last_made_pid = old_pid;

#if defined (JOB_CONTROL)
       
      if (last_command_exit_value == (128 + SIGINT) && last_command_exit_signal == SIGINT)
	kill (getpid (), SIGINT);
#endif  

      ret = alloc_word_desc ();
      ret->word = istring;
      ret->flags = tflag;

      return ret;
    }
}

 

#if defined (ARRAY_VARS)

static arrayind_t
array_length_reference (s)
     char *s;
{
  int len;
  arrayind_t ind;
  char *akey;
  char *t, c;
  ARRAY *array;
  HASH_TABLE *h;
  SHELL_VAR *var;

  var = array_variable_part (s, 0, &t, &len);

   
  if ((var == 0 || invisible_p (var) || (assoc_p (var) == 0 && array_p (var) == 0)) && unbound_vars_is_error)
    {
      c = *--t;
      *t = '\0';
      set_exit_status (EXECUTION_FAILURE);
      err_unboundvar (s);
      *t = c;
      return (-1);
    }
  else if (var == 0 || invisible_p (var))
    return 0;

   

  array = array_p (var) ? array_cell (var) : (ARRAY *)NULL;
  h = assoc_p (var) ? assoc_cell (var) : (HASH_TABLE *)NULL;

  if (ALL_ELEMENT_SUB (t[0]) && t[1] == RBRACK)
    {
      if (assoc_p (var))
	return (h ? assoc_num_elements (h) : 0);
      else if (array_p (var))
	return (array ? array_num_elements (array) : 0);
      else
	return (var_isset (var) ? 1 : 0);
    }

  if (assoc_p (var))
    {
      t[len - 1] = '\0';
      akey = expand_subscript_string (t, 0);	 
      t[len - 1] = RBRACK;
      if (akey == 0 || *akey == 0)
	{
	  err_badarraysub (t);
	  FREE (akey);
	  return (-1);
	}
      t = assoc_reference (assoc_cell (var), akey);
      free (akey);
    }
  else
    {
      ind = array_expand_index (var, t, len, 0);
       
      if (var && array_p (var) && ind < 0)
	ind = array_max_index (array_cell (var)) + 1 + ind;
      if (ind < 0)
	{
	  err_badarraysub (t);
	  return (-1);
	}
      if (array_p (var))
	t = array_reference (array, ind);
      else
	t = (ind == 0) ? value_cell (var) : (char *)NULL;
    }

  len = MB_STRLEN (t);
  return (len);
}
#endif  

static int
valid_brace_expansion_word (name, var_is_special)
     char *name;
     int var_is_special;
{
  if (DIGIT (*name) && all_digits (name))
    return 1;
  else if (var_is_special)
    return 1;
#if defined (ARRAY_VARS)
  else if (valid_array_reference (name, 0))
    return 1;
#endif  
  else if (legal_identifier (name))
    return 1;
  else
    return 0;
}

static int
chk_atstar (name, quoted, pflags, quoted_dollar_atp, contains_dollar_at)
     char *name;
     int quoted, pflags;
     int *quoted_dollar_atp, *contains_dollar_at;
{
  char *temp1;

  if (name == 0)
    {
      if (quoted_dollar_atp)
	*quoted_dollar_atp = 0;
      if (contains_dollar_at)
	*contains_dollar_at = 0;
      return 0;
    }

   
  if (name[0] == '@' && name[1] == 0)
    {
      if ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) && quoted_dollar_atp)
	*quoted_dollar_atp = 1;
      if (contains_dollar_at)
	*contains_dollar_at = 1;
      return 1;
    }
  else if (name[0] == '*' && name[1] == '\0' && quoted == 0)
    {
       
      if (contains_dollar_at && expand_no_split_dollar_star == 0)
	*contains_dollar_at = 1;
      return 1;
    }

   
#if defined (ARRAY_VARS)
  else if (valid_array_reference (name, 0))
    {
      temp1 = mbschr (name, LBRACK);
      if (temp1 && temp1[1] == '@' && temp1[2] == RBRACK)
	{
	  if ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) && quoted_dollar_atp)
	    *quoted_dollar_atp = 1;
	  if (contains_dollar_at)
	    *contains_dollar_at = 1;
	  return 1;
	}
       
      if (temp1 && temp1[1] == '*' && temp1[2] == RBRACK && quoted == 0)
	{
	  if (contains_dollar_at)
	    *contains_dollar_at = 1;
	  return 1;
	}
    }
#endif
  return 0;
}

 
static WORD_DESC *
parameter_brace_expand_word (name, var_is_special, quoted, pflags, estatep)
     char *name;
     int var_is_special, quoted, pflags;
     array_eltstate_t *estatep;
{
  WORD_DESC *ret;
  char *temp, *tt;
  intmax_t arg_index;
  SHELL_VAR *var;
  int rflags;
  array_eltstate_t es;

  ret = 0;
  temp = 0;
  rflags = 0;

#if defined (ARRAY_VARS)
  if (estatep)
    es = *estatep;	 
  else
    {
      init_eltstate (&es);
      es.ind = INTMAX_MIN;
    }
#endif

     
  if (legal_number (name, &arg_index))
    {
      tt = get_dollar_var_value (arg_index);
      if (tt)
 	temp = (*tt && (quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT)))
 		  ? quote_string (tt)
 		  : quote_escapes (tt);
      else
        temp = (char *)NULL;
      FREE (tt);
    }
  else if (var_is_special)       
    {
      int sindex;
      tt = (char *)xmalloc (2 + strlen (name));
      tt[sindex = 0] = '$';
      strcpy (tt + 1, name);

      ret = param_expand (tt, &sindex, quoted, (int *)NULL, (int *)NULL,
			  (int *)NULL, (int *)NULL, pflags);

       
      if ((quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT)) && STR_DOLLAR_AT_STAR (name) &&
	  ret && ret->word && QUOTED_NULL (ret->word))
	ret->flags |= W_HASQUOTEDNULL;

      free (tt);
    }
#if defined (ARRAY_VARS)
  else if (valid_array_reference (name, 0))
    {
expand_arrayref:
      var = array_variable_part (name, 0, &tt, (int *)0);
       
      if (pflags & PF_ASSIGNRHS)
	{
	  if (ALL_ELEMENT_SUB (tt[0]) && tt[1] == RBRACK)
	    {
	       
	      if (var && (array_p (var) || assoc_p (var)))
		temp = array_value (name, quoted|Q_DOUBLE_QUOTES, AV_ASSIGNRHS, &es);
	      else		
		temp = array_value (name, quoted, 0, &es);
	    }
	  else
	    temp = array_value (name, quoted, 0, &es);
	}
       
      else if (pflags & PF_NOSPLIT2)
	{
	   
#if defined (HANDLE_MULTIBYTE)
          if (tt[0] == '@' && tt[1] == RBRACK && var && quoted == 0 && ifs_is_set && ifs_is_null == 0 && ifs_firstc[0] != ' ')
#else
	  if (tt[0] == '@' && tt[1] == RBRACK && var && quoted == 0 && ifs_is_set && ifs_is_null == 0 && ifs_firstc != ' ')
#endif
	    temp = array_value (name, Q_DOUBLE_QUOTES, AV_ASSIGNRHS, &es);
	  else if (tt[0] == '@' && tt[1] == RBRACK)
	    temp = array_value (name, quoted, 0, &es);
	  else if (tt[0] == '*' && tt[1] == RBRACK && expand_no_split_dollar_star && ifs_is_null)
	    temp = array_value (name, Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT, 0, &es);
	  else if (tt[0] == '*' && tt[1] == RBRACK)
	    temp = array_value (name, quoted, 0, &es);
	  else
	    temp = array_value (name, quoted, 0, &es);
	}	  	  
      else if (tt[0] == '*' && tt[1] == RBRACK && expand_no_split_dollar_star && ifs_is_null)
	temp = array_value (name, Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT, 0, &es);
      else
	temp = array_value (name, quoted, 0, &es);
      if (es.subtype == 0 && temp)
	{
	  temp = (*temp && (quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT)))
		    ? quote_string (temp)
		    : quote_escapes (temp);
	  rflags |= W_ARRAYIND;
	}
       
      else if (es.subtype == 1 && temp && QUOTED_NULL (temp) && (quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT)))
	rflags |= W_HASQUOTEDNULL;
      else if (es.subtype == 2 && temp && QUOTED_NULL (temp) && (quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT)))
	rflags |= W_HASQUOTEDNULL;

      if (estatep)
	*estatep = es;	 
      else
	flush_eltstate (&es);
    }
#endif
  else if (var = find_variable (name))
    {
      if (var_isset (var) && invisible_p (var) == 0)
	{
#if defined (ARRAY_VARS)
	   
	  tt = (char *)NULL;
	  if ((pflags & PF_ALLINDS) && assoc_p (var))
	    tt = temp = assoc_empty (assoc_cell (var)) ? (char *)NULL : assoc_to_string (assoc_cell (var), " ", quoted);
	  else if ((pflags & PF_ALLINDS) && array_p (var))
	    tt = temp = array_empty (array_cell (var)) ? (char *)NULL : array_to_string (array_cell (var), " ", quoted);
	  else if (assoc_p (var))
	    temp = assoc_reference (assoc_cell (var), "0");
	  else if (array_p (var))
	    temp = array_reference (array_cell (var), 0);
	  else
	    temp = value_cell (var);
#else
	  temp = value_cell (var);
#endif

	  if (temp)
	    temp = (*temp && (quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT)))
		      ? quote_string (temp)
		      : ((pflags & PF_ASSIGNRHS) ? quote_rhs (temp)
						 : quote_escapes (temp));
	  FREE (tt);
	}
      else
	temp = (char *)NULL;
    }
  else if (var = find_variable_last_nameref (name, 0))
    {
      temp = nameref_cell (var);
#if defined (ARRAY_VARS)
       
      if (temp && *temp && valid_array_reference (temp, 0))
	{
	  name = temp;
	  goto expand_arrayref;
	}
      else
#endif
       
      if (temp && *temp && legal_identifier (temp) == 0)
        {
	  set_exit_status (EXECUTION_FAILURE);
	  report_error (_("%s: invalid variable name for name reference"), temp);
	  temp = &expand_param_error;
        }
      else
	temp = (char *)NULL;
    }
  else
    temp = (char *)NULL;

  if (ret == 0)
    {
      ret = alloc_word_desc ();
      ret->word = temp;
      ret->flags |= rflags;
    }
  return ret;
}

static char *
parameter_brace_find_indir (name, var_is_special, quoted, find_nameref)
     char *name;
     int var_is_special, quoted, find_nameref;
{
  char *temp, *t;
  WORD_DESC *w;
  SHELL_VAR *v;
  int pflags, oldex;

  if (find_nameref && var_is_special == 0 && (v = find_variable_last_nameref (name, 0)) &&
      nameref_p (v) && (t = nameref_cell (v)) && *t)
    return (savestring (t));

   
  pflags = PF_IGNUNBOUND;
   
  if (var_is_special)
    {
      pflags |= PF_ASSIGNRHS;	 
      oldex = expand_no_split_dollar_star;
      expand_no_split_dollar_star = 1;
    }
  w = parameter_brace_expand_word (name, var_is_special, quoted, pflags, 0);
  if (var_is_special)
    expand_no_split_dollar_star = oldex;

  t = w->word;
   
  if (t)
    {
      temp = ((quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT)) || var_is_special)
		? dequote_string (t)
		: dequote_escapes (t);
      free (t);
      t = temp;
    }
  dispose_word_desc (w);

  return t;
}
  
 
static WORD_DESC *
parameter_brace_expand_indir (name, var_is_special, quoted, pflags, quoted_dollar_atp, contains_dollar_at)
     char *name;
     int var_is_special, quoted, pflags;
     int *quoted_dollar_atp, *contains_dollar_at;
{
  char *t;
  WORD_DESC *w;
  SHELL_VAR *v;

   
  if (var_is_special == 0 && (v = find_variable_last_nameref (name, 0)))
    {
      if (nameref_p (v) && (t = nameref_cell (v)) && *t)
	{
	  w = alloc_word_desc ();
	  w->word = savestring (t);
	  w->flags = 0;
	  return w;
	}
    }

   
  if (legal_identifier (name) && v == 0)
    {
      report_error (_("%s: invalid indirect expansion"), name);
      w = alloc_word_desc ();
      w->word = &expand_param_error;
      w->flags = 0;
      return (w);
    }
      
  t = parameter_brace_find_indir (name, var_is_special, quoted, 0);

  chk_atstar (t, quoted, pflags, quoted_dollar_atp, contains_dollar_at);

#if defined (ARRAY_VARS)
   
  if (t == 0 && valid_array_reference (name, 0))
    {
      v = array_variable_part (name, 0, (char **)0, (int *)0);
      if (v == 0)
	{
	  report_error (_("%s: invalid indirect expansion"), name);
	  w = alloc_word_desc ();
	  w->word = &expand_param_error;
	  w->flags = 0;
	  return (w);
	}
      else
        return (WORD_DESC *)NULL;      
    }
#endif

  if (t == 0)
    return (WORD_DESC *)NULL;

  if (valid_brace_expansion_word (t, SPECIAL_VAR (t, 0)) == 0)
    {
      report_error (_("%s: invalid variable name"), t);
      free (t);
      w = alloc_word_desc ();
      w->word = &expand_param_error;
      w->flags = 0;
      return (w);
    }
	
  w = parameter_brace_expand_word (t, SPECIAL_VAR(t, 0), quoted, pflags, 0);
  free (t);

  return w;
}

 
static WORD_DESC *
parameter_brace_expand_rhs (name, value, op, quoted, pflags, qdollaratp, hasdollarat)
     char *name, *value;
     int op, quoted, pflags, *qdollaratp, *hasdollarat;
{
  WORD_DESC *w;
  WORD_LIST *l, *tl;
  char *t, *t1, *temp, *vname, *newval;
  int l_hasdollat, sindex, arrayref;
  SHELL_VAR *v;
  array_eltstate_t es;

 
   
  if ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) && *value)
    {
      sindex = 0;
      temp = string_extract_double_quoted (value, &sindex, SX_STRIPDQ);
    }
  else
    temp = value;

  w = alloc_word_desc ();
  l_hasdollat = 0;
  l = *temp ? expand_string_for_rhs (temp, quoted, op, pflags, &l_hasdollat, (int *)NULL)
	    : (WORD_LIST *)0;
  if (hasdollarat)
    *hasdollarat = l_hasdollat || (l && l->next);
  if (temp != value)
    free (temp);

   
  for (tl = l; tl; tl = tl->next)
    {
      if (tl->word && (tl->word->word == 0 || tl->word->word[0] == 0) &&
	    (tl->word->flags | W_SAWQUOTEDNULL))
	{
	  t = make_quoted_char ('\0');
	  FREE (tl->word->word);
	  tl->word->word = t;
	  tl->word->flags |= W_QUOTED|W_HASQUOTEDNULL;
	  tl->word->flags &= ~W_SAWQUOTEDNULL;
	}
    }

  if (l)
    {
       
      if (qdollaratp && ((l_hasdollat && quoted) || l->next))
	{
 
	  *qdollaratp = 1;
	}

       
      if (l->next && ifs_is_null)
	{
	  temp = string_list_internal (l, " ");
	  w->flags |= W_SPLITSPACE;
	}
      else if (l_hasdollat || l->next)
	temp = string_list_dollar_star (l, quoted, 0);
      else
	{
	  temp = string_list (l);
	  if (temp && (QUOTED_NULL (temp) == 0) && (l->word->flags & W_SAWQUOTEDNULL))
	    w->flags |= W_SAWQUOTEDNULL;	 
	}

       
      if (l->next == 0 && (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) && QUOTED_NULL (temp) && QUOTED_NULL (l->word->word) && (l->word->flags & W_HASQUOTEDNULL))
	{
	  w->flags |= W_HASQUOTEDNULL;
 
	   
	  if (qdollaratp && l_hasdollat)
	    *qdollaratp = 0;
	}
      dispose_words (l);
    }
  else if ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) && l_hasdollat)
    {
       
       

       
      temp = make_quoted_char ('\0');
      w->flags |= W_HASQUOTEDNULL;
 
    }
  else
    temp = (char *)NULL;

  if (op == '-' || op == '+')
    {
      w->word = temp;
      return w;
    }

   
  t1 = temp ? dequote_string (temp) : savestring ("");
  free (temp);

   
  vname = name;
  if (*name == '!' &&
      (legal_variable_starter ((unsigned char)name[1]) || DIGIT (name[1]) || VALID_INDIR_PARAM (name[1])))
    {
      vname = parameter_brace_find_indir (name + 1, SPECIAL_VAR (name, 1), quoted, 1);
      if (vname == 0 || *vname == 0)
	{
	  report_error (_("%s: invalid indirect expansion"), name);
	  free (vname);
	  free (t1);
	  dispose_word (w);
	  return &expand_wdesc_error;
	}
      if (legal_identifier (vname) == 0)
	{
	  report_error (_("%s: invalid variable name"), vname);
	  free (vname);
	  free (t1);
	  dispose_word (w);
	  return &expand_wdesc_error;
	}
    }
    
  arrayref = 0;
#if defined (ARRAY_VARS)
  if (valid_array_reference (vname, 0))
    {
      init_eltstate (&es);
      v = assign_array_element (vname, t1, ASS_ALLOWALLSUB, &es);
      arrayref = 1;
      newval = es.value;
    }
  else
#endif  
  v = bind_variable (vname, t1, 0);

  if (v == 0 || readonly_p (v) || noassign_p (v))	 
    {
      if ((v == 0 || readonly_p (v)) && interactive_shell == 0 && posixly_correct)
	{
	  last_command_exit_value = EXECUTION_FAILURE;
	  exp_jump_to_top_level (FORCE_EOF);
	}
      else
	{
	  if (vname != name)
	    free (vname);
	  last_command_exit_value = EX_BADUSAGE;
	  exp_jump_to_top_level (DISCARD);
	}
    }

  stupidly_hack_special_variables (vname);

   
  if (shell_compatibility_level > 51)
    {
      FREE (t1);
#if defined (ARRAY_VARS)
      if (arrayref)
	{
	  t1 = newval;
	  flush_eltstate (&es);
	}
      else
        t1 = get_variable_value (v);
#else
      t1 = value_cell (v);
#endif
    }

  if (vname != name)
    free (vname);

   

   
  w->word = (quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT)) ? quote_string (t1) : quote_escapes (t1);
   
  if (w->word && w->word[0] && QUOTED_NULL (w->word) == 0)
    w->flags &= ~W_SAWQUOTEDNULL;

   
  if ((quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT)) && QUOTED_NULL (w->word))
    w->flags |= W_HASQUOTEDNULL;

  return w;
}

 
static void
parameter_brace_expand_error (name, value, check_null)
     char *name, *value;
     int check_null;
{
  WORD_LIST *l;
  char *temp;

  set_exit_status (EXECUTION_FAILURE);	 
  if (value && *value)
    {
      l = expand_string (value, 0);
      temp =  string_list (l);
      report_error ("%s: %s", name, temp ? temp : "");	 
      FREE (temp);
      dispose_words (l);
    }
  else if (check_null == 0)
    report_error (_("%s: parameter not set"), name);
  else
    report_error (_("%s: parameter null or not set"), name);

   
  free (name);
  FREE (value);
}

 
static int
valid_length_expression (name)
     char *name;
{
  return (name[1] == '\0' ||					 
	  ((sh_syntaxtab[(unsigned char) name[1]] & CSPECVAR) && name[2] == '\0') ||   
	  (DIGIT (name[1]) && all_digits (name + 1)) ||	 
#if defined (ARRAY_VARS)
	  valid_array_reference (name + 1, 0) ||		 
#endif
	  legal_identifier (name + 1));				 
}

 
static intmax_t
parameter_brace_expand_length (name)
     char *name;
{
  char *t, *newname;
  intmax_t number, arg_index;
  WORD_LIST *list;
  SHELL_VAR *var;

  var = (SHELL_VAR *)NULL;

  if (name[1] == '\0')			 
    number = number_of_args ();
  else if (DOLLAR_AT_STAR (name[1]) && name[2] == '\0')	 
    number = number_of_args ();
  else if ((sh_syntaxtab[(unsigned char) name[1]] & CSPECVAR) && name[2] == '\0')
    {
       
      switch (name[1])
	{
	case '-':
	  t = which_set_flags ();
	  break;
	case '?':
	  t = itos (last_command_exit_value);
	  break;
	case '$':
	  t = itos (dollar_dollar_pid);
	  break;
	case '!':
	  if (last_asynchronous_pid == NO_PID)
	    t = (char *)NULL;	 
	  else
	    t = itos (last_asynchronous_pid);
	  break;
	case '#':
	  t = itos (number_of_args ());
	  break;
	}
      number = STRLEN (t);
      FREE (t);
    }
#if defined (ARRAY_VARS)
  else if (valid_array_reference (name + 1, 0))
    number = array_length_reference (name + 1);
#endif  
  else
    {
      number = 0;

      if (legal_number (name + 1, &arg_index))		 
	{
	  t = get_dollar_var_value (arg_index);
	  if (t == 0 && unbound_vars_is_error)
	    return INTMAX_MIN;
	  number = MB_STRLEN (t);
	  FREE (t);
	}
#if defined (ARRAY_VARS)
      else if ((var = find_variable (name + 1)) && (invisible_p (var) == 0) && (array_p (var) || assoc_p (var)))
	{
	  if (assoc_p (var))
	    t = assoc_reference (assoc_cell (var), "0");
	  else
	    t = array_reference (array_cell (var), 0);
	  if (t == 0 && unbound_vars_is_error)
	    return INTMAX_MIN;
	  number = MB_STRLEN (t);
	}
#endif
       
      else if ((var || (var = find_variable (name + 1))) &&
      		invisible_p (var) == 0 &&
		array_p (var) == 0 && assoc_p (var) == 0 &&
		var->dynamic_value == 0)
	number = value_cell (var) ? MB_STRLEN (value_cell (var)) : 0;
      else if (var == 0 && unbound_vars_is_error == 0)
	number = 0;
      else				 
	{
	  newname = savestring (name);
	  newname[0] = '$';
	  list = expand_string (newname, Q_DOUBLE_QUOTES);
	  t = list ? string_list (list) : (char *)NULL;
	  free (newname);
	  if (list)
	    dispose_words (list);

	  number = t ? MB_STRLEN (t) : 0;
	  FREE (t);
	}
    }

  return (number);
}

 

static char *
skiparith (substr, delim)
     char *substr;
     int delim;
{
  int i;
  char delims[2];

  delims[0] = delim;
  delims[1] = '\0';

  i = skip_to_delim (substr, 0, delims, SD_ARITHEXP);
  return (substr + i);
}

 
static int
verify_substring_values (v, value, substr, vtype, e1p, e2p)
     SHELL_VAR *v;
     char *value, *substr;
     int vtype;
     intmax_t *e1p, *e2p;
{
  char *t, *temp1, *temp2;
  arrayind_t len;
  int expok, eflag;
#if defined (ARRAY_VARS)
 ARRAY *a;
 HASH_TABLE *h;
#endif

   
  t = skiparith (substr, ':');
  if (*t && *t == ':')
    *t = '\0';
  else
    t = (char *)0;

  temp1 = expand_arith_string (substr, Q_DOUBLE_QUOTES|Q_ARITH);
  eflag = (shell_compatibility_level > 51) ? 0 : EXP_EXPANDED;

  *e1p = evalexp (temp1, eflag, &expok);
  free (temp1);
  if (expok == 0)
    return (0);

  len = -1;	 
  switch (vtype)
    {
    case VT_VARIABLE:
    case VT_ARRAYMEMBER:
      len = MB_STRLEN (value);
      break;
    case VT_POSPARMS:
      len = number_of_args () + 1;
      if (*e1p == 0)
	len++;		 
      break;
#if defined (ARRAY_VARS)
    case VT_ARRAYVAR:
       
      if (assoc_p (v))
	{
	  h = assoc_cell (v);
	  len = assoc_num_elements (h) + (*e1p < 0);
	}
      else
	{
	  a = (ARRAY *)value;
	  len = array_max_index (a) + (*e1p < 0);	 
	}
      break;
#endif
    }

  if (len == -1)	 
    return -1;

  if (*e1p < 0)		 
    *e1p += len;

  if (*e1p > len || *e1p < 0)
    return (-1);

#if defined (ARRAY_VARS)
   
  if (vtype == VT_ARRAYVAR)
    len = assoc_p (v) ? assoc_num_elements (h) : array_num_elements (a);
#endif

  if (t)
    {
      t++;
      temp2 = savestring (t);
      temp1 = expand_arith_string (temp2, Q_DOUBLE_QUOTES|Q_ARITH);
      free (temp2);
      t[-1] = ':';
      *e2p = evalexp (temp1, eflag, &expok);
      free (temp1);
      if (expok == 0)
	return (0);

       
#if 1
      if ((vtype == VT_ARRAYVAR || vtype == VT_POSPARMS) && *e2p < 0)
#else  
      if (vtype == VT_ARRAYVAR && *e2p < 0)
#endif
	{
	  internal_error (_("%s: substring expression < 0"), t);
	  return (0);
	}
#if defined (ARRAY_VARS)
       
      if (vtype != VT_ARRAYVAR)
#endif
	{
	  if (*e2p < 0)
	    {
	      *e2p += len;
	      if (*e2p < 0 || *e2p < *e1p)
		{
		  internal_error (_("%s: substring expression < 0"), t);
		  return (0);
		}
	    }
	  else
	    *e2p += *e1p;		 
	  if (*e2p > len)
	    *e2p = len;
	}
    }
  else
    *e2p = len;

  return (1);
}

 
static int
get_var_and_type (varname, value, estatep, quoted, flags, varp, valp)
     char *varname, *value;
     array_eltstate_t *estatep;
     int quoted, flags;
     SHELL_VAR **varp;
     char **valp;
{
  int vtype, want_indir;
  char *temp, *vname;
  SHELL_VAR *v;

  want_indir = *varname == '!' &&
    (legal_variable_starter ((unsigned char)varname[1]) || DIGIT (varname[1])
					|| VALID_INDIR_PARAM (varname[1]));
  if (want_indir)
    vname = parameter_brace_find_indir (varname+1, SPECIAL_VAR (varname, 1), quoted, 1);
     
  else
    vname = varname;

  if (vname == 0)
    {
      vtype = VT_VARIABLE;
      *varp = (SHELL_VAR *)NULL;
      *valp = (char *)NULL;
      return (vtype);
    }

   
  vtype = STR_DOLLAR_AT_STAR (vname);
  if (vtype == VT_POSPARMS && vname[0] == '*')
    vtype |= VT_STARSUB;
  *varp = (SHELL_VAR *)NULL;

#if defined (ARRAY_VARS)
  if (valid_array_reference (vname, 0))
    {
      v = array_variable_part (vname, 0, &temp, (int *)0);
       
      if (estatep && (flags & AV_USEIND) == 0)
	estatep->ind = INTMAX_MIN;

      if (v && invisible_p (v))
	{
	  vtype = VT_ARRAYMEMBER;
	  *varp = (SHELL_VAR *)NULL;
	  *valp = (char *)NULL;
	}
      if (v && (array_p (v) || assoc_p (v)))
	{
	  if (ALL_ELEMENT_SUB (temp[0]) && temp[1] == RBRACK)
	    {
	       
	      vtype = VT_ARRAYVAR;
	      if (temp[0] == '*')
		vtype |= VT_STARSUB;
	      *valp = array_p (v) ? (char *)array_cell (v) : (char *)assoc_cell (v);
	    }
	  else
	    {
	      vtype = VT_ARRAYMEMBER;
	      *valp = array_value (vname, Q_DOUBLE_QUOTES, flags, estatep);
	    }
	  *varp = v;
	}
      else if (v && (ALL_ELEMENT_SUB (temp[0]) && temp[1] == RBRACK))
	{
	  vtype = VT_VARIABLE;
	  *varp = v;
	  if (quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT))
	    *valp = value ? dequote_string (value) : (char *)NULL;
	  else
	    *valp = value ? dequote_escapes (value) : (char *)NULL;
	}
      else
	{
	  vtype = VT_ARRAYMEMBER;
	  *varp = v;
	  *valp = array_value (vname, Q_DOUBLE_QUOTES, flags, estatep);
	}
    }
  else if ((v = find_variable (vname)) && (invisible_p (v) == 0) && (assoc_p (v) || array_p (v)))
    {
      vtype = VT_ARRAYMEMBER;
      *varp = v;
      *valp = assoc_p (v) ? assoc_reference (assoc_cell (v), "0") : array_reference (array_cell (v), 0);
    }
  else
#endif
    {
      if (value && vtype == VT_VARIABLE)
	{
	  *varp = find_variable (vname);
	  if (quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT))
	    *valp = dequote_string (value);
	  else
	    *valp = dequote_escapes (value);
	}
      else
	*valp = value;
    }

  if (want_indir)
    free (vname);

  return vtype;
}

 
 
 
 
 

static char *
string_var_assignment (v, s)
     SHELL_VAR *v;
     char *s;
{
  char flags[MAX_ATTRIBUTES], *ret, *val;
  int i;

  val = (v && (invisible_p (v) || var_isset (v) == 0)) ? (char *)NULL : sh_quote_reusable (s, 0);
  i = var_attribute_string (v, 0, flags);
  if (i == 0 && val == 0)
    return (char *)NULL;

  ret = (char *)xmalloc (i + STRLEN (val) + strlen (v->name) + 16 + MAX_ATTRIBUTES);
  if (i > 0 && val == 0)
    sprintf (ret, "declare -%s %s", flags, v->name);
  else if (i > 0)
    sprintf (ret, "declare -%s %s=%s", flags, v->name, val);
  else
    sprintf (ret, "%s=%s", v->name, val);
  free (val);
  return ret;
}

#if defined (ARRAY_VARS)
static char *
array_var_assignment (v, itype, quoted, atype)
     SHELL_VAR *v;
     int itype, quoted, atype;
{
  char *ret, *val, flags[MAX_ATTRIBUTES];
  int i;

  if (v == 0)
    return (char *)NULL;
  if (atype == 2)
    val = array_p (v) ? array_to_kvpair (array_cell (v), 0)
		      : assoc_to_kvpair (assoc_cell (v), 0);
  else
    val = array_p (v) ? array_to_assign (array_cell (v), 0)
		      : assoc_to_assign (assoc_cell (v), 0);

  if (val == 0 && (invisible_p (v) || var_isset (v) == 0))
    ;	 
  else if (val == 0)
    {
      val = (char *)xmalloc (3);
      val[0] = LPAREN;
      val[1] = RPAREN;
      val[2] = 0;
    }
  else
    {
      ret = (quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT)) ? quote_string (val) : quote_escapes (val);
      free (val);
      val = ret;
    }

  if (atype == 2)
    return val;

  i = var_attribute_string (v, 0, flags);
  ret = (char *)xmalloc (i + STRLEN (val) + strlen (v->name) + 16);
  if (val)
    sprintf (ret, "declare -%s %s=%s", flags, v->name, val);
  else
    sprintf (ret, "declare -%s %s", flags, v->name);
  free (val);
  return ret;
}
#endif

static char *
pos_params_assignment (list, itype, quoted)
     WORD_LIST *list;
     int itype;
     int quoted;
{
  char *temp, *ret;

   
  temp = list_transform ('Q', (SHELL_VAR *)0, list, itype, quoted);
  ret = (char *)xmalloc (strlen (temp) + 8);
  strcpy (ret, "set -- ");
  strcpy (ret + 7, temp);
  free (temp);
  return ret;
}

static char *
string_transform (xc, v, s)
     int xc;
     SHELL_VAR *v;
     char *s;
{
  char *ret, flags[MAX_ATTRIBUTES], *t;
  int i;

  if (((xc == 'A' || xc == 'a') && v == 0))
    return (char *)NULL;
  else if (xc != 'a' && xc != 'A' && s == 0)
    return (char *)NULL;

  switch (xc)
    {
       
      case 'a':
	i = var_attribute_string (v, 0, flags);
	ret = (i > 0) ? savestring (flags) : (char *)NULL;
	break;
      case 'A':
	ret = string_var_assignment (v, s);
	break;
      case 'K':
      case 'k':
	ret = sh_quote_reusable (s, 0);
	break;
       
      case 'E':
	t = ansiexpand (s, 0, strlen (s), (int *)0);
	ret = dequote_escapes (t);
	free (t);
	break;
      case 'P':
	ret = decode_prompt_string (s);
	break;
      case 'Q':
	ret = sh_quote_reusable (s, 0);
	break;
      case 'U':
	ret = sh_modcase (s, 0, CASE_UPPER);
	break;
      case 'u':
	ret = sh_modcase (s, 0, CASE_UPFIRST);	 
 	break;
      case 'L':
 	ret = sh_modcase (s, 0, CASE_LOWER);
 	break;
      default:
	ret = (char *)NULL;
	break;
    }
  return ret;
}

static char *
list_transform (xc, v, list, itype, quoted)
     int xc;
     SHELL_VAR *v;
     WORD_LIST *list;
     int itype, quoted;
{
  WORD_LIST *new, *l;
  WORD_DESC *w;
  char *tword;
  int qflags;

  for (new = (WORD_LIST *)NULL, l = list; l; l = l->next)
    {
      tword = string_transform (xc, v, l->word->word);
      w = alloc_word_desc ();
      w->word = tword ? tword : savestring ("");	 
      new = make_word_list (w, new);
    }
  l = REVERSE_LIST (new, WORD_LIST *);

  qflags = quoted;
   
  if (itype == '*' && expand_no_split_dollar_star && ifs_is_null)
    qflags |= Q_DOUBLE_QUOTES;		 

  tword = string_list_pos_params (itype, l, qflags, 0);
  dispose_words (l);

  return (tword);
}

static char *
parameter_list_transform (xc, itype, quoted)
     int xc;
     int itype;
     int quoted;
{
  char *ret;
  WORD_LIST *list;

  list = list_rest_of_args ();
  if (list == 0)
    return ((char *)NULL);
  if (xc == 'A')
    ret = pos_params_assignment (list, itype, quoted);
  else
    ret = list_transform (xc, (SHELL_VAR *)0, list, itype, quoted);
  dispose_words (list);
  return (ret);
}

#if defined (ARRAY_VARS)
static char *
array_transform (xc, var, starsub, quoted)
     int xc;
     SHELL_VAR *var;
     int starsub;	 
     int quoted;
{
  ARRAY *a;
  HASH_TABLE *h;
  int itype, qflags;
  char *ret;
  WORD_LIST *list;
  SHELL_VAR *v;

  v = var;	 

  itype = starsub ? '*' : '@';

  if (xc == 'A')
    return (array_var_assignment (v, itype, quoted, 1));
  else if (xc == 'K')
    return (array_var_assignment (v, itype, quoted, 2));

   
  if (xc == 'a' && (invisible_p (v) || var_isset (v) == 0))
    {
      char flags[MAX_ATTRIBUTES];
      int i;

      i = var_attribute_string (v, 0, flags);
      return ((i > 0) ? savestring (flags) : (char *)NULL);
    }

  a = (v && array_p (v)) ? array_cell (v) : 0;
  h = (v && assoc_p (v)) ? assoc_cell (v) : 0;

   
  if (xc == 'k')
    {
      if (v == 0)
	return ((char *)NULL);
      list = array_p (v) ? array_to_kvpair_list (a) : assoc_to_kvpair_list (h);
      qflags = quoted;
       
      if (itype == '*' && expand_no_split_dollar_star && ifs_is_null)
	qflags |= Q_DOUBLE_QUOTES;		 

      ret = string_list_pos_params (itype, list, qflags, 0);
      dispose_words (list);
      return ret;
    }

  list = a ? array_to_word_list (a) : (h ? assoc_to_word_list (h) : 0);
  if (list == 0)
   return ((char *)NULL);
  ret = list_transform (xc, v, list, itype, quoted);
  dispose_words (list);

  return ret;
}
#endif  

static int
valid_parameter_transform (xform)
     char *xform;
{
  if (xform[1])
    return 0;

   
  switch (xform[0])
    {
    case 'a':		 
    case 'A':		 
    case 'K':		 
    case 'k':		 
    case 'E':		 
    case 'P':		 
    case 'Q':		 
    case 'U':		 
    case 'u':		 
    case 'L':		 
      return 1;
    default:
      return 0;
    }
}
      
static char *
parameter_brace_transform (varname, value, estatep, xform, rtype, quoted, pflags, flags)
     char *varname, *value;
     array_eltstate_t *estatep;
     char *xform;
     int rtype, quoted, pflags, flags;
{
  int vtype, xc, starsub;
  char *temp1, *val, *oname;
  SHELL_VAR *v;

  xc = xform[0];
  if (value == 0 && xc != 'A' && xc != 'a')
    return ((char *)NULL);

  oname = this_command_name;
  this_command_name = varname;

  vtype = get_var_and_type (varname, value, estatep, quoted, flags, &v, &val);
  if (vtype == -1)
    {
      this_command_name = oname;
      return ((char *)NULL);
    }

  if (xform[0] == 0 || valid_parameter_transform (xform) == 0)
    {
      this_command_name = oname;
      if (vtype == VT_VARIABLE)
	FREE (val);
      return (interactive_shell ? &expand_param_error : &expand_param_fatal);
    }

  starsub = vtype & VT_STARSUB;
  vtype &= ~VT_STARSUB;

   
  if ((xc == 'a' || xc == 'A') && vtype == VT_VARIABLE && varname && v == 0)
    v = find_variable (varname);

  temp1 = (char *)NULL;		 
  switch (vtype)
    {
    case VT_VARIABLE:
    case VT_ARRAYMEMBER:
      temp1 = string_transform (xc, v, val);
      if (vtype == VT_VARIABLE)
	FREE (val);
      if (temp1)
	{
	  val = (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES))
			? quote_string (temp1)
			: quote_escapes (temp1);
	  free (temp1);
	  temp1 = val;
	}
      break;
#if defined (ARRAY_VARS)
    case VT_ARRAYVAR:
      temp1 = array_transform (xc, v, starsub, quoted);
      if (temp1 && quoted == 0 && ifs_is_null)
	{
		 
	}
      else if (temp1 && ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) == 0))
	{
	  val = quote_escapes (temp1);
	  free (temp1);
	  temp1 = val;
	}
      break;
#endif
    case VT_POSPARMS:
      temp1 = parameter_list_transform (xc, varname[0], quoted);
      if (temp1 && quoted == 0 && ifs_is_null)
	{
		 
	}
      else if (temp1 && ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) == 0))
	{
	  val = quote_escapes (temp1);
	  free (temp1);
	  temp1 = val;
	}
      break;
    }

  this_command_name = oname;
  return temp1;
}

 
 
 
 
 

#if defined (HANDLE_MULTIBYTE)
 
static char *
mb_substring (string, s, e)
     char *string;
     int s, e;
{
  char *tt;
  int start, stop, i;
  size_t slen;
  DECLARE_MBSTATE;

  start = 0;
   
  slen = (MB_CUR_MAX > 1) ? STRLEN (string) : 0;

  i = s;
  while (string[start] && i--)
    ADVANCE_CHAR (string, slen, start);
  stop = start;
  i = e - s;
  while (string[stop] && i--)
    ADVANCE_CHAR (string, slen, stop);
  tt = substring (string, start, stop);
  return tt;
}
#endif
  
 

static char *
parameter_brace_substring (varname, value, estatep, substr, quoted, pflags, flags)
     char *varname, *value;
     array_eltstate_t *estatep;
     char *substr;
     int quoted, pflags, flags;
{
  intmax_t e1, e2;
  int vtype, r, starsub;
  char *temp, *val, *tt, *oname;
  SHELL_VAR *v;

  if (value == 0 && ((varname[0] != '@' && varname[0] != '*') || varname[1]))
    return ((char *)NULL);

  oname = this_command_name;
  this_command_name = varname;

  vtype = get_var_and_type (varname, value, estatep, quoted, flags, &v, &val);
  if (vtype == -1)
    {
      this_command_name = oname;
      return ((char *)NULL);
    }

  starsub = vtype & VT_STARSUB;
  vtype &= ~VT_STARSUB;

  r = verify_substring_values (v, val, substr, vtype, &e1, &e2);
  this_command_name = oname;
  if (r <= 0)
    {
      if (vtype == VT_VARIABLE)
	FREE (val);
      return ((r == 0) ? &expand_param_error : (char *)NULL);
    }

  switch (vtype)
    {
    case VT_VARIABLE:
    case VT_ARRAYMEMBER:
#if defined (HANDLE_MULTIBYTE)
      if (MB_CUR_MAX > 1)
	tt = mb_substring (val, e1, e2);
      else
#endif
      tt = substring (val, e1, e2);

      if (vtype == VT_VARIABLE)
	FREE (val);
      if (quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT))
	temp = quote_string (tt);
      else
	temp = tt ? quote_escapes (tt) : (char *)NULL;
      FREE (tt);
      break;
    case VT_POSPARMS:
    case VT_ARRAYVAR:
      if (vtype == VT_POSPARMS)
	tt = pos_params (varname, e1, e2, quoted, pflags);
#if defined (ARRAY_VARS)
         
      else if (assoc_p (v))
	 	
	tt = assoc_subrange (assoc_cell (v), e1, e2, starsub, quoted, pflags);
      else
	 
	tt = array_subrange (array_cell (v), e1, e2, starsub, quoted, pflags);
#endif
       
      if (tt && quoted == 0 && ifs_is_null)
	{
	  temp = tt;	 
	}
      else if (tt && quoted == 0 && (pflags & PF_ASSIGNRHS))
	{
	  temp = tt;	 
	}
      else if ((quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT)) == 0)
	{
	  temp = tt ? quote_escapes (tt) : (char *)NULL;
	  FREE (tt);
	}
      else
	temp = tt;
      break;

    default:
      temp = (char *)NULL;
    }

  return temp;
}

 
 
 
 
 

static int
shouldexp_replacement (s)
     char *s;
{
  size_t slen;
  int sindex, c;
  DECLARE_MBSTATE;

  sindex = 0;
  slen = STRLEN (s);
  while (c = s[sindex])
    {
      if (c == '\\')
	{
	  sindex++;
	  if (s[sindex] == 0)
	    return 0;
	   
	  if (s[sindex] == '&')
	    return 1;
	  if (s[sindex] == '\\')
	    return 1;
	}
      else if (c == '&')
	return 1;
      ADVANCE_CHAR (s, slen, sindex);
    }
  return 0;
}

char *
pat_subst (string, pat, rep, mflags)
     char *string, *pat, *rep;
     int mflags;
{
  char *ret, *s, *e, *str, *rstr, *mstr, *send;
  int rptr, mtype, rxpand, mlen;
  size_t rsize, l, replen, rslen;
  DECLARE_MBSTATE;

  if (string == 0)
    return (savestring (""));

  mtype = mflags & MATCH_TYPEMASK;
  rxpand = mflags & MATCH_EXPREP;

   
  if ((pat == 0 || *pat == 0) && (mtype == MATCH_BEG || mtype == MATCH_END))
    {
      rstr = (mflags & MATCH_EXPREP) ? strcreplace (rep, '&', "", 2) : rep;
      rslen = STRLEN (rstr);
      l = STRLEN (string);
      ret = (char *)xmalloc (rslen + l + 2);
      if (rslen == 0)
	strcpy (ret, string);
      else if (mtype == MATCH_BEG)
	{
	  strcpy (ret, rstr);
	  strcpy (ret + rslen, string);
	}
      else
	{
	  strcpy (ret, string);
	  strcpy (ret + l, rstr);
	}
      if (rstr != rep)
	free (rstr);
      return (ret);
    }
  else if (*string == 0 && (match_pattern (string, pat, mtype, &s, &e) != 0))
    return (mflags & MATCH_EXPREP) ? strcreplace (rep, '&', "", 2)
				   : (rep ? savestring (rep) : savestring (""));

  ret = (char *)xmalloc (rsize = 64);
  ret[0] = '\0';
  send = string + strlen (string);

  for (replen = STRLEN (rep), rptr = 0, str = string; *str;)
    {
      if (match_pattern (str, pat, mtype, &s, &e) == 0)
	break;
      l = s - str;

      if (rep && rxpand)
        {
	  int x;
	  mlen = e - s;
	  mstr = xmalloc (mlen + 1);
	  for (x = 0; x < mlen; x++)
	    mstr[x] = s[x];
	  mstr[mlen] = '\0';
	  rstr = strcreplace (rep, '&', mstr, 2);
	  free (mstr);
	  rslen = strlen (rstr);
        }
      else
	{
	  rstr = rep;
	  rslen = replen;
	}
        
      RESIZE_MALLOCED_BUFFER (ret, rptr, (l + rslen), rsize, 64);

       
      if (l)
	{
	  strncpy (ret + rptr, str, l);
	  rptr += l;
	}
      if (replen)
	{
	  strncpy (ret + rptr, rstr, rslen);
	  rptr += rslen;
	}
      str = e;		 

      if (rstr != rep)
	free (rstr);

      if (((mflags & MATCH_GLOBREP) == 0) || mtype != MATCH_ANY)
	break;

      if (s == e)
	{
	   
	  char *p, *origp, *origs;
	  size_t clen;

	  RESIZE_MALLOCED_BUFFER (ret, rptr, locale_mb_cur_max, rsize, 64);
#if defined (HANDLE_MULTIBYTE)
	  p = origp = ret + rptr;
	  origs = str;
	  COPY_CHAR_P (p, str, send);
	  rptr += p - origp;
	  e += str - origs;
#else
	  ret[rptr++] = *str++;
	  e++;		 
#endif
	}
    }

   
  if (str && *str)
    {
      l = send - str + 1;
      RESIZE_MALLOCED_BUFFER (ret, rptr, l, rsize, 64);
      strcpy (ret + rptr, str);
    }
  else
    ret[rptr] = '\0';

  return ret;
}

 
static char *
pos_params_pat_subst (string, pat, rep, mflags)
     char *string, *pat, *rep;
     int mflags;
{
  WORD_LIST *save, *params;
  WORD_DESC *w;
  char *ret;
  int pchar, qflags, pflags;

  save = params = list_rest_of_args ();
  if (save == 0)
    return ((char *)NULL);

  for ( ; params; params = params->next)
    {
      ret = pat_subst (params->word->word, pat, rep, mflags);
      w = alloc_word_desc ();
      w->word = ret ? ret : savestring ("");
      dispose_word (params->word);
      params->word = w;
    }

  pchar = (mflags & MATCH_STARSUB) == MATCH_STARSUB ? '*' : '@';
  qflags = (mflags & MATCH_QUOTED) == MATCH_QUOTED ? Q_DOUBLE_QUOTES : 0;
  pflags = (mflags & MATCH_ASSIGNRHS) == MATCH_ASSIGNRHS ? PF_ASSIGNRHS : 0;

   
  if (pchar == '*' && (mflags & MATCH_ASSIGNRHS) && expand_no_split_dollar_star && ifs_is_null)
    qflags |= Q_DOUBLE_QUOTES;		 

  ret = string_list_pos_params (pchar, save, qflags, pflags);
  dispose_words (save);

  return (ret);
}

 
static char *
parameter_brace_patsub (varname, value, estatep, patsub, quoted, pflags, flags)
     char *varname, *value;
     array_eltstate_t *estatep;
     char *patsub;
     int quoted, pflags, flags;
{
  int vtype, mflags, starsub, delim;
  char *val, *temp, *pat, *rep, *p, *lpatsub, *tt, *oname;
  SHELL_VAR *v;

  if (value == 0)
    return ((char *)NULL);

  oname = this_command_name;
  this_command_name = varname;		 

  vtype = get_var_and_type (varname, value, estatep, quoted, flags, &v, &val);
  if (vtype == -1)
    {
      this_command_name = oname;
      return ((char *)NULL);
    }

  starsub = vtype & VT_STARSUB;
  vtype &= ~VT_STARSUB;

  mflags = 0;
   
  if (*patsub == '/')
    {
      mflags |= MATCH_GLOBREP;
      patsub++;
    }

   
  lpatsub = savestring (patsub);

  if (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES))
    mflags |= MATCH_QUOTED;

  if (starsub)
    mflags |= MATCH_STARSUB;

  if (pflags & PF_ASSIGNRHS)
    mflags |= MATCH_ASSIGNRHS;

   
  delim = skip_to_delim (lpatsub, ((*patsub == '/') ? 1 : 0), "/", 0);
  if (lpatsub[delim] == '/')
    {
      lpatsub[delim] = 0;
      rep = lpatsub + delim + 1;
    }
  else
    rep = (char *)NULL;

  if (rep && *rep == '\0')
    rep = (char *)NULL;

   
  pat = getpattern (lpatsub, quoted, 1);

  if (rep)
    {
       
      if (shell_compatibility_level > 42 && patsub_replacement == 0)
	rep = expand_string_if_necessary (rep, quoted & ~(Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT), expand_string_unsplit);
      else if (shell_compatibility_level > 42 && patsub_replacement)
	rep = expand_string_for_patsub (rep, quoted & ~(Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT));
             
      else if ((mflags & MATCH_QUOTED) == 0)
	rep = expand_string_if_necessary (rep, quoted, expand_string_unsplit);
      else
	rep = expand_string_to_string_internal (rep, quoted, expand_string_unsplit);

       
      if (patsub_replacement && rep && *rep && shouldexp_replacement (rep))
	mflags |= MATCH_EXPREP;

    }

   
  p = pat;
  if (mflags & MATCH_GLOBREP)
    mflags |= MATCH_ANY;
  else if (pat && pat[0] == '#')
    {
      mflags |= MATCH_BEG;
      p++;
    }
  else if (pat && pat[0] == '%')
    {
      mflags |= MATCH_END;
      p++;
    }
  else
    mflags |= MATCH_ANY;

   

  switch (vtype)
    {
    case VT_VARIABLE:
    case VT_ARRAYMEMBER:
      temp = pat_subst (val, p, rep, mflags);
      if (vtype == VT_VARIABLE)
	FREE (val);
      if (temp)
	{
	  tt = (mflags & MATCH_QUOTED) ? quote_string (temp) : quote_escapes (temp);
	  free (temp);
	  temp = tt;
	}
      break;
    case VT_POSPARMS:
       
      if ((pflags & PF_NOSPLIT2) && (mflags & MATCH_STARSUB))
        mflags |= MATCH_ASSIGNRHS;
      temp = pos_params_pat_subst (val, p, rep, mflags);
      if (temp && quoted == 0 && ifs_is_null)
	{
	   
	}
      else if (temp && quoted == 0 && (pflags & PF_ASSIGNRHS))
	{
	   
	}
      else if (temp && (mflags & MATCH_QUOTED) == 0)
	{
	  tt = quote_escapes (temp);
	  free (temp);
	  temp = tt;
	}
      break;
#if defined (ARRAY_VARS)
    case VT_ARRAYVAR:
       
      if ((mflags & MATCH_STARSUB) && (mflags & MATCH_ASSIGNRHS) && ifs_is_null)
	mflags |= MATCH_QUOTED;		 

       
      if (assoc_p (v))
	temp = assoc_patsub (assoc_cell (v), p, rep, mflags);
      else
	temp = array_patsub (array_cell (v), p, rep, mflags);

      if (temp && quoted == 0 && ifs_is_null)
	{
	   
	}
      else if (temp && (mflags & MATCH_QUOTED) == 0)
	{
	  tt = quote_escapes (temp);
	  free (temp);
	  temp = tt;
	}
      break;
#endif
    }

  FREE (pat);
  FREE (rep);
  free (lpatsub);

  this_command_name = oname;

  return temp;
}

 
 
 
 
 

 

static char *
pos_params_modcase (string, pat, modop, mflags)
     char *string, *pat;
     int modop;
     int mflags;
{
  WORD_LIST *save, *params;
  WORD_DESC *w;
  char *ret;
  int pchar, qflags, pflags;

  save = params = list_rest_of_args ();
  if (save == 0)
    return ((char *)NULL);

  for ( ; params; params = params->next)
    {
      ret = sh_modcase (params->word->word, pat, modop);
      w = alloc_word_desc ();
      w->word = ret ? ret : savestring ("");
      dispose_word (params->word);
      params->word = w;
    }

  pchar = (mflags & MATCH_STARSUB) == MATCH_STARSUB ? '*' : '@';
  qflags = (mflags & MATCH_QUOTED) == MATCH_QUOTED ? Q_DOUBLE_QUOTES : 0;
  pflags = (mflags & MATCH_ASSIGNRHS) == MATCH_ASSIGNRHS ? PF_ASSIGNRHS : 0;

   
  if (pchar == '*' && (mflags & MATCH_ASSIGNRHS) && ifs_is_null)
    qflags |= Q_DOUBLE_QUOTES;		 

  ret = string_list_pos_params (pchar, save, qflags, pflags);
  dispose_words (save);

  return (ret);
}

 
static char *
parameter_brace_casemod (varname, value, estatep, modspec, patspec, quoted, pflags, flags)
     char *varname, *value;
     array_eltstate_t *estatep;
     int modspec;
     char *patspec;
     int quoted, pflags, flags;
{
  int vtype, starsub, modop, mflags, x;
  char *val, *temp, *pat, *p, *lpat, *tt, *oname;
  SHELL_VAR *v;

  if (value == 0)
    return ((char *)NULL);

  oname = this_command_name;
  this_command_name = varname;

  vtype = get_var_and_type (varname, value, estatep, quoted, flags, &v, &val);
  if (vtype == -1)
    {
      this_command_name = oname;
      return ((char *)NULL);
    }

  starsub = vtype & VT_STARSUB;
  vtype &= ~VT_STARSUB;

  modop = 0;
  mflags = 0;
  if (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES))
    mflags |= MATCH_QUOTED;
  if (starsub)
    mflags |= MATCH_STARSUB;
  if (pflags & PF_ASSIGNRHS)
    mflags |= MATCH_ASSIGNRHS;
  
  p = patspec;
  if (modspec == '^')
    {
      x = p && p[0] == modspec;
      modop = x ? CASE_UPPER : CASE_UPFIRST;
      p += x;
    }
  else if (modspec == ',')
    {
      x = p && p[0] == modspec;
      modop = x ? CASE_LOWER : CASE_LOWFIRST;
      p += x;
    }
  else if (modspec == '~')
    {
      x = p && p[0] == modspec;
      modop = x ? CASE_TOGGLEALL : CASE_TOGGLE;
      p += x;
    }
    
  lpat = p ? savestring (p) : 0;
   
  pat = lpat ? getpattern (lpat, quoted, 1) : 0;

   
  switch (vtype)
    {
    case VT_VARIABLE:
    case VT_ARRAYMEMBER:
      temp = sh_modcase (val, pat, modop);
      if (vtype == VT_VARIABLE)
	FREE (val);
      if (temp)
	{
	  tt = (mflags & MATCH_QUOTED) ? quote_string (temp) : quote_escapes (temp);
	  free (temp);
	  temp = tt;
	}
      break;

    case VT_POSPARMS:
      temp = pos_params_modcase (val, pat, modop, mflags);
      if (temp && quoted == 0 && ifs_is_null)
	{
	   
	}
      else if (temp && (mflags & MATCH_QUOTED) == 0)
	{
	  tt = quote_escapes (temp);
	  free (temp);
	  temp = tt;
	}
      break;

#if defined (ARRAY_VARS)
    case VT_ARRAYVAR:
       
      if ((mflags & MATCH_STARSUB) && (mflags & MATCH_ASSIGNRHS) && ifs_is_null)
	mflags |= MATCH_QUOTED;		 

      temp = assoc_p (v) ? assoc_modcase (assoc_cell (v), pat, modop, mflags)
			 : array_modcase (array_cell (v), pat, modop, mflags);

      if (temp && quoted == 0 && ifs_is_null)
	{
	   
	}
      else if (temp && (mflags & MATCH_QUOTED) == 0)
	{
	  tt = quote_escapes (temp);
	  free (temp);
	  temp = tt;
	}

      break;
#endif
    }

  FREE (pat);
  free (lpat);

  this_command_name = oname;

  return temp;
}

 
static int
chk_arithsub (s, len)
     const char *s;
     int len;
{
  int i, count;
  DECLARE_MBSTATE;

  i = count = 0;
  while (i < len)
    {
      if (s[i] == LPAREN)
	count++;
      else if (s[i] == RPAREN)
	{
	  count--;
	  if (count < 0)
	    return 0;
	}

      switch (s[i])
	{
	default:
	  ADVANCE_CHAR (s, len, i);
	  break;

	case '\\':
	  i++;
	  if (s[i])
	    ADVANCE_CHAR (s, len, i);
	  break;

	case '\'':
	  i = skip_single_quoted (s, len, ++i, 0);
	  break;

	case '"':
	  i = skip_double_quoted ((char *)s, len, ++i, 0);
	  break;
	}
    }

  return (count == 0);
}

 
 
 
 
 

 
static WORD_DESC *
parameter_brace_expand (string, indexp, quoted, pflags, quoted_dollar_atp, contains_dollar_at)
     char *string;
     int *indexp, quoted, pflags, *quoted_dollar_atp, *contains_dollar_at;
{
  int check_nullness, var_is_set, var_is_null, var_is_special;
  int want_substring, want_indir, want_patsub, want_casemod, want_attributes;
  char *name, *value, *temp, *temp1;
  WORD_DESC *tdesc, *ret;
  int t_index, sindex, c, tflag, modspec, local_pflags, all_element_arrayref;
  intmax_t number;
  array_eltstate_t es;

  temp = temp1 = value = (char *)NULL;
  var_is_set = var_is_null = var_is_special = check_nullness = 0;
  want_substring = want_indir = want_patsub = want_casemod = want_attributes = 0;

  local_pflags = 0;
  all_element_arrayref = 0;

  sindex = *indexp;
  t_index = ++sindex;
   
  if (string[t_index] == '#' && legal_variable_starter (string[t_index+1]))		 
    name = string_extract (string, &t_index, "}", SX_VARNAME);
  else
#if defined (CASEMOD_EXPANSIONS)
     
#  if defined (CASEMOD_TOGGLECASE)
    name = string_extract (string, &t_index, "#%^,~:-=?+/@}", SX_VARNAME);
#  else
    name = string_extract (string, &t_index, "#%^,:-=?+/@}", SX_VARNAME);
#  endif  
#else
    name = string_extract (string, &t_index, "#%:-=?+/@}", SX_VARNAME);
#endif  

   
  if (*name == 0 && sindex == t_index && string[sindex] == '@')
    {
      name = (char *)xrealloc (name, 2);
      name[0] = '@';
      name[1] = '\0';
      t_index++;
    }
  else if (*name == '!' && t_index > sindex && string[t_index] == '@' && string[t_index+1] == RBRACE)
    {
      name = (char *)xrealloc (name, t_index - sindex + 2);
      name[t_index - sindex] = '@';
      name[t_index - sindex + 1] = '\0';
      t_index++;
    }

  ret = 0;
  tflag = 0;

#if defined (ARRAY_VARS)
  init_eltstate (&es);
#endif
  es.ind = INTMAX_MIN;	 

   
  if ((sindex == t_index && VALID_SPECIAL_LENGTH_PARAM (string[t_index])) ||
      (sindex == t_index && string[sindex] == '#' && VALID_SPECIAL_LENGTH_PARAM (string[sindex + 1])) ||
      (sindex == t_index - 1 && string[sindex] == '!' && VALID_INDIR_PARAM (string[t_index])))
    {
      t_index++;
      temp1 = string_extract (string, &t_index, "#%:-=?+/@}", 0);
      name = (char *)xrealloc (name, 3 + (strlen (temp1)));
      *name = string[sindex];
      if (string[sindex] == '!')
	{
	   
	  name[1] = string[sindex + 1];
	  strcpy (name + 2, temp1);
	}
      else	
	strcpy (name + 1, temp1);
      free (temp1);
    }
  sindex = t_index;

   
  if (c = string[sindex])
    sindex++;

   
  if (c == ':' && VALID_PARAM_EXPAND_CHAR (string[sindex]))
    {
      check_nullness++;
      if (c = string[sindex])
	sindex++;
    }
  else if (c == ':' && string[sindex] != RBRACE)
    want_substring = 1;
  else if (c == '/'  )	 
    want_patsub = 1;
#if defined (CASEMOD_EXPANSIONS)
  else if (c == '^' || c == ',' || c == '~')
    {
      modspec = c;
      want_casemod = 1;
    }
#endif
  else if (c == '@' && (string[sindex] == 'a' || string[sindex] == 'A') && string[sindex+1] == RBRACE)
    {
       
      want_attributes = 1;
      local_pflags |= PF_ALLINDS;
    }

   
   
  if (name[0] == '#' && name[1] == '\0' && check_nullness == 0 &&
	VALID_SPECIAL_LENGTH_PARAM (c) && string[sindex] == RBRACE)
    {
      name = (char *)xrealloc (name, 3);
      name[1] = c;
      name[2] = '\0';
      c = string[sindex++];
    }

   
  if (name[0] == '#' && name[1] == '\0' && check_nullness == 0 &&
	member (c, "%:=+/") && string[sindex] == RBRACE)
    {
      temp = (char *)NULL;
      goto bad_substitution;	 
    }

   
  want_indir = *name == '!' &&
    (legal_variable_starter ((unsigned char)name[1]) || DIGIT (name[1])
					|| VALID_INDIR_PARAM (name[1]));

   

   
  if (SPECIAL_VAR (name, want_indir))
    var_is_special++;

   
  if (*name == '#' && name[1])
    {
       
      if (string[sindex - 1] != RBRACE || (valid_length_expression (name) == 0))
	{
	  temp = (char *)NULL;
	  goto bad_substitution;	 
	}

      number = parameter_brace_expand_length (name);
      if (number == INTMAX_MIN && unbound_vars_is_error)
	{
	  set_exit_status (EXECUTION_FAILURE);
	  err_unboundvar (name+1);
	  free (name);
	  return (interactive_shell ? &expand_wdesc_error : &expand_wdesc_fatal);
	}
      free (name);

      *indexp = sindex;
      if (number < 0)
        return (&expand_wdesc_error);
      else
	{
	  ret = alloc_word_desc ();
	  ret->word = itos (number);
	  return ret;
	}
    }

   
  if (name[0] == '@' && name[1] == '\0')
    {
      if ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) && quoted_dollar_atp)
	*quoted_dollar_atp = 1;

      if (contains_dollar_at)
	*contains_dollar_at = 1;

      tflag |= W_DOLLARAT;
    }

   
  if (want_indir && string[sindex - 1] == RBRACE &&
      (string[sindex - 2] == '*' || string[sindex - 2] == '@') &&
      legal_variable_starter ((unsigned char) name[1]))
    {
      char **x;
      WORD_LIST *xlist;

      temp1 = savestring (name + 1);
      number = strlen (temp1);
      temp1[number - 1] = '\0';
      x = all_variables_matching_prefix (temp1);
      xlist = strvec_to_word_list (x, 0, 0);
      if (string[sindex - 2] == '*')
	temp = string_list_dollar_star (xlist, quoted, 0);
      else
	{
	  temp = string_list_dollar_at (xlist, quoted, 0);
	  if ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) && quoted_dollar_atp)
	    *quoted_dollar_atp = 1;
	  if (contains_dollar_at)
	    *contains_dollar_at = 1;

	  tflag |= W_DOLLARAT;
	}
      free (x);
      dispose_words (xlist);
      free (temp1);
      *indexp = sindex;

      free (name);

      ret = alloc_word_desc ();
      ret->word = temp;
      ret->flags = tflag;	 
      return ret;
    }

#if defined (ARRAY_VARS)      
   
  if (want_indir && string[sindex - 1] == RBRACE &&
      string[sindex - 2] == RBRACK && valid_array_reference (name+1, 0))
    {
      char *x, *x1;

      temp1 = savestring (name + 1);
      x = array_variable_name (temp1, 0, &x1, (int *)0);
      FREE (x);
      if (ALL_ELEMENT_SUB (x1[0]) && x1[1] == RBRACK)
	{
	  temp = array_keys (temp1, quoted, pflags);	 
	  if (x1[0] == '@')
	    {
	      if ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) && quoted_dollar_atp)
		*quoted_dollar_atp = 1;
	      if (contains_dollar_at)
		*contains_dollar_at = 1;

	      tflag |= W_DOLLARAT;
	    }	    

	  free (name);
	  free (temp1);
	  *indexp = sindex;

	  ret = alloc_word_desc ();
	  ret->word = temp;
	  ret->flags = tflag;	 
	  return ret;
	}

      free (temp1);
    }
#endif  
      
   
  if (valid_brace_expansion_word (want_indir ? name + 1 : name,
					var_is_special) == 0)
    {
      temp = (char *)NULL;
      goto bad_substitution;		 
    }

  if (want_indir)
    {
      tdesc = parameter_brace_expand_indir (name + 1, var_is_special, quoted, pflags|local_pflags, quoted_dollar_atp, contains_dollar_at);
      if (tdesc == &expand_wdesc_error || tdesc == &expand_wdesc_fatal)
	{
	  temp = (char *)NULL;
	  goto bad_substitution;
	}

       
      if (tdesc && tdesc->flags)
	tdesc->flags &= ~W_ARRAYIND;

       
      if (contains_dollar_at && *contains_dollar_at)
	all_element_arrayref = 1;
    }
  else
    {
      local_pflags |= PF_IGNUNBOUND|(pflags&(PF_NOSPLIT2|PF_ASSIGNRHS));
      tdesc = parameter_brace_expand_word (name, var_is_special, quoted, local_pflags, &es);
    }

  if (tdesc == &expand_wdesc_error || tdesc == &expand_wdesc_fatal)
    {
      tflag = 0;
      tdesc = 0;
    }

  if (tdesc)
    {
      temp = tdesc->word;
      tflag = tdesc->flags;
      dispose_word_desc (tdesc);
    }
  else
    temp = (char *)0;

  if (temp == &expand_param_error || temp == &expand_param_fatal)
    {
      FREE (name);
      FREE (value);
      return (temp == &expand_param_error ? &expand_wdesc_error : &expand_wdesc_fatal);
    }

#if defined (ARRAY_VARS)
  if (valid_array_reference (name, 0))
    {
      int qflags;
      char *t;

      qflags = quoted;
       

      if (pflags & PF_ASSIGNRHS)
	qflags |= Q_DOUBLE_QUOTES;
       
      t = mbschr (name, LBRACK);
      if (t && ALL_ELEMENT_SUB (t[1]) && t[2] == RBRACK)
	{
	  all_element_arrayref = 1;
	  if (expand_no_split_dollar_star && t[1] == '*')	 
	    qflags |= Q_DOUBLE_QUOTES;
	}
      chk_atstar (name, qflags, pflags, quoted_dollar_atp, contains_dollar_at);
    }
#endif

  var_is_set = temp != (char *)0;
  var_is_null = check_nullness && (var_is_set == 0 || *temp == 0);
   
  if (check_nullness)
    var_is_null |= var_is_set && var_is_special && (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) && QUOTED_NULL (temp);
#if defined (ARRAY_VARS)
  if (check_nullness)
    var_is_null |= var_is_set && 
		   (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) &&
		   QUOTED_NULL (temp) &&
		   valid_array_reference (name, 0) &&
		   chk_atstar (name, 0, 0, (int *)0, (int *)0);
#endif

   
  if (c && c != RBRACE)
    {
       
      value = extract_dollar_brace_string (string, &sindex, quoted, (c == '%' || c == '#' || c =='/' || c == '^' || c == ',' || c ==':') ? SX_POSIXEXP|SX_WORD : SX_WORD);
      if (string[sindex] == RBRACE)
	sindex++;
      else
	goto bad_substitution;		 
    }
  else
    value = (char *)NULL;

  *indexp = sindex;

   
  if (want_substring || want_patsub || want_casemod || c == '@' || c == '#' || c == '%' || c == RBRACE)
    {
      if (var_is_set == 0 && unbound_vars_is_error && ((name[0] != '@' && name[0] != '*') || name[1]) && all_element_arrayref == 0)
	{
	  set_exit_status (EXECUTION_FAILURE);
	  err_unboundvar (name);
	  FREE (value);
	  FREE (temp);
	  free (name);
	  return (interactive_shell ? &expand_wdesc_error : &expand_wdesc_fatal);
	}
    }
    
   
  if (want_substring)
    {
      temp1 = parameter_brace_substring (name, temp, &es, value, quoted, pflags, (tflag & W_ARRAYIND) ? AV_USEIND : 0);
      FREE (value);
      FREE (temp);
#if defined (ARRAY_VARS)
      flush_eltstate (&es);
#endif

      if (temp1 == &expand_param_error || temp1 == &expand_param_fatal)
        {
          FREE (name);
	  return (temp1 == &expand_param_error ? &expand_wdesc_error : &expand_wdesc_fatal);
        }

      ret = alloc_word_desc ();
      ret->word = temp1;
       
      if (temp1 &&
          (quoted_dollar_atp == 0 || *quoted_dollar_atp == 0) &&
	  QUOTED_NULL (temp1) && (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)))
	ret->flags |= W_QUOTED|W_HASQUOTEDNULL;
      else if (temp1 && (name[0] == '*' && name[1] == 0) && quoted == 0 &&
		(pflags & PF_ASSIGNRHS))
	ret->flags |= W_SPLITSPACE;	 	
       
      else if (temp1 && (name[0] == '*' && name[1] == 0) && quoted == 0 && ifs_is_null)
	ret->flags |= W_SPLITSPACE;	 

      FREE (name);
      return ret;
    }
  else if (want_patsub)
    {
      temp1 = parameter_brace_patsub (name, temp, &es, value, quoted, pflags, (tflag & W_ARRAYIND) ? AV_USEIND : 0);
      FREE (value);
      FREE (temp);
#if defined (ARRAY_VARS)
      flush_eltstate (&es);
#endif

      if (temp1 == &expand_param_error || temp1 == &expand_param_fatal)
        {
          FREE (name);
	  return (temp1 == &expand_param_error ? &expand_wdesc_error : &expand_wdesc_fatal);
        }

      ret = alloc_word_desc ();
      ret->word = temp1;
      if (temp1 && 
          (quoted_dollar_atp == 0 || *quoted_dollar_atp == 0) &&
	  QUOTED_NULL (temp1) && (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)))
	ret->flags |= W_QUOTED|W_HASQUOTEDNULL;
       
      else if (temp1 && (name[0] == '*' && name[1] == 0) && quoted == 0 && ifs_is_null)
	ret->flags |= W_SPLITSPACE;	 

      FREE (name);
      return ret;
    }
#if defined (CASEMOD_EXPANSIONS)
  else if (want_casemod)
    {
      temp1 = parameter_brace_casemod (name, temp, &es, modspec, value, quoted, pflags, (tflag & W_ARRAYIND) ? AV_USEIND : 0);
      FREE (value);
      FREE (temp);
#if defined (ARRAY_VARS)
      flush_eltstate (&es);
#endif

      if (temp1 == &expand_param_error || temp1 == &expand_param_fatal)
        {
          FREE (name);
	  return (temp1 == &expand_param_error ? &expand_wdesc_error : &expand_wdesc_fatal);
        }

      ret = alloc_word_desc ();
      ret->word = temp1;
      if (temp1 &&
          (quoted_dollar_atp == 0 || *quoted_dollar_atp == 0) &&
	  QUOTED_NULL (temp1) && (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)))
	ret->flags |= W_QUOTED|W_HASQUOTEDNULL;
       
      else if (temp1 && (name[0] == '*' && name[1] == 0) && quoted == 0 && ifs_is_null)
	ret->flags |= W_SPLITSPACE;	 

      FREE (name);
      return ret;
    }
#endif

   
  switch (c)
    {
    default:
    case '\0':
bad_substitution:
      set_exit_status (EXECUTION_FAILURE);
      report_error (_("%s: bad substitution"), string ? string : "??");
      FREE (value);
      FREE (temp);
      free (name);
#if defined (ARRAY_VARS)
      flush_eltstate (&es);
#endif
      if (shell_compatibility_level <= 43)
	return &expand_wdesc_error;
      else
	return ((posixly_correct && interactive_shell == 0) ? &expand_wdesc_fatal : &expand_wdesc_error);

    case RBRACE:
      break;

    case '@':
      temp1 = parameter_brace_transform (name, temp, &es, value, c, quoted, pflags, (tflag & W_ARRAYIND) ? AV_USEIND : 0);
      free (temp);
      free (value);
#if defined (ARRAY_VARS)
      flush_eltstate (&es);
#endif

      if (temp1 == &expand_param_error || temp1 == &expand_param_fatal)
	{
	  free (name);
	  set_exit_status (EXECUTION_FAILURE);
	  report_error (_("%s: bad substitution"), string ? string : "??");
	  return (temp1 == &expand_param_error ? &expand_wdesc_error : &expand_wdesc_fatal);
	}

      ret = alloc_word_desc ();
      ret->word = temp1;
      if (temp1 && QUOTED_NULL (temp1) && (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)))
	ret->flags |= W_QUOTED|W_HASQUOTEDNULL;
       
      else if (temp1 && (name[0] == '*' && name[1] == 0) && quoted == 0 && ifs_is_null)
	ret->flags |= W_SPLITSPACE;	 

      free (name);
      return ret;

    case '#':	 
    case '%':	 
      if (value == 0 || *value == '\0' || temp == 0 || *temp == '\0')
	{
	  FREE (value);
	  break;
	}
      temp1 = parameter_brace_remove_pattern (name, temp, &es, value, c, quoted, (tflag & W_ARRAYIND) ? AV_USEIND : 0);
      free (temp);
      free (value);
#if defined (ARRAY_VARS)
      flush_eltstate (&es);
#endif

      ret = alloc_word_desc ();
      ret->word = temp1;
      if (temp1 && QUOTED_NULL (temp1) && (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)))
	ret->flags |= W_QUOTED|W_HASQUOTEDNULL;
       
      else if (temp1 && (name[0] == '*' && name[1] == 0) && quoted == 0 && ifs_is_null)
	ret->flags |= W_SPLITSPACE;	 

      free (name);
      return ret;

    case '-':
    case '=':
    case '?':
    case '+':
      if (var_is_set && var_is_null == 0)
	{
	   
	  if (c == '+')
	    {
	       
	      if ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) && quoted_dollar_atp)
		*quoted_dollar_atp = 0;
	      if (contains_dollar_at)
		*contains_dollar_at = 0;

	      FREE (temp);
	      if (value)
		{
		   
		  if (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES))
		    quoted |= Q_DOLBRACE;
		  ret = parameter_brace_expand_rhs (name, value, c,
						    quoted,
						    pflags,
						    quoted_dollar_atp,
						    contains_dollar_at);
		   
		  free (value);
		}
	      else
		temp = (char *)NULL;
	    }
	  else
	    {
	      FREE (value);
	    }
	   
	}
      else	 
	{
	   
	  if (c == '+' && temp && QUOTED_NULL (temp) &&
	      (quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT)))
	    tflag |= W_HASQUOTEDNULL;

	  FREE (temp);
	  temp = (char *)NULL;
	  if (c == '=' && var_is_special)
	    {
	      set_exit_status (EXECUTION_FAILURE);
	      report_error (_("$%s: cannot assign in this way"), name);
	      free (name);
	      free (value);
#if defined (ARRAY_VARS)
	      flush_eltstate (&es);
#endif
	      return &expand_wdesc_error;
	    }
	  else if (c == '?')
	    {
	      parameter_brace_expand_error (name, value, check_nullness);
#if defined (ARRAY_VARS)
	      flush_eltstate (&es);
#endif
	      return (interactive_shell ? &expand_wdesc_error : &expand_wdesc_fatal);
	    }
	  else if (c != '+')
	    {
	       
	      if ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) && quoted_dollar_atp)
		*quoted_dollar_atp = 0;
	      if (contains_dollar_at)
		*contains_dollar_at = 0;

	       
	      if (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES))
		quoted |= Q_DOLBRACE;
	      ret = parameter_brace_expand_rhs (name, value, c, quoted, pflags,
						quoted_dollar_atp,
						contains_dollar_at);
	       
	    }
	  free (value);
	}

      break;
    }
  free (name);
#if defined (ARRAY_VARS)
  flush_eltstate (&es);
#endif

  if (ret == 0)
    {
      ret = alloc_word_desc ();
      ret->flags = tflag;
      ret->word = temp;
    }
  return (ret);
}

 
static WORD_DESC *
param_expand (string, sindex, quoted, expanded_something,
	      contains_dollar_at, quoted_dollar_at_p, had_quoted_null_p,
	      pflags)
     char *string;
     int *sindex, quoted, *expanded_something, *contains_dollar_at;
     int *quoted_dollar_at_p, *had_quoted_null_p, pflags;
{
  char *temp, *temp1, uerror[3], *savecmd;
  int zindex, t_index, expok, eflag;
  unsigned char c;
  intmax_t number;
  SHELL_VAR *var;
  WORD_LIST *list, *l;
  WORD_DESC *tdesc, *ret;
  int tflag, nullarg;

 
  zindex = *sindex;
  c = string[++zindex];

  temp = (char *)NULL;
  ret = tdesc = (WORD_DESC *)NULL;
  tflag = 0;

   
  switch (c)
    {
     
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      temp1 = dollar_vars[TODIGIT (c)];
       
      if (unbound_vars_is_error && temp1 == (char *)NULL)
	{
	  uerror[0] = '$';
	  uerror[1] = c;
	  uerror[2] = '\0';
	  set_exit_status (EXECUTION_FAILURE);
	  err_unboundvar (uerror);
	  return (interactive_shell ? &expand_wdesc_error : &expand_wdesc_fatal);
	}
      if (temp1)
	temp = (*temp1 && (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)))
		  ? quote_string (temp1)
		  : quote_escapes (temp1);
      else
	temp = (char *)NULL;

      break;

     
    case '$':
      temp = itos (dollar_dollar_pid);
      break;

     
    case '#':
      temp = itos (number_of_args ());
      break;

     
    case '?':
      temp = itos (last_command_exit_value);
      break;

     
    case '-':
      temp = which_set_flags ();
      break;

       
    case '!':
       
      if (last_asynchronous_pid == NO_PID)
	{
	  if (expanded_something)
	    *expanded_something = 0;
	  temp = (char *)NULL;
	  if (unbound_vars_is_error && (pflags & PF_IGNUNBOUND) == 0)
	    {
	      uerror[0] = '$';
	      uerror[1] = c;
	      uerror[2] = '\0';
	      set_exit_status (EXECUTION_FAILURE);
	      err_unboundvar (uerror);
	      return (interactive_shell ? &expand_wdesc_error : &expand_wdesc_fatal);
	    }
	}
      else
	temp = itos (last_asynchronous_pid);
      break;

     
    case '*':		 
      list = list_rest_of_args ();

#if 0
       

      if (list == 0 && unbound_vars_is_error && (pflags & PF_IGNUNBOUND) == 0)
	{
	  uerror[0] = '$';
	  uerror[1] = '*';
	  uerror[2] = '\0';
	  set_exit_status (EXECUTION_FAILURE);
	  err_unboundvar (uerror);
	  return (interactive_shell ? &expand_wdesc_error : &expand_wdesc_fatal);
	}
#endif

       
      if ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) && list == 0)
	temp = (char *)NULL;
      else if (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES|Q_PATQUOTE))
	{
	   
	  temp = (quoted & (Q_DOUBLE_QUOTES|Q_PATQUOTE)) ? string_list_dollar_star (list, quoted, 0) : string_list (list);
	  if (temp)
	    {
	      temp1 = (quoted & Q_DOUBLE_QUOTES) ? quote_string (temp) : temp;
	      if (*temp == 0)
		tflag |= W_HASQUOTEDNULL;
	      if (temp != temp1)
		free (temp);
	      temp = temp1;
	    }
	}
      else
	{
	   
	  if (expand_no_split_dollar_star && quoted == 0 && ifs_is_set == 0 && (pflags & PF_ASSIGNRHS))
	    {
	       
	      temp1 = string_list_dollar_star (list, quoted, pflags);
	      temp = temp1 ? quote_string (temp1) : temp1;
	       
	      if (temp1 && *temp1 == 0 && QUOTED_NULL (temp))
		tflag |= W_SAWQUOTEDNULL;
	      FREE (temp1);
	    }
	  else if (expand_no_split_dollar_star && quoted == 0 && ifs_is_null && (pflags & PF_ASSIGNRHS))
	    {
	       
	      temp1 = string_list_dollar_star (list, quoted, pflags);
	      temp = temp1 ? quote_escapes (temp1) : temp1;
	      FREE (temp1);
	    }
	  else if (expand_no_split_dollar_star && quoted == 0 && ifs_is_set && ifs_is_null == 0 && (pflags & PF_ASSIGNRHS))
	    {
	       
	      temp1 = string_list_dollar_star (list, quoted, pflags);
	      temp = temp1 ? quote_string (temp1) : temp1;

	       
	      if (temp1 && *temp1 == 0 && QUOTED_NULL (temp))
		tflag |= W_SAWQUOTEDNULL;
	      FREE (temp1);
	    }
	   
#  if defined (HANDLE_MULTIBYTE)
	  else if (expand_no_split_dollar_star && ifs_firstc[0] == 0)
#  else
	  else if (expand_no_split_dollar_star && ifs_firstc == 0)
#  endif
	     
	    temp = string_list_dollar_star (list, quoted, 0);
	  else
	    {
	      temp = string_list_dollar_at (list, quoted, 0);
	       
#if 0
	      if (quoted == 0 && (ifs_is_set == 0 || ifs_is_null))
#else	 
	      if (quoted == 0 && ifs_is_null)
#endif
		tflag |= W_SPLITSPACE;
	       
	      else if (temp && quoted == 0 && ifs_is_set && (pflags & PF_ASSIGNRHS))
		{
		  temp1 = quote_string (temp);
		  free (temp);
		  temp = temp1;
		}
	    }

	  if (expand_no_split_dollar_star == 0 && contains_dollar_at)
	    *contains_dollar_at = 1;
	}

      dispose_words (list);
      break;

     
    case '@':		 
      list = list_rest_of_args ();

#if 0
       

      if (list == 0 && unbound_vars_is_error && (pflags & PF_IGNUNBOUND) == 0)
	{
	  uerror[0] = '$';
	  uerror[1] = '@';
	  uerror[2] = '\0';
	  set_exit_status (EXECUTION_FAILURE);
	  err_unboundvar (uerror);
	  return (interactive_shell ? &expand_wdesc_error : &expand_wdesc_fatal);
	}
#endif

      for (nullarg = 0, l = list; l; l = l->next)
	{
	  if (l->word && (l->word->word == 0 || l->word->word[0] == 0))
	    nullarg = 1;
	}

       
       
      if (quoted_dollar_at_p && (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)))
	*quoted_dollar_at_p = 1;
      if (contains_dollar_at)
	*contains_dollar_at = 1;

       
       

       
      if (pflags & PF_ASSIGNRHS)
	{
	  temp = string_list_dollar_at (list, (quoted|Q_DOUBLE_QUOTES), pflags);
	  if (nullarg)
	    tflag |= W_HASQUOTEDNULL;	 
	}

       
      else if (pflags & PF_NOSPLIT2)
        {
#if defined (HANDLE_MULTIBYTE)
	  if (quoted == 0 && ifs_is_set && ifs_is_null == 0 && ifs_firstc[0] != ' ')
#else
	  if (quoted == 0 && ifs_is_set && ifs_is_null == 0 && ifs_firstc != ' ')
#endif
	     
	    temp = string_list_dollar_at (list, Q_DOUBLE_QUOTES, pflags);
	  else
	    temp = string_list_dollar_at (list, quoted, pflags);
	}
      else
	temp = string_list_dollar_at (list, quoted, pflags);

      tflag |= W_DOLLARAT;
      dispose_words (list);
      break;

    case LBRACE:
      tdesc = parameter_brace_expand (string, &zindex, quoted, pflags,
				      quoted_dollar_at_p,
				      contains_dollar_at);

      if (tdesc == &expand_wdesc_error || tdesc == &expand_wdesc_fatal)
	return (tdesc);
      temp = tdesc ? tdesc->word : (char *)0;

       
       
       
      if (tdesc && tdesc->word && (tdesc->flags & W_HASQUOTEDNULL) && QUOTED_NULL (temp))
	{
	  if (had_quoted_null_p)
	    *had_quoted_null_p = 1;
	  if (*quoted_dollar_at_p == 0)
	    {
	      free (temp);
	      tdesc->word = temp = (char *)NULL;
	    }
	    
	}

      ret = tdesc;
      goto return0;

     
    case LPAREN:
       
      t_index = zindex + 1;
       
      temp = extract_command_subst (string, &t_index, (pflags&PF_COMPLETE) ? SX_COMPLETE : 0);
      zindex = t_index;

       
      if (temp && *temp == LPAREN)
	{
	  char *temp2;
	  temp1 = temp + 1;
	  temp2 = savestring (temp1);
	  t_index = strlen (temp2) - 1;

	  if (temp2[t_index] != RPAREN)
	    {
	      free (temp2);
	      goto comsub;
	    }

	   
	  temp2[t_index] = '\0';

	  if (chk_arithsub (temp2, t_index) == 0)
	    {
	      free (temp2);
#if 0
	      internal_warning (_("future versions of the shell will force evaluation as an arithmetic substitution"));
#endif
	      goto comsub;
	    }

	   
	  temp1 = expand_arith_string (temp2, Q_DOUBLE_QUOTES|Q_ARITH);
	  free (temp2);

arithsub:
	   
	  savecmd = this_command_name;
	  this_command_name = (char *)NULL;

	  eflag = (shell_compatibility_level > 51) ? 0 : EXP_EXPANDED;
	  number = evalexp (temp1, eflag, &expok);
	  this_command_name = savecmd;
	  free (temp);
	  free (temp1);
	  if (expok == 0)
	    {
	      if (interactive_shell == 0 && posixly_correct)
		{
		  set_exit_status (EXECUTION_FAILURE);
		  return (&expand_wdesc_fatal);
		}
	      else
		return (&expand_wdesc_error);
	    }
	  temp = itos (number);
	  break;
	}

comsub:
      if (pflags & PF_NOCOMSUB)
	 
	temp1 = substring (string, *sindex, zindex+1);
      else
	{
	  tdesc = command_substitute (temp, quoted, pflags&PF_ASSIGNRHS);
	  temp1 = tdesc ? tdesc->word : (char *)NULL;
	  if (tdesc)
	    dispose_word_desc (tdesc);
	}
      FREE (temp);
      temp = temp1;
      break;

     
    case '[':		 
       
      t_index = zindex + 1;
      temp = extract_arithmetic_subst (string, &t_index);
      zindex = t_index;
      if (temp == 0)
	{
	  temp = savestring (string);
	  if (expanded_something)
	    *expanded_something = 0;
	  goto return0;
	}	  

        
      temp1 = expand_arith_string (temp, Q_DOUBLE_QUOTES|Q_ARITH);

      goto arithsub;

    default:
       
      temp = (char *)NULL;

      for (t_index = zindex; (c = string[zindex]) && legal_variable_char (c); zindex++)
	;
      temp1 = (zindex > t_index) ? substring (string, t_index, zindex) : (char *)NULL;

       
      if (temp1 == 0 || *temp1 == '\0')
	{
	  FREE (temp1);
	  temp = (char *)xmalloc (2);
	  temp[0] = '$';
	  temp[1] = '\0';
	  if (expanded_something)
	    *expanded_something = 0;
	  goto return0;
	}

       
      var = find_variable (temp1);

      if (var && invisible_p (var) == 0 && var_isset (var))
	{
#if defined (ARRAY_VARS)
	  if (assoc_p (var) || array_p (var))
	    {
	      temp = array_p (var) ? array_reference (array_cell (var), 0)
				   : assoc_reference (assoc_cell (var), "0");
	      if (temp)
		temp = (*temp && (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)))
			  ? quote_string (temp)
			  : quote_escapes (temp);
	      else if (unbound_vars_is_error)
		goto unbound_variable;
	    }
	  else
#endif
	    {
	      temp = value_cell (var);

	      temp = (*temp && (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)))
			? quote_string (temp)
			: ((pflags & PF_ASSIGNRHS) ? quote_rhs (temp)
						   : quote_escapes (temp));
	    }

	  free (temp1);

	  goto return0;
	}
      else if (var && (invisible_p (var) || var_isset (var) == 0))
	temp = (char *)NULL;
      else if ((var = find_variable_last_nameref (temp1, 0)) && var_isset (var) && invisible_p (var) == 0)
	{
	  temp = nameref_cell (var);
#if defined (ARRAY_VARS)
	  if (temp && *temp && valid_array_reference (temp, 0))
	    {
	      chk_atstar (temp, quoted, pflags, quoted_dollar_at_p, contains_dollar_at);
	      tdesc = parameter_brace_expand_word (temp, SPECIAL_VAR (temp, 0), quoted, pflags, 0);
	      if (tdesc == &expand_wdesc_error || tdesc == &expand_wdesc_fatal)
		return (tdesc);
	      ret = tdesc;
	      goto return0;
	    }
	  else
#endif
	   
	  if (temp && *temp && legal_identifier (temp) == 0)
	    {
	      set_exit_status (EXECUTION_FAILURE);
	      report_error (_("%s: invalid variable name for name reference"), temp);
	      return (&expand_wdesc_error);	 
	    }
	  else
	    temp = (char *)NULL;
	}

      temp = (char *)NULL;

unbound_variable:
      if (unbound_vars_is_error)
	{
	  set_exit_status (EXECUTION_FAILURE);
	  err_unboundvar (temp1);
	}
      else
	{
	  free (temp1);
	  goto return0;
	}

      free (temp1);
      set_exit_status (EXECUTION_FAILURE);
      return ((unbound_vars_is_error && interactive_shell == 0)
		? &expand_wdesc_fatal
		: &expand_wdesc_error);
    }

  if (string[zindex])
    zindex++;

return0:
  *sindex = zindex;

  if (ret == 0)
    {
      ret = alloc_word_desc ();
      ret->flags = tflag;	 
      ret->word = temp;
    }
  return ret;
}

#if defined (ARRAY_VARS)
 
static char abstab[256] = { '\1' };

 
char *
expand_subscript_string (string, quoted)
     char *string;
     int quoted;
{
  WORD_DESC td;
  WORD_LIST *tlist;
  int oe;
  char *ret;

  if (string == 0 || *string == 0)
    return (char *)NULL;

  oe = expand_no_split_dollar_star;
  ret = (char *)NULL;

  td.flags = W_NOPROCSUB|W_NOTILDE|W_NOSPLIT2;	 
  td.word = savestring (string);		 

  expand_no_split_dollar_star = 1;
  tlist = call_expand_word_internal (&td, quoted, 0, (int *)NULL, (int *)NULL);
  expand_no_split_dollar_star = oe;

  if (tlist)
    {
      if (tlist->word)
	{
	  remove_quoted_nulls (tlist->word->word);
	  tlist->word->flags &= ~W_HASQUOTEDNULL;
	}
      dequote_list (tlist);
      ret = string_list (tlist);
      dispose_words (tlist);
    }

  free (td.word);
  return (ret);
}

 
static char *
expand_array_subscript (string, sindex, quoted, flags)
     char *string;
     int *sindex;
     int quoted, flags;
{
  char *ret, *exp, *t;
  size_t slen;
  int si, ni;

  si = *sindex;
  slen = STRLEN (string);

  if (abstab[0] == '\1')
    {
       
      memset (abstab, '\0', sizeof (abstab));
      abstab[LBRACK] = abstab[RBRACK] = 1;
      abstab['$'] = abstab['`'] = abstab['~'] = 1;
      abstab['\\'] = abstab['\''] = 1;
      abstab['"'] = 1;	 
       
    }

   
  ni = skipsubscript (string, si, 0);
   
  if (ni >= slen || string[ni] != RBRACK || (ni - si) == 1 ||
      (string[ni+1] != '\0' && (quoted & Q_ARITH) == 0))
    {
       
      INTERNAL_DEBUG (("expand_array_subscript: bad subscript string: `%s'", string+si));
      ret = (char *)xmalloc (2);	 
      ret[0] = string[si];
      ret[1] = '\0';
      *sindex = si + 1;
      return ret;
    }

   
  exp = substring (string, si+1, ni);
  t = expand_subscript_string (exp, quoted & ~(Q_ARITH|Q_DOUBLE_QUOTES));
  free (exp);
  exp = t ? sh_backslash_quote (t, abstab, 0) : savestring ("");
  free (t);

  slen = STRLEN (exp);
  ret = xmalloc (slen + 2 + 1);
  ret[0] ='[';
  strcpy (ret + 1, exp);
  ret[slen + 1] = ']';
  ret[slen + 2] = '\0';

  free (exp);
  *sindex = ni + 1;

  return ret;
}
#endif

void
invalidate_cached_quoted_dollar_at ()
{
  dispose_words (cached_quoted_dollar_at);
  cached_quoted_dollar_at = 0;
}

 

 
#define UNQUOTED	 0
#define PARTIALLY_QUOTED 1
#define WHOLLY_QUOTED    2

static WORD_LIST *
expand_word_internal (word, quoted, isexp, contains_dollar_at, expanded_something)
     WORD_DESC *word;
     int quoted, isexp;
     int *contains_dollar_at;
     int *expanded_something;
{
  WORD_LIST *list;
  WORD_DESC *tword;

   
  char *istring;

   
  size_t istring_size;

   
  size_t istring_index;

   
  char *temp, *temp1;

   
  register char *string;

   
  size_t string_size;

   
  int sindex;

   
  int quoted_dollar_at;

   
  int quoted_state;

   
  int had_quoted_null;
  int has_quoted_ifs;		 
  int has_dollar_at, temp_has_dollar_at;
  int internal_tilde;
  int split_on_spaces;
  int local_expanded;
  int tflag;
  int pflags;			 
  int mb_cur_max;

  int assignoff;		 

  register unsigned char c;	 
  int t_index;			 

  char twochars[2];

  DECLARE_MBSTATE;

   
  if (STREQ (word->word, "\"$@\"") &&
      (word->flags == (W_HASDOLLAR|W_QUOTED)) &&
      dollar_vars[1])		 
    {
      if (contains_dollar_at)
	*contains_dollar_at = 1;
      if (expanded_something)
	*expanded_something = 1;
      if (cached_quoted_dollar_at)
	return (copy_word_list (cached_quoted_dollar_at));
      list = list_rest_of_args ();
      list = quote_list (list);
      cached_quoted_dollar_at = copy_word_list (list);
      return (list);
    }

  istring = (char *)xmalloc (istring_size = DEFAULT_INITIAL_ARRAY_SIZE);
  istring[istring_index = 0] = '\0';
  quoted_dollar_at = had_quoted_null = has_dollar_at = 0;
  has_quoted_ifs = 0;
  split_on_spaces = 0;
  internal_tilde = 0;		 
  quoted_state = UNQUOTED;

  string = word->word;
  if (string == 0)
    goto finished_with_string;
  mb_cur_max = MB_CUR_MAX;

   
  string_size = (mb_cur_max > 1) ? strlen (string) : 1;

  if (contains_dollar_at)
    *contains_dollar_at = 0;

  assignoff = -1;

   

  for (sindex = 0; ;)
    {
      c = string[sindex];

       
      switch (c)
	{
	case '\0':
	  goto finished_with_string;

	case CTLESC:
	  sindex++;
#if HANDLE_MULTIBYTE
	  if (mb_cur_max > 1 && string[sindex])
	    {
	      SADD_MBQCHAR_BODY(temp, string, sindex, string_size);
	    }
	  else
#endif
	    {
	      temp = (char *)xmalloc (3);
	      temp[0] = CTLESC;
	      temp[1] = c = string[sindex];
	      temp[2] = '\0';
	    }

dollar_add_string:
	  if (string[sindex])
	    sindex++;

add_string:
	  if (temp)
	    {
	      istring = sub_append_string (temp, istring, &istring_index, &istring_size);
	      temp = (char *)0;
	    }

	  break;

#if defined (PROCESS_SUBSTITUTION)
	   
	case '<':
	case '>':
	  {
	        
	    if (string[++sindex] != LPAREN || (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) || (word->flags & W_NOPROCSUB))
	      {
		sindex--;	 
		goto add_character;
	      }
	    else
	      t_index = sindex + 1;  

	    temp1 = extract_process_subst (string, (c == '<') ? "<(" : ">(", &t_index, 0);  
	    sindex = t_index;

	     
	    temp = temp1 ? process_substitute (temp1, (c == '>')) : (char *)0;

	    FREE (temp1);

	    goto dollar_add_string;
	  }
#endif  

#if defined (ARRAY_VARS)
	case '[':		 
	  if ((quoted & Q_ARITH) == 0 || shell_compatibility_level <= 51)
	    {
	      if (isexp == 0 && (word->flags & (W_NOSPLIT|W_NOSPLIT2)) == 0 && isifs (c) && (quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT)) == 0)
		goto add_ifs_character;
	      else
		goto add_character;
	    }
	  else
	    {
	      temp = expand_array_subscript (string, &sindex, quoted, word->flags);
	      goto add_string;
	    }
#endif

	case '=':
	   	
	  if (word->flags & (W_ASSIGNRHS|W_NOTILDE))
	    {
	      if (isexp == 0 && (word->flags & (W_NOSPLIT|W_NOSPLIT2)) == 0 && isifs (c))
		goto add_ifs_character;
	      else
		goto add_character;
	    }
	   
	  if ((word->flags & W_ASSIGNMENT) &&
	      (posixly_correct == 0 || (word->flags & W_TILDEEXP)) &&
	      assignoff == -1 && sindex > 0)
	    assignoff = sindex;
	  if (sindex == assignoff && string[sindex+1] == '~')	 
	    internal_tilde = 1;

	  if (word->flags & W_ASSIGNARG)
	    word->flags |= W_ASSIGNRHS;		 

	  if (isexp == 0 && (word->flags & (W_NOSPLIT|W_NOSPLIT2)) == 0 && isifs (c))
	    {
	      has_quoted_ifs++;
	      goto add_ifs_character;
	    }
	  else
	    goto add_character;

	case ':':
	  if (word->flags & (W_NOTILDE|W_NOASSNTILDE))
	    {
	      if (isexp == 0 && (word->flags & (W_NOSPLIT|W_NOSPLIT2)) == 0 && isifs (c))
		goto add_ifs_character;
	      else
		goto add_character;
	    }

	  if ((word->flags & (W_ASSIGNMENT|W_ASSIGNRHS)) &&
	      (posixly_correct == 0 || (word->flags & W_TILDEEXP)) &&
	      string[sindex+1] == '~')
	    internal_tilde = 1;

	  if (isexp == 0 && (word->flags & (W_NOSPLIT|W_NOSPLIT2)) == 0 && isifs (c))
	    goto add_ifs_character;
	  else
	    goto add_character;

	case '~':
	   

	  if ((word->flags & W_NOTILDE) ||
	      (sindex > 0 && (internal_tilde == 0)) ||
	      (quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT)))
	    {
	      internal_tilde = 0;
	      if (isexp == 0 && (word->flags & (W_NOSPLIT|W_NOSPLIT2)) == 0 && isifs (c) && (quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT)) == 0)
		goto add_ifs_character;
	      else
		goto add_character;
	    }

	  if (word->flags & W_ASSIGNRHS)
	    tflag = 2;
	  else if (word->flags & (W_ASSIGNMENT|W_TILDEEXP))
	    tflag = 1;
	  else
	    tflag = 0;

	  temp = bash_tilde_find_word (string + sindex, tflag, &t_index);
	    
	  internal_tilde = 0;

	  if (temp && *temp && t_index > 0)
	    {
	      temp1 = bash_tilde_expand (temp, tflag);
	      if  (temp1 && *temp1 == '~' && STREQ (temp, temp1))
		{
		  FREE (temp);
		  FREE (temp1);
		  goto add_character;		 
		}
	      free (temp);
	      temp = temp1;
	      sindex += t_index;
	      goto add_quoted_string;		 
	    }
	  else
	    {
	      FREE (temp);
	      goto add_character;
	    }
	
	case '$':
	  if (expanded_something)
	    *expanded_something = 1;
	  local_expanded = 1;

	  temp_has_dollar_at = 0;
	  pflags = (word->flags & W_NOCOMSUB) ? PF_NOCOMSUB : 0;
	  if (word->flags & W_NOSPLIT2)
	    pflags |= PF_NOSPLIT2;
	  if (word->flags & W_ASSIGNRHS)
	    pflags |= PF_ASSIGNRHS;
	  if (word->flags & W_COMPLETE)
	    pflags |= PF_COMPLETE;

	  tword = param_expand (string, &sindex, quoted, expanded_something,
			       &temp_has_dollar_at, &quoted_dollar_at,
			       &had_quoted_null, pflags);
	  has_dollar_at += temp_has_dollar_at;
	  split_on_spaces += (tword->flags & W_SPLITSPACE);

	  if (tword == &expand_wdesc_error || tword == &expand_wdesc_fatal)
	    {
	      free (string);
	      free (istring);
	      return ((tword == &expand_wdesc_error) ? &expand_word_error
						     : &expand_word_fatal);
	    }
	  if (contains_dollar_at && has_dollar_at)
	    *contains_dollar_at = 1;

	  if (tword && (tword->flags & W_HASQUOTEDNULL))
	    had_quoted_null = 1;		 
	  if (tword && (tword->flags & W_SAWQUOTEDNULL))
	    had_quoted_null = 1;		 

	  temp = tword ? tword->word : (char *)NULL;
	  dispose_word_desc (tword);

	   
	  if (had_quoted_null && temp && QUOTED_NULL (temp))
	    {
	      FREE (temp);
	      temp = (char *)NULL;
	    }

	  goto add_string;
	  break;

	case '`':		 
	  {
	    t_index = sindex++;

	    temp = string_extract (string, &sindex, "`", (word->flags & W_COMPLETE) ? SX_COMPLETE : SX_REQMATCH);
	     
	    if (temp == &extract_string_error || temp == &extract_string_fatal)
	      {
		if (sindex - 1 == t_index)
		  {
		    sindex = t_index;
		    goto add_character;
		  }
		set_exit_status (EXECUTION_FAILURE);
		report_error (_("bad substitution: no closing \"`\" in %s") , string+t_index);
		free (string);
		free (istring);
		return ((temp == &extract_string_error) ? &expand_word_error
							: &expand_word_fatal);
	      }
		
	    if (expanded_something)
	      *expanded_something = 1;
	    local_expanded = 1;

	    if (word->flags & W_NOCOMSUB)
	       
	      temp1 = substring (string, t_index, sindex + 1);
	    else
	      {
		de_backslash (temp);
		tword = command_substitute (temp, quoted, PF_BACKQUOTE);
		temp1 = tword ? tword->word : (char *)NULL;
		if (tword)
		  dispose_word_desc (tword);
	      }
	    FREE (temp);
	    temp = temp1;
	    goto dollar_add_string;
	  }

	case '\\':
	  if (string[sindex + 1] == '\n')
	    {
	      sindex += 2;
	      continue;
	    }

	  c = string[++sindex];

	   
	  if ((quoted & Q_HERE_DOCUMENT) && (quoted & Q_DOLBRACE) && c == '"')
	    tflag = CBSDQUOTE;		 
	  else if (quoted & Q_HERE_DOCUMENT)
	    tflag = CBSHDOC;
	  else if (quoted & Q_DOUBLE_QUOTES)
	    tflag = CBSDQUOTE;
	  else
	    tflag = 0;

	   
	  if ((quoted & Q_DOLBRACE) && c == RBRACE)
	    {
	      SCOPY_CHAR_I (twochars, CTLESC, c, string, sindex, string_size);
	    }
	   
	  else if ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) && ((sh_syntaxtab[c] & tflag) == 0) && isexp == 0 && isifs (c))
	    {
	      RESIZE_MALLOCED_BUFFER (istring, istring_index, 2, istring_size,
				      DEFAULT_ARRAY_SIZE);
	      istring[istring_index++] = CTLESC;
	      istring[istring_index++] = '\\';
	      istring[istring_index] = '\0';

	      SCOPY_CHAR_I (twochars, CTLESC, c, string, sindex, string_size);
	    }
	  else if ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) && c == 0)
	    {
	      RESIZE_MALLOCED_BUFFER (istring, istring_index, 2, istring_size,
				      DEFAULT_ARRAY_SIZE);
	      istring[istring_index++] = CTLESC;
	      istring[istring_index++] = '\\';
	      istring[istring_index] = '\0';
	      break;	      
	    }
	  else if ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) && ((sh_syntaxtab[c] & tflag) == 0))
	    {
	      SCOPY_CHAR_I (twochars, '\\', c, string, sindex, string_size);
	    }
	  else if (c == 0)
	    {
	      c = CTLNUL;
	      sindex--;		 
	      goto add_character;
	    }
	  else
	    {
	      SCOPY_CHAR_I (twochars, CTLESC, c, string, sindex, string_size);
	    }

	  sindex++;
add_twochars:
	   
	  RESIZE_MALLOCED_BUFFER (istring, istring_index, 2, istring_size,
				  DEFAULT_ARRAY_SIZE);
	  istring[istring_index++] = twochars[0];
	  istring[istring_index++] = twochars[1];
	  istring[istring_index] = '\0';

	  break;

	case '"':
	   
	  if ((quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT)) && ((quoted & Q_ARITH) == 0))
	    goto add_character;

	  t_index = ++sindex;
	  temp = string_extract_double_quoted (string, &sindex, (word->flags & W_COMPLETE) ? SX_COMPLETE : 0);

	   
	  quoted_state = (t_index == 1 && string[sindex] == '\0')
			    ? WHOLLY_QUOTED
			    : PARTIALLY_QUOTED;

	  if (temp && *temp)
	    {
	      tword = alloc_word_desc ();
	      tword->word = temp;

	      if (word->flags & W_ASSIGNARG)
		tword->flags |= word->flags & (W_ASSIGNARG|W_ASSIGNRHS);  
	      if (word->flags & W_COMPLETE)
		tword->flags |= W_COMPLETE;	 
	      if (word->flags & W_NOCOMSUB)
		tword->flags |= W_NOCOMSUB;
	      if (word->flags & W_NOPROCSUB)
		tword->flags |= W_NOPROCSUB;

	      if (word->flags & W_ASSIGNRHS)
		tword->flags |= W_ASSIGNRHS;

	      temp = (char *)NULL;

	      temp_has_dollar_at = 0;	 
	       
	       
	      list = expand_word_internal (tword, Q_DOUBLE_QUOTES|(quoted&Q_ARITH), 0, &temp_has_dollar_at, (int *)NULL);
	      has_dollar_at += temp_has_dollar_at;

	      if (list == &expand_word_error || list == &expand_word_fatal)
		{
		  free (istring);
		  free (string);
		   
		  tword->word = (char *)NULL;
		  dispose_word (tword);
		  return list;
		}

	      dispose_word (tword);

	       
	      if (list == 0 && temp_has_dollar_at)	 
		{
		  quoted_dollar_at++;
		  break;
		}

	       
	      if (list && list->word && list->next == 0 && (list->word->flags & W_HASQUOTEDNULL))
		{
		  if (had_quoted_null && temp_has_dollar_at)
		    quoted_dollar_at++;
		  had_quoted_null = 1;		 
		}

	       
	      if (list)
		dequote_list (list);

	      if (temp_has_dollar_at)		 
		{
		  quoted_dollar_at++;
		  if (contains_dollar_at)
		    *contains_dollar_at = 1;
		  if (expanded_something)
		    *expanded_something = 1;
		  local_expanded = 1;
		}
	    }
	  else
	    {
	       
	      FREE (temp);
	      list = (WORD_LIST *)NULL;
	      had_quoted_null = 1;	 
	    }

	   
	  if (list)
	    {
	      if (list->next)
		{
		   
		  temp = quoted_dollar_at
				? string_list_dollar_at (list, Q_DOUBLE_QUOTES, 0)
				: string_list (quote_list (list));
		  dispose_words (list);
		  goto add_string;
		}
	      else
		{
		  temp = savestring (list->word->word);
		  tflag = list->word->flags;
		  dispose_words (list);

		   
		   
		  if ((tflag & W_HASQUOTEDNULL) && QUOTED_NULL (temp) == 0)
		    remove_quoted_nulls (temp);	 
		}
	    }
	  else
	    temp = (char *)NULL;

	  if (temp == 0 && quoted_state == PARTIALLY_QUOTED)
	    had_quoted_null = 1;	 

	   
	  if (temp == 0 && quoted_state == PARTIALLY_QUOTED && quoted == 0 && (word->flags & (W_NOSPLIT|W_EXPANDRHS|W_ASSIGNRHS)) == W_EXPANDRHS)
	    {
	      c = CTLNUL;
	      sindex--;
	      had_quoted_null = 1;
	      goto add_character;
	    }
	  if (temp == 0 && quoted_state == PARTIALLY_QUOTED && (word->flags & (W_NOSPLIT|W_NOSPLIT2)))
	    continue;

	add_quoted_string:

	  if (temp)
	    {
	      temp1 = temp;
	      temp = quote_string (temp);
	      free (temp1);
	      goto add_string;
	    }
	  else
	    {
	       
	      c = CTLNUL;
	      sindex--;		 
	      had_quoted_null = 1;	 
	      goto add_character;
	    }

	   

	case '\'':
	  if ((quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT)))
	    goto add_character;

	  t_index = ++sindex;
	  temp = string_extract_single_quoted (string, &sindex, 0);

	   
	  quoted_state = (t_index == 1 && string[sindex] == '\0')
			    ? WHOLLY_QUOTED
			    : PARTIALLY_QUOTED;

	   
	  if (*temp == '\0')
	    {
	      free (temp);
	      temp = (char *)NULL;
	    }
	  else
	    remove_quoted_escapes (temp);	 

	  if (temp == 0 && quoted_state == PARTIALLY_QUOTED)
	    had_quoted_null = 1;	 

	   
	  if (temp == 0 && quoted_state == PARTIALLY_QUOTED && quoted == 0 && (word->flags & (W_NOSPLIT|W_EXPANDRHS|W_ASSIGNRHS)) == W_EXPANDRHS)
	    {
	      c = CTLNUL;
	      sindex--;
	      goto add_character;
	    }

	  if (temp == 0 && (quoted_state == PARTIALLY_QUOTED) && (word->flags & (W_NOSPLIT|W_NOSPLIT2)))
	    continue;

	   
	  if (temp == 0)
	    {
	      c = CTLNUL;
	      sindex--;		 
	      goto add_character;
	    }
	  else
	    goto add_quoted_string;

	   

	case ' ':
	   
	  if (ifs_is_null || split_on_spaces || ((word->flags & (W_NOSPLIT|W_NOSPLIT2|W_ASSIGNRHS)) && (word->flags & W_EXPANDRHS) == 0))
	    {
	      if (string[sindex])
		sindex++;
	      twochars[0] = CTLESC;
	      twochars[1] = c;
	      goto add_twochars;
	    }
	   
	  
	default:
	   
add_ifs_character:
	  if ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) || (isexp == 0 && isifs (c) && (word->flags & (W_NOSPLIT|W_NOSPLIT2)) == 0))
	    {
	      if ((quoted&(Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) == 0)
		has_quoted_ifs++;
add_quoted_character:
	      if (string[sindex])	 
		sindex++;
	      if (c == 0)
		{
		  c = CTLNUL;
		  goto add_character;
		}
	      else
		{
#if HANDLE_MULTIBYTE
		   
		  if (mb_cur_max > 1)
		    sindex--;

		  if (mb_cur_max > 1)
		    {
		      SADD_MBQCHAR_BODY(temp, string, sindex, string_size);
		    }
		  else
#endif
		    {
		      twochars[0] = CTLESC;
		      twochars[1] = c;
		      goto add_twochars;
		    }
		}
	    }

	  SADD_MBCHAR (temp, string, sindex, string_size);

add_character:
	  RESIZE_MALLOCED_BUFFER (istring, istring_index, 1, istring_size,
				  DEFAULT_ARRAY_SIZE);
	  istring[istring_index++] = c;
	  istring[istring_index] = '\0';

	   
	  sindex++;
	}
    }

finished_with_string:
   

   

   

   

  if (*istring == '\0')
    {
#if 0
      if (quoted_dollar_at == 0 && (had_quoted_null || quoted_state == PARTIALLY_QUOTED))
#else
      if (had_quoted_null || (quoted_dollar_at == 0 && quoted_state == PARTIALLY_QUOTED))
#endif
	{
	  istring[0] = CTLNUL;
	  istring[1] = '\0';
	  tword = alloc_word_desc ();
	  tword->word = istring;
	  istring = 0;		 
	  tword->flags |= W_HASQUOTEDNULL;		 
	  list = make_word_list (tword, (WORD_LIST *)NULL);
	  if (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES))
	    tword->flags |= W_QUOTED;
	}
       
       
      else  if (quoted_state == UNQUOTED || quoted_dollar_at)
	list = (WORD_LIST *)NULL;
      else
	list = (WORD_LIST *)NULL;
    }
  else if (word->flags & W_NOSPLIT)
    {
      tword = alloc_word_desc ();
      tword->word = istring;
      if (had_quoted_null && QUOTED_NULL (istring))
	tword->flags |= W_HASQUOTEDNULL;
      istring = 0;		 
      if (word->flags & W_ASSIGNMENT)
	tword->flags |= W_ASSIGNMENT;	 
      if (word->flags & W_COMPASSIGN)
	tword->flags |= W_COMPASSIGN;	 
      if (word->flags & W_NOGLOB)
	tword->flags |= W_NOGLOB;	 
      if (word->flags & W_NOBRACE)
	tword->flags |= W_NOBRACE;	 
      if (word->flags & W_ARRAYREF)
	tword->flags |= W_ARRAYREF;
      if (quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES))
	tword->flags |= W_QUOTED;
      list = make_word_list (tword, (WORD_LIST *)NULL);
    }
  else if (word->flags & W_ASSIGNRHS)
    {
      list = list_string (istring, "", quoted);
      tword = list->word;
      if (had_quoted_null && QUOTED_NULL (istring))
	tword->flags |= W_HASQUOTEDNULL;
      free (list);
      free (istring);
      istring = 0;			 
      goto set_word_flags;
    }
  else
    {
      char *ifs_chars;

      ifs_chars = (quoted_dollar_at || has_dollar_at) ? ifs_value : (char *)NULL;

       
      if (split_on_spaces)
	{
	   
	  if (ifs_is_set == 0)
	    list = list_string (istring, " \t\n", 1);	 
	  else
	    list = list_string (istring, " ", 1);	 
	}

       
      else if (has_dollar_at && quoted_dollar_at == 0 && ifs_chars && quoted == 0 && (word->flags & W_NOSPLIT2))
	{
	  tword = alloc_word_desc ();
	   
	  if (*ifs_chars && *ifs_chars != ' ')
	    {
	       
	      list = list_string (istring, *ifs_chars ? ifs_chars : " ", 1);
	       
	      tword->word = string_list (list);	
	    }
	  else
	    tword->word = istring;
	  if (had_quoted_null && QUOTED_NULL (istring))
	    tword->flags |= W_HASQUOTEDNULL;	 
	  if (tword->word != istring)
	    free (istring);
	  istring = 0;			 
	  goto set_word_flags;
	}
      else if (has_dollar_at && ifs_chars)
	list = list_string (istring, *ifs_chars ? ifs_chars : " ", 1);
      else
	{
	  tword = alloc_word_desc ();
	  if (expanded_something && *expanded_something == 0 && has_quoted_ifs)
	    tword->word = remove_quoted_ifs (istring);
	  else
	    tword->word = istring;
	  if (had_quoted_null && QUOTED_NULL (istring))	 
	    tword->flags |= W_HASQUOTEDNULL;	 
	  else if (had_quoted_null)
	    tword->flags |= W_SAWQUOTEDNULL;	 
	  if (tword->word != istring)
	    free (istring);
	  istring = 0;			 
set_word_flags:
	  if ((quoted & (Q_DOUBLE_QUOTES|Q_HERE_DOCUMENT)) || (quoted_state == WHOLLY_QUOTED))
	    tword->flags |= W_QUOTED;
	  if (word->flags & W_ASSIGNMENT)
	    tword->flags |= W_ASSIGNMENT;
	  if (word->flags & W_COMPASSIGN)
	    tword->flags |= W_COMPASSIGN;
	  if (word->flags & W_NOGLOB)
	    tword->flags |= W_NOGLOB;
	  if (word->flags & W_NOBRACE)
	    tword->flags |= W_NOBRACE;
	  if (word->flags & W_ARRAYREF)
	    tword->flags |= W_ARRAYREF;
	  list = make_word_list (tword, (WORD_LIST *)NULL);
	}
    }

  free (istring);
  return (list);
}

 
 
 
 
 

 
char *
string_quote_removal (string, quoted)
     char *string;
     int quoted;
{
  size_t slen;
  char *r, *result_string, *temp, *send;
  int sindex, tindex, dquote;
  unsigned char c;
  DECLARE_MBSTATE;

   
  slen = strlen (string);
  send = string + slen;

  r = result_string = (char *)xmalloc (slen + 1);

  for (dquote = sindex = 0; c = string[sindex];)
    {
      switch (c)
	{
	case '\\':
	  c = string[++sindex];
	  if (c == 0)
	    {
	      *r++ = '\\';
	      break;
	    }
	  if (((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) || dquote) && (sh_syntaxtab[c] & CBSDQUOTE) == 0)
	    *r++ = '\\';
	   

	default:
	  SCOPY_CHAR_M (r, string, send, sindex);
	  break;

	case '\'':
	  if ((quoted & (Q_HERE_DOCUMENT|Q_DOUBLE_QUOTES)) || dquote)
	    {
	      *r++ = c;
	      sindex++;
	      break;
	    }
	  tindex = sindex + 1;
	  temp = string_extract_single_quoted (string, &tindex, 0);
	  if (temp)
	    {
	      strcpy (r, temp);
	      r += strlen (r);
	      free (temp);
	    }
	  sindex = tindex;
	  break;

	case '"':
	  dquote = 1 - dquote;
	  sindex++;
	  break;
	}
    }
    *r = '\0';
    return (result_string);
}

#if 0
 
 
WORD_DESC *
word_quote_removal (word, quoted)
     WORD_DESC *word;
     int quoted;
{
  WORD_DESC *w;
  char *t;

  t = string_quote_removal (word->word, quoted);
  w = alloc_word_desc ();
  w->word = t ? t : savestring ("");
  return (w);
}

 
WORD_LIST *
word_list_quote_removal (list, quoted)
     WORD_LIST *list;
     int quoted;
{
  WORD_LIST *result, *t, *tresult, *e;

  for (t = list, result = (WORD_LIST *)NULL; t; t = t->next)
    {
      tresult = make_word_list (word_quote_removal (t->word, quoted), (WORD_LIST *)NULL);
#if 0
      result = (WORD_LIST *) list_append (result, tresult);
#else
      if (result == 0)
	result = e = tresult;
      else
	{
	  e->next = tresult;
	  while (e->next)
	    e = e->next;
	}
#endif
    }
  return (result);
}
#endif

 

void
setifs (v)
     SHELL_VAR *v;
{
  char *t;
  unsigned char uc;

  ifs_var = v;
  ifs_value = (v && value_cell (v)) ? value_cell (v) : " \t\n";

  ifs_is_set = ifs_var != 0;
  ifs_is_null = ifs_is_set && (*ifs_value == 0);

   
  memset (ifs_cmap, '\0', sizeof (ifs_cmap));
  for (t = ifs_value ; t && *t; t++)
    {
      uc = *t;
      ifs_cmap[uc] = 1;
    }

#if defined (HANDLE_MULTIBYTE)
  if (ifs_value == 0)
    {
      ifs_firstc[0] = '\0';	 
      ifs_firstc_len = 1;
    }
  else
    {
      if (locale_utf8locale && UTF8_SINGLEBYTE (*ifs_value))
	ifs_firstc_len = (*ifs_value != 0) ? 1 : 0;
      else
	{
	  size_t ifs_len;
	  ifs_len = strnlen (ifs_value, MB_CUR_MAX);
	  ifs_firstc_len = MBLEN (ifs_value, ifs_len);
	}
      if (ifs_firstc_len == 1 || ifs_firstc_len == 0 || MB_INVALIDCH (ifs_firstc_len))
	{
	  ifs_firstc[0] = ifs_value[0];
	  ifs_firstc[1] = '\0';
	  ifs_firstc_len = 1;
	}
      else
	memcpy (ifs_firstc, ifs_value, ifs_firstc_len);
    }
#else
  ifs_firstc = ifs_value ? *ifs_value : 0;
#endif
}

char *
getifs ()
{
  return ifs_value;
}

 
WORD_LIST *
word_split (w, ifs_chars)
     WORD_DESC *w;
     char *ifs_chars;
{
  WORD_LIST *result;

  if (w)
    {
      char *xifs;

      xifs = ((w->flags & W_QUOTED) || ifs_chars == 0) ? "" : ifs_chars;
      result = list_string (w->word, xifs, w->flags & W_QUOTED);
    }
  else
    result = (WORD_LIST *)NULL;

  return (result);
}

 
static WORD_LIST *
word_list_split (list)
     WORD_LIST *list;
{
  WORD_LIST *result, *t, *tresult, *e;
  WORD_DESC *w;

  for (t = list, result = (WORD_LIST *)NULL; t; t = t->next)
    {
      tresult = word_split (t->word, ifs_value);
       
      if (tresult == 0 && t->word && (t->word->flags & W_SAWQUOTEDNULL))	 
	{
	  w = alloc_word_desc ();
	  w->word = (char *)xmalloc (1);
	  w->word[0] = '\0';
	  tresult = make_word_list (w, (WORD_LIST *)NULL);
	}
#if defined (ARRAY_VARS)
       
      if (tresult && tresult->next == 0 && t->next == 0 && (t->word->flags & W_ARRAYREF) && STREQ (t->word->word, tresult->word->word))
	tresult->word->flags |= W_ARRAYREF;
#endif
      if (result == 0)
        result = e = tresult;
      else
	{
	  e->next = tresult;
	  while (e->next)
	    e = e->next;
	}
    }
  return (result);
}

 

 
static void
exp_jump_to_top_level (v)
     int v;
{
  set_pipestatus_from_exit (last_command_exit_value);

   
  expand_no_split_dollar_star = 0;	 
  if (expanding_redir)
    undo_partial_redirects ();
  expanding_redir = 0;
  assigning_in_environment = 0;

  if (parse_and_execute_level == 0)
    top_level_cleanup ();			 

  jump_to_top_level (v);
}

 
#define PREPEND_LIST(nlist, elist) \
	do { nlist->next = elist; elist = nlist; } while (0)

 
static WORD_LIST *
separate_out_assignments (tlist)
     WORD_LIST *tlist;
{
  register WORD_LIST *vp, *lp;

  if (tlist == 0)
    return ((WORD_LIST *)NULL);

  if (subst_assign_varlist)
    dispose_words (subst_assign_varlist);	 

  subst_assign_varlist = (WORD_LIST *)NULL;
  vp = lp = tlist;

   
  while (lp && (lp->word->flags & W_ASSIGNMENT))
    {
      vp = lp;
      lp = lp->next;
    }

   
  if (lp != tlist)
    {
      subst_assign_varlist = tlist;
       
      vp->next = (WORD_LIST *)NULL;	 
      tlist = lp;			 
    }

   
   
  if (!tlist)
     
    return ((WORD_LIST *)NULL);

   
   

   
  if (place_keywords_in_env)
    {
      WORD_LIST *tp;	 

      tp = tlist;
      lp = tlist->next;

       
       
      while (lp)
	{
	  if (lp->word->flags & W_ASSIGNMENT)
	    {
	       
	      if (!subst_assign_varlist)
		subst_assign_varlist = vp = lp;
	      else
		{
		  vp->next = lp;
		  vp = lp;
		}

	       
	      tp->next = lp->next;
	       
	      lp->next = (WORD_LIST *)NULL;
	      lp = tp->next;
	    }
	  else
	    {
	      tp = lp;
	      lp = lp->next;
	    }
	}
    }
  return (tlist);
}

#define WEXP_VARASSIGN	0x001
#define WEXP_BRACEEXP	0x002
#define WEXP_TILDEEXP	0x004
#define WEXP_PARAMEXP	0x008
#define WEXP_PATHEXP	0x010

 
#define WEXP_ALL	(WEXP_VARASSIGN|WEXP_BRACEEXP|WEXP_TILDEEXP|WEXP_PARAMEXP|WEXP_PATHEXP)

 
#define WEXP_NOVARS	(WEXP_BRACEEXP|WEXP_TILDEEXP|WEXP_PARAMEXP|WEXP_PATHEXP)

 
#define WEXP_SHELLEXP	(WEXP_BRACEEXP|WEXP_TILDEEXP|WEXP_PARAMEXP)

 

WORD_LIST *
expand_words (list)
     WORD_LIST *list;
{
  return (expand_word_list_internal (list, WEXP_ALL));
}

 
WORD_LIST *
expand_words_no_vars (list)
     WORD_LIST *list;
{
  return (expand_word_list_internal (list, WEXP_NOVARS));
}

WORD_LIST *
expand_words_shellexp (list)
     WORD_LIST *list;
{
  return (expand_word_list_internal (list, WEXP_SHELLEXP));
}

static WORD_LIST *
glob_expand_word_list (tlist, eflags)
     WORD_LIST *tlist;
     int eflags;
{
  char **glob_array, *temp_string;
  register int glob_index;
  WORD_LIST *glob_list, *output_list, *disposables, *next;
  WORD_DESC *tword;
  int x;

  output_list = disposables = (WORD_LIST *)NULL;
  glob_array = (char **)NULL;
  while (tlist)
    {
       
      next = tlist->next;

       
      if ((tlist->word->flags & W_NOGLOB) == 0 &&
	  unquoted_glob_pattern_p (tlist->word->word))
	{
	  glob_array = shell_glob_filename (tlist->word->word, QGLOB_CTLESC);	 

	   

	  if (glob_array == 0 || GLOB_FAILED (glob_array))
	    {
	      glob_array = (char **)xmalloc (sizeof (char *));
	      glob_array[0] = (char *)NULL;
	    }

	   
	  if (glob_array[0] == NULL)
	    {
	      temp_string = dequote_string (tlist->word->word);
	      free (tlist->word->word);
	      tlist->word->word = temp_string;
	    }

	   
	  glob_list = (WORD_LIST *)NULL;
	  for (glob_index = 0; glob_array[glob_index]; glob_index++)
	    {
	      tword = make_bare_word (glob_array[glob_index]);
	      glob_list = make_word_list (tword, glob_list);
	    }

	  if (glob_list)
	    {
	      output_list = (WORD_LIST *)list_append (glob_list, output_list);
	      PREPEND_LIST (tlist, disposables);
	    }
	  else if (fail_glob_expansion != 0)
	    {
	      last_command_exit_value = EXECUTION_FAILURE;
	      report_error (_("no match: %s"), tlist->word->word);
	      exp_jump_to_top_level (DISCARD);
	    }
	  else if (allow_null_glob_expansion == 0)
	    {
	       
	      PREPEND_LIST (tlist, output_list);
	    }
	  else
	    {
	       
	      PREPEND_LIST (tlist, disposables);
	    }
	}
      else
	{
	   
	  temp_string = dequote_string (tlist->word->word);
	  free (tlist->word->word);
	  tlist->word->word = temp_string;
	  PREPEND_LIST (tlist, output_list);
	}

      strvec_dispose (glob_array);
      glob_array = (char **)NULL;

      tlist = next;
    }

  if (disposables)
    dispose_words (disposables);

  if (output_list)
    output_list = REVERSE_LIST (output_list, WORD_LIST *);

  return (output_list);
}

#if defined (BRACE_EXPANSION)
static WORD_LIST *
brace_expand_word_list (tlist, eflags)
     WORD_LIST *tlist;
     int eflags;
{
  register char **expansions;
  char *temp_string;
  WORD_LIST *disposables, *output_list, *next;
  WORD_DESC *w;
  int eindex;

  for (disposables = output_list = (WORD_LIST *)NULL; tlist; tlist = next)
    {
      next = tlist->next;

      if (tlist->word->flags & W_NOBRACE)
        {
 
	  PREPEND_LIST (tlist, output_list);
	  continue;
        }

      if ((tlist->word->flags & (W_COMPASSIGN|W_ASSIGNARG)) == (W_COMPASSIGN|W_ASSIGNARG))
        {
 
	  PREPEND_LIST (tlist, output_list);
	  continue;
        }

       
      if (mbschr (tlist->word->word, LBRACE))
	{
	  expansions = brace_expand (tlist->word->word);

	  for (eindex = 0; temp_string = expansions[eindex]; eindex++)
	    {
	      w = alloc_word_desc ();
	      w->word = temp_string;

	       
	      if (STREQ (temp_string, tlist->word->word))
		w->flags = tlist->word->flags;
	      else
		w = make_word_flags (w, temp_string);

	      output_list = make_word_list (w, output_list);
	    }
	  free (expansions);

	   
	  PREPEND_LIST (tlist, disposables);
	}
      else
	PREPEND_LIST (tlist, output_list);
    }

  if (disposables)
    dispose_words (disposables);

  if (output_list)
    output_list = REVERSE_LIST (output_list, WORD_LIST *);

  return (output_list);
}
#endif

#if defined (ARRAY_VARS)
 
static int
make_internal_declare (word, option, cmd)
     char *word;
     char *option;
     char *cmd;
{
  int t, r;
  WORD_LIST *wl;
  WORD_DESC *w;

  w = make_word (word);

  t = assignment (w->word, 0);
  if (w->word[t] == '=')
    {
      w->word[t] = '\0';
      if (w->word[t - 1] == '+')	 
	w->word[t - 1] = '\0';
    }

  wl = make_word_list (w, (WORD_LIST *)NULL);
  wl = make_word_list (make_word (option), wl);

  r = declare_builtin (wl);

  dispose_words (wl);
  return r;
}  

 

static WORD_LIST *
expand_oneword (value, flags)
     char *value;
     int flags;
{
  WORD_LIST *l, *nl;
  char *t;
  int kvpair;
  
  if (flags == 0)
    {
       
      l = expand_compound_array_assignment ((SHELL_VAR *)NULL, value, flags);
       
      quote_compound_array_list (l, flags);
      return l;
    }
  else
    {
       
      l = parse_string_to_word_list (value, 1, "array assign");
#if ASSOC_KVPAIR_ASSIGNMENT
      kvpair = kvpair_assignment_p (l);
#endif

       
      for (nl = l; nl; nl = nl->next)
	{
#if ASSOC_KVPAIR_ASSIGNMENT
	  if (kvpair)
	     
	    t = expand_and_quote_kvpair_word (nl->word->word);
	  else
#endif
	  if ((nl->word->flags & W_ASSIGNMENT) == 0)
	    t = sh_single_quote (nl->word->word ? nl->word->word : "");
	  else
	    t = expand_and_quote_assoc_word (nl->word->word, flags);
	  free (nl->word->word);
	  nl->word->word = t;
	}
      return l;
    }
}

 
static void
expand_compound_assignment_word (tlist, flags)
     WORD_LIST *tlist;
     int flags;
{
  WORD_LIST *l;
  int wlen, oind, t;
  char *value, *temp;

 
  t = assignment (tlist->word->word, 0);

   
  oind = 1;
  value = extract_array_assignment_list (tlist->word->word + t + 1, &oind);
   
  l = expand_oneword (value, flags);
  free (value);

  value = string_list (l);
  dispose_words (l);

  wlen = STRLEN (value);

   
  temp = xmalloc (t + 3 + wlen + 1);	 
  memcpy (temp, tlist->word->word, ++t);
  temp[t++] = '(';
  if (value)
    memcpy (temp + t, value, wlen);
  t += wlen;
  temp[t++] = ')';
  temp[t] = '\0';
 

  free (tlist->word->word);
  tlist->word->word = temp;

  free (value);
}

 
static WORD_LIST *
expand_declaration_argument (tlist, wcmd)
     WORD_LIST *tlist, *wcmd;
{
  char opts[16], omap[128];
  int t, opti, oind, skip, inheriting;
  WORD_LIST *l;

  inheriting = localvar_inherit;
  opti = 0;
  if (tlist->word->flags & (W_ASSIGNASSOC|W_ASSNGLOBAL|W_CHKLOCAL|W_ASSIGNARRAY))
    opts[opti++] = '-';

  if ((tlist->word->flags & (W_ASSIGNASSOC|W_ASSNGLOBAL)) == (W_ASSIGNASSOC|W_ASSNGLOBAL))
    {
      opts[opti++] = 'g';
      opts[opti++] = 'A';
    }
  else if (tlist->word->flags & W_ASSIGNASSOC)
    {
      opts[opti++] = 'A';
    }
  else if ((tlist->word->flags & (W_ASSIGNARRAY|W_ASSNGLOBAL)) == (W_ASSIGNARRAY|W_ASSNGLOBAL))
    {
      opts[opti++] = 'g';
      opts[opti++] = 'a';
    }
  else if (tlist->word->flags & W_ASSIGNARRAY)
    {
      opts[opti++] = 'a';
    }
  else if (tlist->word->flags & W_ASSNGLOBAL)
    opts[opti++] = 'g';

  if (tlist->word->flags & W_CHKLOCAL)
    opts[opti++] = 'G';

   

  memset (omap, '\0', sizeof (omap));
  for (l = wcmd->next; l != tlist; l = l->next)
    {
      int optchar;

      if (l->word->word[0] != '-' && l->word->word[0] != '+')
	break;	 
      if (l->word->word[0] == '-' && l->word->word[1] == '-' && l->word->word[2] == 0)
	break;	 
      optchar = l->word->word[0];
      for (oind = 1; l->word->word[oind]; oind++)
	switch (l->word->word[oind])
	  {
	    case 'I':
	      inheriting = 1;
	    case 'i':
	    case 'l':
	    case 'u':
	    case 'c':
	      omap[l->word->word[oind]] = 1;
	      if (opti == 0)
		opts[opti++] = optchar;
	      break;
	    default:
	      break;
	  }
    }

  for (oind = 0; oind < sizeof (omap); oind++)
    if (omap[oind])
      opts[opti++] = oind;

   
  if ((tlist->word->flags & (W_ASSIGNASSOC|W_ASSIGNARRAY)) == 0)
    {
      if (opti == 0)
	{
	  opts[opti++] = '-';
          opts[opti++] = '-';
	}
    }
  opts[opti] = '\0';

   
  expand_compound_assignment_word (tlist, (tlist->word->flags & W_ASSIGNASSOC) ? 1 : 0);

  skip = 0;
  if (opti > 0)
    {
      t = make_internal_declare (tlist->word->word, opts, wcmd ? wcmd->word->word : (char *)0);
      if (t != EXECUTION_SUCCESS)
	{
	  last_command_exit_value = t;
	  if (tlist->word->flags & W_FORCELOCAL)	 
	    skip = 1;
	  else
	    exp_jump_to_top_level (DISCARD);
	}
    }

  if (skip == 0)
    {
      t = do_word_assignment (tlist->word, 0);
      if (t == 0)
	{
	  last_command_exit_value = EXECUTION_FAILURE;
	  exp_jump_to_top_level (DISCARD);
	}
    }

   
  t = assignment (tlist->word->word, 0);
  tlist->word->word[t] = '\0';
  if (tlist->word->word[t - 1] == '+')
    tlist->word->word[t - 1] = '\0';	 
  tlist->word->flags &= ~(W_ASSIGNMENT|W_NOSPLIT|W_COMPASSIGN|W_ASSIGNARG|W_ASSIGNASSOC|W_ASSIGNARRAY);

  return (tlist);
}
#endif  

static WORD_LIST *
shell_expand_word_list (tlist, eflags)
     WORD_LIST *tlist;
     int eflags;
{
  WORD_LIST *expanded, *orig_list, *new_list, *next, *temp_list, *wcmd;
  int expanded_something, has_dollar_at;

   
  wcmd = new_list = (WORD_LIST *)NULL;

  for (orig_list = tlist; tlist; tlist = next)
    {
      if (wcmd == 0 && (tlist->word->flags & W_ASSNBLTIN))
	wcmd = tlist;
	
      next = tlist->next;

#if defined (ARRAY_VARS)
       
      if ((tlist->word->flags & (W_COMPASSIGN|W_ASSIGNARG)) == (W_COMPASSIGN|W_ASSIGNARG))
	expand_declaration_argument (tlist, wcmd);
#endif

      expanded_something = 0;
      expanded = expand_word_internal
	(tlist->word, 0, 0, &has_dollar_at, &expanded_something);

      if (expanded == &expand_word_error || expanded == &expand_word_fatal)
	{
	   
	  tlist->word->word = (char *)NULL;

	   
	  dispose_words (orig_list);
	   
	  dispose_words (new_list);

	  last_command_exit_value = EXECUTION_FAILURE;
	  if (expanded == &expand_word_error)
	    exp_jump_to_top_level (DISCARD);
	  else
	    exp_jump_to_top_level (FORCE_EOF);
	}

       
      if (expanded_something && (tlist->word->flags & W_NOSPLIT) == 0)
	{
	  temp_list = word_list_split (expanded);
	  dispose_words (expanded);
	}
      else
	{
	   
	  word_list_remove_quoted_nulls (expanded);
	  temp_list = expanded;
	}

      expanded = REVERSE_LIST (temp_list, WORD_LIST *);
      new_list = (WORD_LIST *)list_append (expanded, new_list);
    }

  if (orig_list)  
    dispose_words (orig_list);

  if (new_list)
    new_list = REVERSE_LIST (new_list, WORD_LIST *);

  return (new_list);
}

 
static int
do_assignment_statements (varlist, command, is_nullcmd)
     WORD_LIST *varlist;
     char *command;
     int is_nullcmd;
{
  WORD_LIST *temp_list;
  char *savecmd;
  sh_wassign_func_t *assign_func;
  int is_special_builtin, is_builtin_or_func, tint;

   
  assign_func = is_nullcmd ? do_word_assignment : assign_in_env;
  tempenv_assign_error = 0;

  is_builtin_or_func = command && (find_shell_builtin (command) || find_function (command));
   
  is_special_builtin = posixly_correct && command && find_special_builtin (command);

  savecmd = this_command_name;
  for (temp_list = varlist; temp_list; temp_list = temp_list->next)
    {
      this_command_name = (char *)NULL;
      assigning_in_environment = is_nullcmd == 0;
      tint = (*assign_func) (temp_list->word, is_builtin_or_func);
      assigning_in_environment = 0;
      this_command_name = savecmd;

       
      if (tint == 0)
	{
	  if (is_nullcmd)	 
	    {
	      last_command_exit_value = EXECUTION_FAILURE;
#if defined (STRICT_POSIX)
	      if (posixly_correct && interactive_shell == 0)
#else
	      if (posixly_correct && interactive_shell == 0 && executing_command_builtin == 0)
#endif
	        exp_jump_to_top_level (FORCE_EOF);
	      else
		exp_jump_to_top_level (DISCARD);
	    }
	   
	  else if (posixly_correct)
	    {
	      last_command_exit_value = EXECUTION_FAILURE;
#if defined (STRICT_POSIX)
	      exp_jump_to_top_level ((interactive_shell == 0) ? FORCE_EOF : DISCARD);
#else
	      if (interactive_shell == 0 && is_special_builtin)
		exp_jump_to_top_level (FORCE_EOF);
	      else if (interactive_shell == 0)
		exp_jump_to_top_level (DISCARD);	 
	      else
		exp_jump_to_top_level (DISCARD);
#endif
	    }
	  else
	    tempenv_assign_error++;
	}
    }
  return (tempenv_assign_error);
}

 
static WORD_LIST *
expand_word_list_internal (list, eflags)
     WORD_LIST *list;
     int eflags;
{
  WORD_LIST *new_list, *temp_list;

  tempenv_assign_error = 0;
  if (list == 0)
    return ((WORD_LIST *)NULL);

  garglist = new_list = copy_word_list (list);
  if (eflags & WEXP_VARASSIGN)
    {
      garglist = new_list = separate_out_assignments (new_list);
      if (new_list == 0)
	{
	  if (subst_assign_varlist)
	    do_assignment_statements (subst_assign_varlist, (char *)NULL, 1);
	    
	  dispose_words (subst_assign_varlist);
	  subst_assign_varlist = (WORD_LIST *)NULL;

	  return ((WORD_LIST *)NULL);
	}
    }

   

#if defined (BRACE_EXPANSION)
   
  if ((eflags & WEXP_BRACEEXP) && brace_expansion && new_list)
    new_list = brace_expand_word_list (new_list, eflags);
#endif  

   
  new_list = shell_expand_word_list (new_list, eflags);

   
  if (new_list)
    {
      if ((eflags & WEXP_PATHEXP) && disallow_filename_globbing == 0)
	 
	new_list = glob_expand_word_list (new_list, eflags);
      else
	 
	new_list = dequote_list (new_list);
    }

  if ((eflags & WEXP_VARASSIGN) && subst_assign_varlist)
    {
      do_assignment_statements (subst_assign_varlist, (new_list && new_list->word) ? new_list->word->word : (char *)NULL, new_list == 0);

      dispose_words (subst_assign_varlist);
      subst_assign_varlist = (WORD_LIST *)NULL;
    }

  return (new_list);
}
