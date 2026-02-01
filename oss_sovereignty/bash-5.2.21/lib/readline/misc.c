 

 

#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
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

 
#include "rldefs.h"
#include "rlmbutil.h"

 
#include "readline.h"
#include "history.h"

#include "rlprivate.h"
#include "histlib.h"
#include "rlshell.h"
#include "xmalloc.h"

static int rl_digit_loop (void);
static void _rl_history_set_point (void);

 
int _rl_history_preserve_point = 0;

_rl_arg_cxt _rl_argcxt;

 
int _rl_history_saved_point = -1;

 
 
 
 
 

int
_rl_arg_overflow (void)
{
  if (rl_numeric_arg > 1000000)
    {
      _rl_argcxt = 0;
      rl_explicit_arg = rl_numeric_arg = 0;
      rl_ding ();
      rl_restore_prompt ();
      rl_clear_message ();
      RL_UNSETSTATE(RL_STATE_NUMERICARG);
      return 1;
    }
  return 0;
}

void
_rl_arg_init (void)
{
  rl_save_prompt ();
  _rl_argcxt = 0;
  RL_SETSTATE(RL_STATE_NUMERICARG);
}

int
_rl_arg_getchar (void)
{
  int c;

  rl_message ("(arg: %d) ", rl_arg_sign * rl_numeric_arg);
  RL_SETSTATE(RL_STATE_MOREINPUT);
  c = rl_read_key ();
  RL_UNSETSTATE(RL_STATE_MOREINPUT);

  return c;
}

 
int
_rl_arg_dispatch (_rl_arg_cxt cxt, int c)
{
  int key, r;

  key = c;

   
  if (c >= 0 && _rl_keymap[c].type == ISFUNC && _rl_keymap[c].function == rl_universal_argument)
    {
      if ((cxt & NUM_SAWDIGITS) == 0)
	{
	  rl_numeric_arg *= 4;
	  return 1;
	}
      else if (RL_ISSTATE (RL_STATE_CALLBACK))
        {
          _rl_argcxt |= NUM_READONE;
          return 0;	 
        }
      else
	{
	  key = _rl_bracketed_read_key ();
	  rl_restore_prompt ();
	  rl_clear_message ();
	  RL_UNSETSTATE(RL_STATE_NUMERICARG);
	  if (key < 0)
	    return -1;
	  return (_rl_dispatch (key, _rl_keymap));
	}
    }

  c = UNMETA (c);

  if (_rl_digit_p (c))
    {
      r = _rl_digit_value (c);    	
      rl_numeric_arg = rl_explicit_arg ? (rl_numeric_arg * 10) +  r : r;
      rl_explicit_arg = 1;
      _rl_argcxt |= NUM_SAWDIGITS;
    }
  else if (c == '-' && rl_explicit_arg == 0)
    {
      rl_numeric_arg = 1;
      _rl_argcxt |= NUM_SAWMINUS;
      rl_arg_sign = -1;
    }
  else
    {
       
      if ((_rl_argcxt & NUM_SAWMINUS) && rl_numeric_arg == 1 && rl_explicit_arg == 0)
	rl_explicit_arg = 1;
      rl_restore_prompt ();
      rl_clear_message ();
      RL_UNSETSTATE(RL_STATE_NUMERICARG);

      r = _rl_dispatch (key, _rl_keymap);
      if (RL_ISSTATE (RL_STATE_CALLBACK))
	{
	   
	  if (rl_done == 0)
	    (*rl_redisplay_function) ();
	  r = 0;
	}
      return r;
    }

  return 1;
}

 
static int
rl_digit_loop (void)
{
  int c, r;

  while (1)
    {
      if (_rl_arg_overflow ())
	return 1;

      c = _rl_arg_getchar ();

      if (c < 0)
	{
	  _rl_abort_internal ();
	  return -1;
	}

      r = _rl_arg_dispatch (_rl_argcxt, c);
      if (r <= 0 || (RL_ISSTATE (RL_STATE_NUMERICARG) == 0))
        break;
    }

  return r;
}

 
void
_rl_reset_argument (void)
{
  rl_numeric_arg = rl_arg_sign = 1;
  rl_explicit_arg = 0;
  _rl_argcxt = 0;
}

 
int
rl_digit_argument (int ignore, int key)
{
  _rl_arg_init ();
  if (RL_ISSTATE (RL_STATE_CALLBACK))
    {
      _rl_arg_dispatch (_rl_argcxt, key);
      rl_message ("(arg: %d) ", rl_arg_sign * rl_numeric_arg);
      return 0;
    }
  else
    {
      rl_execute_next (key);
      return (rl_digit_loop ());
    }
}

 
int
rl_universal_argument (int count, int key)
{
  _rl_arg_init ();
  rl_numeric_arg *= 4;

  return (RL_ISSTATE (RL_STATE_CALLBACK) ? 0 : rl_digit_loop ());
}

int
_rl_arg_callback (_rl_arg_cxt cxt)
{
  int c, r;

  c = _rl_arg_getchar ();
  if (c < 0)
    return (1);		 

  if (_rl_argcxt & NUM_READONE)
    {
      _rl_argcxt &= ~NUM_READONE;
      rl_restore_prompt ();
      rl_clear_message ();
      RL_UNSETSTATE(RL_STATE_NUMERICARG);
      rl_execute_next (c);
      return 0;
    }

  r = _rl_arg_dispatch (cxt, c);
  if (r > 0)
    rl_message ("(arg: %d) ", rl_arg_sign * rl_numeric_arg);
  return (r != 1);
}

 
int
rl_discard_argument (void)
{
  rl_ding ();
  rl_clear_message ();
  _rl_reset_argument ();

  return 0;
}

 
 
 
 
 

 

 
HIST_ENTRY *_rl_saved_line_for_history = (HIST_ENTRY *)NULL;

 
void
_rl_start_using_history (void)
{
  using_history ();
  if (_rl_saved_line_for_history)
    _rl_free_saved_history_line ();
  _rl_saved_line_for_history = (HIST_ENTRY *)NULL;
  _rl_history_search_pos = -99;		 
}

 
void
_rl_free_history_entry (HIST_ENTRY *entry)
{
  if (entry == 0)
    return;

  FREE (entry->line);
  FREE (entry->timestamp);

  xfree (entry);
}

 
int
rl_maybe_replace_line (void)
{
  HIST_ENTRY *temp;

  temp = current_history ();
   
  if (temp && ((UNDO_LIST *)(temp->data) != rl_undo_list))
    {
      temp = replace_history_entry (where_history (), rl_line_buffer, (histdata_t)rl_undo_list);
      xfree (temp->line);
      FREE (temp->timestamp);
      xfree (temp);
    }
  return 0;
}

 
int
rl_maybe_unsave_line (void)
{
  if (_rl_saved_line_for_history)
    {
       
      rl_replace_line (_rl_saved_line_for_history->line, 0);
      rl_undo_list = (UNDO_LIST *)_rl_saved_line_for_history->data;

       
      _rl_free_history_entry (_rl_saved_line_for_history);
      _rl_saved_line_for_history = (HIST_ENTRY *)NULL;
      rl_point = rl_end;	 
    }
  else
    rl_ding ();
  return 0;
}

 
int
rl_maybe_save_line (void)
{
  if (_rl_saved_line_for_history == 0)
    {
      _rl_saved_line_for_history = (HIST_ENTRY *)xmalloc (sizeof (HIST_ENTRY));
      _rl_saved_line_for_history->line = savestring (rl_line_buffer);
      _rl_saved_line_for_history->timestamp = (char *)NULL;
      _rl_saved_line_for_history->data = (char *)rl_undo_list;
    }

  return 0;
}

int
_rl_free_saved_history_line (void)
{
  UNDO_LIST *orig;

  if (_rl_saved_line_for_history)
    {
      if (rl_undo_list && rl_undo_list == (UNDO_LIST *)_rl_saved_line_for_history->data)
	rl_undo_list = 0;
       
      if (_rl_saved_line_for_history->data)
	_rl_free_undo_list ((UNDO_LIST *)_rl_saved_line_for_history->data);
      _rl_free_history_entry (_rl_saved_line_for_history);
      _rl_saved_line_for_history = (HIST_ENTRY *)NULL;
    }
  return 0;
}

static void
_rl_history_set_point (void)
{
  rl_point = (_rl_history_preserve_point && _rl_history_saved_point != -1)
		? _rl_history_saved_point
		: rl_end;
  if (rl_point > rl_end)
    rl_point = rl_end;

#if defined (VI_MODE)
  if (rl_editing_mode == vi_mode && _rl_keymap != vi_insertion_keymap)
    rl_point = 0;
#endif  

  if (rl_editing_mode == emacs_mode)
    rl_mark = (rl_point == rl_end ? 0 : rl_end);
}

void
rl_replace_from_history (HIST_ENTRY *entry, int flags)
{
   
  rl_replace_line (entry->line, 0);
  rl_undo_list = (UNDO_LIST *)entry->data;
  rl_point = rl_end;
  rl_mark = 0;

#if defined (VI_MODE)
  if (rl_editing_mode == vi_mode)
    {
      rl_point = 0;
      rl_mark = rl_end;
    }
#endif
}

 
void
_rl_revert_previous_lines (void)
{
  int hpos;
  HIST_ENTRY *entry;
  UNDO_LIST *ul, *saved_undo_list;
  char *lbuf;

  lbuf = savestring (rl_line_buffer);
  saved_undo_list = rl_undo_list;
  hpos = where_history ();

  entry = (hpos == history_length) ? previous_history () : current_history ();
  while (entry)
    {
      if (ul = (UNDO_LIST *)entry->data)
	{
	  if (ul == saved_undo_list)
	    saved_undo_list = 0;
	   
	  rl_replace_from_history (entry, 0);	 
	  entry->data = 0;			 
	   
	  while (rl_undo_list)
	    rl_do_undo ();
	   
	  FREE (entry->line);
	  entry->line = savestring (rl_line_buffer);
	}
      entry = previous_history ();
    }

   
  rl_undo_list = saved_undo_list;	 
  history_set_pos (hpos);
  
   
  rl_replace_line (lbuf, 0);
  _rl_set_the_line ();

   
  xfree (lbuf);
}  

 
void
_rl_revert_all_lines (void)
{
  int pos;

  pos = where_history ();
  using_history ();
  _rl_revert_previous_lines ();
  history_set_pos (pos);
}

 
void
rl_clear_history (void)
{
  HIST_ENTRY **hlist, *hent;
  register int i;
  UNDO_LIST *ul, *saved_undo_list;

  saved_undo_list = rl_undo_list;
  hlist = history_list ();		 

  for (i = 0; i < history_length; i++)
    {
      hent = hlist[i];
      if (ul = (UNDO_LIST *)hent->data)
	{
	  if (ul == saved_undo_list)
	    saved_undo_list = 0;
	  _rl_free_undo_list (ul);
	  hent->data = 0;
	}
      _rl_free_history_entry (hent);
    }

  history_offset = history_length = 0;
  rl_undo_list = saved_undo_list;	 
}

 
 
 
 
 

 
int
rl_beginning_of_history (int count, int key)
{
  return (rl_get_previous_history (1 + where_history (), key));
}

 
int
rl_end_of_history (int count, int key)
{
  rl_maybe_replace_line ();
  using_history ();
  rl_maybe_unsave_line ();
  return 0;
}

 
int
rl_get_next_history (int count, int key)
{
  HIST_ENTRY *temp;

  if (count < 0)
    return (rl_get_previous_history (-count, key));

  if (count == 0)
    return 0;

  rl_maybe_replace_line ();

   
  if (_rl_history_saved_point == -1 && (rl_point || rl_end))
    _rl_history_saved_point = (rl_point == rl_end) ? -1 : rl_point;

  temp = (HIST_ENTRY *)NULL;
  while (count)
    {
      temp = next_history ();
      if (!temp)
	break;
      --count;
    }

  if (temp == 0)
    rl_maybe_unsave_line ();
  else
    {
      rl_replace_from_history (temp, 0);
      _rl_history_set_point ();
    }
  return 0;
}

 
int
rl_get_previous_history (int count, int key)
{
  HIST_ENTRY *old_temp, *temp;
  int had_saved_line;

  if (count < 0)
    return (rl_get_next_history (-count, key));

  if (count == 0 || history_list () == 0)
    return 0;

   
  if (_rl_history_saved_point == -1 && (rl_point || rl_end))
    _rl_history_saved_point = (rl_point == rl_end) ? -1 : rl_point;

   
  had_saved_line = _rl_saved_line_for_history != 0;
  rl_maybe_save_line ();

   
  rl_maybe_replace_line ();

  temp = old_temp = (HIST_ENTRY *)NULL;
  while (count)
    {
      temp = previous_history ();
      if (temp == 0)
	break;

      old_temp = temp;
      --count;
    }

   
  if (!temp && old_temp)
    temp = old_temp;

  if (temp == 0)
    {
      if (had_saved_line == 0)
	_rl_free_saved_history_line ();
      rl_ding ();
    }
  else
    {
      rl_replace_from_history (temp, 0);
      _rl_history_set_point ();
    }

  return 0;
}

 
int
rl_fetch_history (int count, int c)
{
  int wanted, nhist;

   
  if (rl_explicit_arg)
    {
      nhist = history_base + where_history ();
       
      wanted = (count >= 0) ? nhist - count : -count;

      if (wanted <= 0 || wanted >= nhist)
	{
	   
	  if (rl_editing_mode == vi_mode)
	    rl_ding ();
	  else
	    rl_beginning_of_history (0, 0);
	}
      else
        rl_get_previous_history (wanted, c);
    }
  else
    rl_beginning_of_history (count, 0);

  return (0);
}

 

 
static rl_hook_func_t *_rl_saved_internal_startup_hook = 0;
static int saved_history_logical_offset = -1;

#define HISTORY_FULL() (history_is_stifled () && history_length >= history_max_entries)

static int
set_saved_history ()
{
  int absolute_offset, count;

  if (saved_history_logical_offset >= 0)
    {
      absolute_offset = saved_history_logical_offset - history_base;
      count = where_history () - absolute_offset;
      rl_get_previous_history (count, 0);
    }
  saved_history_logical_offset = -1;
  _rl_internal_startup_hook = _rl_saved_internal_startup_hook;

  return (0);
}

int
rl_operate_and_get_next (int count, int c)
{
   
  rl_newline (1, c);

  saved_history_logical_offset = rl_explicit_arg ? count : where_history () + history_base + 1;

  _rl_saved_internal_startup_hook = _rl_internal_startup_hook;
  _rl_internal_startup_hook = set_saved_history;

  return 0;
}

 
 
 
 
 
 
int
rl_vi_editing_mode (int count, int key)
{
#if defined (VI_MODE)
  _rl_set_insert_mode (RL_IM_INSERT, 1);	 
  rl_editing_mode = vi_mode;
  rl_vi_insert_mode (1, key);
#endif  

  return 0;
}

int
rl_emacs_editing_mode (int count, int key)
{
  rl_editing_mode = emacs_mode;
  _rl_set_insert_mode (RL_IM_INSERT, 1);  
  _rl_keymap = emacs_standard_keymap;

  if (_rl_show_mode_in_prompt)
    _rl_reset_prompt ();

  return 0;
}

 
void
_rl_set_insert_mode (int im, int force)
{
#ifdef CURSOR_MODE
  _rl_set_cursor (im, force);
#endif

  rl_insert_mode = im;
}

 
int
rl_overwrite_mode (int count, int key)
{
  if (rl_explicit_arg == 0)
    _rl_set_insert_mode (rl_insert_mode ^ 1, 0);
  else if (count > 0)
    _rl_set_insert_mode (RL_IM_OVERWRITE, 0);
  else
    _rl_set_insert_mode (RL_IM_INSERT, 0);

  return 0;
}
