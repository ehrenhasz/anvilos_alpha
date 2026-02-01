 

 

#include "config.h"

#if defined (HISTORY)

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
 #    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include "bashtypes.h"
#include <stdio.h>
#include <errno.h>
#include "bashansi.h"
#include "posixstat.h"
#include "filecntl.h"

#include "bashintl.h"

#if defined (SYSLOG_HISTORY)
#  include <syslog.h>
#endif

#include "shell.h"
#include "flags.h"
#include "parser.h"
#include "input.h"
#include "parser.h"	 
#include "pathexp.h"	 
#include "bashhist.h"	 
#include "builtins/common.h"

#include <readline/history.h>
#include <glob/glob.h>
#include <glob/strmatch.h>

#if defined (READLINE)
#  include "bashline.h"
extern int rl_done, rl_dispatching;	 
#endif

#ifndef HISTSIZE_DEFAULT
#  define HISTSIZE_DEFAULT "500"
#endif

#if !defined (errno)
extern int errno;
#endif

static int histignore_item_func PARAMS((struct ign *));
static int check_history_control PARAMS((char *));
static void hc_erasedups PARAMS((char *));
static void really_add_history PARAMS((char *));

static struct ignorevar histignore =
{
  "HISTIGNORE",
  (struct ign *)0,
  0,
  (char *)0,
  (sh_iv_item_func_t *)histignore_item_func,
};

#define HIGN_EXPAND 0x01

 
 
int remember_on_history = 0;
int enable_history_list = -1;	 

 
int history_lines_this_session;

 
int history_lines_in_file;

#if defined (BANG_HISTORY)
 
int history_expansion_inhibited;
 
int double_quotes_inhibit_history_expansion = 0;
#endif

 
int command_oriented_history = 1;

 
int current_command_first_line_saved = 0;

 
int current_command_line_comment = 0;

 
int literal_history;

 
int force_append_history;

 
int history_control;

 
int hist_last_line_added;

 
int hist_last_line_pushed;

#if defined (READLINE)
 
int history_reediting;

 
int hist_verify;

#endif  

 
int dont_save_function_defs;

#if defined (BANG_HISTORY)
static int bash_history_inhibit_expansion PARAMS((char *, int));
#endif
#if defined (READLINE)
static void re_edit PARAMS((char *));
#endif
static int history_expansion_p PARAMS((char *));
static int shell_comment PARAMS((char *));
static int should_expand PARAMS((char *));
static HIST_ENTRY *last_history_entry PARAMS((void));
static char *expand_histignore_pattern PARAMS((char *));
static int history_should_ignore PARAMS((char *));

#if defined (BANG_HISTORY)
 
static int
bash_history_inhibit_expansion (string, i)
     char *string;
     int i;
{
  int t, si;
  char hx[2];

  hx[0] = history_expansion_char;
  hx[1] = '\0';

   
  if (i > 0 && (string[i - 1] == '[') && member (']', string + i + 1))
    return (1);
   
  else if (i > 1 && string[i - 1] == '{' && string[i - 2] == '$' &&
	     member ('}', string + i + 1))
    return (1);
   
  else if (i > 1 && string[i - 1] == '$' && string[i] == '!')
    return (1);
#if defined (EXTENDED_GLOB)
  else if (extended_glob && i > 1 && string[i+1] == '(' && member (')', string + i + 2))
    return (1);
#endif

  si = 0;
   
  if (history_quoting_state == '\'')
    {
      si = skip_to_delim (string, 0, "'", SD_NOJMP|SD_HISTEXP);
      if (string[si] == 0 || si >= i)
	return (1);
      si++;
    }

   
  if ((t = skip_to_histexp (string, si, hx, SD_NOJMP|SD_HISTEXP)) > 0)
    {
       
      while (t < i)
	{
	  t = skip_to_histexp (string, t+1, hx, SD_NOJMP|SD_HISTEXP);
	  if (t <= 0)
	    return 0;
	}
      return (t > i);
    }
  else
    return (0);
}
#endif

void
bash_initialize_history ()
{
  history_quotes_inhibit_expansion = 1;
  history_search_delimiter_chars = ";&()|<>";
#if defined (BANG_HISTORY)
  history_inhibit_expansion_function = bash_history_inhibit_expansion;
  sv_histchars ("histchars");
#endif
}

void
bash_history_reinit (interact)
     int interact;
{
#if defined (BANG_HISTORY)
  history_expansion = (interact == 0) ? histexp_flag : HISTEXPAND_DEFAULT;
  history_expansion_inhibited = (interact == 0) ? 1 - histexp_flag : 0;	 
  history_inhibit_expansion_function = bash_history_inhibit_expansion;
#endif
  remember_on_history = enable_history_list;
}

void
bash_history_disable ()
{
  remember_on_history = 0;
#if defined (BANG_HISTORY)
  history_expansion_inhibited = 1;
#endif
}

void
bash_history_enable ()
{
  remember_on_history = enable_history_list = 1;
#if defined (BANG_HISTORY)
  history_expansion_inhibited = 0;
  history_inhibit_expansion_function = bash_history_inhibit_expansion;
#endif
  sv_history_control ("HISTCONTROL");
  sv_histignore ("HISTIGNORE");
}

 
void
load_history ()
{
  char *hf;

   
  set_if_not ("HISTSIZE", HISTSIZE_DEFAULT);
  sv_histsize ("HISTSIZE");

  set_if_not ("HISTFILESIZE", get_string_value ("HISTSIZE"));
  sv_histsize ("HISTFILESIZE");

   
  hf = get_string_value ("HISTFILE");

  if (hf && *hf && file_exists (hf))
    {
      read_history (hf);
       
      history_lines_in_file = history_lines_read_from_file;
      using_history ();
       
    }
}

void
bash_clear_history ()
{
  clear_history ();
  history_lines_this_session = 0;
   
}

 
int
bash_delete_histent (i)
     int i;
{
  HIST_ENTRY *discard;

  discard = remove_history (i);
  if (discard)
    {
      free_history_entry (discard);
      history_lines_this_session--;
    }
  return discard != 0;
}

int
bash_delete_history_range (first, last)
     int first, last;
{
  register int i;
  HIST_ENTRY **discard_list;

  discard_list = remove_history_range (first, last);
  if (discard_list == 0)
    return 0;
  for (i = 0; discard_list[i]; i++)
    free_history_entry (discard_list[i]);
  free (discard_list);
  history_lines_this_session -= i;

  return 1;
}

int
bash_delete_last_history ()
{
  register int i;
  HIST_ENTRY **hlist, *histent;
  int r;

  hlist = history_list ();
  if (hlist == NULL)
    return 0;

  for (i = 0; hlist[i]; i++)
    ;
  i--;

   
  histent = history_get (history_base + i);	 
  if (histent == NULL)
    return 0;

  r = bash_delete_histent (i);

  if (where_history () > history_length)
    history_set_pos (history_length);

  return r;
}

#ifdef INCLUDE_UNUSED
 
void
save_history ()
{
  char *hf;
  int r;

  hf = get_string_value ("HISTFILE");
  if (hf && *hf && file_exists (hf))
    {
       
      using_history ();

      if (history_lines_this_session <= where_history () || force_append_history)
	r = append_history (history_lines_this_session, hf);
      else
	r = write_history (hf);
      sv_histsize ("HISTFILESIZE");
    }
}
#endif

int
maybe_append_history (filename)
     char *filename;
{
  int fd, result, histlen;
  struct stat buf;

  result = EXECUTION_SUCCESS;
  if (history_lines_this_session > 0)
    {
       
      if (stat (filename, &buf) == -1 && errno == ENOENT)
	{
	  fd = open (filename, O_WRONLY|O_CREAT, 0600);
	  if (fd < 0)
	    {
	      builtin_error (_("%s: cannot create: %s"), filename, strerror (errno));
	      return (EXECUTION_FAILURE);
	    }
	  close (fd);
	}
       
      histlen = where_history ();
      if (histlen > 0 && history_lines_this_session > histlen)
	history_lines_this_session = histlen;	 
      result = append_history (history_lines_this_session, filename);
       
      history_lines_in_file += history_lines_this_session;
      history_lines_this_session = 0;
    }
  else
    history_lines_this_session = 0;	 

  return (result);
}

 
int
maybe_save_shell_history ()
{
  int result;
  char *hf;

  result = 0;
  if (history_lines_this_session > 0)
    {
      hf = get_string_value ("HISTFILE");

      if (hf && *hf)
	{
	   
	  if (file_exists (hf) == 0)
	    {
	      int file;
	      file = open (hf, O_CREAT | O_TRUNC | O_WRONLY, 0600);
	      if (file != -1)
		close (file);
	    }

	   
	  using_history ();
	  if (history_lines_this_session <= where_history () || force_append_history)
	    {
	      result = append_history (history_lines_this_session, hf);
	      history_lines_in_file += history_lines_this_session;
	    }
	  else
	    {
	      result = write_history (hf);
	      history_lines_in_file = history_lines_written_to_file;
	       
	    }
	  history_lines_this_session = 0;

	  sv_histsize ("HISTFILESIZE");
	}
    }
  return (result);
}

#if defined (READLINE)
 
static void
re_edit (text)
     char *text;
{
  if (bash_input.type == st_stdin)
    bash_re_edit (text);
}
#endif  

 
static int
history_expansion_p (line)
     char *line;
{
  register char *s;

  for (s = line; *s; s++)
    if (*s == history_expansion_char || *s == history_subst_char)
      return 1;
  return 0;
}

 
char *
pre_process_line (line, print_changes, addit)
     char *line;
     int print_changes, addit;
{
  char *history_value;
  char *return_value;
  int expanded;

  return_value = line;
  expanded = 0;

#  if defined (BANG_HISTORY)
   
  if (!history_expansion_inhibited && history_expansion && history_expansion_p (line))
    {
      int old_len;

       
      old_len = history_length;
      if (history_length > 0 && command_oriented_history && current_command_first_line_saved && current_command_line_count > 1)
        history_length--;
      expanded = history_expand (line, &history_value);
      if (history_length >= 0 && command_oriented_history && current_command_first_line_saved && current_command_line_count > 1)
        history_length = old_len;

      if (expanded)
	{
	  if (print_changes)
	    {
	      if (expanded < 0)
		internal_error ("%s", history_value);
#if defined (READLINE)
	      else if (hist_verify == 0 || expanded == 2)
#else
	      else
#endif
		fprintf (stderr, "%s\n", history_value);
	    }

	   
	  if (expanded < 0 || expanded == 2)	 
	    {
#    if defined (READLINE)
	      if (expanded == 2 && rl_dispatching == 0 && *history_value)
#    else	      
	      if (expanded == 2 && *history_value)
#    endif  
		maybe_add_history (history_value);

	      free (history_value);

#    if defined (READLINE)
	       
	      if (history_reediting && expanded < 0 && rl_done)
		re_edit (line);
#    endif  
	      return ((char *)NULL);
	    }

#    if defined (READLINE)
	  if (hist_verify && expanded == 1)
	    {
	      re_edit (history_value);
	      free (history_value);
	      return ((char *)NULL);
	    }
#    endif
	}

       
      expanded = 1;
      return_value = history_value;
    }
#  endif  

  if (addit && remember_on_history && *return_value)
    maybe_add_history (return_value);

#if 0
  if (expanded == 0)
    return_value = savestring (line);
#endif

  return (return_value);
}

 
static int
shell_comment (line)
     char *line;
{
  char *p;
  int n;

  if (dstack.delimiter_depth != 0 || (parser_state & PST_HEREDOC))
    return 0;
  if (line == 0)
    return 0;
  for (p = line; p && *p && whitespace (*p); p++)
    ;
  if (p && *p == '#')
    return 1;
  n = skip_to_delim (line, p - line, "#", SD_NOJMP|SD_GLOB|SD_EXTGLOB|SD_COMPLETE);
  return (line[n] == '#') ? 2 : 0;
}

#ifdef INCLUDE_UNUSED
 
static char *
filter_comments (line)
     char *line;
{
  char *p;

  for (p = line; p && *p && *p != '#'; p++)
    ;
  if (p && *p == '#')
    *p = '\0';
  return (line);
}
#endif

 
static int
check_history_control (line)
     char *line;
{
  HIST_ENTRY *temp;
  int r;

  if (history_control == 0)
    return 1;

   
  if ((history_control & HC_IGNSPACE) && *line == ' ')
    return 0;

   
  if (history_control & HC_IGNDUPS)
    {
      using_history ();
      temp = previous_history ();

      r = (temp == 0 || STREQ (temp->line, line) == 0);

      using_history ();

      if (r == 0)
	return r;
    }

  return 1;
}

 
static void
hc_erasedups (line)
     char *line;
{
  HIST_ENTRY *temp;
  int r;

  using_history ();
  while (temp = previous_history ())
    {
      if (STREQ (temp->line, line))
	{
	  r = where_history ();
	  temp = remove_history (r);
	  if (temp)
	    free_history_entry (temp);
	}
    }
  using_history ();
}

 
void
maybe_add_history (line)
     char *line;
{
  int is_comment;

  hist_last_line_added = 0;
  is_comment = shell_comment (line);

   
  if (current_command_line_count > 1)
    {
      if (current_command_first_line_saved &&
	  ((parser_state & PST_HEREDOC) || literal_history || dstack.delimiter_depth != 0 || is_comment != 1))
	bash_add_history (line);
      current_command_line_comment = is_comment ? current_command_line_count : -2;
      return;
    }

   
  current_command_line_comment = is_comment ? current_command_line_count : -2;
  current_command_first_line_saved = check_add_history (line, 0);
}

 
int
check_add_history (line, force)
     char *line;
     int force;
{
  if (check_history_control (line) && history_should_ignore (line) == 0)
    {
       
      if (history_control & HC_ERASEDUPS)
	hc_erasedups (line);
        
      if (force)
	{
	  really_add_history (line);
	  using_history ();
	}
      else
	bash_add_history (line);
      return 1;
    }
  return 0;
}

#if defined (SYSLOG_HISTORY)
#define SYSLOG_MAXMSG	1024
#define SYSLOG_MAXLEN	SYSLOG_MAXMSG
#define SYSLOG_MAXHDR	256

#ifndef OPENLOG_OPTS
#define OPENLOG_OPTS 0
#endif

#if defined (SYSLOG_SHOPT)
int syslog_history = SYSLOG_SHOPT;
#else
int syslog_history = 1;
#endif

void
bash_syslog_history (line)
     const char *line;
{
  char trunc[SYSLOG_MAXLEN], *msg;
  char loghdr[SYSLOG_MAXHDR];
  char seqbuf[32], *seqnum;
  int hdrlen, msglen, seqlen, chunks, i;
  static int first = 1;

  if (first)
    {
      openlog (shell_name, OPENLOG_OPTS, SYSLOG_FACILITY);
      first = 0;
    }

  hdrlen = snprintf (loghdr, sizeof(loghdr), "HISTORY: PID=%d UID=%d", getpid(), current_user.uid);
  msglen = strlen (line);

  if ((msglen + hdrlen + 1) < SYSLOG_MAXLEN)
    syslog (SYSLOG_FACILITY|SYSLOG_LEVEL, "%s %s", loghdr, line);
  else
    {
      chunks = ((msglen + hdrlen) / SYSLOG_MAXLEN) + 1;
      for (msg = line, i = 0; i < chunks; i++)
	{
	  seqnum = inttostr (i + 1, seqbuf, sizeof (seqbuf));
	  seqlen = STRLEN (seqnum);

	   
	  strncpy (trunc, msg, SYSLOG_MAXLEN - hdrlen - seqlen - 7 - 1);
	  trunc[SYSLOG_MAXLEN - 1] = '\0';
	  syslog (SYSLOG_FACILITY|SYSLOG_LEVEL, "%s (seq=%s) %s", loghdr, seqnum, trunc);
	  msg += SYSLOG_MAXLEN - hdrlen - seqlen - 8;
	}
    }
}
#endif
     	
 
void
bash_add_history (line)
     char *line;
{
  int add_it, offset, curlen, is_comment;
  HIST_ENTRY *current, *old;
  char *chars_to_add, *new_line;

  add_it = 1;
  if (command_oriented_history && current_command_line_count > 1)
    {
      is_comment = shell_comment (line);

       
       
      if ((parser_state & PST_HEREDOC) && here_doc_first_line == 0 && line[strlen (line) - 1] == '\n')
	chars_to_add = "";
      else if (current_command_line_count == current_command_line_comment+1)
	chars_to_add = "\n";
      else if (literal_history)
	chars_to_add = "\n";
      else
	chars_to_add = history_delimiting_chars (line);

      using_history ();
      current = previous_history ();

      current_command_line_comment = is_comment ? current_command_line_count : -2;

      if (current)
	{
	   
	  curlen = strlen (current->line);

	  if (dstack.delimiter_depth == 0 && current->line[curlen - 1] == '\\' &&
	      current->line[curlen - 2] != '\\')
	    {
	      current->line[curlen - 1] = '\0';
	      curlen--;
	      chars_to_add = "";
	    }

	   
	  if (dstack.delimiter_depth == 0 && current->line[curlen - 1] == '\n' && *chars_to_add == ';')
	    chars_to_add++;

	  new_line = (char *)xmalloc (1
				      + curlen
				      + strlen (line)
				      + strlen (chars_to_add));
	  sprintf (new_line, "%s%s%s", current->line, chars_to_add, line);
	  offset = where_history ();
	  old = replace_history_entry (offset, new_line, current->data);
	  free (new_line);

	  if (old)
	    free_history_entry (old);

	  add_it = 0;
	}
    }

  if (add_it && history_is_stifled() && history_length == 0 && history_length == history_max_entries)
    add_it = 0;

  if (add_it)
    really_add_history (line);

#if defined (SYSLOG_HISTORY)
  if (syslog_history)
    bash_syslog_history (line);
#endif

  using_history ();
}

static void
really_add_history (line)
     char *line;
{
  hist_last_line_added = 1;
  hist_last_line_pushed = 0;
  add_history (line);
  history_lines_this_session++;
}

int
history_number ()
{
  using_history ();
  return ((remember_on_history || enable_history_list) ? history_base + where_history () : 1);
}

static int
should_expand (s)
     char *s;
{
  char *p;

  for (p = s; p && *p; p++)
    {
      if (*p == '\\')
	p++;
      else if (*p == '&')
	return 1;
    }
  return 0;
}

static int
histignore_item_func (ign)
     struct ign *ign;
{
  if (should_expand (ign->val))
    ign->flags |= HIGN_EXPAND;
  return (0);
}

void
setup_history_ignore (varname)
     char *varname;
{
  setup_ignore_patterns (&histignore);
}

static HIST_ENTRY *
last_history_entry ()
{
  HIST_ENTRY *he;

  using_history ();
  he = previous_history ();
  using_history ();
  return he;
}

char *
last_history_line ()
{
  HIST_ENTRY *he;

  he = last_history_entry ();
  if (he == 0)
    return ((char *)NULL);
  return he->line;
}

static char *
expand_histignore_pattern (pat)
     char *pat;
{
  HIST_ENTRY *phe;
  char *ret;

  phe = last_history_entry ();

  if (phe == (HIST_ENTRY *)0)
    return (savestring (pat));

  ret = strcreplace (pat, '&', phe->line, 1);

  return ret;
}

 
static int
history_should_ignore (line)
     char *line;
{
  register int i, match;
  char *npat;

  if (histignore.num_ignores == 0)
    return 0;

  for (i = match = 0; i < histignore.num_ignores; i++)
    {
      if (histignore.ignores[i].flags & HIGN_EXPAND)
	npat = expand_histignore_pattern (histignore.ignores[i].val);
      else
	npat = histignore.ignores[i].val;

      match = strmatch (npat, line, FNMATCH_EXTFLAG) != FNM_NOMATCH;

      if (histignore.ignores[i].flags & HIGN_EXPAND)
	free (npat);

      if (match)
	break;
    }

  return match;
}
#endif  
