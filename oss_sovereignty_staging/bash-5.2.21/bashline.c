 

 

#include "config.h"

#if defined (READLINE)

#include "bashtypes.h"
#include "posixstat.h"

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#if defined (HAVE_GRP_H)
#  include <grp.h>
#endif

#if defined (HAVE_NETDB_H)
#  include <netdb.h>
#endif

#include <signal.h>

#include <stdio.h>
#include "chartypes.h"
#include "bashansi.h"
#include "bashintl.h"

#include "shell.h"
#include "input.h"
#include "parser.h"
#include "builtins.h"
#include "bashhist.h"
#include "bashline.h"
#include "execute_cmd.h"
#include "findcmd.h"
#include "pathexp.h"
#include "shmbutil.h"
#include "trap.h"
#include "flags.h"
#include "timer.h"

#if defined (HAVE_MBSTR_H) && defined (HAVE_MBSCHR)
#  include <mbstr.h>		 
#endif

#include "builtins/common.h"
#include "builtins/builtext.h"		 

#include <readline/rlconf.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <readline/rlmbutil.h>

#include <glob/glob.h>

#if defined (ALIAS)
#  include "alias.h"
#endif

#if defined (PROGRAMMABLE_COMPLETION)
#  include "pcomplete.h"
#endif

 
#ifndef EMACS_EDITING_MODE
#  define NO_EDITING_MODE	-1
#  define EMACS_EDITING_MODE	 1
#  define VI_EDITING_MODE	 0
#endif

 
#ifndef FUNCTION_TO_KEYMAP

#if defined (CRAY)
#  define FUNCTION_TO_KEYMAP(map, key)	(Keymap)((int)map[key].function)
#  define KEYMAP_TO_FUNCTION(data)	(rl_command_func_t *)((int)(data))
#else
#  define FUNCTION_TO_KEYMAP(map, key)	(Keymap)(map[key].function)
#  define KEYMAP_TO_FUNCTION(data)	(rl_command_func_t *)(data)
#endif

#endif

#define RL_BOOLEAN_VARIABLE_VALUE(s)	((s)[0] == 'o' && (s)[1] == 'n' && (s)[2] == '\0')

#if defined (BRACE_COMPLETION)
extern int bash_brace_completion PARAMS((int, int));
#endif  

 
#ifdef _MINIX
extern int tputs PARAMS((const char *string, int nlines, void (*outx)(int)));
#else
extern int tputs PARAMS((const char *string, int nlines, int (*outx)(int)));
#endif

 

 
static int shell_expand_line PARAMS((int, int));
static int display_shell_version PARAMS((int, int));

static int bash_ignore_filenames PARAMS((char **));
static int bash_ignore_everything PARAMS((char **));
static int bash_progcomp_ignore_filenames PARAMS((char **));

#if defined (BANG_HISTORY)
static char *history_expand_line_internal PARAMS((char *));
static int history_expand_line PARAMS((int, int));
static int tcsh_magic_space PARAMS((int, int));
#endif  
#ifdef ALIAS
static int alias_expand_line PARAMS((int, int));
#endif
#if defined (BANG_HISTORY) && defined (ALIAS)
static int history_and_alias_expand_line PARAMS((int, int));
#endif

static int bash_forward_shellword PARAMS((int, int));
static int bash_backward_shellword PARAMS((int, int));
static int bash_kill_shellword PARAMS((int, int));
static int bash_backward_kill_shellword PARAMS((int, int));
static int bash_transpose_shellwords PARAMS((int, int));

static int bash_spell_correct_shellword PARAMS((int, int));

 
static char *restore_tilde PARAMS((char *, char *));
static char *maybe_restore_tilde PARAMS((char *, char *));

static char *bash_filename_rewrite_hook PARAMS((char *, int));

static void bash_directory_expansion PARAMS((char **));
static int bash_filename_stat_hook PARAMS((char **));
static int bash_command_name_stat_hook PARAMS((char **));
static int bash_directory_completion_hook PARAMS((char **));
static int filename_completion_ignore PARAMS((char **));
static int bash_push_line PARAMS((void));

static int executable_completion PARAMS((const char *, int));

static rl_icppfunc_t *save_directory_hook PARAMS((void));
static void restore_directory_hook PARAMS((rl_icppfunc_t));

static int directory_exists PARAMS((const char *, int));

static void cleanup_expansion_error PARAMS((void));
static void maybe_make_readline_line PARAMS((char *));
static void set_up_new_line PARAMS((char *));

static int check_redir PARAMS((int));
static char **attempt_shell_completion PARAMS((const char *, int, int));
static char *variable_completion_function PARAMS((const char *, int));
static char *hostname_completion_function PARAMS((const char *, int));
static char *command_subst_completion_function PARAMS((const char *, int));

static void build_history_completion_array PARAMS((void));
static char *history_completion_generator PARAMS((const char *, int));
static int dynamic_complete_history PARAMS((int, int));
static int bash_dabbrev_expand PARAMS((int, int));

static void initialize_hostname_list PARAMS((void));
static void add_host_name PARAMS((char *));
static void snarf_hosts_from_file PARAMS((char *));
static char **hostnames_matching PARAMS((char *));

static void _ignore_completion_names PARAMS((char **, sh_ignore_func_t *));
static int name_is_acceptable PARAMS((const char *));
static int test_for_directory PARAMS((const char *));
static int test_for_canon_directory PARAMS((const char *));
static int return_zero PARAMS((const char *));

static char *bash_dequote_filename PARAMS((char *, int));
static char *quote_word_break_chars PARAMS((char *));
static int bash_check_expchar PARAMS((char *, int, int *, int *));
static void set_filename_quote_chars PARAMS((int, int, int));
static void set_filename_bstab PARAMS((const char *));
static char *bash_quote_filename PARAMS((char *, int, char *));

#ifdef _MINIX
static void putx PARAMS((int));
#else
static int putx PARAMS((int));
#endif
static int readline_get_char_offset PARAMS((int));
static void readline_set_char_offset PARAMS((int, int *));

static Keymap get_cmd_xmap_from_edit_mode PARAMS((void));
static Keymap get_cmd_xmap_from_keymap PARAMS((Keymap));

static void init_unix_command_map PARAMS((void));
static int isolate_sequence PARAMS((char *, int, int, int *));

static int set_saved_history PARAMS((void));

#if defined (ALIAS)
static int posix_edit_macros PARAMS((int, int));
#endif

static int bash_event_hook PARAMS((void));

#if defined (PROGRAMMABLE_COMPLETION)
static int find_cmd_start PARAMS((int));
static int find_cmd_end PARAMS((int));
static char *find_cmd_name PARAMS((int, int *, int *));
static char *prog_complete_return PARAMS((const char *, int));

static char **prog_complete_matches;
#endif

extern int no_symbolic_links;
extern STRING_INT_ALIST word_token_alist[];
extern sh_timer *read_timeout;

 
#define SPECIFIC_COMPLETION_FUNCTIONS

#if defined (SPECIFIC_COMPLETION_FUNCTIONS)
static int bash_specific_completion PARAMS((int, rl_compentry_func_t *));

static int bash_complete_filename_internal PARAMS((int));
static int bash_complete_username_internal PARAMS((int));
static int bash_complete_hostname_internal PARAMS((int));
static int bash_complete_variable_internal PARAMS((int));
static int bash_complete_command_internal PARAMS((int));

static int bash_complete_filename PARAMS((int, int));
static int bash_possible_filename_completions PARAMS((int, int));
static int bash_complete_username PARAMS((int, int));
static int bash_possible_username_completions PARAMS((int, int));
static int bash_complete_hostname PARAMS((int, int));
static int bash_possible_hostname_completions PARAMS((int, int));
static int bash_complete_variable PARAMS((int, int));
static int bash_possible_variable_completions PARAMS((int, int));
static int bash_complete_command PARAMS((int, int));
static int bash_possible_command_completions PARAMS((int, int));

static int completion_glob_pattern PARAMS((char *));
static char *glob_complete_word PARAMS((const char *, int));
static int bash_glob_completion_internal PARAMS((int));
static int bash_glob_complete_word PARAMS((int, int));
static int bash_glob_expand_word PARAMS((int, int));
static int bash_glob_list_expansions PARAMS((int, int));

#endif  

static int edit_and_execute_command PARAMS((int, int, int, char *));
#if defined (VI_MODE)
static int vi_edit_and_execute_command PARAMS((int, int));
static int bash_vi_complete PARAMS((int, int));
#endif
static int emacs_edit_and_execute_command PARAMS((int, int));

 
int bash_readline_initialized = 0;

 
int perform_hostname_completion = 1;

 
int no_empty_command_completion;

 
int force_fignore = 1;

 
int dircomplete_spelling = 0;

 
#if DIRCOMPLETE_EXPAND_DEFAULT
int dircomplete_expand = 1;
int dircomplete_expand_relpath = 1;
#else
int dircomplete_expand = 0;
int dircomplete_expand_relpath = 0;
#endif

 
int complete_fullquote = 1;

static char *bash_completer_word_break_characters = " \t\n\"'@><=;|&(:";
static char *bash_nohostname_word_break_characters = " \t\n\"'><=;|&(:";
 

static const char *default_filename_quote_characters = " \t\n\\\"'@<>=;|&()#$`?*[!:{~";	 
static char *custom_filename_quote_characters = 0;
static char filename_bstab[256];

static rl_hook_func_t *old_rl_startup_hook = (rl_hook_func_t *)NULL;

static int dot_in_path = 0;

 
static int dabbrev_expand_active = 0;

 
#define COMPLETE_DQUOTE  1
#define COMPLETE_SQUOTE  2
#define COMPLETE_BSQUOTE 3
static int completion_quoting_style = COMPLETE_BSQUOTE;

 
#define DEFCOMP_CMDPOS		1

static rl_command_func_t *vi_tab_binding = rl_complete;

 
void
posix_readline_initialize (on_or_off)
     int on_or_off;
{
  static char kseq[2] = { CTRL ('I'), 0 };		 

  if (on_or_off)
    rl_variable_bind ("comment-begin", "#");
#if defined (VI_MODE)
  if (on_or_off)
    {
      vi_tab_binding = rl_function_of_keyseq (kseq, vi_insertion_keymap, (int *)NULL);
      rl_bind_key_in_map (CTRL ('I'), rl_insert, vi_insertion_keymap);
    }
  else
    {
      if (rl_function_of_keyseq (kseq, vi_insertion_keymap, (int *)NULL) == rl_insert)
        rl_bind_key_in_map (CTRL ('I'), vi_tab_binding, vi_insertion_keymap);
    }
#endif
}

void
reset_completer_word_break_chars ()
{
  rl_completer_word_break_characters = perform_hostname_completion ? savestring (bash_completer_word_break_characters) : savestring (bash_nohostname_word_break_characters);
}

 
int
enable_hostname_completion (on_or_off)
     int on_or_off;
{
  int old_value;
  char *nv, *nval;
  const char *at;

  old_value = perform_hostname_completion;

  if (on_or_off)
    {
      perform_hostname_completion = 1;
      rl_special_prefixes = "$@";
    }
  else
    {
      perform_hostname_completion = 0;
      rl_special_prefixes = "$";
    }

   

   

  if (bash_readline_initialized == 0 &&
      (rl_completer_word_break_characters == 0 || 
       rl_completer_word_break_characters == rl_basic_word_break_characters))
    {
      if (on_or_off)
	rl_completer_word_break_characters = savestring (bash_completer_word_break_characters);
      else
	rl_completer_word_break_characters = savestring (bash_nohostname_word_break_characters);
    }
  else
    {
       
      at = strchr (rl_completer_word_break_characters, '@');
      if ((at == 0 && on_or_off == 0) || (at != 0 && on_or_off != 0))
        return old_value;

       
      nval = (char *)xmalloc (strlen (rl_completer_word_break_characters) + 1 + on_or_off);

      if (on_or_off == 0)
	{
	   
	  for (nv = nval, at = rl_completer_word_break_characters; *at; )
	    if (*at != '@')
	      *nv++ = *at++;
	    else
	      at++;
	  *nv = '\0';
	}
      else
	{
	  nval[0] = '@';
	  strcpy (nval + 1, rl_completer_word_break_characters);
        }

      free ((void *)rl_completer_word_break_characters);
      rl_completer_word_break_characters = nval;
    }

  return (old_value);
}

 
void
initialize_readline ()
{
  rl_command_func_t *func;
  char kseq[2];

  if (bash_readline_initialized)
    return;

  rl_terminal_name = get_string_value ("TERM");
  rl_instream = stdin;
  rl_outstream = stderr;

   
  rl_readline_name = "Bash";

   
  rl_add_defun ("shell-expand-line", shell_expand_line, -1);
#ifdef BANG_HISTORY
  rl_add_defun ("history-expand-line", history_expand_line, -1);
  rl_add_defun ("magic-space", tcsh_magic_space, -1);
#endif

  rl_add_defun ("shell-forward-word", bash_forward_shellword, -1);
  rl_add_defun ("shell-backward-word", bash_backward_shellword, -1);
  rl_add_defun ("shell-kill-word", bash_kill_shellword, -1);
  rl_add_defun ("shell-backward-kill-word", bash_backward_kill_shellword, -1);
  rl_add_defun ("shell-transpose-words", bash_transpose_shellwords, -1);

  rl_add_defun ("spell-correct-word", bash_spell_correct_shellword, -1);
  rl_bind_key_if_unbound_in_map ('s', bash_spell_correct_shellword, emacs_ctlx_keymap);

#ifdef ALIAS
  rl_add_defun ("alias-expand-line", alias_expand_line, -1);
#  ifdef BANG_HISTORY
  rl_add_defun ("history-and-alias-expand-line", history_and_alias_expand_line, -1);
#  endif
#endif

   
  rl_add_defun ("insert-last-argument", rl_yank_last_arg, -1);

  rl_add_defun ("display-shell-version", display_shell_version, -1);
  rl_add_defun ("edit-and-execute-command", emacs_edit_and_execute_command, -1);
#if defined (VI_MODE)
  rl_add_defun ("vi-edit-and-execute-command", vi_edit_and_execute_command, -1);
#endif

#if defined (BRACE_COMPLETION)
  rl_add_defun ("complete-into-braces", bash_brace_completion, -1);
#endif

#if defined (SPECIFIC_COMPLETION_FUNCTIONS)
  rl_add_defun ("complete-filename", bash_complete_filename, -1);
  rl_add_defun ("possible-filename-completions", bash_possible_filename_completions, -1);
  rl_add_defun ("complete-username", bash_complete_username, -1);
  rl_add_defun ("possible-username-completions", bash_possible_username_completions, -1);
  rl_add_defun ("complete-hostname", bash_complete_hostname, -1);
  rl_add_defun ("possible-hostname-completions", bash_possible_hostname_completions, -1);
  rl_add_defun ("complete-variable", bash_complete_variable, -1);
  rl_add_defun ("possible-variable-completions", bash_possible_variable_completions, -1);
  rl_add_defun ("complete-command", bash_complete_command, -1);
  rl_add_defun ("possible-command-completions", bash_possible_command_completions, -1);
  rl_add_defun ("glob-complete-word", bash_glob_complete_word, -1);
  rl_add_defun ("glob-expand-word", bash_glob_expand_word, -1);
  rl_add_defun ("glob-list-expansions", bash_glob_list_expansions, -1);
#endif

  rl_add_defun ("dynamic-complete-history", dynamic_complete_history, -1);
  rl_add_defun ("dabbrev-expand", bash_dabbrev_expand, -1);

   
  if (RL_ISSTATE(RL_STATE_INITIALIZED) == 0)
    rl_initialize ();

   
  rl_bind_key_if_unbound_in_map (CTRL('E'), shell_expand_line, emacs_meta_keymap);

#ifdef BANG_HISTORY
  rl_bind_key_if_unbound_in_map ('^', history_expand_line, emacs_meta_keymap);
#endif

  rl_bind_key_if_unbound_in_map (CTRL ('V'), display_shell_version, emacs_ctlx_keymap);

   
  kseq[0] = CTRL('J');
  kseq[1] = '\0';
  func = rl_function_of_keyseq (kseq, emacs_meta_keymap, (int *)NULL);
  if (func == rl_vi_editing_mode)
    rl_unbind_key_in_map (CTRL('J'), emacs_meta_keymap);
  kseq[0] = CTRL('M');
  func = rl_function_of_keyseq (kseq, emacs_meta_keymap, (int *)NULL);
  if (func == rl_vi_editing_mode)
    rl_unbind_key_in_map (CTRL('M'), emacs_meta_keymap);
#if defined (VI_MODE)
  kseq[0] = CTRL('E');
  func = rl_function_of_keyseq (kseq, vi_movement_keymap, (int *)NULL);
  if (func == rl_emacs_editing_mode)
    rl_unbind_key_in_map (CTRL('E'), vi_movement_keymap);
#endif

#if defined (BRACE_COMPLETION)
  rl_bind_key_if_unbound_in_map ('{', bash_brace_completion, emacs_meta_keymap);  
#endif  

#if defined (SPECIFIC_COMPLETION_FUNCTIONS)
  rl_bind_key_if_unbound_in_map ('/', bash_complete_filename, emacs_meta_keymap);
  rl_bind_key_if_unbound_in_map ('/', bash_possible_filename_completions, emacs_ctlx_keymap);

   
  kseq[0] = '~';
  kseq[1] = '\0';
  func = rl_function_of_keyseq (kseq, emacs_meta_keymap, (int *)NULL);
  if (func == 0 || func == rl_tilde_expand)
    rl_bind_keyseq_in_map (kseq, bash_complete_username, emacs_meta_keymap);

  rl_bind_key_if_unbound_in_map ('~', bash_possible_username_completions, emacs_ctlx_keymap);

  rl_bind_key_if_unbound_in_map ('@', bash_complete_hostname, emacs_meta_keymap);
  rl_bind_key_if_unbound_in_map ('@', bash_possible_hostname_completions, emacs_ctlx_keymap);

  rl_bind_key_if_unbound_in_map ('$', bash_complete_variable, emacs_meta_keymap);
  rl_bind_key_if_unbound_in_map ('$', bash_possible_variable_completions, emacs_ctlx_keymap);

  rl_bind_key_if_unbound_in_map ('!', bash_complete_command, emacs_meta_keymap);
  rl_bind_key_if_unbound_in_map ('!', bash_possible_command_completions, emacs_ctlx_keymap);

  rl_bind_key_if_unbound_in_map ('g', bash_glob_complete_word, emacs_meta_keymap);
  rl_bind_key_if_unbound_in_map ('*', bash_glob_expand_word, emacs_ctlx_keymap);
  rl_bind_key_if_unbound_in_map ('g', bash_glob_list_expansions, emacs_ctlx_keymap);

#endif  

  kseq[0] = TAB;
  kseq[1] = '\0';
  func = rl_function_of_keyseq (kseq, emacs_meta_keymap, (int *)NULL);
  if (func == 0 || func == rl_tab_insert)
    rl_bind_key_in_map (TAB, dynamic_complete_history, emacs_meta_keymap);

   
  rl_attempted_completion_function = attempt_shell_completion;

   
  set_directory_hook ();

  rl_filename_rewrite_hook = bash_filename_rewrite_hook;

  rl_filename_stat_hook = bash_filename_stat_hook;

   
  rl_ignore_some_completions_function = filename_completion_ignore;

   
  rl_bind_key_if_unbound_in_map (CTRL ('E'), emacs_edit_and_execute_command, emacs_ctlx_keymap);
#if defined (VI_MODE)
  rl_bind_key_if_unbound_in_map ('v', vi_edit_and_execute_command, vi_movement_keymap);
#  if defined (ALIAS)
  rl_bind_key_if_unbound_in_map ('@', posix_edit_macros, vi_movement_keymap);
#  endif

  rl_bind_key_in_map ('\\', bash_vi_complete, vi_movement_keymap);
  rl_bind_key_in_map ('*', bash_vi_complete, vi_movement_keymap);
  rl_bind_key_in_map ('=', bash_vi_complete, vi_movement_keymap);
#endif

  rl_completer_quote_characters = "'\"";

   
  enable_hostname_completion (perform_hostname_completion);

   
  rl_filename_quote_characters = default_filename_quote_characters;
  set_filename_bstab (rl_filename_quote_characters);

  rl_filename_quoting_function = bash_quote_filename;
  rl_filename_dequoting_function = bash_dequote_filename;
  rl_char_is_quoted_p = char_is_quoted;

   
  rl_bind_key_if_unbound_in_map (CTRL('B'), bash_backward_shellword, emacs_meta_keymap);
  rl_bind_key_if_unbound_in_map (CTRL('D'), bash_kill_shellword, emacs_meta_keymap);
  rl_bind_key_if_unbound_in_map (CTRL('F'), bash_forward_shellword, emacs_meta_keymap);
  rl_bind_key_if_unbound_in_map (CTRL('T'), bash_transpose_shellwords, emacs_meta_keymap);

#if 0
   
  if (posixly_correct)
    posix_readline_initialize (1);
#endif

  bash_readline_initialized = 1;
}

void
bashline_reinitialize ()
{
  bash_readline_initialized = 0;
}

void
bashline_set_event_hook ()
{
  rl_signal_event_hook = bash_event_hook;
}

void
bashline_reset_event_hook ()
{
  rl_signal_event_hook = 0;
}

 
void
bashline_reset ()
{
  tilde_initialize ();
  rl_attempted_completion_function = attempt_shell_completion;
  rl_completion_entry_function = NULL;
  rl_ignore_some_completions_function = filename_completion_ignore;

  complete_fullquote = 1;
  rl_filename_quote_characters = default_filename_quote_characters;
  set_filename_bstab (rl_filename_quote_characters);

  set_directory_hook ();
  rl_filename_stat_hook = bash_filename_stat_hook;

  bashline_reset_event_hook ();

  rl_sort_completion_matches = 1;
}

 
static char *push_to_readline = (char *)NULL;

 
static int
bash_push_line ()
{
  if (push_to_readline)
    {
      rl_insert_text (push_to_readline);
      free (push_to_readline);
      push_to_readline = (char *)NULL;
      rl_startup_hook = old_rl_startup_hook;
    }
  return 0;
}

 
int
bash_re_edit (line)
     char *line;
{
  FREE (push_to_readline);

  push_to_readline = savestring (line);
  old_rl_startup_hook = rl_startup_hook;
  rl_startup_hook = bash_push_line;

  return (0);
}

static int
display_shell_version (count, c)
     int count, c;
{
  rl_crlf ();
  show_shell_version (0);
  putc ('\r', rl_outstream);
  fflush (rl_outstream);
  rl_on_new_line ();
  rl_redisplay ();
  return 0;
}

 
 
 
 
 

 

 
 
static char **hostname_list = (char **)NULL;

 
static int hostname_list_size;

 
static int hostname_list_length;

 
int hostname_list_initialized = 0;

 
static void
initialize_hostname_list ()
{
  char *temp;

  temp = get_string_value ("HOSTFILE");
  if (temp == 0)
    temp = get_string_value ("hostname_completion_file");
  if (temp == 0)
    temp = DEFAULT_HOSTS_FILE;

  snarf_hosts_from_file (temp);

  if (hostname_list)
    hostname_list_initialized++;
}

 
static void
add_host_name (name)
     char *name;
{
  if (hostname_list_length + 2 > hostname_list_size)
    {
      hostname_list_size = (hostname_list_size + 32) - (hostname_list_size % 32);
      hostname_list = strvec_resize (hostname_list, hostname_list_size);
    }

  hostname_list[hostname_list_length++] = savestring (name);
  hostname_list[hostname_list_length] = (char *)NULL;
}

#define cr_whitespace(c) ((c) == '\r' || (c) == '\n' || whitespace(c))

static void
snarf_hosts_from_file (filename)
     char *filename;
{
  FILE *file;
  char *temp, buffer[256], name[256];
  register int i, start;

  file = fopen (filename, "r");
  if (file == 0)
    return;

  while (temp = fgets (buffer, 255, file))
    {
       
      for (i = 0; buffer[i] && cr_whitespace (buffer[i]); i++)
	;

       
      if (buffer[i] == '\0' || buffer[i] == '#')
	continue;

       
      if (strncmp (buffer + i, "$include ", 9) == 0)
	{
	  char *incfile, *t;

	   
	  for (incfile = buffer + i + 9; *incfile && whitespace (*incfile); incfile++)
	    ;

	   
	  for (t = incfile; *t && cr_whitespace (*t) == 0; t++)
	    ;

	  *t = '\0';

	  snarf_hosts_from_file (incfile);
	  continue;
	}

       
      if (DIGIT (buffer[i]))
	for (; buffer[i] && cr_whitespace (buffer[i]) == 0; i++);

       
      while (buffer[i])
	{
	  for (; cr_whitespace (buffer[i]); i++)
	    ;
	  if (buffer[i] == '\0' || buffer[i] ==  '#')
	    break;

	   
	  for (start = i; buffer[i] && cr_whitespace (buffer[i]) == 0; i++)
	    ;
	  if (i == start)
	    continue;
	  strncpy (name, buffer + start, i - start);
	  name[i - start] = '\0';
	  add_host_name (name);
	}
    }
  fclose (file);
}

 
char **
get_hostname_list ()
{
  if (hostname_list_initialized == 0)
    initialize_hostname_list ();
  return (hostname_list);
}

void
clear_hostname_list ()
{
  register int i;

  if (hostname_list_initialized == 0)
    return;
  for (i = 0; i < hostname_list_length; i++)
    free (hostname_list[i]);
  hostname_list_length = hostname_list_initialized = 0;
}

 
static char **
hostnames_matching (text)
     char *text;
{
  register int i, len, nmatch, rsize;
  char **result;

  if (hostname_list_initialized == 0)
    initialize_hostname_list ();

  if (hostname_list_initialized == 0)
    return ((char **)NULL);

   
  if (*text == '\0')
    {
      result = strvec_create (1 + hostname_list_length);
      for (i = 0; i < hostname_list_length; i++)
	result[i] = hostname_list[i];
      result[i] = (char *)NULL;
      return (result);
    }

   
  len = strlen (text);
  result = (char **)NULL;
  for (i = nmatch = rsize = 0; i < hostname_list_length; i++)
    {
      if (STREQN (text, hostname_list[i], len) == 0)
	continue;

       
      if (nmatch >= (rsize - 1))
	{
	  rsize = (rsize + 16) - (rsize % 16);
	  result = strvec_resize (result, rsize);
	}

      result[nmatch++] = hostname_list[i];
    }
  if (nmatch)
    result[nmatch] = (char *)NULL;
  return (result);
}

 

#define VI_EDIT_COMMAND		"fc -e \"${VISUAL:-${EDITOR:-vi}}\""
#define EMACS_EDIT_COMMAND	"fc -e \"${VISUAL:-${EDITOR:-emacs}}\""
#define POSIX_VI_EDIT_COMMAND	"fc -e vi"

static int
edit_and_execute_command (count, c, editing_mode, edit_command)
     int count, c, editing_mode;
     char *edit_command;
{
  char *command, *metaval;
  int r, rrs, metaflag;
  sh_parser_state_t ps;

  rrs = rl_readline_state;
  saved_command_line_count = current_command_line_count;

   
  rl_newline (1, c);

  if (rl_explicit_arg)
    {
      command = (char *)xmalloc (strlen (edit_command) + 8);
      sprintf (command, "%s %d", edit_command, count);
    }
  else
    {
       
       
      using_history ();
      current_command_line_count++;	 
      bash_add_history (rl_line_buffer);
      current_command_line_count = 0;	 
      bash_add_history ("");
      history_lines_this_session++;
      using_history ();
      command = savestring (edit_command);
    }

  metaval = rl_variable_value ("input-meta");
  metaflag = RL_BOOLEAN_VARIABLE_VALUE (metaval);
  
  if (rl_deprep_term_function)
    (*rl_deprep_term_function) ();
  rl_clear_signals ();
  save_parser_state (&ps);
  r = parse_and_execute (command, (editing_mode == VI_EDITING_MODE) ? "v" : "C-xC-e", SEVAL_NOHIST);
  restore_parser_state (&ps);

   
  reset_readahead_token ();

  if (rl_prep_term_function)
    (*rl_prep_term_function) (metaflag);
  rl_set_signals ();

  current_command_line_count = saved_command_line_count;

   
  rl_line_buffer[0] = '\0';	 
  rl_point = rl_end = 0;
  rl_done = 0;
  rl_readline_state = rrs;

#if defined (VI_MODE)
  if (editing_mode == VI_EDITING_MODE)
    rl_vi_insertion_mode (1, c);
#endif

  rl_forced_update_display ();

  return r;
}

#if defined (VI_MODE)
static int
vi_edit_and_execute_command (count, c)
     int count, c;
{
  if (posixly_correct)
    return (edit_and_execute_command (count, c, VI_EDITING_MODE, POSIX_VI_EDIT_COMMAND));
  else
    return (edit_and_execute_command (count, c, VI_EDITING_MODE, VI_EDIT_COMMAND));
}
#endif  

static int
emacs_edit_and_execute_command (count, c)
     int count, c;
{
  return (edit_and_execute_command (count, c, EMACS_EDITING_MODE, EMACS_EDIT_COMMAND));
}

#if defined (ALIAS)
static int
posix_edit_macros (count, key)
     int count, key;
{
  int c;
  char alias_name[3], *alias_value, *macro;

  c = rl_read_key ();
  if (c <= 0)
    return 0;
  alias_name[0] = '_';
  alias_name[1] = c;
  alias_name[2] = '\0';

  alias_value = get_alias_value (alias_name);
  if (alias_value && *alias_value)
    {
      macro = savestring (alias_value);
      rl_push_macro_input (macro);
    }
  return 0;
}
#endif

 

#define WORDDELIM(c)	(shellmeta(c) || shellblank(c))

static int
bash_forward_shellword (count, key)
     int count, key;
{
  size_t slen;
  int c, p;
  DECLARE_MBSTATE;

  if (count < 0)
    return (bash_backward_shellword (-count, key));

   
   

  p = rl_point;
  slen = rl_end;

  while (count)
    {
      if (p == rl_end)
	{
	  rl_point = rl_end;
	  return 0;
	}

       
      if (char_is_quoted (rl_line_buffer, p) && p > 0 && rl_line_buffer[p-1] != '\\')
	{
	  do
	    ADVANCE_CHAR (rl_line_buffer, slen, p);
	  while (p < rl_end && char_is_quoted (rl_line_buffer, p));
	  count--;
	  continue;
	}

       
       
      while (p < rl_end && (c = rl_line_buffer[p]) && WORDDELIM (c))
	{
	  switch (c)
	    {
	    default:
	      ADVANCE_CHAR (rl_line_buffer, slen, p);
	      continue;		 
	    case '\\':
	      if (p < rl_end && rl_line_buffer[p])
		ADVANCE_CHAR (rl_line_buffer, slen, p);
	      break;
	    case '\'':
	      p = skip_to_delim (rl_line_buffer, ++p, "'", SD_NOJMP);
	      break;
	    case '"':
	      p = skip_to_delim (rl_line_buffer, ++p, "\"", SD_NOJMP);
	      break;
	    }

	  if (p < rl_end)
	    p++;
	}

      if (rl_line_buffer[p] == 0 || p == rl_end)
        {
	  rl_point = rl_end;
	  rl_ding ();
	  return 0;
        }
	
       
      while (p < rl_end && (c = rl_line_buffer[p]) && WORDDELIM (c) == 0)
	{
	  switch (c)
	    {
	    default:
	      ADVANCE_CHAR (rl_line_buffer, slen, p);
	      continue;		 
	    case '\\':
	      if (p < rl_end && rl_line_buffer[p])
		ADVANCE_CHAR (rl_line_buffer, slen, p);
	      break;
	    case '\'':
	      p = skip_to_delim (rl_line_buffer, ++p, "'", SD_NOJMP);
	      break;
	    case '"':
	      p = skip_to_delim (rl_line_buffer, ++p, "\"", SD_NOJMP);
	      break;
	    }

	  if (p < rl_end)
	    p++;
	}

      if (p == rl_end || rl_line_buffer[p] == 0)
	{
	  rl_point = rl_end;
	  return (0);
	}

      count--;      
    }

  rl_point = p;
  return (0);
}

static int
bash_backward_shellword (count, key)
     int count, key;
{
  size_t slen;
  int c, p, prev_p;
  DECLARE_MBSTATE;

  if (count < 0)
    return (bash_forward_shellword (-count, key));

  p = rl_point;
  slen = rl_end;

  while (count)
    {
      if (p == 0)
	{
	  rl_point = 0;
	  return 0;
	}

       
      BACKUP_CHAR (rl_line_buffer, slen, p);
      while (p > 0)
	{
	  c = rl_line_buffer[p];
	  if (WORDDELIM (c) == 0 || char_is_quoted (rl_line_buffer, p))
	    break;
	  BACKUP_CHAR (rl_line_buffer, slen, p);
	}

      if (p == 0)
	{
	  rl_point = 0;
	  return 0;
	}

       
      prev_p = p;
      while (p > 0)
	{
	  c = rl_line_buffer[p];
	  if (WORDDELIM (c) && char_is_quoted (rl_line_buffer, p) == 0)
	    {
	      p = prev_p;
	      break;
	    }
	  prev_p = p;
	  BACKUP_CHAR (rl_line_buffer, slen, p);
	}

      count--;
    }

  rl_point = p;
  return 0;
}

static int
bash_kill_shellword (count, key)
     int count, key;
{
  int p;

  if (count < 0)
    return (bash_backward_kill_shellword (-count, key));

  p = rl_point;
  bash_forward_shellword (count, key);

  if (rl_point != p)
    rl_kill_text (p, rl_point);

  rl_point = p;
  if (rl_editing_mode == EMACS_EDITING_MODE)	 
    rl_mark = rl_point;

  return 0;
}

static int
bash_backward_kill_shellword (count, key)
     int count, key;
{
  int p;

  if (count < 0)
    return (bash_kill_shellword (-count, key));

  p = rl_point;
  bash_backward_shellword (count, key);

  if (rl_point != p)
    rl_kill_text (p, rl_point);

  if (rl_editing_mode == EMACS_EDITING_MODE)	 
    rl_mark = rl_point;

  return 0;
}

static int
bash_transpose_shellwords (count, key)
     int count, key;
{
  char *word1, *word2;
  int w1_beg, w1_end, w2_beg, w2_end;
  int orig_point = rl_point;

  if (count == 0)
    return 0;

   
  bash_forward_shellword (count, key);
  w2_end = rl_point;
  bash_backward_shellword (1, key);
  w2_beg = rl_point;
  bash_backward_shellword (count, key);
  w1_beg = rl_point;
  bash_forward_shellword (1, key);
  w1_end = rl_point;

   
  if ((w1_beg == w2_beg) || (w2_beg < w1_end))
    {
      rl_ding ();
      rl_point = orig_point;
      return 1;
    }

   
  word1 = rl_copy_text (w1_beg, w1_end);
  word2 = rl_copy_text (w2_beg, w2_end);

   
  rl_begin_undo_group ();

   
  rl_point = w2_beg;
  rl_delete_text (w2_beg, w2_end);
  rl_insert_text (word1);

  rl_point = w1_beg;
  rl_delete_text (w1_beg, w1_end);
  rl_insert_text (word2);

   
  rl_point = w2_end;

   
  rl_end_undo_group ();
  xfree (word1);
  xfree (word2);

  return 0;
}

 
static int
bash_spell_correct_shellword (count, key)
     int count, key;
{
  int opoint, wbeg, wend;
  char *text, *newdir;

  opoint = rl_point;
  while (count)
    {
      bash_backward_shellword (1, key);
      wbeg = rl_point;
      bash_forward_shellword (1, key);
      wend = rl_point;

      if (wbeg > wend)
	break;

      text = rl_copy_text (wbeg, wend);

      newdir = dirspell (text);
      if (newdir)
	{
	  rl_begin_undo_group ();
	  rl_delete_text (wbeg, wend);
	  rl_point = wbeg;
	  if (*newdir)
	    rl_insert_text (newdir);
	  rl_mark = wbeg;
	  rl_end_undo_group ();
	}

      free (text);
      free (newdir);

      if (rl_point >= rl_end)
	break;

      count--;

      if (count)
	bash_forward_shellword (1, key);		 
    }

  return 0;
}

 
 
 
 
 

#define COMMAND_SEPARATORS ";|&{(`"
  
#define COMMAND_SEPARATORS_PLUS_WS ";|&{(` \t"
  

 
static int
check_redir (ti)
     int ti;
{
  register int this_char, prev_char;

   
  this_char = rl_line_buffer[ti];
  prev_char = (ti > 0) ? rl_line_buffer[ti - 1] : 0;

  if ((this_char == '&' && (prev_char == '<' || prev_char == '>')) ||
      (this_char == '|' && prev_char == '>'))
    return (1);
  else if (this_char == '{' && prev_char == '$')  
    return (1);
#if 0	 
  else if (this_char == '(' && prev_char == '$')  
    return (1);
  else if (this_char == '(' && prev_char == '<')  
    return (1);
#if defined (EXTENDED_GLOB)
  else if (extended_glob && this_char == '(' && prev_char == '!')  
    return (1);
#endif
#endif
  else if (char_is_quoted (rl_line_buffer, ti))
    return (1);
  return (0);
}

#if defined (PROGRAMMABLE_COMPLETION)
 
static int
find_cmd_start (start)
     int start;
{
  register int s, os, ns;

  os = 0;
   
  while (((s = skip_to_delim (rl_line_buffer, os, COMMAND_SEPARATORS, SD_NOJMP|SD_COMPLETE )) <= start) &&
	 rl_line_buffer[s])
    {
       
      if (s > 0 && rl_line_buffer[s] == '|' && rl_line_buffer[s-1] == '>')
	{
	  ns = skip_to_delim (rl_line_buffer, s+1, COMMAND_SEPARATORS, SD_NOJMP|SD_COMPLETE );
	  if (ns > start || rl_line_buffer[ns] == 0)
	    return os;
	  os = ns+1;
	  continue;
	}
       
      if (s >= os && rl_line_buffer[s] == '{')
	{
	  int pc, nc;	 
	  for (pc = (s > os) ? s - 1 : os; pc > os && whitespace(rl_line_buffer[pc]); pc--)
	    ;
	  nc = rl_line_buffer[s+1];
	   
	  if ((pc > os && (rl_line_buffer[s-1] == '{' || strchr (COMMAND_SEPARATORS, rl_line_buffer[pc]) == 0)) ||
	      (shellbreak(nc) == 0))	 
	    {
	       
	      ns = skip_to_delim (rl_line_buffer, s+1, COMMAND_SEPARATORS, SD_NOJMP|SD_COMPLETE );
	      if (ns > start || rl_line_buffer[ns] == 0)
		return os;
	      os = ns+1;
	      continue;
	    }
	}
      os = s+1;
    }
  return os;
}

static int
find_cmd_end (end)
     int end;
{
  register int e;

  e = skip_to_delim (rl_line_buffer, end, COMMAND_SEPARATORS, SD_NOJMP|SD_COMPLETE);
  return e;
}

static char *
find_cmd_name (start, sp, ep)
     int start;
     int *sp, *ep;
{
  char *name;
  register int s, e;

  for (s = start; whitespace (rl_line_buffer[s]); s++)
    ;

   
  e = skip_to_delim (rl_line_buffer, s, "()<>;&| \t\n", SD_NOJMP|SD_COMPLETE);

  name = substring (rl_line_buffer, s, e);

  if (sp)
    *sp = s;
  if (ep)
    *ep = e;

  return (name);
}

static char *
prog_complete_return (text, matchnum)
     const char *text;
     int matchnum;
{
  static int ind;

  if (matchnum == 0)
    ind = 0;

  if (prog_complete_matches == 0 || prog_complete_matches[ind] == 0)
    return (char *)NULL;
  return (prog_complete_matches[ind++]);
}

#endif  

 
static int
invalid_completion (text, ind)
     const char *text;
     int ind;
{
  int pind;

   
  if (ind > 0 && rl_line_buffer[ind] == '(' &&	 
		 member (rl_line_buffer[ind-1], "$<>"))
    return 0;

  pind = ind - 1;
  while (pind > 0 && whitespace (rl_line_buffer[pind]))
    pind--;
   
  if (ind >= 0 && pind <= 0 && rl_line_buffer[ind] == '(')	 
    return 0;
   
  if (ind > 0 && rl_line_buffer[ind] == '(' &&	 
		 member (rl_line_buffer[pind], COMMAND_SEPARATORS) == 0)
    return 1;

  return 0;
}

 
static char **
attempt_shell_completion (text, start, end)
     const char *text;
     int start, end;
{
  int in_command_position, ti, qc, dflags;
  char **matches, *command_separator_chars;
#if defined (PROGRAMMABLE_COMPLETION)
  int have_progcomps, was_assignment;
  COMPSPEC *iw_compspec;
#endif

  command_separator_chars = COMMAND_SEPARATORS;
  matches = (char **)NULL;
  rl_ignore_some_completions_function = filename_completion_ignore;

  complete_fullquote = 1;		 
  rl_filename_quote_characters = default_filename_quote_characters;
  set_filename_bstab (rl_filename_quote_characters);
  set_directory_hook ();
  rl_filename_stat_hook = bash_filename_stat_hook;

  rl_sort_completion_matches = 1;	 

   
  ti = start - 1;
  qc = -1;

  while ((ti > -1) && (whitespace (rl_line_buffer[ti])))
    ti--;

#if 1
   
  if (ti >= 0 && (rl_line_buffer[ti] == '"' || rl_line_buffer[ti] == '\''))
    {
      qc = rl_line_buffer[ti];
      ti--;
      while (ti > -1 && (whitespace (rl_line_buffer[ti])))
	ti--;
    }
#endif
      
  in_command_position = 0;
  if (ti < 0)
    {
       
      if (current_prompt_string == ps1_prompt)
	in_command_position++;
      else if (parser_in_command_position ())
	in_command_position++;
    }
  else if (member (rl_line_buffer[ti], command_separator_chars))
    {
      in_command_position++;

      if (check_redir (ti) == 1)
	in_command_position = -1;	 
    }
  else
    {
       
    }

  if (in_command_position > 0 && invalid_completion (text, ti))
    {
      rl_attempted_completion_over = 1;
      return ((char **)NULL);
    }

   
  if (in_command_position > 0 && ti >= 0 && rl_line_buffer[ti] == '`' &&
	*text != '`' && unclosed_pair (rl_line_buffer, end, "`") == 0)
    in_command_position = -1;	 

   
  if (*text == '`' && rl_completion_quote_character != '\'' &&
	(in_command_position > 0 || (unclosed_pair (rl_line_buffer, start, "`") &&
				     unclosed_pair (rl_line_buffer, end, "`"))))
    matches = rl_completion_matches (text, command_subst_completion_function);

#if defined (PROGRAMMABLE_COMPLETION)
   
  have_progcomps = prog_completion_enabled && (progcomp_size () > 0);
  iw_compspec = progcomp_search (INITIALWORD);
  if (matches == 0 &&
      (in_command_position == 0 || text[0] == '\0' || (in_command_position > 0 && iw_compspec)) &&
      current_prompt_string == ps1_prompt)
    {
      int s, e, s1, e1, os, foundcs;
      char *n;

       
      if (prog_complete_matches)
	free (prog_complete_matches);
      prog_complete_matches = (char **)NULL;

      os = start;
      n = 0;
      was_assignment = 0;
      s = find_cmd_start (os);
      e = find_cmd_end (end);
      do
	{
	   
	  if (s > rl_end)
	    {
	      s1 = s = e1;
	      break;
	    }
	   
	  else if (was_assignment && s > rl_point)
	    {
	      s1 = s = e1;
	      break;
	    }
	   
	  FREE (n);
	  n = find_cmd_name (s, &s1, &e1);
	  s = e1 + 1;
	}
      while (was_assignment = assignment (n, 0));
      s = s1;		 

       
      if (start == 0 && end == 0 && e != 0 && text[0] == '\0')	 
        foundcs = 0;
      else if (start == end && start == s1 && e != 0 && e1 > end)	 
	foundcs = 0;
      else if (e == 0 && e == s && text[0] == '\0' && have_progcomps)	 
        prog_complete_matches = programmable_completions (EMPTYCMD, text, s, e, &foundcs);
      else if (start == end && text[0] == '\0' && s1 > start && whitespace (rl_line_buffer[start]))
        foundcs = 0;		 
      else if (e > s && was_assignment == 0 && e1 == end && rl_line_buffer[e] == 0 && whitespace (rl_line_buffer[e-1]) == 0)
	{
	   
	  foundcs = 0;
	  in_command_position = s == start && STREQ (n, text);	 
	}
      else if (e > s && was_assignment == 0 && have_progcomps)
	{
	  prog_complete_matches = programmable_completions (n, text, s, e, &foundcs);
	   
	   
	  in_command_position = s == start && (iw_compspec || STREQ (n, text));	 
	  if (iw_compspec && in_command_position)
	    foundcs = 0;
	}
       
      else if (s >= e && n[0] == '\0' && text[0] == '\0' && start > 0 &&
		was_assignment == 0 && member (rl_line_buffer[start-1], COMMAND_SEPARATORS))
	{
	  foundcs = 0;
	  in_command_position = 1;
	}
      else if (s >= e && n[0] == '\0' && text[0] == '\0' && start > 0)
        {
          foundcs = 0;	 
          in_command_position += was_assignment;
        }
      else if (s == start && e == end && STREQ (n, text) && start > 0)
        {
          foundcs = 0;	 
          in_command_position = 1;
        }
      else
	foundcs = 0;

       
      if (in_command_position && have_progcomps && foundcs == 0 && iw_compspec)
	prog_complete_matches = programmable_completions (INITIALWORD, text, s, e, &foundcs);

      FREE (n);
       
      if (foundcs)
	{
	  pcomp_set_readline_variables (foundcs, 1);
	   
	  matches = rl_completion_matches (text, prog_complete_return);
	  if ((foundcs & COPT_DEFAULT) == 0)
	    rl_attempted_completion_over = 1;	 
	  if (matches || ((foundcs & COPT_BASHDEFAULT) == 0))
	    return (matches);
	}
    }
#endif

  if (matches == 0)
    {
      dflags = 0;
      if (in_command_position > 0)
	dflags |= DEFCOMP_CMDPOS;
      matches = bash_default_completion (text, start, end, qc, dflags);
    }

  return matches;
}

char **
bash_default_completion (text, start, end, qc, compflags)
     const char *text;
     int start, end, qc, compflags;
{
  char **matches, *t;

  matches = (char **)NULL;

   
  if (*text == '$')
    {
      if (qc != '\'' && text[1] == '(')  
	matches = rl_completion_matches (text, command_subst_completion_function);
      else
	{
	  matches = rl_completion_matches (text, variable_completion_function);
	   
	  if (matches && matches[0] && matches[1] == 0)
	    {
	      t = savestring (matches[0]);
	      bash_filename_stat_hook (&t);
	       
	      if (file_isdir (t))
		rl_completion_append_character = '/';
	      free (t);
	    }
	}
    }

   
  if (matches == 0 && *text == '~' && mbschr (text, '/') == 0)
    matches = rl_completion_matches (text, rl_username_completion_function);

   
  if (matches == 0 && perform_hostname_completion && *text == '@')
    matches = rl_completion_matches (text, hostname_completion_function);

   
  if (matches == 0 && (compflags & DEFCOMP_CMDPOS))
    {
       
      if (no_empty_command_completion && end == start && text[0] == '\0')
	{
	  matches = (char **)NULL;
	  rl_ignore_some_completions_function = bash_ignore_everything;
	}
      else
	{
#define CMD_IS_DIR(x)	(absolute_pathname(x) == 0 && absolute_program(x) == 0 && *(x) != '~' && test_for_directory (x))

	  dot_in_path = 0;
	  matches = rl_completion_matches (text, command_word_completion_function);

	   
	  if (matches == (char **)NULL)
	    rl_ignore_some_completions_function = bash_ignore_filenames;
	  else if (matches[1] == 0 && CMD_IS_DIR(matches[0]) && dot_in_path == 0)
	     
	    {
	      rl_completion_suppress_append = 1;
	      rl_filename_completion_desired = 0;
	    }
	  else if (matches[0] && matches[1] && STREQ (matches[0], matches[1]) && CMD_IS_DIR (matches[0]))
	     
	    {
	      rl_completion_suppress_append = 1;
	      rl_filename_completion_desired = 0;
	    }
	}
    }

   
  if (!matches && completion_glob_pattern ((char *)text))
    {
      matches = rl_completion_matches (text, glob_complete_word);
       
      if (matches && matches[1] && rl_completion_type == TAB)
	{
	  strvec_dispose (matches);
	  matches = (char **)0;
	}
      else if (matches && matches[1] && rl_completion_type == '!')
	{
	  rl_completion_suppress_append = 1;
	  rl_filename_completion_desired = 0;
	}
    }

  return (matches);
}

static int
bash_command_name_stat_hook (name)
     char **name;
{
  char *cname, *result;

   
  if (absolute_program (*name))
    return (bash_filename_stat_hook (name));

  cname = *name;
   
  result = search_for_command (cname, 0);
  if (result)
    {
      *name = result;
      return 1;
    }
  return 0;
}

static int
executable_completion (filename, searching_path)
     const char *filename;
     int searching_path;
{
  char *f, c;
  int r;

   
#if 0
  f = savestring (filename);
#else
  c = 0;
  f = bash_quote_filename ((char *)filename, SINGLE_MATCH, &c);
#endif
  bash_directory_completion_hook (&f);
  
  r = searching_path ? executable_file (f) : executable_or_directory (f);
  free (f);
  return r;
}

 
char *
command_word_completion_function (hint_text, state)
     const char *hint_text;
     int state;
{
  static char *hint = (char *)NULL;
  static char *path = (char *)NULL;
  static char *val = (char *)NULL;
  static char *filename_hint = (char *)NULL;
  static char *fnhint = (char *)NULL;
  static char *dequoted_hint = (char *)NULL;
  static char *directory_part = (char *)NULL;
  static char **glob_matches = (char **)NULL;
  static int path_index, hint_len, istate, igncase;
  static int mapping_over, local_index, searching_path, hint_is_dir;
  static int old_glob_ignore_case, globpat;
  static SHELL_VAR **varlist = (SHELL_VAR **)NULL;
#if defined (ALIAS)
  static alias_t **alias_list = (alias_t **)NULL;
#endif  
  char *temp, *cval;

   
  if (state == 0)
    {
      rl_filename_stat_hook = bash_command_name_stat_hook;

      if (dequoted_hint && dequoted_hint != hint)
	free (dequoted_hint);
      if (hint)
	free (hint);

      mapping_over = searching_path = 0;
      hint_is_dir = CMD_IS_DIR (hint_text);
      val = (char *)NULL;

      temp = rl_variable_value ("completion-ignore-case");
      igncase = RL_BOOLEAN_VARIABLE_VALUE (temp);

      old_glob_ignore_case = glob_ignore_case;

      if (glob_matches)
	{
	  free (glob_matches);
	  glob_matches = (char **)NULL;
	}

      globpat = completion_glob_pattern ((char *)hint_text);

       
      if (globpat || absolute_program (hint_text))
	{
	   
	  if (*hint_text == '~')
	    {
	      hint = bash_tilde_expand (hint_text, 0);
	      directory_part = savestring (hint_text);
	      temp = strchr (directory_part, '/');
	      if (temp)
		*temp = 0;
	      else
		{
		  free (directory_part);
		  directory_part = (char *)NULL;
		}
	    }
	  else if (dircomplete_expand)
	    {
	      hint = savestring (hint_text);
	      bash_directory_completion_hook (&hint);
	    }
	  else
	    hint = savestring (hint_text);

	  dequoted_hint = hint;
	   
	  if (rl_completion_found_quote && rl_completion_quote_character == 0)
	    {
	      dequoted_hint = bash_dequote_filename (hint, 0);
	      free (hint);
	      hint = dequoted_hint;
	    }
	  hint_len = strlen (hint);

	  if (filename_hint)
	    free (filename_hint);

	  fnhint = filename_hint = savestring (hint);

	  istate = 0;

	  if (globpat)
	    {
	      mapping_over = 5;
	      goto globword;
	    }
	  else
	    {
	     if (dircomplete_expand && path_dot_or_dotdot (filename_hint))
		{
		  dircomplete_expand = 0;
		  set_directory_hook ();
		  dircomplete_expand = 1;
		}
	      mapping_over = 4;
	      goto inner;
	    }
	}

      dequoted_hint = hint = savestring (hint_text);
      hint_len = strlen (hint);

      if (rl_completion_found_quote && rl_completion_quote_character == 0)
	dequoted_hint = bash_dequote_filename (hint, 0);
      
      path = get_string_value ("PATH");
      path_index = dot_in_path = 0;

       
      local_index = 0;

      if (varlist)
	free (varlist);

      varlist = all_visible_functions ();

#if defined (ALIAS)
      if (alias_list)
	free (alias_list);

      alias_list = all_aliases ();
#endif  
    }

   

  switch (mapping_over)
    {
    case 0:			 
#if defined (ALIAS)
      while (alias_list && alias_list[local_index])
	{
	  register char *alias;

	  alias = alias_list[local_index++]->name;

	  if (igncase == 0 && (STREQN (alias, hint, hint_len)))
	    return (savestring (alias));
	  else if (igncase && strncasecmp (alias, hint, hint_len) == 0)
	    return (savestring (alias));
	}
#endif  
      local_index = 0;
      mapping_over++;

    case 1:			 
      {
	while (word_token_alist[local_index].word)
	  {
	    register char *reserved_word;

	    reserved_word = word_token_alist[local_index++].word;

	    if (STREQN (reserved_word, hint, hint_len))
	      return (savestring (reserved_word));
	  }
	local_index = 0;
	mapping_over++;
      }

    case 2:			 
      while (varlist && varlist[local_index])
	{
	  register char *varname;

	  varname = varlist[local_index++]->name;

	   
	  if (igncase == 0 && (STREQN (varname, hint, hint_len)))
	    return (savestring (varname));
	  else if (igncase && strncasecmp (varname, hint, hint_len) == 0)
	    return (savestring (varname));
	}
      local_index = 0;
      mapping_over++;

    case 3:			 
      for (; local_index < num_shell_builtins; local_index++)
	{
	   
	  if (!shell_builtins[local_index].function ||
	      (shell_builtins[local_index].flags & BUILTIN_ENABLED) == 0)
	    continue;

	  if (STREQN (shell_builtins[local_index].name, hint, hint_len))
	    {
	      int i = local_index++;

	      return (savestring (shell_builtins[i].name));
	    }
	}
      local_index = 0;
      mapping_over++;
    }

globword:
   
  if (globpat)
    {
      if (state == 0)
	{
	  rl_filename_completion_desired = 1;

	  glob_ignore_case = igncase;
	  glob_matches = shell_glob_filename (hint, 0);
	  glob_ignore_case = old_glob_ignore_case;

	  if (GLOB_FAILED (glob_matches) || glob_matches == 0)
	    {
	      glob_matches = (char **)NULL;
	      return ((char *)NULL);
	    }

	  local_index = 0;
		
	  if (glob_matches[1] && rl_completion_type == TAB)	 
	    return ((char *)NULL);
	}

      while (val = glob_matches[local_index++])
        {
	  if (executable_or_directory (val))
	    {
	      if (*hint_text == '~' && directory_part)
		{
		  temp = maybe_restore_tilde (val, directory_part);
		  free (val);
		  val = temp;
		}
	      return (val);
	    }
	  free (val);
        }

      glob_ignore_case = old_glob_ignore_case;
      return ((char *)NULL);
    }

   
  if (hint_is_dir)
    {
      hint_is_dir = 0;	 
      return (savestring (hint_text));
    }
    
   
 outer:

  istate = (val != (char *)NULL);

  if (istate == 0)
    {
      char *current_path;

       
      if (path == 0 || path[path_index] == 0 ||
	  (current_path = extract_colon_unit (path, &path_index)) == 0)
	return ((char *)NULL);

      searching_path = 1;
      if (*current_path == 0)
	{
	  free (current_path);
	  current_path = savestring (".");
	}

      if (*current_path == '~')
	{
	  char *t;

	  t = bash_tilde_expand (current_path, 0);
	  free (current_path);
	  current_path = t;
	}

      if (current_path[0] == '.' && current_path[1] == '\0')
	dot_in_path = 1;

      if (fnhint && fnhint != filename_hint)
	free (fnhint);
      if (filename_hint)
	free (filename_hint);

      filename_hint = sh_makepath (current_path, hint, 0);
       
      if (strpbrk (filename_hint, "\"'\\"))
	fnhint = sh_backslash_quote (filename_hint, filename_bstab, 0);
      else
	fnhint = filename_hint;
      free (current_path);		 
    }

 inner:
  val = rl_filename_completion_function (fnhint, istate);
  if (mapping_over == 4 && dircomplete_expand)
    set_directory_hook ();

  istate = 1;

  if (val == 0)
    {
       
      if (absolute_program (hint))
	return ((char *)NULL);

      goto outer;
    }
  else
    {
      int match, freetemp;

      if (absolute_program (hint))
	{
#if 0
	  if (igncase == 0)
	    match = strncmp (val, hint, hint_len) == 0;
	  else
	    match = strncasecmp (val, hint, hint_len) == 0;
#else
	   
	  match = 1;
#endif

	   
	  if (*hint_text == '~')
	    temp = maybe_restore_tilde (val, directory_part);
	  else
	    temp = savestring (val);
	  freetemp = 1;
	}
      else
	{
	  temp = strrchr (val, '/');

	  if (temp)
	    {
	      temp++;
	      if (igncase == 0)
		freetemp = match = strncmp (temp, hint, hint_len) == 0;
	      else
		freetemp = match = strncasecmp (temp, hint, hint_len) == 0;
	      if (match)
		temp = savestring (temp);
	    }
	  else
	    freetemp = match = 0;
	}

       
#if 0
       
       
      if (match && searching_path == 0 && *val == '.')
	{
	  char *t, *t1;

	  t = get_working_directory ("command-word-completion");
	  t1 = make_absolute (val, t);
	  free (t);
	  cval = sh_canonpath (t1, PATH_CHECKDOTDOT|PATH_CHECKEXISTS);
	}
      else
#endif
	cval = val;

      if (match && executable_completion ((searching_path ? val : cval), searching_path))
	{
	  if (cval != val)
	    free (cval);
	  free (val);
	  val = "";		 
	  return (temp);
	}
      else
	{
	  if (freetemp)
	    free (temp);
	  if (cval != val)
	    free (cval);
	  free (val);
	  goto inner;
	}
    }
}

 
static char *
command_subst_completion_function (text, state)
     const char *text;
     int state;
{
  static char **matches = (char **)NULL;
  static const char *orig_start;
  static char *filename_text = (char *)NULL;
  static int cmd_index, start_len;
  char *value;

  if (state == 0)
    {
      if (filename_text)
	free (filename_text);
      orig_start = text;
      if (*text == '`')
	text++;
      else if (*text == '$' && text[1] == '(')	 
	text += 2;
       
      rl_completion_suppress_quote = 1;
      start_len = text - orig_start;
      filename_text = savestring (text);
      if (matches)
	free (matches);

       
      for (value = filename_text + strlen (filename_text) - 1; value > filename_text; value--)
        if (whitespace (*value) || member (*value, COMMAND_SEPARATORS))
          break;
      if (value <= filename_text)
	matches = rl_completion_matches (filename_text, command_word_completion_function);
      else
	{
	  value++;
	  start_len += value - filename_text;
	  if (whitespace (value[-1]))
	    matches = rl_completion_matches (value, rl_filename_completion_function);
	  else
	    matches = rl_completion_matches (value, command_word_completion_function);
	}

       
      cmd_index = matches && matches[0] && matches[1];

       
      if (matches && matches[0] && matches[1] == 0 && test_for_directory (matches[0]))
	rl_completion_append_character = '/';
      else
	rl_completion_suppress_append = 1;
    }

  if (matches == 0 || matches[cmd_index] == 0)
    {
      rl_filename_quoting_desired = 0;	 
      return ((char *)NULL);
    }
  else
    {
      value = (char *)xmalloc (1 + start_len + strlen (matches[cmd_index]));

      if (start_len == 1)
	value[0] = *orig_start;
      else
	strncpy (value, orig_start, start_len);

      strcpy (value + start_len, matches[cmd_index]);

      cmd_index++;
      return (value);
    }
}

 
static char *
variable_completion_function (text, state)
     const char *text;
     int state;
{
  static char **varlist = (char **)NULL;
  static int varlist_index;
  static char *varname = (char *)NULL;
  static int first_char, first_char_loc;

  if (!state)
    {
      if (varname)
	free (varname);

      first_char_loc = 0;
      first_char = text[0];

      if (first_char == '$')
	first_char_loc++;

      if (text[first_char_loc] == '{')
	first_char_loc++;

      varname = savestring (text + first_char_loc);

      if (varlist)
	strvec_dispose (varlist);

      varlist = all_variables_matching_prefix (varname);
      varlist_index = 0;
    }

  if (!varlist || !varlist[varlist_index])
    {
      return ((char *)NULL);
    }
  else
    {
      char *value;

      value = (char *)xmalloc (4 + strlen (varlist[varlist_index]));

      if (first_char_loc)
	{
	  value[0] = first_char;
	  if (first_char_loc == 2)
	    value[1] = '{';
	}

      strcpy (value + first_char_loc, varlist[varlist_index]);
      if (first_char_loc == 2)
	strcat (value, "}");

      varlist_index++;
      return (value);
    }
}

 
static char *
hostname_completion_function (text, state)
     const char *text;
     int state;
{
  static char **list = (char **)NULL;
  static int list_index = 0;
  static int first_char, first_char_loc;

   
  if (state == 0)
    {
      FREE (list);

      list = (char **)NULL;

      first_char_loc = 0;
      first_char = *text;

      if (first_char == '@')
	first_char_loc++;

      list = hostnames_matching ((char *)text+first_char_loc);
      list_index = 0;
    }

  if (list && list[list_index])
    {
      char *t;

      t = (char *)xmalloc (2 + strlen (list[list_index]));
      *t = first_char;
      strcpy (t + first_char_loc, list[list_index]);
      list_index++;
      return (t);
    }

  return ((char *)NULL);
}

 
char *
bash_servicename_completion_function (text, state)
     const char *text;
     int state;
{
#if defined (__WIN32__) || defined (__OPENNT) || !defined (HAVE_GETSERVENT)
  return ((char *)NULL);
#else
  static char *sname = (char *)NULL;
  static struct servent *srvent;
  static int snamelen;
  char *value;
  char **alist, *aentry;
  int afound;

  if (state == 0)
    {
      FREE (sname);

      sname = savestring (text);
      snamelen = strlen (sname);
      setservent (0);
    }

  while (srvent = getservent ())
    {
      afound = 0;
      if (snamelen == 0 || (STREQN (sname, srvent->s_name, snamelen)))
	break;
       
      for (alist = srvent->s_aliases; *alist; alist++)
	{
	  aentry = *alist;
	  if (STREQN (sname, aentry, snamelen))
	    {
	      afound = 1;
	      break;
	    }
	}

      if (afound)
	break;
    }

  if (srvent == 0)
    {
      endservent ();
      return ((char *)NULL);
    }

  value = afound ? savestring (aentry) : savestring (srvent->s_name);
  return value;
#endif
}

 
char *
bash_groupname_completion_function (text, state)
     const char *text;
     int state;
{
#if defined (__WIN32__) || defined (__OPENNT) || !defined (HAVE_GRP_H)
  return ((char *)NULL);
#else
  static char *gname = (char *)NULL;
  static struct group *grent;
  static int gnamelen;
  char *value;

  if (state == 0)
    {
      FREE (gname);
      gname = savestring (text);
      gnamelen = strlen (gname);

      setgrent ();
    }

  while (grent = getgrent ())
    {
      if (gnamelen == 0 || (STREQN (gname, grent->gr_name, gnamelen)))
        break;
    }

  if (grent == 0)
    {
      endgrent ();
      return ((char *)NULL);
    }

  value = savestring (grent->gr_name);
  return (value);
#endif
}

 

#if defined (BANG_HISTORY)
 
static char *
history_expand_line_internal (line)
     char *line;
{
  char *new_line;
  int old_verify;

  old_verify = hist_verify;
  hist_verify = 0;
  new_line = pre_process_line (line, 0, 0);
  hist_verify = old_verify;

  return (new_line == line) ? savestring (line) : new_line;
}
#endif

 
static void
cleanup_expansion_error ()
{
  char *to_free;
#if defined (BANG_HISTORY)
  int old_verify;

  old_verify = hist_verify;
  hist_verify = 0;
#endif

  fprintf (rl_outstream, "\r\n");
  to_free = pre_process_line (rl_line_buffer, 1, 0);
#if defined (BANG_HISTORY)
  hist_verify = old_verify;
#endif
  if (to_free != rl_line_buffer)
    FREE (to_free);
  putc ('\r', rl_outstream);
  rl_forced_update_display ();
}

 
static void
maybe_make_readline_line (new_line)
     char *new_line;
{
  if (new_line && strcmp (new_line, rl_line_buffer) != 0)
    {
      rl_point = rl_end;

      rl_add_undo (UNDO_BEGIN, 0, 0, 0);
      rl_delete_text (0, rl_point);
      rl_point = rl_end = rl_mark = 0;
      rl_insert_text (new_line);
      rl_add_undo (UNDO_END, 0, 0, 0);
    }
}

 
static void
set_up_new_line (new_line)
     char *new_line;
{
  int old_point, at_end;

  old_point = rl_point;
  at_end = rl_point == rl_end;

   
  maybe_make_readline_line (new_line);
  free (new_line);

   
  if (at_end)
    rl_point = rl_end;
  else if (old_point < rl_end)
    {
      rl_point = old_point;
      if (!whitespace (rl_line_buffer[rl_point]))
	rl_forward_word (1, 0);
    }
}

#if defined (ALIAS)
 
static int
alias_expand_line (count, ignore)
     int count, ignore;
{
  char *new_line;

  new_line = alias_expand (rl_line_buffer);

  if (new_line)
    {
      set_up_new_line (new_line);
      return (0);
    }
  else
    {
      cleanup_expansion_error ();
      return (1);
    }
}
#endif

#if defined (BANG_HISTORY)
 
static int
history_expand_line (count, ignore)
     int count, ignore;
{
  char *new_line;

  new_line = history_expand_line_internal (rl_line_buffer);

  if (new_line)
    {
      set_up_new_line (new_line);
      return (0);
    }
  else
    {
      cleanup_expansion_error ();
      return (1);
    }
}

 
static int
tcsh_magic_space (count, ignore)
     int count, ignore;
{
  int dist_from_end, old_point;

  old_point = rl_point;
  dist_from_end = rl_end - rl_point;
  if (history_expand_line (count, ignore) == 0)
    {
       
      rl_point = (old_point == 0) ? old_point : rl_end - dist_from_end;
      rl_insert (1, ' ');
      return (0);
    }
  else
    return (1);
}
#endif  

 
static int
history_and_alias_expand_line (count, ignore)
     int count, ignore;
{
  char *new_line, *t;

  new_line = 0;
#if defined (BANG_HISTORY)
  new_line = history_expand_line_internal (rl_line_buffer);
#endif

#if defined (ALIAS)
  if (new_line)
    {
      char *alias_line;

      alias_line = alias_expand (new_line);
      free (new_line);
      new_line = alias_line;
    }
#endif  

  if (new_line)
    {
      set_up_new_line (new_line);
      return (0);
    }
  else
    {
      cleanup_expansion_error ();
      return (1);
    }
}

 
static int
shell_expand_line (count, ignore)
     int count, ignore;
{
  char *new_line, *t;
  WORD_LIST *expanded_string;
  WORD_DESC *w;

  new_line = 0;
#if defined (BANG_HISTORY)
  new_line = history_expand_line_internal (rl_line_buffer);
#endif

  t = expand_string_dollar_quote (new_line ? new_line : rl_line_buffer, 0);
  FREE (new_line);
  new_line = t;

#if defined (ALIAS)
  if (new_line)
    {
      char *alias_line;

      alias_line = alias_expand (new_line);
      free (new_line);
      new_line = alias_line;
    }
#endif  

  if (new_line)
    {
      int old_point = rl_point;
      int at_end = rl_point == rl_end;

       
      maybe_make_readline_line (new_line);
      free (new_line);

       

#if 1
      w = alloc_word_desc ();
      w->word = savestring (rl_line_buffer);
      w->flags = rl_explicit_arg ? (W_NOPROCSUB|W_NOCOMSUB) : 0;
      expanded_string = expand_word (w, rl_explicit_arg ? Q_HERE_DOCUMENT : 0);
      dispose_word (w);
#else
      new_line = savestring (rl_line_buffer);
      expanded_string = expand_string (new_line, 0);
      FREE (new_line);
#endif

      if (expanded_string == 0)
	{
	  new_line = (char *)xmalloc (1);
	  new_line[0] = '\0';
	}
      else
	{
	  new_line = string_list (expanded_string);
	  dispose_words (expanded_string);
	}

      maybe_make_readline_line (new_line);
      free (new_line);

       
      if (at_end)
	rl_point = rl_end;
      else if (old_point < rl_end)
	{
	  rl_point = old_point;
	  if (!whitespace (rl_line_buffer[rl_point]))
	    rl_forward_word (1, 0);
	}
      return 0;
    }
  else
    {
      cleanup_expansion_error ();
      return 1;
    }
}

 

static struct ignorevar fignore =
{
  "FIGNORE",
  (struct ign *)0,
  0,
  (char *)0,
  (sh_iv_item_func_t *) 0,
};

static void
_ignore_completion_names (names, name_func)
     char **names;
     sh_ignore_func_t *name_func;
{
  char **newnames;
  int idx, nidx;
  char **oldnames;
  int oidx;

   
  if (names[1] == (char *)0)
    {
      if (force_fignore)
	if ((*name_func) (names[0]) == 0)
	  {
	    free (names[0]);
	    names[0] = (char *)NULL;
	  }

      return;
    }

   
  for (nidx = 1; names[nidx]; nidx++)
    ;
  newnames = strvec_create (nidx + 1);

  if (force_fignore == 0)
    {
      oldnames = strvec_create (nidx - 1);
      oidx = 0;
    }

  newnames[0] = names[0];
  for (idx = nidx = 1; names[idx]; idx++)
    {
      if ((*name_func) (names[idx]))
	newnames[nidx++] = names[idx];
      else if (force_fignore == 0)
	oldnames[oidx++] = names[idx];
      else
	free (names[idx]);
    }

  newnames[nidx] = (char *)NULL;

   
  if (nidx == 1)
    {
      if (force_fignore)
	{
	  free (names[0]);
	  names[0] = (char *)NULL;
	}
      else
	free (oldnames);

      free (newnames);
      return;
    }

  if (force_fignore == 0)
    {
      while (oidx)
	free (oldnames[--oidx]);
      free (oldnames);
    }

   
  if (nidx == 2)
    {
      free (names[0]);
      names[0] = newnames[1];
      names[1] = (char *)NULL;
      free (newnames);
      return;
    }

   
  for (nidx = 1; newnames[nidx]; nidx++)
    names[nidx] = newnames[nidx];
  names[nidx] = (char *)NULL;
  free (newnames);
}

static int
name_is_acceptable (name)
     const char *name;
{
  struct ign *p;
  int nlen;

  for (nlen = strlen (name), p = fignore.ignores; p->val; p++)
    {
      if (nlen > p->len && p->len > 0 && STREQ (p->val, &name[nlen - p->len]))
	return (0);
    }

  return (1);
}

#if 0
static int
ignore_dot_names (name)
     char *name;
{
  return (name[0] != '.');
}
#endif

static int
filename_completion_ignore (names)
     char **names;
{
#if 0
  if (glob_dot_filenames == 0)
    _ignore_completion_names (names, ignore_dot_names);
#endif

  setup_ignore_patterns (&fignore);

  if (fignore.num_ignores == 0)
    return 0;

  _ignore_completion_names (names, name_is_acceptable);

  return 0;
}

 
static int
test_for_directory (name)
     const char *name;
{
  char *fn;
  int r;

  fn = bash_tilde_expand (name, 0);
  r = file_isdir (fn);
  free (fn);

  return (r);
}

static int
test_for_canon_directory (name)
     const char *name;
{
  char *fn;
  int r;

  fn = (*name == '~') ? bash_tilde_expand (name, 0) : savestring (name);
  bash_filename_stat_hook (&fn);
  r = file_isdir (fn);
  free (fn);

  return (r);
}

 
static int
bash_ignore_filenames (names)
     char **names;
{
  _ignore_completion_names (names, test_for_directory);
  return 0;
}

static int
bash_progcomp_ignore_filenames (names)
     char **names;
{
  _ignore_completion_names (names, test_for_canon_directory);
  return 0;
}

static int
return_zero (name)
     const char *name;
{
  return 0;
}

static int
bash_ignore_everything (names)
     char **names;
{
  _ignore_completion_names (names, return_zero);
  return 0;
}

 
static char *
restore_tilde (val, directory_part)
     char *val, *directory_part;
{
  int l, vl, dl2, xl;
  char *dh2, *expdir, *ret, *v;

  vl = strlen (val);

   
  dh2 = directory_part ? bash_dequote_filename (directory_part, 0) : 0;
  bash_directory_expansion (&dh2);
  dl2 = strlen (dh2);

  expdir = bash_tilde_expand (directory_part, 0);
  xl = strlen (expdir);
  if (*directory_part == '~' && STREQ (directory_part, expdir))
    {
       
      v = mbschr (val, '/');
      vl = STRLEN (v);
      ret = (char *)xmalloc (xl + vl + 2);
      strcpy (ret, directory_part);
      if (v && *v)
	strcpy (ret + xl, v);

      free (dh2);
      free (expdir);

      return ret;
    }
  free (expdir);

   
  l = (vl - xl) + 1;
  if (l <= 0)
    {
      free (dh2);
      return (savestring (val));		 
    }

  ret = (char *)xmalloc (dl2 + 2 + l);
  strcpy (ret, dh2);
  strcpy (ret + dl2, val + xl);

  free (dh2);
  return (ret);
}

static char *
maybe_restore_tilde (val, directory_part)
     char *val, *directory_part;
{
  rl_icppfunc_t *save;
  char *ret;

  save = (dircomplete_expand == 0) ? save_directory_hook () : (rl_icppfunc_t *)0;
  ret = restore_tilde (val, directory_part);
  if (save)
    restore_directory_hook (save);
  return ret;
}

 
static void
bash_directory_expansion (dirname)
     char **dirname;
{
  char *d, *nd;

  d = savestring (*dirname);

  if ((rl_directory_rewrite_hook) && (*rl_directory_rewrite_hook) (&d))
    {
      free (*dirname);
      *dirname = d;
    }
  else if (rl_directory_completion_hook && (*rl_directory_completion_hook) (&d))
    {
      free (*dirname);
      *dirname = d;
    }
  else if (rl_completion_found_quote)
    {
      nd = bash_dequote_filename (d, rl_completion_quote_character);
      free (*dirname);
      free (d);
      *dirname = nd;
    }
  else
    free (d);
}

 
static char *
bash_filename_rewrite_hook (fname, fnlen)
     char *fname;
     int fnlen;
{
  char *conv;

  conv = fnx_fromfs (fname, fnlen);
  if (conv != fname)
    conv = savestring (conv);
  return conv;
}

 
 
void
set_directory_hook ()
{
  if (dircomplete_expand)
    {
      rl_directory_completion_hook = bash_directory_completion_hook;
      rl_directory_rewrite_hook = (rl_icppfunc_t *)0;
    }
  else
    {
      rl_directory_rewrite_hook = bash_directory_completion_hook;
      rl_directory_completion_hook = (rl_icppfunc_t *)0;
    }
}

static rl_icppfunc_t *
save_directory_hook ()
{
  rl_icppfunc_t *ret;

  if (dircomplete_expand)
    {
      ret = rl_directory_completion_hook;
      rl_directory_completion_hook = (rl_icppfunc_t *)NULL;
    }
  else
    {
      ret = rl_directory_rewrite_hook;
      rl_directory_rewrite_hook = (rl_icppfunc_t *)NULL;
    }

  return ret;
}

static void
restore_directory_hook (hookf)
     rl_icppfunc_t *hookf;
{
  if (dircomplete_expand)
    rl_directory_completion_hook = hookf;
  else
    rl_directory_rewrite_hook = hookf;
}

 
static int
directory_exists (dirname, should_dequote)
     const char *dirname;
     int should_dequote;
{
  char *new_dirname;
  int dirlen, r;
  struct stat sb;

   
  new_dirname = should_dequote ? bash_dequote_filename ((char *)dirname, rl_completion_quote_character) : savestring (dirname);
  dirlen = STRLEN (new_dirname);
  if (new_dirname[dirlen - 1] == '/')
    new_dirname[dirlen - 1] = '\0';
#if defined (HAVE_LSTAT)
  r = lstat (new_dirname, &sb) == 0;
#else
  r = stat (new_dirname, &sb) == 0;
#endif
  free (new_dirname);
  return (r);
}
  
 
static int
bash_filename_stat_hook (dirname)
     char **dirname;
{
  char *local_dirname, *new_dirname, *t;
  int should_expand_dirname, return_value;
  int global_nounset;
  WORD_LIST *wl;

  local_dirname = *dirname;
  should_expand_dirname = return_value = 0;
  if (t = mbschr (local_dirname, '$'))
    should_expand_dirname = '$';
  else if (t = mbschr (local_dirname, '`'))	 
    should_expand_dirname = '`';

  if (should_expand_dirname && directory_exists (local_dirname, 0))
    should_expand_dirname = 0;
  
  if (should_expand_dirname)  
    {
      new_dirname = savestring (local_dirname);
       
      global_nounset = unbound_vars_is_error;
      unbound_vars_is_error = 0;
      wl = expand_prompt_string (new_dirname, 0, W_NOCOMSUB|W_NOPROCSUB|W_COMPLETE);	 
      unbound_vars_is_error = global_nounset;
      if (wl)
	{
	  free (new_dirname);
	  new_dirname = string_list (wl);
	   
	  if (new_dirname && *new_dirname)
	    {
	      free (local_dirname);	 
	      local_dirname = *dirname = new_dirname;
	      return_value = STREQ (local_dirname, *dirname) == 0;
	    }
	  else
	    free (new_dirname);
	  dispose_words (wl);
	}
      else
	free (new_dirname);
    }	

   
  if (no_symbolic_links == 0 && (local_dirname[0] != '.' || local_dirname[1]))
    {
      char *temp1, *temp2;

      t = get_working_directory ("symlink-hook");
      temp1 = make_absolute (local_dirname, t);
      free (t);
      temp2 = sh_canonpath (temp1, PATH_CHECKDOTDOT|PATH_CHECKEXISTS);

       
      if (temp2 == 0)
	{
	  free (temp1);
	  return return_value;
	}

      free (local_dirname);
      *dirname = temp2;
      free (temp1);
    }

  return (return_value);
}

 
static int
bash_directory_completion_hook (dirname)
     char **dirname;
{
  char *local_dirname, *new_dirname, *t;
  int return_value, should_expand_dirname, nextch, closer;
  WORD_LIST *wl;

  return_value = should_expand_dirname = nextch = closer = 0;
  local_dirname = *dirname;

  should_expand_dirname = bash_check_expchar (local_dirname, 1, &nextch, &closer);

  if (should_expand_dirname && directory_exists (local_dirname, 1))
    should_expand_dirname = 0;

  if (should_expand_dirname)  
    {
      new_dirname = savestring (local_dirname);
      wl = expand_prompt_string (new_dirname, 0, W_NOCOMSUB|W_NOPROCSUB|W_COMPLETE);	 
      if (wl)
	{
	  *dirname = string_list (wl);
	   
	  return_value = STREQ (local_dirname, *dirname) == 0;
	  free (local_dirname);
	  free (new_dirname);
	  dispose_words (wl);
	  local_dirname = *dirname;

	  set_filename_quote_chars (should_expand_dirname, nextch, closer);
	}
      else
	{
	  free (new_dirname);
	  free (local_dirname);
	  *dirname = (char *)xmalloc (1);
	  **dirname = '\0';
	  return 1;
	}
    }
  else 
    {
       
      new_dirname = bash_dequote_filename (local_dirname, rl_completion_quote_character);
      return_value = STREQ (local_dirname, new_dirname) == 0;
      free (local_dirname);
      local_dirname = *dirname = new_dirname;
    }

   

   
  if (no_symbolic_links == 0 && (local_dirname[0] != '.' || local_dirname[1]))
    {
      char *temp1, *temp2;
      int len1, len2;

       
      t = get_working_directory ("symlink-hook");
      temp1 = make_absolute (local_dirname, t);
      free (t);
      temp2 = sh_canonpath (temp1, PATH_CHECKDOTDOT|PATH_CHECKEXISTS);

       
      if (temp2 == 0 && dircomplete_spelling && dircomplete_expand)
	{
	  size_t l1, l2;

	  temp2 = dirspell (temp1);
	  l2 = STRLEN (temp2);
	   
	  if (temp2 && l2 < strlen (temp1) && STREQN (temp1, temp2, l2))
	    {
	      free (temp2);
	      temp2 = 0;
	    }
	  if (temp2)
	    {
	      free (temp1);
	      temp1 = temp2;
	      temp2 = sh_canonpath (temp1, PATH_CHECKDOTDOT|PATH_CHECKEXISTS);
	      return_value |= temp2 != 0;
	    }
	}
       
      if (temp2 == 0)
	{
	  free (temp1);
	  return return_value;
	}
      len1 = strlen (temp1);
      if (temp1[len1 - 1] == '/')
	{
	  len2 = strlen (temp2);
	  if (len2 > 2)		 
	    {
	      temp2 = (char *)xrealloc (temp2, len2 + 2);
	      temp2[len2] = '/';
	      temp2[len2 + 1] = '\0';
	    }
	}

       
      if (dircomplete_expand_relpath || ((local_dirname[0] != '/' && local_dirname[0] != '.') && STREQ (temp1, temp2) == 0))
	return_value |= STREQ (local_dirname, temp2) == 0;
      free (local_dirname);
      *dirname = temp2;
      free (temp1);
    }

  return (return_value);
}

static char **history_completion_array = (char **)NULL;
static int harry_size;
static int harry_len;

static void
build_history_completion_array ()
{
  register int i, j;
  HIST_ENTRY **hlist;
  char **tokens;

   
  if (harry_size)
    {
      strvec_dispose (history_completion_array);
      history_completion_array = (char **)NULL;
      harry_size = 0;
      harry_len = 0;
    }

   
  hlist = history_list ();

  if (hlist)
    {
      for (i = 0; hlist[i]; i++)
	;
      for ( --i; i >= 0; i--)
	{
	   
	  tokens = history_tokenize (hlist[i]->line);

	  for (j = 0; tokens && tokens[j]; j++)
	    {
	      if (harry_len + 2 > harry_size)
	        history_completion_array = strvec_resize (history_completion_array, harry_size += 10);

	      history_completion_array[harry_len++] = tokens[j];
	      history_completion_array[harry_len] = (char *)NULL;
	    }
	  free (tokens);
	}

       
      if (dabbrev_expand_active == 0)
        qsort (history_completion_array, harry_len, sizeof (char *), (QSFUNC *)strvec_strcmp);
    }
}

static char *
history_completion_generator (hint_text, state)
     const char *hint_text;
     int state;
{
  static int local_index, len;
  static const char *text;

   
  if (state == 0)
    {
      if (dabbrev_expand_active)	 
	rl_completion_suppress_append = 1;
      local_index = 0;
      build_history_completion_array ();
      text = hint_text;
      len = strlen (text);
    }

  while (history_completion_array && history_completion_array[local_index])
    {
       
      if (strncmp (text, history_completion_array[local_index++], len) == 0)
	return (savestring (history_completion_array[local_index - 1]));
    }
  return ((char *)NULL);
}

static int
dynamic_complete_history (count, key)
     int count, key;
{
  int r;
  rl_compentry_func_t *orig_func;
  rl_completion_func_t *orig_attempt_func;
  rl_compignore_func_t *orig_ignore_func;

  orig_func = rl_completion_entry_function;
  orig_attempt_func = rl_attempted_completion_function;
  orig_ignore_func = rl_ignore_some_completions_function;

  rl_completion_entry_function = history_completion_generator;
  rl_attempted_completion_function = (rl_completion_func_t *)NULL;
  rl_ignore_some_completions_function = filename_completion_ignore;

   
  if (rl_last_func == dynamic_complete_history)
    r = rl_complete_internal ('?');
  else
    r = rl_complete_internal (TAB);

  rl_completion_entry_function = orig_func;
  rl_attempted_completion_function = orig_attempt_func;
  rl_ignore_some_completions_function = orig_ignore_func;

  return r;
}

static int
bash_dabbrev_expand (count, key)
     int count, key;
{
  int r, orig_suppress, orig_sort;
  rl_compentry_func_t *orig_func;
  rl_completion_func_t *orig_attempt_func;
  rl_compignore_func_t *orig_ignore_func;

  orig_func = rl_menu_completion_entry_function;
  orig_attempt_func = rl_attempted_completion_function;
  orig_ignore_func = rl_ignore_some_completions_function;
  orig_suppress = rl_completion_suppress_append;
  orig_sort = rl_sort_completion_matches;

  rl_menu_completion_entry_function = history_completion_generator;
  rl_attempted_completion_function = (rl_completion_func_t *)NULL;
  rl_ignore_some_completions_function = filename_completion_ignore;
  rl_filename_completion_desired = 0;
  rl_completion_suppress_append = 1;
  rl_sort_completion_matches = 0;

   
  dabbrev_expand_active = 1;
  if (rl_last_func == bash_dabbrev_expand)
    rl_last_func = rl_menu_complete;
  r = rl_menu_complete (count, key);
  dabbrev_expand_active = 0;

  rl_last_func = bash_dabbrev_expand;
  rl_menu_completion_entry_function = orig_func;
  rl_attempted_completion_function = orig_attempt_func;
  rl_ignore_some_completions_function = orig_ignore_func;
  rl_completion_suppress_append = orig_suppress;
  rl_sort_completion_matches = orig_sort;

  return r;
}

#if defined (SPECIFIC_COMPLETION_FUNCTIONS)
static int
bash_complete_username (ignore, ignore2)
     int ignore, ignore2;
{
  return bash_complete_username_internal (rl_completion_mode (bash_complete_username));
}

static int
bash_possible_username_completions (ignore, ignore2)
     int ignore, ignore2;
{
  return bash_complete_username_internal ('?');
}

static int
bash_complete_username_internal (what_to_do)
     int what_to_do;
{
  return bash_specific_completion (what_to_do, rl_username_completion_function);
}

static int
bash_complete_filename (ignore, ignore2)
     int ignore, ignore2;
{
  return bash_complete_filename_internal (rl_completion_mode (bash_complete_filename));
}

static int
bash_possible_filename_completions (ignore, ignore2)
     int ignore, ignore2;
{
  return bash_complete_filename_internal ('?');
}

static int
bash_complete_filename_internal (what_to_do)
     int what_to_do;
{
  rl_compentry_func_t *orig_func;
  rl_completion_func_t *orig_attempt_func;
  rl_icppfunc_t *orig_dir_func;
  rl_compignore_func_t *orig_ignore_func;
  const char *orig_rl_completer_word_break_characters;
  int r;

  orig_func = rl_completion_entry_function;
  orig_attempt_func = rl_attempted_completion_function;
  orig_ignore_func = rl_ignore_some_completions_function;
  orig_rl_completer_word_break_characters = rl_completer_word_break_characters;

  orig_dir_func = save_directory_hook ();

  rl_completion_entry_function = rl_filename_completion_function;
  rl_attempted_completion_function = (rl_completion_func_t *)NULL;
  rl_ignore_some_completions_function = filename_completion_ignore;
  rl_completer_word_break_characters = " \t\n\"\'";

  r = rl_complete_internal (what_to_do);

  rl_completion_entry_function = orig_func;
  rl_attempted_completion_function = orig_attempt_func;
  rl_ignore_some_completions_function = orig_ignore_func;
  rl_completer_word_break_characters = orig_rl_completer_word_break_characters;

  restore_directory_hook (orig_dir_func);

  return r;
}

static int
bash_complete_hostname (ignore, ignore2)
     int ignore, ignore2;
{
  return bash_complete_hostname_internal (rl_completion_mode (bash_complete_hostname));
}

static int
bash_possible_hostname_completions (ignore, ignore2)
     int ignore, ignore2;
{
  return bash_complete_hostname_internal ('?');
}

static int
bash_complete_variable (ignore, ignore2)
     int ignore, ignore2;
{
  return bash_complete_variable_internal (rl_completion_mode (bash_complete_variable));
}

static int
bash_possible_variable_completions (ignore, ignore2)
     int ignore, ignore2;
{
  return bash_complete_variable_internal ('?');
}

static int
bash_complete_command (ignore, ignore2)
     int ignore, ignore2;
{
  return bash_complete_command_internal (rl_completion_mode (bash_complete_command));
}

static int
bash_possible_command_completions (ignore, ignore2)
     int ignore, ignore2;
{
  return bash_complete_command_internal ('?');
}

static int
bash_complete_hostname_internal (what_to_do)
     int what_to_do;
{
  return bash_specific_completion (what_to_do, hostname_completion_function);
}

static int
bash_complete_variable_internal (what_to_do)
     int what_to_do;
{
  return bash_specific_completion (what_to_do, variable_completion_function);
}

static int
bash_complete_command_internal (what_to_do)
     int what_to_do;
{
  return bash_specific_completion (what_to_do, command_word_completion_function);
}

static int
completion_glob_pattern (string)
     char *string;
{
  return (glob_pattern_p (string) == 1);
}

static char *globtext;
static char *globorig;

static char *
glob_complete_word (text, state)
     const char *text;
     int state;
{
  static char **matches = (char **)NULL;
  static int ind;
  int glen;
  char *ret, *ttext;

  if (state == 0)
    {
      rl_filename_completion_desired = 1;
      FREE (matches);
      if (globorig != globtext)
	FREE (globorig);
      FREE (globtext);

      ttext = bash_tilde_expand (text, 0);

      if (rl_explicit_arg)
	{
	  globorig = savestring (ttext);
	  glen = strlen (ttext);
	  globtext = (char *)xmalloc (glen + 2);
	  strcpy (globtext, ttext);
	  globtext[glen] = '*';
	  globtext[glen+1] = '\0';
	}
      else
        globtext = globorig = savestring (ttext);

      if (ttext != text)
	free (ttext);

      matches = shell_glob_filename (globtext, 0);
      if (GLOB_FAILED (matches))
	matches = (char **)NULL;
      ind = 0;
    }

  ret = matches ? matches[ind] : (char *)NULL;
  ind++;
  return ret;
}

static int
bash_glob_completion_internal (what_to_do)
     int what_to_do;
{
  return bash_specific_completion (what_to_do, glob_complete_word);
}

 
static char *
bash_glob_quote_filename (s, rtype, qcp)
     char *s;
     int rtype;
     char *qcp;
{
  if (globorig && qcp && *qcp == '\0' && STREQ (s, globorig))
    return (savestring (s));
  else
    return (bash_quote_filename (s, rtype, qcp));
}

static int
bash_glob_complete_word (count, key)
     int count, key;
{
  int r;
  rl_quote_func_t *orig_quoting_function;

  if (rl_editing_mode == EMACS_EDITING_MODE)
    rl_explicit_arg = 1;	 
  orig_quoting_function = rl_filename_quoting_function;
  rl_filename_quoting_function = bash_glob_quote_filename;
  
  r = bash_glob_completion_internal (rl_completion_mode (bash_glob_complete_word));

  rl_filename_quoting_function = orig_quoting_function;
  return r;
}

static int
bash_glob_expand_word (count, key)
     int count, key;
{
  return bash_glob_completion_internal ('*');
}

static int
bash_glob_list_expansions (count, key)
     int count, key;
{
  return bash_glob_completion_internal ('?');
}

static int
bash_specific_completion (what_to_do, generator)
     int what_to_do;
     rl_compentry_func_t *generator;
{
  rl_compentry_func_t *orig_func;
  rl_completion_func_t *orig_attempt_func;
  rl_compignore_func_t *orig_ignore_func;
  int r;

  orig_func = rl_completion_entry_function;
  orig_attempt_func = rl_attempted_completion_function;
  orig_ignore_func = rl_ignore_some_completions_function;
  rl_completion_entry_function = generator;
  rl_attempted_completion_function = NULL;
  rl_ignore_some_completions_function = orig_ignore_func;

  r = rl_complete_internal (what_to_do);

  rl_completion_entry_function = orig_func;
  rl_attempted_completion_function = orig_attempt_func;
  rl_ignore_some_completions_function = orig_ignore_func;

  return r;
}

#endif	 

#if defined (VI_MODE)
 
static int
bash_vi_complete (count, key)
     int count, key;
{
#if defined (SPECIFIC_COMPLETION_FUNCTIONS)
  int p, r;
  char *t;

  if ((rl_point < rl_end) && (!whitespace (rl_line_buffer[rl_point])))
    {
      if (!whitespace (rl_line_buffer[rl_point + 1]))
	rl_vi_end_word (1, 'E');
      rl_point++;
    }

   
  t = 0;
  if (rl_point > 0)
    {
      p = rl_point;
      rl_vi_bWord (1, 'B');
      r = rl_point;
      rl_point = p;
      p = r;

      t = substring (rl_line_buffer, p, rl_point);
    }      

  if (t && completion_glob_pattern (t) == 0)
    rl_explicit_arg = 1;	 
  FREE (t);

  if (key == '*')	 
    r = bash_glob_expand_word (count, key);
  else if (key == '=')	 
    r = bash_glob_list_expansions (count, key);
  else if (key == '\\')	 
    r = bash_glob_complete_word (count, key);
  else
    r = rl_complete (0, key);

  if (key == '*' || key == '\\')
    rl_vi_start_inserting (key, 1, 1);

  return (r);
#else
  return rl_vi_complete (count, key);
#endif  
}
#endif  

 
 
static char *
bash_dequote_filename (text, quote_char)
     char *text;
     int quote_char;
{
  char *ret, *p, *r;
  int l, quoted;

  l = strlen (text);
  ret = (char *)xmalloc (l + 1);
  for (quoted = quote_char, p = text, r = ret; p && *p; p++)
    {
       
      if (*p == '\\')
	{
	   
	  if (quoted == '\'')
	    *r++ = *p;
	   
	  else if (quoted == '"' && ((sh_syntaxtab[(unsigned char)p[1]] & CBSDQUOTE) == 0))
	    *r++ = *p;

	  *r++ = *++p;
	  if (*p == '\0')
	    return ret;		 
	  continue;
	}
       
      if (quoted && *p == quoted)
	{
	  quoted = 0;
	  continue;
	}
       
      if (quoted == 0 && (*p == '\'' || *p == '"'))
	{
	  quoted = *p;
	  continue;
	}
      *r++ = *p;
    }
  *r = '\0';
  return ret;
}

 
static char *
quote_word_break_chars (text)
     char *text;
{
  char *ret, *r, *s;
  int l;

  l = strlen (text);
  ret = (char *)xmalloc ((2 * l) + 1);
  for (s = text, r = ret; *s; s++)
    {
       
      if (*s == '\\')
	{
	  *r++ = '\\';
	  *r++ = *++s;
	  if (*s == '\0')
	    break;
	  continue;
	}
       
      if (mbschr (rl_completer_word_break_characters, *s))
	*r++ = '\\';
       
      if (s == text && *s == '~' && file_exists (text))
        *r++ = '\\';
      *r++ = *s;
    }
  *r = '\0';
  return ret;
}

 
static int
bash_check_expchar (dirname, need_closer, nextp, closerp)
     char *dirname;
     int need_closer;
     int *nextp, *closerp;
{
  char *t;
  int ret, n, c;

  ret = n = c = 0;
  if (t = mbschr (dirname, '$'))
    {
      ret = '$';
      n = t[1];
       
      if (n == '(')
	c = ')';
      else if (n == '{')
	c = '}';
      else
	n = 0;

      if (c && need_closer)		 
	{
	  int p;
	  char delims[2];

	  delims[0] = c; delims[1] = 0;
	  p = skip_to_delim (t, 1, delims, SD_NOJMP|SD_COMPLETE);
	  if (t[p] != c)
	    ret = 0;
	}
    }
  else if (dirname[0] == '~')
    ret = '~';
  else
    {
      t = mbschr (dirname, '`');
      if (t)
	{
	  if (need_closer == 0)
	    ret = '`';
	  else if (unclosed_pair (dirname, strlen (dirname), "`") == 0)
	    ret = '`';
	}
    }

  if (nextp)
    *nextp = n;
  if (closerp)
    *closerp = c;

  return ret;
}

 
static void
set_filename_quote_chars (expchar, nextch, closer)
     int expchar, nextch, closer;
{
  int i, j, c;

  if (rl_filename_quote_characters && *rl_filename_quote_characters)
    {
      i = strlen (default_filename_quote_characters);
      custom_filename_quote_characters = xrealloc (custom_filename_quote_characters, i+1);
      for (i = j = 0; c = default_filename_quote_characters[i]; i++)
	{
	  if (c == expchar || c == nextch || c == closer)
	    continue;
	  custom_filename_quote_characters[j++] = c;
	}
      custom_filename_quote_characters[j] = '\0';
      rl_filename_quote_characters = custom_filename_quote_characters;
      set_filename_bstab (rl_filename_quote_characters);
    }
}

 
static void
set_filename_bstab (string)
     const char *string;
{
  const char *s;

  memset (filename_bstab, 0, sizeof (filename_bstab));
  for (s = string; s && *s; s++)
    filename_bstab[(unsigned char)*s] = 1;
}

 
static char *
bash_quote_filename (s, rtype, qcp)
     char *s;
     int rtype;
     char *qcp;
{
  char *rtext, *mtext, *ret;
  int rlen, cs;
  int expchar, nextch, closer;

  rtext = (char *)NULL;

   

  cs = completion_quoting_style;
   
  expchar = nextch = closer = 0;
  if (*qcp == '\0' && cs == COMPLETE_BSQUOTE && dircomplete_expand == 0 &&
      (expchar = bash_check_expchar (s, 0, &nextch, &closer)) &&
      file_exists (s) == 0)
    {
       
      if (rl_filename_quote_characters != custom_filename_quote_characters)
	set_filename_quote_chars (expchar, nextch, closer);
      complete_fullquote = 0;
    }
  else if (*qcp == '\0' && cs == COMPLETE_BSQUOTE && mbschr (s, '\n'))
    cs = COMPLETE_SQUOTE;
  else if (*qcp == '"')
    cs = COMPLETE_DQUOTE;
  else if (*qcp == '\'')
    cs = COMPLETE_SQUOTE;
#if defined (BANG_HISTORY)
  else if (*qcp == '\0' && history_expansion && cs == COMPLETE_DQUOTE &&
	   history_expansion_inhibited == 0 && mbschr (s, '!'))
    cs = COMPLETE_BSQUOTE;

  if (*qcp == '"' && history_expansion && cs == COMPLETE_DQUOTE &&
	history_expansion_inhibited == 0 && mbschr (s, '!'))
    {
      cs = COMPLETE_BSQUOTE;
      *qcp = '\0';
    }
#endif

   
  mtext = s;
  if (mtext[0] == '~' && rtype == SINGLE_MATCH && cs != COMPLETE_BSQUOTE)
    mtext = bash_tilde_expand (s, 0);

  switch (cs)
    {
    case COMPLETE_DQUOTE:
      rtext = sh_double_quote (mtext);
      break;
    case COMPLETE_SQUOTE:
      rtext = sh_single_quote (mtext);
      break;
    case COMPLETE_BSQUOTE:
      rtext = sh_backslash_quote (mtext, complete_fullquote ? 0 : filename_bstab, 0);
      break;
    }

  if (mtext != s)
    free (mtext);

   
   
  if (rtext && cs == COMPLETE_BSQUOTE && rl_completer_word_break_characters)
    {
      mtext = quote_word_break_chars (rtext);
      free (rtext);
      rtext = mtext;
    }

   
  if (rtext)
    {
      rlen = strlen (rtext);
      ret = (char *)xmalloc (rlen + 1);
      strcpy (ret, rtext);
    }
  else
    {
      ret = (char *)xmalloc (rlen = 1);
      ret[0] = '\0';
    }

   
  if (rtype == MULT_MATCH && cs != COMPLETE_BSQUOTE)
    ret[rlen - 1] = '\0';
  free (rtext);
  return ret;
}

 

static Keymap emacs_std_cmd_xmap;
#if defined (VI_MODE)
static Keymap vi_insert_cmd_xmap;
static Keymap vi_movement_cmd_xmap;
#endif

#ifdef _MINIX
static void
#else
static int
#endif
putx(c)
     int c;
{
  int x;
  x = putc (c, rl_outstream);
#ifndef _MINIX
  return x;
#endif
}

static int
readline_get_char_offset (ind)
     int ind;
{
  int r, old_ch;

  r = ind;
#if defined (HANDLE_MULTIBYTE)
  if (locale_mb_cur_max > 1)
    {
      old_ch = rl_line_buffer[ind];
      rl_line_buffer[ind] = '\0';
      r = MB_STRLEN (rl_line_buffer);
      rl_line_buffer[ind] = old_ch;
    }
#endif
  return r;
}

static void
readline_set_char_offset (ind, varp)
     int ind;
     int *varp;
{
  int i;

  i = ind;

#if defined (HANDLE_MULTIBYTE)
  if (i > 0 && locale_mb_cur_max > 1)
    i = _rl_find_next_mbchar (rl_line_buffer, 0, i, 0);		 
#endif
  if (i != *varp)
    {
      if (i > rl_end)
	i = rl_end;
      else if (i < 0)
	i = 0;
      *varp = i;
    }
}

int
bash_execute_unix_command (count, key)
     int count;	 
     int key;
{
  int type;
  register int i, r;
  intmax_t mi;
  sh_parser_state_t ps;
  char *cmd, *value, *ce, old_ch;
  SHELL_VAR *v;
  char ibuf[INT_STRLEN_BOUND(int) + 1];
  Keymap cmd_xmap;
  const char *kseq;
  size_t kslen;

  kseq = rl_executing_keyseq;
  kslen = rl_key_sequence_length;

   
  if (count > 1 || rl_explicit_arg)
    {
      i = rl_trim_arg_from_keyseq (rl_executing_keyseq, rl_key_sequence_length, rl_get_keymap ());
      if (i > 0)
	{
	 kseq = rl_executing_keyseq + i;
	 kslen = rl_key_sequence_length - i;
	}
    }

   
  cmd_xmap = get_cmd_xmap_from_keymap (rl_get_keymap ());
  cmd = (char *)rl_function_of_keyseq_len (kseq, kslen, cmd_xmap, &type);

  if (type == ISKMAP && (type = ((Keymap) cmd)[ANYOTHERKEY].type) == ISMACR)
    cmd = (char*)((Keymap) cmd)[ANYOTHERKEY].function;

  if (cmd == 0 || type != ISMACR)
    {
      rl_crlf ();
      internal_error (_("bash_execute_unix_command: cannot find keymap for command"));
      rl_forced_update_display ();
      return 1;
    }

  ce = rl_get_termcap ("ce");
  if (ce)	 
    {
      rl_clear_visible_line ();
      fflush (rl_outstream);
    }
  else
    rl_crlf ();	 

  v = bind_variable ("READLINE_LINE", rl_line_buffer, 0);
  if (v)
    VSETATTR (v, att_exported);

  i = readline_get_char_offset (rl_point);
  value = inttostr (i, ibuf, sizeof (ibuf));
  v = bind_int_variable ("READLINE_POINT", value, 0);
  if (v)
    VSETATTR (v, att_exported);

  i = readline_get_char_offset (rl_mark);
  value = inttostr (i, ibuf, sizeof (ibuf));
  v = bind_int_variable ("READLINE_MARK", value, 0);
  if (v)
    VSETATTR (v, att_exported);

  if (count > 1 || rl_explicit_arg)
    {
      value = inttostr (count, ibuf, sizeof (ibuf));
      v = bind_int_variable ("READLINE_ARGUMENT", value, 0);
      if (v)
        VSETATTR (v, att_exported);
    }
  array_needs_making = 1;

  save_parser_state (&ps);
  rl_clear_signals ();
  r = parse_and_execute (savestring (cmd), "bash_execute_unix_command", SEVAL_NOHIST);
  rl_set_signals ();
  restore_parser_state (&ps);

  v = find_variable ("READLINE_LINE");
  maybe_make_readline_line (v ? value_cell (v) : 0);

  v = find_variable ("READLINE_POINT");
  if (v && legal_number (value_cell (v), &mi))
    readline_set_char_offset (mi, &rl_point);

  v = find_variable ("READLINE_MARK");
  if (v && legal_number (value_cell (v), &mi))
    readline_set_char_offset (mi, &rl_mark);

  check_unbind_variable ("READLINE_LINE");
  check_unbind_variable ("READLINE_POINT");
  check_unbind_variable ("READLINE_MARK");
  check_unbind_variable ("READLINE_ARGUMENT");
  array_needs_making = 1;

   
   
  if (ce && r != 124)
    rl_redraw_prompt_last_line ();
  else
    rl_forced_update_display ();

  return 0;
}

int
print_unix_command_map ()
{
  Keymap save, cmd_xmap;

  save = rl_get_keymap ();
  cmd_xmap = get_cmd_xmap_from_keymap (save);
  rl_set_keymap (cmd_xmap);
  rl_macro_dumper (1);
  rl_set_keymap (save);
  return 0;
}

static void
init_unix_command_map ()
{
  emacs_std_cmd_xmap = rl_make_bare_keymap ();

  emacs_std_cmd_xmap[CTRL('X')].type = ISKMAP;
  emacs_std_cmd_xmap[CTRL('X')].function = KEYMAP_TO_FUNCTION (rl_make_bare_keymap ());
  emacs_std_cmd_xmap[ESC].type = ISKMAP;
  emacs_std_cmd_xmap[ESC].function = KEYMAP_TO_FUNCTION (rl_make_bare_keymap ());

#if defined (VI_MODE)  
  vi_insert_cmd_xmap = rl_make_bare_keymap ();
  vi_movement_cmd_xmap = rl_make_bare_keymap ();
#endif
}

static Keymap
get_cmd_xmap_from_edit_mode ()
{
  if (emacs_std_cmd_xmap == 0)
    init_unix_command_map ();

  switch (rl_editing_mode)
    {
    case EMACS_EDITING_MODE:
      return emacs_std_cmd_xmap;
#if defined (VI_MODE)
    case VI_EDITING_MODE:
      return (get_cmd_xmap_from_keymap (rl_get_keymap ()));
#endif
    default:
      return (Keymap)NULL;
    }
}

static Keymap
get_cmd_xmap_from_keymap (kmap)
     Keymap kmap;
{
  if (emacs_std_cmd_xmap == 0)
    init_unix_command_map ();

  if (kmap == emacs_standard_keymap)
    return emacs_std_cmd_xmap;
  else if (kmap == emacs_meta_keymap)
    return (FUNCTION_TO_KEYMAP (emacs_std_cmd_xmap, ESC));
  else if (kmap == emacs_ctlx_keymap)
    return (FUNCTION_TO_KEYMAP (emacs_std_cmd_xmap, CTRL('X')));
#if defined (VI_MODE)
  else if (kmap == vi_insertion_keymap)
    return vi_insert_cmd_xmap;
  else if (kmap == vi_movement_keymap)
    return vi_movement_cmd_xmap;
#endif
  else
    return (Keymap)NULL;
}

static int
isolate_sequence (string, ind, need_dquote, startp)
     char *string;
     int ind, need_dquote, *startp;
{
  register int i;
  int c, passc, delim;

  for (i = ind; string[i] && whitespace (string[i]); i++)
    ;
   
  if (need_dquote && string[i] != '"')
    {
      builtin_error (_("%s: first non-whitespace character is not `\"'"), string);
      return -1;
    }

   
  delim = (string[i] == '"' || string[i] == '\'') ? string[i] : 0;
    
  if (startp)
    *startp = delim ? ++i : i;

  for (passc = 0; c = string[i]; i++)
    {
      if (passc)
	{
	  passc = 0;
	  continue;
	}
      if (c == '\\')
	{
	  passc++;
	  continue;
	}
      if (c == delim)
	break;
    }

  if (delim && string[i] != delim)
    {
      builtin_error (_("no closing `%c' in %s"), delim, string);
      return -1;
    }

  return i;
}

int
bind_keyseq_to_unix_command (line)
     char *line;
{
  Keymap kmap, cmd_xmap;
  char *kseq, *value;
  int i, kstart;

  kmap = rl_get_keymap ();

   
  i = isolate_sequence (line, 0, 1, &kstart);
  if (i < 0)
    return -1;

   
  kseq = substring (line, kstart, i);

  for ( ; line[i] && line[i] != ':'; i++)
    ;
  if (line[i] != ':')
    {
      builtin_error (_("%s: missing colon separator"), line);
      FREE (kseq);
      return -1;
    }

  i = isolate_sequence (line, i + 1, 0, &kstart);
  if (i < 0)
    {
      FREE (kseq);
      return -1;
    }

   
  value = substring (line, kstart, i);

   
  cmd_xmap = get_cmd_xmap_from_keymap (kmap);
  rl_generic_bind (ISMACR, kseq, value, cmd_xmap);

   
  rl_bind_keyseq_in_map (kseq, bash_execute_unix_command, kmap);

  free (kseq);  
  return 0;
}

int
unbind_unix_command (kseq)
     char *kseq;
{
  Keymap cmd_xmap;

  cmd_xmap = get_cmd_xmap_from_keymap (rl_get_keymap ());
  if (rl_bind_keyseq_in_map (kseq, (rl_command_func_t *)NULL, cmd_xmap) != 0)
    {
      builtin_error (_("`%s': cannot unbind in command keymap"), kseq);
      return 0;
    }
  return 1;
}

 
char **
bash_directory_completion_matches (text)
     const char *text;
{
  char **m1;
  char *dfn;
  int qc;

  qc = rl_dispatching ? rl_completion_quote_character : 0;  
   
  if (rl_dispatching && rl_completion_found_quote == 0)
    dfn = bash_dequote_filename ((char *)text, qc);
  else
    dfn = (char *)text;
  m1 = rl_completion_matches (dfn, rl_filename_completion_function);
  if (dfn != text)
    free (dfn);

  if (m1 == 0 || m1[0] == 0)
    return m1;
   
  (void)bash_progcomp_ignore_filenames (m1);
  return m1;
}

char *
bash_dequote_text (text)
     const char *text;
{
  char *dtxt;
  int qc;

  qc = (text[0] == '"' || text[0] == '\'') ? text[0] : 0;
  dtxt = bash_dequote_filename ((char *)text, qc);
  return (dtxt);
}

 
static int
bash_event_hook ()
{
  int sig;

   
  if (sigterm_received)
    {
       
      return 0;
    }

  sig = 0;
  if (terminating_signal)
    sig = terminating_signal;
  else if (interrupt_state)
    sig = SIGINT;
  else if (read_timeout && read_timeout->alrmflag)
    sig = SIGALRM;
  else if (RL_ISSTATE (RL_STATE_TIMEOUT))		 
    {
      sig = SIGALRM;
      if (read_timeout)
	read_timeout->alrmflag = 1;
    }
  else
    sig = first_pending_trap ();

   
  if (terminating_signal || interrupt_state || (read_timeout && read_timeout->alrmflag))
    rl_cleanup_after_signal ();
  bashline_reset_event_hook ();

  RL_UNSETSTATE (RL_STATE_TIMEOUT);			 

   
  if (posixly_correct && this_shell_builtin == read_builtin && sig == SIGINT)
    {
      last_command_exit_value = 128|SIGINT;
      throw_to_top_level ();
    }

  check_signals_and_traps ();	 
  return 0;
}

#endif  
