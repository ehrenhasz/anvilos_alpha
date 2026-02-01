 

 

#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <sys/types.h>
#include "posixstat.h"
#include <fcntl.h>
#if defined (HAVE_SYS_FILE_H)
#  include <sys/file.h>
#endif  

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif  

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif  

#if defined (HAVE_LOCALE_H)
#  include <locale.h>
#endif

#include <stdio.h>
#include "posixjmp.h"
#include <errno.h>

#if !defined (errno)
extern int errno;
#endif  

 
#include "rldefs.h"
#include "rlmbutil.h"

#if defined (__EMX__)
#  define INCL_DOSPROCESS
#  include <os2.h>
#endif  

 
#include "readline.h"
#include "history.h"

#include "rlprivate.h"
#include "rlshell.h"
#include "xmalloc.h"

#if defined (COLOR_SUPPORT)
#  include "parse-colors.h"
#endif

#ifndef RL_LIBRARY_VERSION
#  define RL_LIBRARY_VERSION "8.1"
#endif

#ifndef RL_READLINE_VERSION
#  define RL_READLINE_VERSION	0x0801
#endif

 
static char *readline_internal (void);
static void readline_initialize_everything (void);

static void run_startup_hooks (void);

static void bind_arrow_keys_internal (Keymap);
static void bind_arrow_keys (void);

static void bind_bracketed_paste_prefix (void);

static void readline_default_bindings (void);
static void reset_default_bindings (void);

static int _rl_subseq_result (int, Keymap, int, int);
static int _rl_subseq_getchar (int);

 
 
 
 
 

const char *rl_library_version = RL_LIBRARY_VERSION;

int rl_readline_version = RL_READLINE_VERSION;

 
int rl_gnu_readline_p = 1;

 
Keymap _rl_keymap = emacs_standard_keymap;

 
int rl_editing_mode = emacs_mode;

 
int rl_insert_mode = RL_IM_DEFAULT;

 
int rl_dispatching;

 
int _rl_last_command_was_kill = 0;

 
int rl_numeric_arg = 1;

 
int rl_explicit_arg = 0;

 
int rl_arg_sign = 1;

 
static int rl_initialized;

#if 0
 
static int running_in_emacs;
#endif

 
unsigned long rl_readline_state = RL_STATE_NONE;

 
int rl_point;

 
int rl_mark;

 
int rl_end;

 
int rl_done;

 
int rl_eof_found = 0;

 
rl_command_func_t *rl_last_func = (rl_command_func_t *)NULL;

 
procenv_t _rl_top_level;

 
FILE *_rl_in_stream, *_rl_out_stream;

 
FILE *rl_instream = (FILE *)NULL;
FILE *rl_outstream = (FILE *)NULL;

 
int _rl_echoing_p = 0;

 
char *rl_prompt = (char *)NULL;
int rl_visible_prompt_length = 0;

 
int rl_already_prompted = 0;

 
int rl_key_sequence_length = 0;

 
rl_hook_func_t *rl_startup_hook = (rl_hook_func_t *)NULL;

 
rl_hook_func_t *_rl_internal_startup_hook = (rl_hook_func_t *)NULL;

 
rl_hook_func_t *rl_pre_input_hook = (rl_hook_func_t *)NULL;

 
static char *the_line;

 
int _rl_eof_char = CTRL ('D');

 
int rl_pending_input = 0;

 
const char *rl_terminal_name = (const char *)NULL;

 
int _rl_horizontal_scroll_mode = 0;

 
int _rl_mark_modified_lines = 0;

 
int _rl_bell_preference = AUDIBLE_BELL;
     
 
char *_rl_comment_begin;

 
Keymap rl_executing_keymap;

 
rl_command_func_t *_rl_executing_func;

 
Keymap _rl_dispatching_keymap;

 
int rl_erase_empty_line = 0;

 
int rl_num_chars_to_read = 0;

 
char *rl_line_buffer = (char *)NULL;
int rl_line_buffer_len = 0;

 
_rl_keyseq_cxt *_rl_kscxt = 0;

int rl_executing_key;
char *rl_executing_keyseq = 0;
int _rl_executing_keyseq_size = 0;

struct _rl_cmd _rl_pending_command;
struct _rl_cmd *_rl_command_to_execute = (struct _rl_cmd *)NULL;

 
int _rl_keyseq_timeout = 500;

#define RESIZE_KEYSEQ_BUFFER() \
  do \
    { \
      if (rl_key_sequence_length + 2 >= _rl_executing_keyseq_size) \
	{ \
	  _rl_executing_keyseq_size += 16; \
	  rl_executing_keyseq = xrealloc (rl_executing_keyseq, _rl_executing_keyseq_size); \
	} \
    } \
  while (0);
        
 

 
 
 
 
 

 
unsigned char _rl_parsing_conditionalized_out = 0;

 
int _rl_convert_meta_chars_to_ascii = 1;

 
int _rl_output_meta_chars = 0;

 
int _rl_bind_stty_chars = 1;

 
int _rl_revert_all_at_newline = 0;

 
int _rl_echo_control_chars = 1;

 
int _rl_show_mode_in_prompt = 0;

 
int _rl_enable_bracketed_paste = BRACKETED_PASTE_DEFAULT;
int _rl_enable_active_region = BRACKETED_PASTE_DEFAULT;

 
 
 
 
 

 
int _rl_meta_flag = 0;	 

 
int
rl_set_prompt (const char *prompt)
{
  FREE (rl_prompt);
  rl_prompt = prompt ? savestring (prompt) : (char *)NULL;
  rl_display_prompt = rl_prompt ? rl_prompt : "";

  rl_visible_prompt_length = rl_expand_prompt (rl_prompt);
  return 0;
}
  
 
char *
readline (const char *prompt)
{
  char *value;
#if 0
  int in_callback;
#endif

   
  if (rl_pending_input == EOF)
    {
      rl_clear_pending_input ();
      return ((char *)NULL);
    }

#if 0
   
  if (in_callback = RL_ISSTATE (RL_STATE_CALLBACK))
    RL_UNSETSTATE (RL_STATE_CALLBACK);
#endif

  rl_set_prompt (prompt);

  rl_initialize ();
  if (rl_prep_term_function)
    (*rl_prep_term_function) (_rl_meta_flag);

#if defined (HANDLE_SIGNALS)
  rl_set_signals ();
#endif

  value = readline_internal ();
  if (rl_deprep_term_function)
    (*rl_deprep_term_function) ();

#if defined (HANDLE_SIGNALS)
  rl_clear_signals ();
#endif

#if 0
  if (in_callback)
    RL_SETSTATE (RL_STATE_CALLBACK);
#endif

#if HAVE_DECL_AUDIT_USER_TTY && defined (HAVE_LIBAUDIT_H) && defined (ENABLE_TTY_AUDIT_SUPPORT)
  if (value)
    _rl_audit_tty (value);
#endif

  return (value);
}

static void
run_startup_hooks (void)
{
  if (rl_startup_hook)
    (*rl_startup_hook) ();

  if (_rl_internal_startup_hook)
    (*_rl_internal_startup_hook) ();
}

#if defined (READLINE_CALLBACKS)
#  define STATIC_CALLBACK
#else
#  define STATIC_CALLBACK static
#endif

STATIC_CALLBACK void
readline_internal_setup (void)
{
  char *nprompt;

  _rl_in_stream = rl_instream;
  _rl_out_stream = rl_outstream;

   
  if (_rl_enable_meta & RL_ISSTATE (RL_STATE_TERMPREPPED))
    _rl_enable_meta_key ();

  run_startup_hooks ();

  rl_deactivate_mark ();

#if defined (VI_MODE)
  if (rl_editing_mode == vi_mode)
    rl_vi_insertion_mode (1, 'i');	 
  else
#endif  
    if (_rl_show_mode_in_prompt)
      _rl_reset_prompt ();

   
  if (_rl_echoing_p == 0 && rl_redisplay_function == rl_redisplay)
    {
      if (rl_prompt && rl_already_prompted == 0)
	{
	  nprompt = _rl_strip_prompt (rl_prompt);
	  fprintf (_rl_out_stream, "%s", nprompt);
	  fflush (_rl_out_stream);
	  xfree (nprompt);
	}
    }
  else
    {
      if (rl_prompt && rl_already_prompted)
	rl_on_new_line_with_prompt ();
      else
	rl_on_new_line ();
      (*rl_redisplay_function) ();
    }

  if (rl_pre_input_hook)
    (*rl_pre_input_hook) ();

  RL_CHECK_SIGNALS ();
}

STATIC_CALLBACK char *
readline_internal_teardown (int eof)
{
  char *temp;
  HIST_ENTRY *entry;

  RL_CHECK_SIGNALS ();

  if (eof)
    RL_SETSTATE (RL_STATE_EOF);		 

   
  entry = current_history ();

   
  if (entry && rl_undo_list)
   {
      temp = savestring (the_line);
      rl_revert_line (1, 0);
      entry = replace_history_entry (where_history (), the_line, (histdata_t)NULL);
      _rl_free_history_entry (entry);

      strcpy (the_line, temp);
      xfree (temp);
    }

  if (_rl_revert_all_at_newline)
    _rl_revert_all_lines ();

   
  if (rl_undo_list)
    rl_free_undo_list ();

   
  _rl_disable_meta_key ();

   
  _rl_set_insert_mode (RL_IM_INSERT, 0);

  return (eof ? (char *)NULL : savestring (the_line));
}

void
_rl_internal_char_cleanup (void)
{
  if (_rl_keep_mark_active)
    _rl_keep_mark_active = 0;
  else if (rl_mark_active_p ())
    rl_deactivate_mark ();

#if defined (VI_MODE)
   
  if (rl_editing_mode == vi_mode && _rl_keymap == vi_movement_keymap)
    rl_vi_check ();
#endif  

  if (rl_num_chars_to_read && rl_end >= rl_num_chars_to_read)
    {
      (*rl_redisplay_function) ();
      _rl_want_redisplay = 0;
      rl_newline (1, '\n');
    }

  if (rl_done == 0)
    {
      (*rl_redisplay_function) ();
      _rl_want_redisplay = 0;
    }

   
  if (rl_erase_empty_line && rl_done && rl_last_func == rl_newline &&
      rl_point == 0 && rl_end == 0)
    _rl_erase_entire_line ();
}

STATIC_CALLBACK int
#if defined (READLINE_CALLBACKS)
readline_internal_char (void)
#else
readline_internal_charloop (void)
#endif
{
  static int lastc, eof_found;
  int c, code, lk, r;

  lastc = EOF;

#if !defined (READLINE_CALLBACKS)
  eof_found = 0;
  while (rl_done == 0)
    {
#endif
      lk = _rl_last_command_was_kill;

#if defined (HAVE_POSIX_SIGSETJMP)
      code = sigsetjmp (_rl_top_level, 0);
#else
      code = setjmp (_rl_top_level);
#endif

      if (code)
	{
	  (*rl_redisplay_function) ();
	  _rl_want_redisplay = 0;

	   
	  if (RL_ISSTATE (RL_STATE_TIMEOUT))
	    {
	      RL_SETSTATE (RL_STATE_DONE);
	      rl_done = 1;
	      return 1;
	    }

	   
	  if (RL_ISSTATE (RL_STATE_CALLBACK))
	    return (0);
	}

      if (rl_pending_input == 0)
	{
	   
	  _rl_reset_argument ();
	  rl_executing_keyseq[rl_key_sequence_length = 0] = '\0';
	}

      RL_SETSTATE(RL_STATE_READCMD);
      c = rl_read_key ();
      RL_UNSETSTATE(RL_STATE_READCMD);

       
      if (c == READERR)
	{
#if defined (READLINE_CALLBACKS)
	  RL_SETSTATE(RL_STATE_DONE);
	  return (rl_done = 1);
#else
	  RL_SETSTATE(RL_STATE_EOF);
	  eof_found = 1;
	  break;
#endif
	}

       
      if (c == EOF && rl_end)
	{
	  if (RL_SIG_RECEIVED ())
	    {
	      RL_CHECK_SIGNALS ();
	      if (rl_signal_event_hook)
		(*rl_signal_event_hook) ();		 
	    }

	   
	  if (RL_ISSTATE (RL_STATE_TERMPREPPED))
	    {
	      if (lastc == _rl_eof_char || lastc == EOF)
		rl_end = 0;
	      else
	        c = _rl_eof_char;
	    }
	  else
	    c = NEWLINE;
	}

       
      if (((c == _rl_eof_char && lastc != c) || c == EOF) && rl_end == 0)
	{
#if defined (READLINE_CALLBACKS)
	  RL_SETSTATE(RL_STATE_DONE);
	  return (rl_done = 1);
#else
	  RL_SETSTATE(RL_STATE_EOF);
	  eof_found = 1;
	  break;
#endif
	}

      lastc = c;
      r = _rl_dispatch ((unsigned char)c, _rl_keymap);
      RL_CHECK_SIGNALS ();

      if (_rl_command_to_execute)
	{
	  (*rl_redisplay_function) ();

	  rl_executing_keymap = _rl_command_to_execute->map;
	  rl_executing_key = _rl_command_to_execute->key;

	  _rl_executing_func = _rl_command_to_execute->func;

	  rl_dispatching = 1;
	  RL_SETSTATE(RL_STATE_DISPATCHING);
	  r = (*(_rl_command_to_execute->func)) (_rl_command_to_execute->count, _rl_command_to_execute->key);
	  _rl_command_to_execute = 0;
	  RL_UNSETSTATE(RL_STATE_DISPATCHING);
	  rl_dispatching = 0;

	  RL_CHECK_SIGNALS ();
	}

       
      if (rl_pending_input == 0 && lk == _rl_last_command_was_kill)
	_rl_last_command_was_kill = 0;

      _rl_internal_char_cleanup ();

#if defined (READLINE_CALLBACKS)
      return 0;
#else
    }

  return (eof_found);
#endif
}

#if defined (READLINE_CALLBACKS)
static int
readline_internal_charloop (void)
{
  int eof = 1;

  while (rl_done == 0)
    eof = readline_internal_char ();
  return (eof);
}
#endif  

 
static char *
readline_internal (void)
{
  readline_internal_setup ();
  rl_eof_found = readline_internal_charloop ();
  return (readline_internal_teardown (rl_eof_found));
}

void
_rl_init_line_state (void)
{
  rl_point = rl_end = rl_mark = 0;
  the_line = rl_line_buffer;
  the_line[0] = 0;
}

void
_rl_set_the_line (void)
{
  the_line = rl_line_buffer;
}

#if defined (READLINE_CALLBACKS)
_rl_keyseq_cxt *
_rl_keyseq_cxt_alloc (void)
{
  _rl_keyseq_cxt *cxt;

  cxt = (_rl_keyseq_cxt *)xmalloc (sizeof (_rl_keyseq_cxt));

  cxt->flags = cxt->subseq_arg = cxt->subseq_retval = 0;

  cxt->okey = 0;
  cxt->ocxt = _rl_kscxt;
  cxt->childval = 42;		 

  return cxt;
}

void
_rl_keyseq_cxt_dispose (_rl_keyseq_cxt *cxt)
{
  xfree (cxt);
}

void
_rl_keyseq_chain_dispose (void)
{
  _rl_keyseq_cxt *cxt;

  while (_rl_kscxt)
    {
      cxt = _rl_kscxt;
      _rl_kscxt = _rl_kscxt->ocxt;
      _rl_keyseq_cxt_dispose (cxt);
    }
}
#endif

static int
_rl_subseq_getchar (int key)
{
  int k;

  if (key == ESC)
    RL_SETSTATE(RL_STATE_METANEXT);
  RL_SETSTATE(RL_STATE_MOREINPUT);
  k = rl_read_key ();
  RL_UNSETSTATE(RL_STATE_MOREINPUT);
  if (key == ESC)
    RL_UNSETSTATE(RL_STATE_METANEXT);

  return k;
}

#if defined (READLINE_CALLBACKS)
int
_rl_dispatch_callback (_rl_keyseq_cxt *cxt)
{
  int nkey, r;

   
   
  if ((cxt->flags & KSEQ_DISPATCHED) == 0)
    {
      nkey = _rl_subseq_getchar (cxt->okey);
      if (nkey < 0)
	{
	  _rl_abort_internal ();
	  return -1;
	}
      r = _rl_dispatch_subseq (nkey, cxt->dmap, cxt->subseq_arg);
      cxt->flags |= KSEQ_DISPATCHED;
    }
  else
    r = cxt->childval;

   
  if (r != -3)	 
    r = _rl_subseq_result (r, cxt->oldmap, cxt->okey, (cxt->flags & KSEQ_SUBSEQ));

  RL_CHECK_SIGNALS ();
   
  if (r >= 0 || (r == -1 && (cxt->flags & KSEQ_SUBSEQ) == 0))	 
    {
      _rl_keyseq_chain_dispose ();
      RL_UNSETSTATE (RL_STATE_MULTIKEY);
      return r;
    }

  if (r != -3)			 
    _rl_kscxt = cxt->ocxt;
  if (_rl_kscxt)
    _rl_kscxt->childval = r;
  if (r != -3)
    _rl_keyseq_cxt_dispose (cxt);

  return r;
}
#endif  
  
 
int
_rl_dispatch (register int key, Keymap map)
{
  _rl_dispatching_keymap = map;
  return _rl_dispatch_subseq (key, map, 0);
}

int
_rl_dispatch_subseq (register int key, Keymap map, int got_subseq)
{
  int r, newkey;
  char *macro;
  rl_command_func_t *func;
#if defined (READLINE_CALLBACKS)
  _rl_keyseq_cxt *cxt;
#endif

  if (META_CHAR (key) && _rl_convert_meta_chars_to_ascii)
    {
      if (map[ESC].type == ISKMAP)
	{
	  if (RL_ISSTATE (RL_STATE_MACRODEF))
	    _rl_add_macro_char (ESC);
	  RESIZE_KEYSEQ_BUFFER ();
	  rl_executing_keyseq[rl_key_sequence_length++] = ESC;
	  map = FUNCTION_TO_KEYMAP (map, ESC);
	  key = UNMETA (key);
	  return (_rl_dispatch (key, map));
	}
      else
	rl_ding ();
      return 0;
    }

  if (RL_ISSTATE (RL_STATE_MACRODEF))
    _rl_add_macro_char (key);

  r = 0;
  switch (map[key].type)
    {
    case ISFUNC:
      func = map[key].function;
      if (func)
	{
	   
	  if (func == rl_do_lowercase_version)
	     
	    return (_rl_dispatch (_rl_to_lower ((unsigned char)key), map));

	  rl_executing_keymap = map;
	  rl_executing_key = key;

	  _rl_executing_func = func;

	  RESIZE_KEYSEQ_BUFFER();
	  rl_executing_keyseq[rl_key_sequence_length++] = key;
	  rl_executing_keyseq[rl_key_sequence_length] = '\0';

	  rl_dispatching = 1;
	  RL_SETSTATE(RL_STATE_DISPATCHING);
	  r = (*func) (rl_numeric_arg * rl_arg_sign, key);
	  RL_UNSETSTATE(RL_STATE_DISPATCHING);
	  rl_dispatching = 0;

	   
#if defined (VI_MODE)
	  if (rl_pending_input == 0 && map[key].function != rl_digit_argument && map[key].function != rl_vi_arg_digit)
#else
	  if (rl_pending_input == 0 && map[key].function != rl_digit_argument)
#endif
	    rl_last_func = map[key].function;

	  RL_CHECK_SIGNALS ();
	}
      else if (map[ANYOTHERKEY].function)
	{
	   
	  if (RL_ISSTATE (RL_STATE_MACROINPUT))
	    _rl_prev_macro_key ();
	  else
	    _rl_unget_char  (key);
	  if (rl_key_sequence_length > 0)
	    rl_executing_keyseq[--rl_key_sequence_length] = '\0';
	  return -2;
	}
      else if (got_subseq)
	{
	   
	  if (RL_ISSTATE (RL_STATE_MACROINPUT))
	    _rl_prev_macro_key ();
	  else
	    _rl_unget_char (key);
	  if (rl_key_sequence_length > 0)
	    rl_executing_keyseq[--rl_key_sequence_length] = '\0';
	  return -1;
	}
      else
	{
#if defined (READLINE_CALLBACKS)
	  RL_UNSETSTATE (RL_STATE_MULTIKEY);
	  _rl_keyseq_chain_dispose ();
#endif
	  _rl_abort_internal ();
	  return -1;
	}
      break;

    case ISKMAP:
      if (map[key].function != 0)
	{
#if defined (VI_MODE)
	   
	   
	  if (rl_editing_mode == vi_mode && key == ESC && map == vi_insertion_keymap &&
	      (RL_ISSTATE (RL_STATE_INPUTPENDING|RL_STATE_MACROINPUT) == 0) &&
              _rl_pushed_input_available () == 0 &&
	      _rl_input_queued ((_rl_keyseq_timeout > 0) ? _rl_keyseq_timeout*1000 : 0) == 0)
	    return (_rl_dispatch (ANYOTHERKEY, FUNCTION_TO_KEYMAP (map, key)));
	   
	  if (rl_editing_mode == vi_mode && key == ESC && map == vi_insertion_keymap &&
	      (RL_ISSTATE (RL_STATE_INPUTPENDING) == 0) &&
	      (RL_ISSTATE (RL_STATE_MACROINPUT) && _rl_peek_macro_key () == 0) &&
	      _rl_pushed_input_available () == 0 &&
	      _rl_input_queued ((_rl_keyseq_timeout > 0) ? _rl_keyseq_timeout*1000 : 0) == 0)
	    return (_rl_dispatch (ANYOTHERKEY, FUNCTION_TO_KEYMAP (map, key)));	      
#endif

	  RESIZE_KEYSEQ_BUFFER ();
	  rl_executing_keyseq[rl_key_sequence_length++] = key;
	  _rl_dispatching_keymap = FUNCTION_TO_KEYMAP (map, key);

	   
#if defined (READLINE_CALLBACKS)
#  if defined (VI_MODE)
	   
	  if (_rl_vi_redoing && RL_ISSTATE (RL_STATE_CALLBACK) &&
	      map[ANYOTHERKEY].function != 0)
	    return (_rl_subseq_result (-2, map, key, got_subseq));
#  endif
	  if (RL_ISSTATE (RL_STATE_CALLBACK))
	    {
	       
	      r = RL_ISSTATE (RL_STATE_MULTIKEY) ? -3 : 0;
	      cxt = _rl_keyseq_cxt_alloc ();

	      if (got_subseq)
		cxt->flags |= KSEQ_SUBSEQ;
	      cxt->okey = key;
	      cxt->oldmap = map;
	      cxt->dmap = _rl_dispatching_keymap;
	      cxt->subseq_arg = got_subseq || cxt->dmap[ANYOTHERKEY].function;

	      RL_SETSTATE (RL_STATE_MULTIKEY);
	      _rl_kscxt = cxt;

	      return r;		 
	    }
#endif

	   
	   
	  if (_rl_keyseq_timeout > 0 &&
	  	(RL_ISSTATE (RL_STATE_INPUTPENDING|RL_STATE_MACROINPUT) == 0) &&
	  	_rl_pushed_input_available () == 0 &&
		_rl_dispatching_keymap[ANYOTHERKEY].function &&
		_rl_input_queued (_rl_keyseq_timeout*1000) == 0)
	    {
	      if (rl_key_sequence_length > 0)
		rl_executing_keyseq[--rl_key_sequence_length] = '\0';
	      return (_rl_subseq_result (-2, map, key, got_subseq));
	    }

	  newkey = _rl_subseq_getchar (key);
	  if (newkey < 0)
	    {
	      _rl_abort_internal ();
	      return -1;
	    }

	  r = _rl_dispatch_subseq (newkey, _rl_dispatching_keymap, got_subseq || map[ANYOTHERKEY].function);
	  return _rl_subseq_result (r, map, key, got_subseq);
	}
      else
	{
	  _rl_abort_internal ();	 
	  return -1;
	}
      break;

    case ISMACR:
      if (map[key].function != 0)
	{
	  rl_executing_keyseq[rl_key_sequence_length] = '\0';
	  macro = savestring ((char *)map[key].function);
	  _rl_with_macro_input (macro);
	  return 0;
	}
      break;
    }

#if defined (VI_MODE)
  if (rl_editing_mode == vi_mode && _rl_keymap == vi_movement_keymap &&
      key != ANYOTHERKEY &&
      _rl_dispatching_keymap == vi_movement_keymap &&
      _rl_vi_textmod_command (key))
    _rl_vi_set_last (key, rl_numeric_arg, rl_arg_sign);
#endif

  return (r);
}

static int
_rl_subseq_result (int r, Keymap map, int key, int got_subseq)
{
  Keymap m;
  int type, nt;
  rl_command_func_t *func, *nf;

  if (r == -2)
     
    {
      m = _rl_dispatching_keymap;
      type = m[ANYOTHERKEY].type;
      func = m[ANYOTHERKEY].function;
      if (type == ISFUNC && func == rl_do_lowercase_version)
	r = _rl_dispatch (_rl_to_lower ((unsigned char)key), map);
      else if (type == ISFUNC)
	{
	   
	  nt = m[key].type;
	  nf = m[key].function;

	  m[key].type = type;
	  m[key].function = func;
	   
	  _rl_dispatching_keymap = map;		 
	  r = _rl_dispatch_subseq (key, m, 0);
	  m[key].type = nt;
	  m[key].function = nf;
	}
      else
	 
	r = _rl_dispatch (ANYOTHERKEY, m);
    }
  else if (r < 0 && map[ANYOTHERKEY].function)
    {
       
      if (RL_ISSTATE (RL_STATE_MACROINPUT))
	_rl_prev_macro_key ();
      else
	_rl_unget_char (key);
      if (rl_key_sequence_length > 0)
	rl_executing_keyseq[--rl_key_sequence_length] = '\0';
      _rl_dispatching_keymap = map;
      return -2;
    }
  else if (r < 0 && got_subseq)		 
    {
       
      if (RL_ISSTATE (RL_STATE_MACROINPUT))
	_rl_prev_macro_key ();
      else
	_rl_unget_char (key);
      if (rl_key_sequence_length > 0)
	rl_executing_keyseq[--rl_key_sequence_length] = '\0';
      _rl_dispatching_keymap = map;
      return -1;
    }

  return r;
}

 
 
 
 
 

 
int
rl_initialize (void)
{
   
  _rl_timeout_init ();

   
  if (rl_initialized == 0)
    {
      RL_SETSTATE(RL_STATE_INITIALIZING);
      readline_initialize_everything ();
      RL_UNSETSTATE(RL_STATE_INITIALIZING);
      rl_initialized++;
      RL_SETSTATE(RL_STATE_INITIALIZED);
    }
  else
    _rl_reset_locale ();	 

   
  _rl_init_line_state ();

   
  rl_done = 0;
  RL_UNSETSTATE(RL_STATE_DONE|RL_STATE_TIMEOUT|RL_STATE_EOF);

   
  _rl_start_using_history ();

   
  rl_reset_line_state ();

   
  rl_last_func = (rl_command_func_t *)NULL;

   
  _rl_parsing_conditionalized_out = 0;

#if defined (VI_MODE)
  if (rl_editing_mode == vi_mode)
    _rl_vi_initialize_line ();
#endif

   
  _rl_set_insert_mode (RL_IM_DEFAULT, 1);

  return 0;
}

#if 0
#if defined (__EMX__)
static void
_emx_build_environ (void)
{
  TIB *tibp;
  PIB *pibp;
  char *t, **tp;
  int c;

  DosGetInfoBlocks (&tibp, &pibp);
  t = pibp->pib_pchenv;
  for (c = 1; *t; c++)
    t += strlen (t) + 1;
  tp = environ = (char **)xmalloc ((c + 1) * sizeof (char *));
  t = pibp->pib_pchenv;
  while (*t)
    {
      *tp++ = t;
      t += strlen (t) + 1;
    }
  *tp = 0;
}
#endif  
#endif

 
static void
readline_initialize_everything (void)
{
#if 0
#if defined (__EMX__)
  if (environ == 0)
    _emx_build_environ ();
#endif
#endif

#if 0
   
  running_in_emacs = sh_get_env_value ("EMACS") != (char *)0;
#endif

   
  if (!rl_instream)
    rl_instream = stdin;

  if (!rl_outstream)
    rl_outstream = stdout;

   
  _rl_in_stream = rl_instream;
  _rl_out_stream = rl_outstream;

   
  if (rl_line_buffer == 0)
    rl_line_buffer = (char *)xmalloc (rl_line_buffer_len = DEFAULT_BUFFER_SIZE);

   
  if (rl_terminal_name == 0)
    rl_terminal_name = sh_get_env_value ("TERM");
  _rl_init_terminal_io (rl_terminal_name);

   
  readline_default_bindings ();

   
  rl_initialize_funmap ();

   
  _rl_init_eightbit ();
      
   
  rl_read_init_file ((char *)NULL);

   
  if (_rl_horizontal_scroll_mode && _rl_term_autowrap)
    {
      _rl_screenwidth--;
      _rl_screenchars -= _rl_screenheight;
    }

   
  rl_set_keymap_from_edit_mode ();

   
  bind_arrow_keys ();

   
  bind_bracketed_paste_prefix ();

   
  if (rl_completer_word_break_characters == 0)
    rl_completer_word_break_characters = rl_basic_word_break_characters;

#if defined (COLOR_SUPPORT)
  if (_rl_colored_stats || _rl_colored_completion_prefix)
    _rl_parse_colors ();
#endif

  rl_executing_keyseq = malloc (_rl_executing_keyseq_size = 16);
  if (rl_executing_keyseq)
    rl_executing_keyseq[rl_key_sequence_length = 0] = '\0';
}

 
static void
readline_default_bindings (void)
{
  if (_rl_bind_stty_chars)
    rl_tty_set_default_bindings (_rl_keymap);
}

 
static void
reset_default_bindings (void)
{
  if (_rl_bind_stty_chars)
    {
      rl_tty_unset_default_bindings (_rl_keymap);
      rl_tty_set_default_bindings (_rl_keymap);
    }
}

 
static void
bind_arrow_keys_internal (Keymap map)
{
  Keymap xkeymap;

  xkeymap = _rl_keymap;
  _rl_keymap = map;

#if defined (__MSDOS__)
  rl_bind_keyseq_if_unbound ("\033[0A", rl_get_previous_history);
  rl_bind_keyseq_if_unbound ("\033[0B", rl_backward_char);
  rl_bind_keyseq_if_unbound ("\033[0C", rl_forward_char);
  rl_bind_keyseq_if_unbound ("\033[0D", rl_get_next_history);
#endif

  rl_bind_keyseq_if_unbound ("\033[A", rl_get_previous_history);
  rl_bind_keyseq_if_unbound ("\033[B", rl_get_next_history);
  rl_bind_keyseq_if_unbound ("\033[C", rl_forward_char);
  rl_bind_keyseq_if_unbound ("\033[D", rl_backward_char);
  rl_bind_keyseq_if_unbound ("\033[H", rl_beg_of_line);
  rl_bind_keyseq_if_unbound ("\033[F", rl_end_of_line);

  rl_bind_keyseq_if_unbound ("\033OA", rl_get_previous_history);
  rl_bind_keyseq_if_unbound ("\033OB", rl_get_next_history);
  rl_bind_keyseq_if_unbound ("\033OC", rl_forward_char);
  rl_bind_keyseq_if_unbound ("\033OD", rl_backward_char);
  rl_bind_keyseq_if_unbound ("\033OH", rl_beg_of_line);
  rl_bind_keyseq_if_unbound ("\033OF", rl_end_of_line);

   
  rl_bind_keyseq_if_unbound ("\033[1;5C", rl_forward_word);
  rl_bind_keyseq_if_unbound ("\033[1;5D", rl_backward_word);
  rl_bind_keyseq_if_unbound ("\033[3;5~", rl_kill_word);

   
  rl_bind_keyseq_if_unbound ("\033[1;3C", rl_forward_word);
  rl_bind_keyseq_if_unbound ("\033[1;3D", rl_backward_word);

#if defined (__MINGW32__)
  rl_bind_keyseq_if_unbound ("\340H", rl_get_previous_history);
  rl_bind_keyseq_if_unbound ("\340P", rl_get_next_history);
  rl_bind_keyseq_if_unbound ("\340M", rl_forward_char);
  rl_bind_keyseq_if_unbound ("\340K", rl_backward_char);
  rl_bind_keyseq_if_unbound ("\340G", rl_beg_of_line);
  rl_bind_keyseq_if_unbound ("\340O", rl_end_of_line);
  rl_bind_keyseq_if_unbound ("\340S", rl_delete);
  rl_bind_keyseq_if_unbound ("\340R", rl_overwrite_mode);

   
  rl_bind_keyseq_if_unbound ("\\000H", rl_get_previous_history);
  rl_bind_keyseq_if_unbound ("\\000P", rl_get_next_history);
  rl_bind_keyseq_if_unbound ("\\000M", rl_forward_char);
  rl_bind_keyseq_if_unbound ("\\000K", rl_backward_char);
  rl_bind_keyseq_if_unbound ("\\000G", rl_beg_of_line);
  rl_bind_keyseq_if_unbound ("\\000O", rl_end_of_line);
  rl_bind_keyseq_if_unbound ("\\000S", rl_delete);
  rl_bind_keyseq_if_unbound ("\\000R", rl_overwrite_mode);
#endif

  _rl_keymap = xkeymap;
}

 
static void
bind_arrow_keys (void)
{
  bind_arrow_keys_internal (emacs_standard_keymap);

#if defined (VI_MODE)
  bind_arrow_keys_internal (vi_movement_keymap);
   
  if (vi_movement_keymap[ESC].type == ISKMAP)
    rl_bind_keyseq_in_map ("\033", (rl_command_func_t *)NULL, vi_movement_keymap);
  bind_arrow_keys_internal (vi_insertion_keymap);
#endif
}

static void
bind_bracketed_paste_prefix (void)
{
  Keymap xkeymap;

  xkeymap = _rl_keymap;

  _rl_keymap = emacs_standard_keymap;
  rl_bind_keyseq_if_unbound (BRACK_PASTE_PREF, rl_bracketed_paste_begin);

#if defined (VI_MODE)
  _rl_keymap = vi_insertion_keymap;
  rl_bind_keyseq_if_unbound (BRACK_PASTE_PREF, rl_bracketed_paste_begin);
   
#endif

  _rl_keymap = xkeymap;
}
  
 
 
 
 
 

int
rl_save_state (struct readline_state *sp)
{
  if (sp == 0)
    return -1;

  sp->point = rl_point;
  sp->end = rl_end;
  sp->mark = rl_mark;
  sp->buffer = rl_line_buffer;
  sp->buflen = rl_line_buffer_len;
  sp->ul = rl_undo_list;
  sp->prompt = rl_prompt;

  sp->rlstate = rl_readline_state;
  sp->done = rl_done;
  sp->kmap = _rl_keymap;

  sp->lastfunc = rl_last_func;
  sp->insmode = rl_insert_mode;
  sp->edmode = rl_editing_mode;
  sp->kseq = rl_executing_keyseq;
  sp->kseqlen = rl_key_sequence_length;
  sp->inf = rl_instream;
  sp->outf = rl_outstream;
  sp->pendingin = rl_pending_input;
  sp->macro = rl_executing_macro;

  sp->catchsigs = rl_catch_signals;
  sp->catchsigwinch = rl_catch_sigwinch;

  sp->entryfunc = rl_completion_entry_function;
  sp->menuentryfunc = rl_menu_completion_entry_function;
  sp->ignorefunc = rl_ignore_some_completions_function;
  sp->attemptfunc = rl_attempted_completion_function;
  sp->wordbreakchars = rl_completer_word_break_characters;

  return (0);
}

int
rl_restore_state (struct readline_state *sp)
{
  if (sp == 0)
    return -1;

  rl_point = sp->point;
  rl_end = sp->end;
  rl_mark = sp->mark;
  the_line = rl_line_buffer = sp->buffer;
  rl_line_buffer_len = sp->buflen;
  rl_undo_list = sp->ul;
  rl_prompt = sp->prompt;

  rl_readline_state = sp->rlstate;
  rl_done = sp->done;
  _rl_keymap = sp->kmap;

  rl_last_func = sp->lastfunc;
  rl_insert_mode = sp->insmode;
  rl_editing_mode = sp->edmode;
  rl_executing_keyseq = sp->kseq;
  rl_key_sequence_length = sp->kseqlen;
  rl_instream = sp->inf;
  rl_outstream = sp->outf;
  rl_pending_input = sp->pendingin;
  rl_executing_macro = sp->macro;

  rl_catch_signals = sp->catchsigs;
  rl_catch_sigwinch = sp->catchsigwinch;

  rl_completion_entry_function = sp->entryfunc;
  rl_menu_completion_entry_function = sp->menuentryfunc;
  rl_ignore_some_completions_function = sp->ignorefunc;
  rl_attempted_completion_function = sp->attemptfunc;
  rl_completer_word_break_characters = sp->wordbreakchars;

  rl_deactivate_mark ();

  return (0);
}

 

void
_rl_init_executing_keyseq (void)
{
  rl_executing_keyseq[rl_key_sequence_length = 0] = '\0';
}

void
_rl_term_executing_keyseq (void)
{
  rl_executing_keyseq[rl_key_sequence_length] = '\0';
}

void
_rl_end_executing_keyseq (void)
{
  if (rl_key_sequence_length > 0)
    rl_executing_keyseq[--rl_key_sequence_length] = '\0';
}

void
_rl_add_executing_keyseq (int key)
{
  RESIZE_KEYSEQ_BUFFER ();
 rl_executing_keyseq[rl_key_sequence_length++] = key;
}

 
void
_rl_del_executing_keyseq (void)
{
  if (rl_key_sequence_length > 0)
    rl_key_sequence_length--;
}
