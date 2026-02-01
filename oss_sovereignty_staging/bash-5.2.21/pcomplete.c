 

 

#include <config.h>

#if defined (PROGRAMMABLE_COMPLETION)

#include "bashtypes.h"
#include "posixstat.h"

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include <signal.h>

#if defined (PREFER_STDARG)
#  include <stdarg.h>
#else
#  include <varargs.h>
#endif

#include "posixtime.h"

#include <stdio.h>
#include "bashansi.h"
#include "bashintl.h"

#include "shell.h"
#include "pcomplete.h"
#include "alias.h"
#include "bashline.h"
#include "execute_cmd.h"
#include "pathexp.h"

#if defined (JOB_CONTROL)
#  include "jobs.h"
#endif

#if !defined (NSIG)
#  include "trap.h"
#endif

#include "shmbutil.h"

#include "builtins.h"
#include "builtins/common.h"
#include "builtins/builtext.h"

#include <glob/glob.h>
#include <glob/strmatch.h>

#include <readline/rlconf.h>
#include <readline/readline.h>
#include <readline/history.h>

#ifdef STRDUP
#  undef STRDUP
#endif
#define STRDUP(x)	((x) ? savestring (x) : (char *)NULL)

typedef SHELL_VAR **SVFUNC ();

#ifndef HAVE_STRPBRK
extern char *strpbrk PARAMS((char *, char *));
#endif

extern STRING_INT_ALIST word_token_alist[];
extern char *signal_names[];

#if defined (DEBUG)
#if defined (PREFER_STDARG)
static void debug_printf (const char *, ...)  __attribute__((__format__ (printf, 1, 2)));
#endif
#endif  

static int it_init_joblist PARAMS((ITEMLIST *, int));

static int it_init_aliases PARAMS((ITEMLIST *));
static int it_init_arrayvars PARAMS((ITEMLIST *));
static int it_init_bindings PARAMS((ITEMLIST *));
static int it_init_builtins PARAMS((ITEMLIST *));
static int it_init_disabled PARAMS((ITEMLIST *));
static int it_init_enabled PARAMS((ITEMLIST *));
static int it_init_exported PARAMS((ITEMLIST *));
static int it_init_functions PARAMS((ITEMLIST *));
static int it_init_helptopics PARAMS((ITEMLIST *));
static int it_init_hostnames PARAMS((ITEMLIST *));
static int it_init_jobs PARAMS((ITEMLIST *));
static int it_init_running PARAMS((ITEMLIST *));
static int it_init_stopped PARAMS((ITEMLIST *));
static int it_init_keywords PARAMS((ITEMLIST *));
static int it_init_signals PARAMS((ITEMLIST *));
static int it_init_variables PARAMS((ITEMLIST *));
static int it_init_setopts PARAMS((ITEMLIST *));
static int it_init_shopts PARAMS((ITEMLIST *));

static int shouldexp_filterpat PARAMS((char *));
static char *preproc_filterpat PARAMS((char *, const char *));

static void init_itemlist_from_varlist PARAMS((ITEMLIST *, SVFUNC *));

static STRINGLIST *gen_matches_from_itemlist PARAMS((ITEMLIST *, const char *));
static STRINGLIST *gen_action_completions PARAMS((COMPSPEC *, const char *));
static STRINGLIST *gen_globpat_matches PARAMS((COMPSPEC *, const char *));
static STRINGLIST *gen_wordlist_matches PARAMS((COMPSPEC *, const char *));
static STRINGLIST *gen_shell_function_matches PARAMS((COMPSPEC *, const char *,
						   const char *,
						   char *, int, WORD_LIST *,
						   int, int, int *));
static STRINGLIST *gen_command_matches PARAMS((COMPSPEC *, const char *,
					    const char *,
					    char *, int, WORD_LIST *,
					    int, int));

static STRINGLIST *gen_progcomp_completions PARAMS((const char *, const char *,
						 const char *,
						 int, int, int *, int *,
						 COMPSPEC **));

static char *pcomp_filename_completion_function PARAMS((const char *, int));

#if defined (ARRAY_VARS)
static SHELL_VAR *bind_comp_words PARAMS((WORD_LIST *));
#endif
static void bind_compfunc_variables PARAMS((char *, int, WORD_LIST *, int, int));
static void unbind_compfunc_variables PARAMS((int));
static WORD_LIST *build_arg_list PARAMS((char *, const char *, const char *, WORD_LIST *, int));
static WORD_LIST *command_line_to_word_list PARAMS((char *, int, int, int *, int *));

#ifdef DEBUG
static int progcomp_debug = 0;
#endif

int prog_completion_enabled = 1;

#ifdef ALIAS
int progcomp_alias = 0;	 
#endif

 
ITEMLIST it_aliases = { 0, it_init_aliases, (STRINGLIST *)0 };
ITEMLIST it_arrayvars  = { LIST_DYNAMIC, it_init_arrayvars, (STRINGLIST *)0 };
ITEMLIST it_bindings  = { 0, it_init_bindings, (STRINGLIST *)0 };
ITEMLIST it_builtins  = { 0, it_init_builtins, (STRINGLIST *)0 };
ITEMLIST it_commands = { LIST_DYNAMIC };	 
ITEMLIST it_directories = { LIST_DYNAMIC };	 
ITEMLIST it_disabled = { 0, it_init_disabled, (STRINGLIST *)0 };
ITEMLIST it_enabled = { 0, it_init_enabled, (STRINGLIST *)0 };
ITEMLIST it_exports  = { LIST_DYNAMIC, it_init_exported, (STRINGLIST *)0 };
ITEMLIST it_files = { LIST_DYNAMIC };		 
ITEMLIST it_functions  = { 0, it_init_functions, (STRINGLIST *)0 };
ITEMLIST it_helptopics  = { 0, it_init_helptopics, (STRINGLIST *)0 };
ITEMLIST it_hostnames  = { LIST_DYNAMIC, it_init_hostnames, (STRINGLIST *)0 };
ITEMLIST it_groups = { LIST_DYNAMIC };		 
ITEMLIST it_jobs = { LIST_DYNAMIC, it_init_jobs, (STRINGLIST *)0 };
ITEMLIST it_keywords = { 0, it_init_keywords, (STRINGLIST *)0 };
ITEMLIST it_running = { LIST_DYNAMIC, it_init_running, (STRINGLIST *)0 };
ITEMLIST it_services = { LIST_DYNAMIC };	 
ITEMLIST it_setopts = { 0, it_init_setopts, (STRINGLIST *)0 };
ITEMLIST it_shopts = { 0, it_init_shopts, (STRINGLIST *)0 };
ITEMLIST it_signals = { 0, it_init_signals, (STRINGLIST *)0 };
ITEMLIST it_stopped = { LIST_DYNAMIC, it_init_stopped, (STRINGLIST *)0 };
ITEMLIST it_users = { LIST_DYNAMIC };		 
ITEMLIST it_variables = { LIST_DYNAMIC, it_init_variables, (STRINGLIST *)0 };

COMPSPEC *pcomp_curcs;
const char *pcomp_curcmd;
const char *pcomp_curtxt;

char *pcomp_line;
int pcomp_ind;

#ifdef DEBUG
 
static void
#if defined (PREFER_STDARG)
debug_printf (const char *format, ...)
#else
debug_printf (format, va_alist)
     const char *format;
     va_dcl
#endif
{
  va_list args;

  if (progcomp_debug == 0)
    return;

  SH_VA_START (args, format);

  fprintf (stdout, "DEBUG: ");
  vfprintf (stdout, format, args);
  fprintf (stdout, "\n");

  rl_on_new_line ();

  va_end (args);
}
#endif

 

void
set_itemlist_dirty (it)
     ITEMLIST *it;
{
  it->flags |= LIST_DIRTY;
}

void
initialize_itemlist (itp)
     ITEMLIST *itp;
{
  (*itp->list_getter) (itp);
  itp->flags |= LIST_INITIALIZED;
  itp->flags &= ~LIST_DIRTY;
}

void
clean_itemlist (itp)
     ITEMLIST *itp;
{
  STRINGLIST *sl;

  sl = itp->slist;
  if (sl)
    {
      if ((itp->flags & (LIST_DONTFREEMEMBERS|LIST_DONTFREE)) == 0)
	strvec_flush (sl->list);
      if ((itp->flags & LIST_DONTFREE) == 0)
	free (sl->list);
      free (sl);
    }
  itp->slist = (STRINGLIST *)NULL;
  itp->flags &= ~(LIST_DONTFREE|LIST_DONTFREEMEMBERS|LIST_INITIALIZED|LIST_DIRTY);
}


static int
shouldexp_filterpat (s)
     char *s;
{
  register char *p;

  for (p = s; p && *p; p++)
    {
      if (*p == '\\')
	p++;
      else if (*p == '&')
	return 1;
    }
  return 0;
}

 
static char *
preproc_filterpat (pat, text)
     char *pat;
     const char *text;
{
  char *ret;

  ret = strcreplace (pat, '&', text, 1);
  return ret;
}
	
    
STRINGLIST *
filter_stringlist (sl, filterpat, text)
     STRINGLIST *sl;
     char *filterpat;
     const char *text;
{
  int i, m, not;
  STRINGLIST *ret;
  char *npat, *t;

  if (sl == 0 || sl->list == 0 || sl->list_len == 0)
    return sl;

  npat = shouldexp_filterpat (filterpat) ? preproc_filterpat (filterpat, text) : filterpat;

#if defined (EXTENDED_GLOB)
  not = (npat[0] == '!' && (extended_glob == 0 || npat[1] != '('));	 
#else
  not = (npat[0] == '!');
#endif
  t = not ? npat + 1 : npat;

  ret = strlist_create (sl->list_size);
  for (i = 0; i < sl->list_len; i++)
    {
      m = strmatch (t, sl->list[i], FNMATCH_EXTFLAG | FNMATCH_IGNCASE);
      if ((not && m == FNM_NOMATCH) || (not == 0 && m != FNM_NOMATCH))
	free (sl->list[i]);
      else
	ret->list[ret->list_len++] = sl->list[i];
    }

  ret->list[ret->list_len] = (char *)NULL;
  if (npat != filterpat)
    free (npat);

  return ret;
}

 
STRINGLIST *
completions_to_stringlist (matches)
     char **matches;
{
  STRINGLIST *sl;
  int mlen, i, n;

  mlen = (matches == 0) ? 0 : strvec_len (matches);
  sl = strlist_create (mlen + 1);

  if (matches == 0 || matches[0] == 0)
    return sl;

  if (matches[1] == 0)
    {
      sl->list[0] = STRDUP (matches[0]);
      sl->list[sl->list_len = 1] = (char *)NULL;
      return sl;
    }

  for (i = 1, n = 0; i < mlen; i++, n++)
    sl->list[n] = STRDUP (matches[i]);
  sl->list_len = n;
  sl->list[n] = (char *)NULL;

  return sl;
}

 

static int
it_init_aliases (itp)
     ITEMLIST *itp;
{
#ifdef ALIAS
  alias_t **alias_list;
  register int i, n;
  STRINGLIST *sl;

  alias_list = all_aliases ();
  if (alias_list == 0)
    {
      itp->slist = (STRINGLIST *)NULL;
      return 0;
    }
  for (n = 0; alias_list[n]; n++)
    ;
  sl = strlist_create (n+1);
  for (i = 0; i < n; i++)
    sl->list[i] = STRDUP (alias_list[i]->name);
  sl->list[n] = (char *)NULL;
  sl->list_size = sl->list_len = n;
  itp->slist = sl;
#else
  itp->slist = (STRINGLIST *)NULL;
#endif
  free (alias_list);
  return 1;
}

static void
init_itemlist_from_varlist (itp, svfunc)
     ITEMLIST *itp;
     SVFUNC *svfunc;
{
  SHELL_VAR **vlist;
  STRINGLIST *sl;
  register int i, n;

  vlist = (*svfunc) ();
  if (vlist == 0)
    {
      itp->slist = (STRINGLIST *)NULL;
      return;
    }    
  for (n = 0; vlist[n]; n++)
    ;
  sl = strlist_create (n+1);
  for (i = 0; i < n; i++)
    sl->list[i] = savestring (vlist[i]->name);
  sl->list[sl->list_len = n] = (char *)NULL;
  itp->slist = sl;
  free (vlist);
}

static int
it_init_arrayvars (itp)
     ITEMLIST *itp;
{
#if defined (ARRAY_VARS)
  init_itemlist_from_varlist (itp, all_array_variables);
  return 1;
#else
  return 0;
#endif
}

static int
it_init_bindings (itp)
     ITEMLIST *itp;
{
  char **blist;
  STRINGLIST *sl;

   
  blist = (char **)rl_funmap_names ();	 
  sl = strlist_create (0);
  sl->list = blist;
  sl->list_size = 0;
  sl->list_len = strvec_len (sl->list);
  itp->flags |= LIST_DONTFREEMEMBERS;
  itp->slist = sl;

  return 0;
}

static int
it_init_builtins (itp)
     ITEMLIST *itp;
{
  STRINGLIST *sl;
  register int i, n;

  sl = strlist_create (num_shell_builtins);
  for (i = n = 0; i < num_shell_builtins; i++)
    if (shell_builtins[i].function)
      sl->list[n++] = shell_builtins[i].name;
  sl->list[sl->list_len = n] = (char *)NULL;
  itp->flags |= LIST_DONTFREEMEMBERS;
  itp->slist = sl;
  return 0;
}

static int
it_init_enabled (itp)
     ITEMLIST *itp;
{
  STRINGLIST *sl;
  register int i, n;

  sl = strlist_create (num_shell_builtins);
  for (i = n = 0; i < num_shell_builtins; i++)
    {
      if (shell_builtins[i].function && (shell_builtins[i].flags & BUILTIN_ENABLED))
	sl->list[n++] = shell_builtins[i].name;
    }
  sl->list[sl->list_len = n] = (char *)NULL;
  itp->flags |= LIST_DONTFREEMEMBERS;
  itp->slist = sl;
  return 0;
}

static int
it_init_disabled (itp)
     ITEMLIST *itp;
{
  STRINGLIST *sl;
  register int i, n;

  sl = strlist_create (num_shell_builtins);
  for (i = n = 0; i < num_shell_builtins; i++)
    {
      if (shell_builtins[i].function && ((shell_builtins[i].flags & BUILTIN_ENABLED) == 0))
	sl->list[n++] = shell_builtins[i].name;
    }
  sl->list[sl->list_len = n] = (char *)NULL;
  itp->flags |= LIST_DONTFREEMEMBERS;
  itp->slist = sl;
  return 0;
}

static int
it_init_exported (itp)
     ITEMLIST *itp;
{
  init_itemlist_from_varlist (itp, all_exported_variables);
  return 0;
}

static int
it_init_functions (itp)
     ITEMLIST *itp;
{
  init_itemlist_from_varlist (itp, all_visible_functions);
  return 0;
}

 
static int
it_init_helptopics (itp)
     ITEMLIST *itp;
{
  STRINGLIST *sl;
  register int i, n;

  sl = strlist_create (num_shell_builtins);
  for (i = n = 0; i < num_shell_builtins; i++)
    sl->list[n++] = shell_builtins[i].name;
  sl->list[sl->list_len = n] = (char *)NULL;
  itp->flags |= LIST_DONTFREEMEMBERS;
  itp->slist = sl;
  return 0;
}

static int
it_init_hostnames (itp)
     ITEMLIST *itp;
{
  STRINGLIST *sl;

  sl = strlist_create (0);
  sl->list = get_hostname_list ();
  sl->list_len = sl->list ? strvec_len (sl->list) : 0;
  sl->list_size = sl->list_len;
  itp->slist = sl;
  itp->flags |= LIST_DONTFREEMEMBERS|LIST_DONTFREE;
  return 0;
}

static int
it_init_joblist (itp, jstate)
     ITEMLIST *itp;
     int jstate;
{
#if defined (JOB_CONTROL)
  STRINGLIST *sl;
  register int i;
  register PROCESS *p;
  char *s, *t;
  JOB *j;
  JOB_STATE ws;		 

  ws = JNONE;
  if (jstate == 0)
    ws = JRUNNING;
  else if (jstate == 1)
    ws = JSTOPPED;

  sl = strlist_create (js.j_jobslots);
  for (i = js.j_jobslots - 1; i >= 0; i--)
    {
      j = get_job_by_jid (i);
      if (j == 0)
	continue;
      p = j->pipe;
      if (jstate == -1 || JOBSTATE(i) == ws)
	{
	  s = savestring (p->command);
	  t = strpbrk (s, " \t\n");
	  if (t)
	    *t = '\0';
	  sl->list[sl->list_len++] = s;
	}
    }
  itp->slist = sl;
#else
  itp->slist = (STRINGLIST *)NULL;
#endif
  return 0;
}

static int
it_init_jobs (itp)
     ITEMLIST *itp;
{
  return (it_init_joblist (itp, -1));
}

static int
it_init_running (itp)
     ITEMLIST *itp;
{
  return (it_init_joblist (itp, 0));
}

static int
it_init_stopped (itp)
     ITEMLIST *itp;
{
  return (it_init_joblist (itp, 1));
}

static int
it_init_keywords (itp)
     ITEMLIST *itp;
{
  STRINGLIST *sl;
  register int i, n;

  for (n = 0; word_token_alist[n].word; n++)
    ;
  sl = strlist_create (n);
  for (i = 0; i < n; i++)
    sl->list[i] = word_token_alist[i].word;
  sl->list[sl->list_len = i] = (char *)NULL;
  itp->flags |= LIST_DONTFREEMEMBERS;
  itp->slist = sl;
  return 0;
}

static int
it_init_signals (itp)
     ITEMLIST *itp;
{
  STRINGLIST *sl;

  sl = strlist_create (0);
  sl->list = signal_names;
  sl->list_len = strvec_len (sl->list);
  itp->flags |= LIST_DONTFREE;
  itp->slist = sl;
  return 0;
}

static int
it_init_variables (itp)
     ITEMLIST *itp;
{
  init_itemlist_from_varlist (itp, all_visible_variables);
  return 0;
}

static int
it_init_setopts (itp)
     ITEMLIST *itp;
{
  STRINGLIST *sl;

  sl = strlist_create (0);
  sl->list = get_minus_o_opts ();
  sl->list_len = strvec_len (sl->list);
  itp->slist = sl;
  itp->flags |= LIST_DONTFREEMEMBERS;
  return 0;
}

static int
it_init_shopts (itp)
     ITEMLIST *itp;
{
  STRINGLIST *sl;

  sl = strlist_create (0);
  sl->list = get_shopt_options ();
  sl->list_len = strvec_len (sl->list);
  itp->slist = sl;
  itp->flags |= LIST_DONTFREEMEMBERS;
  return 0;
}

 
static STRINGLIST *
gen_matches_from_itemlist (itp, text)
     ITEMLIST *itp;
     const char *text;
{
  STRINGLIST *ret, *sl;
  int tlen, i, n;
  char *ntxt;

  if ((itp->flags & (LIST_DIRTY|LIST_DYNAMIC)) ||
      (itp->flags & LIST_INITIALIZED) == 0)
    {
      if (itp->flags & (LIST_DIRTY|LIST_DYNAMIC))
	clean_itemlist (itp);
      if ((itp->flags & LIST_INITIALIZED) == 0)
	initialize_itemlist (itp);
    }
  if (itp->slist == 0)
    return ((STRINGLIST *)NULL);
  ret = strlist_create (itp->slist->list_len+1);
  sl = itp->slist;

  ntxt = bash_dequote_text (text);
  tlen = STRLEN (ntxt);

  for (i = n = 0; i < sl->list_len; i++)
    {
      if (tlen == 0 || STREQN (sl->list[i], ntxt, tlen))
	ret->list[n++] = STRDUP (sl->list[i]);
    }
  ret->list[ret->list_len = n] = (char *)NULL;

  FREE (ntxt);
  return ret;
}

 
static char *
pcomp_filename_completion_function (text, state)
     const char *text;
     int state;
{
  static char *dfn;	 
  int iscompgen, iscompleting;

  if (state == 0)
    {
      FREE (dfn);
       
       
      iscompgen = this_shell_builtin == compgen_builtin;
      iscompleting = RL_ISSTATE (RL_STATE_COMPLETING);
      if (iscompgen && iscompleting == 0 && rl_completion_found_quote == 0
	  && rl_filename_dequoting_function)
	{
	   
	  dfn = (*rl_filename_dequoting_function) ((char *)text, rl_completion_quote_character);
	}
       
      else if (iscompgen && iscompleting &&
	       pcomp_curtxt && *pcomp_curtxt == 0 &&
	       text && (*text == '\'' || *text == '"') && text[1] == text[0] && text[2] == 0 && 
	       rl_filename_dequoting_function)
	dfn = (*rl_filename_dequoting_function) ((char *)text, rl_completion_quote_character);
       
       
      else if (iscompgen && iscompleting && rl_filename_dequoting_function &&
	       pcomp_curtxt && text &&
	       STREQ (pcomp_curtxt, text) == 0 &&
	       variable_context &&
	       sh_contains_quotes (text))	 
	dfn = (*rl_filename_dequoting_function) ((char *)text, rl_completion_quote_character);
      else
	dfn = savestring (text);
    }

  return (rl_filename_completion_function (dfn, state));
}

#define GEN_COMPS(bmap, flag, it, text, glist, tlist) \
  do { \
    if (bmap & flag) \
      { \
	tlist = gen_matches_from_itemlist (it, text); \
	if (tlist) \
	  { \
	    glist = strlist_append (glist, tlist); \
	    strlist_dispose (tlist); \
	  } \
      } \
  } while (0)

#define GEN_XCOMPS(bmap, flag, text, func, cmatches, glist, tlist) \
  do { \
    if (bmap & flag) \
      { \
	cmatches = rl_completion_matches (text, func); \
	tlist = completions_to_stringlist (cmatches); \
	glist = strlist_append (glist, tlist); \
	strvec_dispose (cmatches); \
	strlist_dispose (tlist); \
      } \
  } while (0)

 

static STRINGLIST *
gen_action_completions (cs, text)
     COMPSPEC *cs;
     const char *text;
{
  STRINGLIST *ret, *tmatches;
  char **cmatches;	 
  unsigned long flags;
  int t;

  ret = tmatches = (STRINGLIST *)NULL;
  flags = cs->actions;

  GEN_COMPS (flags, CA_ALIAS, &it_aliases, text, ret, tmatches);
  GEN_COMPS (flags, CA_ARRAYVAR, &it_arrayvars, text, ret, tmatches);
  GEN_COMPS (flags, CA_BINDING, &it_bindings, text, ret, tmatches);
  GEN_COMPS (flags, CA_BUILTIN, &it_builtins, text, ret, tmatches);
  GEN_COMPS (flags, CA_DISABLED, &it_disabled, text, ret, tmatches);
  GEN_COMPS (flags, CA_ENABLED, &it_enabled, text, ret, tmatches);
  GEN_COMPS (flags, CA_EXPORT, &it_exports, text, ret, tmatches);
  GEN_COMPS (flags, CA_FUNCTION, &it_functions, text, ret, tmatches);
  GEN_COMPS (flags, CA_HELPTOPIC, &it_helptopics, text, ret, tmatches);
  GEN_COMPS (flags, CA_HOSTNAME, &it_hostnames, text, ret, tmatches);
  GEN_COMPS (flags, CA_JOB, &it_jobs, text, ret, tmatches);
  GEN_COMPS (flags, CA_KEYWORD, &it_keywords, text, ret, tmatches);
  GEN_COMPS (flags, CA_RUNNING, &it_running, text, ret, tmatches);
  GEN_COMPS (flags, CA_SETOPT, &it_setopts, text, ret, tmatches);
  GEN_COMPS (flags, CA_SHOPT, &it_shopts, text, ret, tmatches);
  GEN_COMPS (flags, CA_SIGNAL, &it_signals, text, ret, tmatches);
  GEN_COMPS (flags, CA_STOPPED, &it_stopped, text, ret, tmatches);
  GEN_COMPS (flags, CA_VARIABLE, &it_variables, text, ret, tmatches);

  GEN_XCOMPS(flags, CA_COMMAND, text, command_word_completion_function, cmatches, ret, tmatches);
  GEN_XCOMPS(flags, CA_FILE, text, pcomp_filename_completion_function, cmatches, ret, tmatches);
  GEN_XCOMPS(flags, CA_USER, text, rl_username_completion_function, cmatches, ret, tmatches);
  GEN_XCOMPS(flags, CA_GROUP, text, bash_groupname_completion_function, cmatches, ret, tmatches);
  GEN_XCOMPS(flags, CA_SERVICE, text, bash_servicename_completion_function, cmatches, ret, tmatches);

   
  if (flags & CA_DIRECTORY)
    {
      t = rl_filename_completion_desired;
      rl_completion_mark_symlink_dirs = 1;	 
      cmatches = bash_directory_completion_matches (text);
       
      if (t == 0 && cmatches == 0 && rl_filename_completion_desired == 1)
        rl_filename_completion_desired = 0;
      tmatches = completions_to_stringlist (cmatches);
      ret = strlist_append (ret, tmatches);
      strvec_dispose (cmatches);
      strlist_dispose (tmatches);
    }

  return ret;
}

 
static STRINGLIST *
gen_globpat_matches (cs, text)
      COMPSPEC *cs;
      const char *text;
{
  STRINGLIST *sl;
  int gflags;

  sl = strlist_create (0);
  gflags = glob_star ? GX_GLOBSTAR : 0;
  sl->list = glob_filename (cs->globpat, gflags);
  if (GLOB_FAILED (sl->list))
    sl->list = (char **)NULL;
  if (sl->list)
    sl->list_len = sl->list_size = strvec_len (sl->list);
  return sl;
}

 
static STRINGLIST *
gen_wordlist_matches (cs, text)
     COMPSPEC *cs;
     const char *text;
{
  WORD_LIST *l, *l2;
  STRINGLIST *sl;
  int nw, tlen;
  char *ntxt;		 

  if (cs->words == 0 || cs->words[0] == '\0')
    return ((STRINGLIST *)NULL);

   
   
  l = split_at_delims (cs->words, strlen (cs->words), (char *)NULL, -1, 0, (int *)NULL, (int *)NULL);
  if (l == 0)
    return ((STRINGLIST *)NULL);
   
  l2 = expand_words_shellexp (l);
  dispose_words (l);

  nw = list_length (l2);
  sl = strlist_create (nw + 1);

  ntxt = bash_dequote_text (text);
  tlen = STRLEN (ntxt);

  for (nw = 0, l = l2; l; l = l->next)
    {
      if (tlen == 0 || STREQN (l->word->word, ntxt, tlen))
	sl->list[nw++] = STRDUP (l->word->word);
    }
  sl->list[sl->list_len = nw] = (char *)NULL;

  dispose_words (l2);
  FREE (ntxt);
  return sl;
}

#ifdef ARRAY_VARS

static SHELL_VAR *
bind_comp_words (lwords)
     WORD_LIST *lwords;
{
  SHELL_VAR *v;

  v = find_variable_noref ("COMP_WORDS");
  if (v == 0)
    v = make_new_array_variable ("COMP_WORDS");
  if (nameref_p (v))
    VUNSETATTR (v, att_nameref);
#if 0
  if (readonly_p (v))
    VUNSETATTR (v, att_readonly);
#endif
  if (array_p (v) == 0)
    v = convert_var_to_array (v);
  v = assign_array_var_from_word_list (v, lwords, 0);

  VUNSETATTR (v, att_invisible);
  return v;
}
#endif  

static void
bind_compfunc_variables (line, ind, lwords, cw, exported)
     char *line;
     int ind;
     WORD_LIST *lwords;
     int cw, exported;
{
  char ibuf[INT_STRLEN_BOUND(int) + 1];
  char *value;
  SHELL_VAR *v;
  size_t llen;
  int c;

   
  v = bind_variable ("COMP_LINE", line, 0);
  if (v && exported)
    VSETATTR(v, att_exported);

   
  c = line[ind];
  line[ind] = '\0';
  llen = MB_STRLEN (line);
  line[ind] = c;
  value = inttostr (llen, ibuf, sizeof(ibuf));
  v = bind_int_variable ("COMP_POINT", value, 0);
  if (v && exported)
    VSETATTR(v, att_exported);

  value = inttostr (rl_completion_type, ibuf, sizeof (ibuf));
  v = bind_int_variable ("COMP_TYPE", value, 0);
  if (v && exported)
    VSETATTR(v, att_exported);

  value = inttostr (rl_completion_invoking_key, ibuf, sizeof (ibuf));
  v = bind_int_variable ("COMP_KEY", value, 0);
  if (v && exported)
    VSETATTR(v, att_exported);

   
  if (exported == 0)
    {
#ifdef ARRAY_VARS
      v = bind_comp_words (lwords);
      value = inttostr (cw, ibuf, sizeof(ibuf));
      bind_int_variable ("COMP_CWORD", value, 0);
#endif
    }
  else
    array_needs_making = 1;
}

static void
unbind_compfunc_variables (exported)
     int exported;
{
  unbind_variable_noref ("COMP_LINE");
  unbind_variable_noref ("COMP_POINT");
  unbind_variable_noref ("COMP_TYPE");
  unbind_variable_noref ("COMP_KEY");
#ifdef ARRAY_VARS
  unbind_variable_noref ("COMP_WORDS");
  unbind_variable_noref ("COMP_CWORD");
#endif
  if (exported)
    array_needs_making = 1;
}

 

static WORD_LIST *
build_arg_list (cmd, cname, text, lwords, ind)
     char *cmd;
     const char *cname;
     const char *text;
     WORD_LIST *lwords;
     int ind;
{
  WORD_LIST *ret, *cl, *l;
  WORD_DESC *w;
  int i;

  ret = (WORD_LIST *)NULL;
  w = make_word (cmd);
  ret = make_word_list (w, (WORD_LIST *)NULL);	 

  w = make_word (cname);			 
  cl = ret->next = make_word_list (w, (WORD_LIST *)NULL);

  w = make_word (text);
  cl->next = make_word_list (w, (WORD_LIST *)NULL);	 
  cl = cl->next;

   
  for (l = lwords, i = 1; l && i < ind-1; l = l->next, i++)
    ;
  w = (l && l->word) ? copy_word (l->word) : make_word ("");
  cl->next = make_word_list (w, (WORD_LIST *)NULL);

  return ret;
}

 

static STRINGLIST *
gen_shell_function_matches (cs, cmd, text, line, ind, lwords, nw, cw, foundp)
     COMPSPEC *cs;
     const char *cmd;
     const char *text;
     char *line;
     int ind;
     WORD_LIST *lwords;
     int nw, cw;
     int *foundp;
{
  char *funcname;
  STRINGLIST *sl;
  SHELL_VAR *f, *v;
  WORD_LIST *cmdlist;
  int fval, found;
  sh_parser_state_t ps;
  sh_parser_state_t * restrict pps;
#if defined (ARRAY_VARS)
  ARRAY *a;
#endif

  found = 0;
  if (foundp)
    *foundp = found;

  funcname = cs->funcname;
  f = find_function (funcname);
  if (f == 0)
    {
      internal_error (_("completion: function `%s' not found"), funcname);
      rl_ding ();
      rl_on_new_line ();
      return ((STRINGLIST *)NULL);
    }

#if !defined (ARRAY_VARS)
  return ((STRINGLIST *)NULL);
#else

   
  bind_compfunc_variables (line, ind, lwords, cw - 1, 0);

  cmdlist = build_arg_list (funcname, cmd, text, lwords, cw);

  pps = &ps;
  save_parser_state (pps);
  begin_unwind_frame ("gen-shell-function-matches");
  add_unwind_protect (restore_parser_state, (char *)pps);
  add_unwind_protect (dispose_words, (char *)cmdlist);
  add_unwind_protect (unbind_compfunc_variables, (char *)0);

  fval = execute_shell_function (f, cmdlist);  

  discard_unwind_frame ("gen-shell-function-matches");
  restore_parser_state (pps);

  found = fval != EX_NOTFOUND;
  if (fval == EX_RETRYFAIL)
    found |= PCOMP_RETRYFAIL;
  if (foundp)
    *foundp = found;

   
  dispose_words (cmdlist);
  unbind_compfunc_variables (0);

   
  v = find_variable ("COMPREPLY");
  if (v == 0)
    return ((STRINGLIST *)NULL);
  if (array_p (v) == 0 && assoc_p (v) == 0)
    v = convert_var_to_array (v);

  VUNSETATTR (v, att_invisible);

  a = array_cell (v);
  if (found == 0 || (found & PCOMP_RETRYFAIL) || a == 0 || array_p (v) == 0 || array_empty (a))
    sl = (STRINGLIST *)NULL;
  else
    {
       
      sl = strlist_create (0);
      sl->list = array_to_argv (a, 0);
      sl->list_len = sl->list_size = array_num_elements (a);
    }

   
  unbind_variable_noref ("COMPREPLY");

  return (sl);
#endif
}

 

static STRINGLIST *
gen_command_matches (cs, cmd, text, line, ind, lwords, nw, cw)
     COMPSPEC *cs;
     const char *cmd;
     const char *text;
     char *line;
     int ind;
     WORD_LIST *lwords;
     int nw, cw;
{
  char *csbuf, *cscmd, *t;
  int cmdlen, cmdsize, n, ws, we;
  WORD_LIST *cmdlist, *cl;
  WORD_DESC *tw;
  STRINGLIST *sl;

  bind_compfunc_variables (line, ind, lwords, cw, 1);
  cmdlist = build_arg_list (cs->command, cmd, text, lwords, cw);

   
  n = strlen (cs->command);
  cmdsize = n + 1;
  for (cl = cmdlist->next; cl; cl = cl->next)
    cmdsize += STRLEN (cl->word->word) + 3;
  cmdsize += 2;

   
  cscmd = (char *)xmalloc (cmdsize + 1);

  strcpy (cscmd, cs->command);			 
  cmdlen = n;
  cscmd[cmdlen++] = ' ';
  for (cl = cmdlist->next; cl; cl = cl->next)    
    {
      t = sh_single_quote (cl->word->word ? cl->word->word : "");
      n = strlen (t);
      RESIZE_MALLOCED_BUFFER (cscmd, cmdlen, n + 2, cmdsize, 64);
      strcpy (cscmd + cmdlen, t);
      cmdlen += n;
      if (cl->next)
	cscmd[cmdlen++] = ' ';
      free (t);
    }
  cscmd[cmdlen] = '\0';

  tw = command_substitute (cscmd, 0, 0);
  csbuf = tw ? tw->word : (char *)NULL;
  if (tw)
    dispose_word_desc (tw);

   
  dispose_words (cmdlist);
  free (cscmd);
  unbind_compfunc_variables (1);

  if (csbuf == 0 || *csbuf == '\0')
    {
      FREE (csbuf);
      return ((STRINGLIST *)NULL);
    }

   
  sl = strlist_create (16);
  for (ws = 0; csbuf[ws]; )
    {
      we = ws;
      while (csbuf[we] && csbuf[we] != '\n')
	{
	  if (csbuf[we] == '\\' && csbuf[we+1] == '\n')
	    we++;
	  we++;
	}
      t = substring (csbuf, ws, we);
      if (sl->list_len >= sl->list_size - 1)
	strlist_resize (sl, sl->list_size + 16);
      sl->list[sl->list_len++] = t;
      while (csbuf[we] == '\n') we++;
      ws = we;
    }
  sl->list[sl->list_len] = (char *)NULL;

  free (csbuf);
  return (sl);
}

static WORD_LIST *
command_line_to_word_list (line, llen, sentinel, nwp, cwp)
     char *line;
     int llen, sentinel, *nwp, *cwp;
{
  WORD_LIST *ret;
  const char *delims;

#if 0
  delims = "()<>;&| \t\n";	 
#else
  delims = rl_completer_word_break_characters;
#endif
  ret = split_at_delims (line, llen, delims, sentinel, SD_NOQUOTEDELIM|SD_COMPLETE, nwp, cwp);
  return (ret);
}

 

STRINGLIST *
gen_compspec_completions (cs, cmd, word, start, end, foundp)
     COMPSPEC *cs;
     const char *cmd;
     const char *word;
     int start, end;
     int *foundp;
{
  STRINGLIST *ret, *tmatches;
  char *line;
  int llen, nw, cw, found, foundf;
  WORD_LIST *lwords;
  WORD_DESC *lw;
  COMPSPEC *tcs;

  found = 1;

#ifdef DEBUG
  debug_printf ("gen_compspec_completions (%s, %s, %d, %d)", cmd, word, start, end);
  debug_printf ("gen_compspec_completions: %s -> %p", cmd, cs);
#endif
  ret = gen_action_completions (cs, word);
#ifdef DEBUG
  if (ret && progcomp_debug)
    {
      debug_printf ("gen_action_completions (%p, %s) -->", cs, word);
      strlist_print (ret, "\t");
      rl_on_new_line ();
    }
#endif

   
  if (cs->globpat)
    {
      tmatches = gen_globpat_matches (cs, word);
      if (tmatches)
	{
#ifdef DEBUG
	  if (progcomp_debug)
	    {
	      debug_printf ("gen_globpat_matches (%p, %s) -->", cs, word);
	      strlist_print (tmatches, "\t");
	      rl_on_new_line ();
	    }
#endif
	  ret = strlist_append (ret, tmatches);
	  strlist_dispose (tmatches);
	  rl_filename_completion_desired = 1;
	}
    }

  if (cs->words)
    {
      tmatches = gen_wordlist_matches (cs, word);
      if (tmatches)
	{
#ifdef DEBUG
	  if (progcomp_debug)
	    {
	      debug_printf ("gen_wordlist_matches (%p, %s) -->", cs, word);
	      strlist_print (tmatches, "\t");
	      rl_on_new_line ();
	    }
#endif
	  ret = strlist_append (ret, tmatches);
	  strlist_dispose (tmatches);
	}
    }

  lwords = (WORD_LIST *)NULL;
  line = (char *)NULL;
  if (cs->command || cs->funcname)
    {
       
      line = substring (pcomp_line, start, end);
      llen = end - start;

#ifdef DEBUG
      debug_printf ("command_line_to_word_list (%s, %d, %d, %p, %p)",
		line, llen, pcomp_ind - start, &nw, &cw);
#endif
      lwords = command_line_to_word_list (line, llen, pcomp_ind - start, &nw, &cw);
       
      if (lwords && lwords->word && cmd[0] == 0 && lwords->word->word[0] != 0)
	{
	  lw = make_bare_word (cmd);
	  lwords = make_word_list (lw, lwords);
	  nw++;
	  cw++;
	}
#ifdef DEBUG
      if (lwords == 0 && llen > 0)
	debug_printf ("ERROR: command_line_to_word_list returns NULL");
      else if (progcomp_debug)
	{
	  debug_printf ("command_line_to_word_list -->");
	  printf ("\t");
	  print_word_list (lwords, "!");
	  printf ("\n");
	  fflush(stdout);
	  rl_on_new_line ();
	}
#endif
    }

  if (cs->funcname)
    {
      foundf = 0;
      tmatches = gen_shell_function_matches (cs, cmd, word, line, pcomp_ind - start, lwords, nw, cw, &foundf);
      if (foundf != 0)
	found = foundf;
      if (tmatches)
	{
#ifdef DEBUG
	  if (progcomp_debug)
	    {
	      debug_printf ("gen_shell_function_matches (%p, %s, %s, %p, %d, %d) -->", cs, cmd, word, lwords, nw, cw);
	      strlist_print (tmatches, "\t");
	      rl_on_new_line ();
	    }
#endif
	  ret = strlist_append (ret, tmatches);
	  strlist_dispose (tmatches);
	}
    }

  if (cs->command)
    {
      tmatches = gen_command_matches (cs, cmd, word, line, pcomp_ind - start, lwords, nw, cw);
      if (tmatches)
	{
#ifdef DEBUG
	  if (progcomp_debug)
	    {
	      debug_printf ("gen_command_matches (%p, %s, %s, %p, %d, %d) -->", cs, cmd, word, lwords, nw, cw);
	      strlist_print (tmatches, "\t");
	      rl_on_new_line ();
	    }
#endif
	  ret = strlist_append (ret, tmatches);
	  strlist_dispose (tmatches);
	}
    }

  if (cs->command || cs->funcname)
    {
      if (lwords)
	dispose_words (lwords);
      FREE (line);
    }

  if (foundp)
    *foundp = found;

  if (found == 0 || (found & PCOMP_RETRYFAIL))
    {
      strlist_dispose (ret);
      return NULL;
    }

  if (cs->filterpat)
    {
      tmatches = filter_stringlist (ret, cs->filterpat, word);
#ifdef DEBUG
      if (progcomp_debug)
	{
	  debug_printf ("filter_stringlist (%p, %s, %s) -->", ret, cs->filterpat, word);
	  strlist_print (tmatches, "\t");
	  rl_on_new_line ();
	}
#endif
      if (ret && ret != tmatches)
	{
	  FREE (ret->list);
	  free (ret);
	}
      ret = tmatches;
    }

  if (cs->prefix || cs->suffix)
    ret = strlist_prefix_suffix (ret, cs->prefix, cs->suffix);

   
  if ((ret == 0 || ret->list_len == 0) && (cs->options & COPT_DIRNAMES))
    {
      tcs = compspec_create ();
      tcs->actions = CA_DIRECTORY;
      FREE (ret);
      ret = gen_action_completions (tcs, word);
      compspec_dispose (tcs);
    }
  else if (cs->options & COPT_PLUSDIRS)
    {
      tcs = compspec_create ();
      tcs->actions = CA_DIRECTORY;
      tmatches = gen_action_completions (tcs, word);
      ret = strlist_append (ret, tmatches);
      strlist_dispose (tmatches);
      compspec_dispose (tcs);
    }

  return (ret);
}

void
pcomp_set_readline_variables (flags, nval)
     int flags, nval;
{
   
  if (flags & COPT_FILENAMES)
    rl_filename_completion_desired = nval;
   
  if (flags & COPT_NOSPACE)
    rl_completion_suppress_append = nval;
   
  if (flags & COPT_NOQUOTE)
    rl_filename_quoting_desired = 1 - nval;
  if (flags & COPT_NOSORT)
    rl_sort_completion_matches = 1 - nval;
}

 
void
pcomp_set_compspec_options (cs, flags, set_or_unset)
     COMPSPEC *cs;
     int flags, set_or_unset;
{
  if (cs == 0 && ((cs = pcomp_curcs) == 0))
    return;
  if (set_or_unset)
    cs->options |= flags;
  else
    cs->options &= ~flags;
}

static STRINGLIST *
gen_progcomp_completions (ocmd, cmd, word, start, end, foundp, retryp, lastcs)
     const char *ocmd;
     const char *cmd;
     const char *word;
     int start, end;
     int *foundp, *retryp;
     COMPSPEC **lastcs;
{
  COMPSPEC *cs, *oldcs;
  const char *oldcmd, *oldtxt;
  STRINGLIST *ret;

  cs = progcomp_search (ocmd);

  if (cs == 0 || cs == *lastcs)
    {
#if 0
      if (foundp)
	*foundp = 0;
#endif
      return (NULL);
    }

  if (*lastcs)
    compspec_dispose (*lastcs);
  cs->refcount++;	 
  *lastcs = cs;

  cs = compspec_copy (cs);

  oldcs = pcomp_curcs;
  oldcmd = pcomp_curcmd;
  oldtxt = pcomp_curtxt;

  pcomp_curcs = cs;
  pcomp_curcmd = cmd;
  pcomp_curtxt = word;

  ret = gen_compspec_completions (cs, cmd, word, start, end, foundp);

  pcomp_curcs = oldcs;
  pcomp_curcmd = oldcmd;
  pcomp_curtxt = oldtxt;

   
  if (retryp)
    *retryp = foundp && (*foundp & PCOMP_RETRYFAIL);    	

  if (foundp)
    {
      *foundp &= ~PCOMP_RETRYFAIL;
      *foundp |= cs->options;
    }

  compspec_dispose (cs);
  return ret;  
}

 
char **
programmable_completions (cmd, word, start, end, foundp)
     const char *cmd;
     const char *word;
     int start, end, *foundp;
{
  COMPSPEC *lastcs;
  STRINGLIST *ret;
  char **rmatches, *t;
  int found, retry, count;
  char *ocmd;
  int oend;
#if defined (ALIAS)
  alias_t *al;
#endif

  lastcs = 0;
  found = count = 0;

  pcomp_line = rl_line_buffer;
  pcomp_ind = rl_point;

  ocmd = (char *)cmd;
  oend = end;

  do
    {
      retry = 0;

       
      ret = gen_progcomp_completions (ocmd, ocmd, word, start, oend, &found, &retry, &lastcs);
      if (found == 0)
	{
	  t = strrchr (ocmd, '/');
	  if (t && *(++t))
	    ret = gen_progcomp_completions (t, ocmd, word, start, oend, &found, &retry, &lastcs);
	}

      if (found == 0)
	ret = gen_progcomp_completions (DEFAULTCMD, ocmd, word, start, oend, &found, &retry, &lastcs);

#if defined (ALIAS)
       
       
      if (found == 0 && retry == 0 && progcomp_alias && (al = find_alias (ocmd)))
	{
	  char *ncmd, *nline, *ntxt;
	  int ind, lendiff;
	  size_t nlen, olen, llen;

	   
	  ntxt = al->value;
	  nlen = strlen (ntxt);
	  if (nlen == 0)
	    break;
	  olen = strlen (ocmd);
	  lendiff = nlen - olen;	 
	  llen = strlen (pcomp_line);

	  nline = (char *)xmalloc (llen + lendiff + 1);
	  if (start > 0)
	    strncpy (nline, pcomp_line, start);
	  strncpy (nline + start, ntxt, nlen);
	  strcpy (nline + start + nlen, pcomp_line + start + olen);

	   
	  ind = skip_to_delim (ntxt, 0, "()<>;&| \t\n", SD_NOJMP|SD_COMPLETE);
	  if (ind > 0)
	    ncmd = substring (ntxt, 0, ind);
	  else
	    {
	      free (nline);
	      break;		 
	    }

	   
	  pcomp_ind += lendiff;
	  oend += lendiff;

	   
	  if (ocmd != cmd)
	    free (ocmd);
	  if (pcomp_line != rl_line_buffer)
	    free (pcomp_line);

	  ocmd = ncmd;
	  pcomp_line = nline;

	   
	  retry = 1;
	}
#endif  

      count++;

      if (count > 32)
	{
	  internal_warning (_("programmable_completion: %s: possible retry loop"), cmd);
	  break;
	}
    }
  while (retry);

  if (pcomp_line != rl_line_buffer)
    free (pcomp_line);
  if (ocmd != cmd)
    free (ocmd);

  if (ret)
    {
      rmatches = ret->list;
      free (ret);
    }
  else
    rmatches = (char **)NULL;

  if (foundp)
    *foundp = found;

  if (lastcs)	 
    compspec_dispose (lastcs);

   
  pcomp_line = rl_line_buffer;
  pcomp_ind = rl_point;

  return (rmatches);
}

#endif  
