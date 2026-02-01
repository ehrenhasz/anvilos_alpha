 

 

#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <sys/types.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>            
#endif  

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif  

#include <stdio.h>

 
#include "rldefs.h"

 
#include "readline.h"
#include "history.h"

#include "rlprivate.h"
#include "xmalloc.h"

 
 
 
 
 

 
#define DEFAULT_MAX_KILLS 10

 
static int rl_max_kills =  DEFAULT_MAX_KILLS;

 
static char **rl_kill_ring = (char **)NULL;

 
static int rl_kill_index;

 
static int rl_kill_ring_length;

static int _rl_copy_to_kill_ring (char *, int);
static int region_kill_internal (int);
static int _rl_copy_word_as_kill (int, int);
static int rl_yank_nth_arg_internal (int, int, int);

 
int
rl_set_retained_kills (int num)
{
  return 0;
}

 
static int
_rl_copy_to_kill_ring (char *text, int append)
{
  char *old, *new;
  int slot;

   
  if (_rl_last_command_was_kill == 0 || rl_kill_ring == 0)
    {
       
      if (rl_kill_ring == 0)
	{
	   
	  rl_kill_ring = (char **)
	    xmalloc (((rl_kill_ring_length = 1) + 1) * sizeof (char *));
	  rl_kill_ring[slot = 0] = (char *)NULL;
	}
      else
	{
	   
	  slot = rl_kill_ring_length;
	  if (slot == rl_max_kills)
	    {
	      register int i;
	      xfree (rl_kill_ring[0]);
	      for (i = 0; i < slot; i++)
		rl_kill_ring[i] = rl_kill_ring[i + 1];
	    }
	  else
	    {
	      slot = rl_kill_ring_length += 1;
	      rl_kill_ring = (char **)xrealloc (rl_kill_ring, (slot + 1) * sizeof (char *));
	    }
	  rl_kill_ring[--slot] = (char *)NULL;
	}
    }
  else
    slot = rl_kill_ring_length - 1;

   
  if (_rl_last_command_was_kill && rl_kill_ring[slot] && rl_editing_mode != vi_mode)
    {
      old = rl_kill_ring[slot];
      new = (char *)xmalloc (1 + strlen (old) + strlen (text));

      if (append)
	{
	  strcpy (new, old);
	  strcat (new, text);
	}
      else
	{
	  strcpy (new, text);
	  strcat (new, old);
	}
      xfree (old);
      xfree (text);
      rl_kill_ring[slot] = new;
    }
  else
    rl_kill_ring[slot] = text;

  rl_kill_index = slot;
  return 0;
}

 
int
rl_kill_text (int from, int to)
{
  char *text;

   
  if (from == to)
    {
      _rl_last_command_was_kill++;
      return 0;
    }

  text = rl_copy_text (from, to);

   
  rl_delete_text (from, to);

  _rl_copy_to_kill_ring (text, from < to);

  _rl_last_command_was_kill++;
  return 0;
}

 

 
 
 
 
 

 
int
rl_kill_word (int count, int key)
{
  int orig_point;

  if (count < 0)
    return (rl_backward_kill_word (-count, key));
  else
    {
      orig_point = rl_point;
      rl_forward_word (count, key);

      if (rl_point != orig_point)
	rl_kill_text (orig_point, rl_point);

      rl_point = orig_point;
      if (rl_editing_mode == emacs_mode)
	rl_mark = rl_point;
    }
  return 0;
}

 
int
rl_backward_kill_word (int count, int key)
{
  int orig_point;

  if (count < 0)
    return (rl_kill_word (-count, key));
  else
    {
      orig_point = rl_point;
      rl_backward_word (count, key);

      if (rl_point != orig_point)
	rl_kill_text (orig_point, rl_point);

      if (rl_editing_mode == emacs_mode)
	rl_mark = rl_point;
    }
  return 0;
}

 
int
rl_kill_line (int direction, int key)
{
  int orig_point;

  if (direction < 0)
    return (rl_backward_kill_line (1, key));
  else
    {
      orig_point = rl_point;
      rl_end_of_line (1, key);
      if (orig_point != rl_point)
	rl_kill_text (orig_point, rl_point);
      rl_point = orig_point;
      if (rl_editing_mode == emacs_mode)
	rl_mark = rl_point;
    }
  return 0;
}

 
int
rl_backward_kill_line (int direction, int key)
{
  int orig_point;

  if (direction < 0)
    return (rl_kill_line (1, key));
  else
    {
      if (rl_point == 0)
	rl_ding ();
      else
	{
	  orig_point = rl_point;
	  rl_beg_of_line (1, key);
	  if (rl_point != orig_point)
	    rl_kill_text (orig_point, rl_point);
	  if (rl_editing_mode == emacs_mode)
	    rl_mark = rl_point;
	}
    }
  return 0;
}

 
int
rl_kill_full_line (int count, int key)
{
  rl_begin_undo_group ();
  rl_point = 0;
  rl_kill_text (rl_point, rl_end);
  rl_mark = 0;
  rl_end_undo_group ();
  return 0;
}

 

 
int
rl_unix_word_rubout (int count, int key)
{
  int orig_point;

  if (rl_point == 0)
    rl_ding ();
  else
    {
      orig_point = rl_point;
      if (count <= 0)
	count = 1;

      while (count--)
	{
	  while (rl_point && whitespace (rl_line_buffer[rl_point - 1]))
	    rl_point--;

	  while (rl_point && (whitespace (rl_line_buffer[rl_point - 1]) == 0))
	    rl_point--;		 
	}

      rl_kill_text (orig_point, rl_point);
      if (rl_editing_mode == emacs_mode)
	rl_mark = rl_point;
    }

  return 0;
}

 
int
rl_unix_filename_rubout (int count, int key)
{
  int orig_point, c;

  if (rl_point == 0)
    rl_ding ();
  else
    {
      orig_point = rl_point;
      if (count <= 0)
	count = 1;

      while (count--)
	{
	  c = rl_line_buffer[rl_point - 1];

	   
	  while (rl_point && whitespace (c))
	    {
	      rl_point--;
	      c = rl_line_buffer[rl_point - 1];
	    }

	   
	  if (c == '/')
	    {
	      int i;

	      i = rl_point - 1;
	      while (i > 0 && c == '/')
		c = rl_line_buffer[--i];
	      if (i == 0 || whitespace (c))
		{
		  rl_point = i + whitespace (c);
		  continue;	 
		}
	      c = '/';
	    }

	  while (rl_point && (whitespace (c) || c == '/'))
	    {
	      rl_point--;
	      c = rl_line_buffer[rl_point - 1];
	    }

	  while (rl_point && (whitespace (c) == 0) && c != '/')
	    {
	      rl_point--;	 
	      c = rl_line_buffer[rl_point - 1];
	    }
	}

      rl_kill_text (orig_point, rl_point);
      if (rl_editing_mode == emacs_mode)
	rl_mark = rl_point;
    }

  return 0;
}

 
int
rl_unix_line_discard (int count, int key)
{
  if (rl_point == 0)
    rl_ding ();
  else
    {
      rl_kill_text (rl_point, 0);
      rl_point = 0;
      if (rl_editing_mode == emacs_mode)
	rl_mark = rl_point;
    }
  return 0;
}

 
static int
region_kill_internal (int delete)
{
  char *text;

  if (rl_mark != rl_point)
    {
      text = rl_copy_text (rl_point, rl_mark);
      if (delete)
	rl_delete_text (rl_point, rl_mark);
      _rl_copy_to_kill_ring (text, rl_point < rl_mark);
    }

  _rl_fix_point (1);
  _rl_last_command_was_kill++;
  return 0;
}

 
int
rl_copy_region_to_kill (int count, int key)
{
  return (region_kill_internal (0));
}

 
int
rl_kill_region (int count, int key)
{
  int r, npoint;

  npoint = (rl_point < rl_mark) ? rl_point : rl_mark;
  r = region_kill_internal (1);
  rl_point = npoint;
  _rl_fix_point (1);
  return r;
}

 
static int
_rl_copy_word_as_kill (int count, int dir)
{
  int om, op, r;

  om = rl_mark;
  op = rl_point;

  if (dir > 0)
    rl_forward_word (count, 0);
  else
    rl_backward_word (count, 0);

  rl_mark = rl_point;

  if (dir > 0)
    rl_backward_word (count, 0);
  else
    rl_forward_word (count, 0);

  r = region_kill_internal (0);

  rl_mark = om;
  rl_point = op;

  return r;
}

int
rl_copy_forward_word (int count, int key)
{
  if (count < 0)
    return (rl_copy_backward_word (-count, key));

  return (_rl_copy_word_as_kill (count, 1));
}

int
rl_copy_backward_word (int count, int key)
{
  if (count < 0)
    return (rl_copy_forward_word (-count, key));

  return (_rl_copy_word_as_kill (count, -1));
}
  
 
int
rl_yank (int count, int key)
{
  if (rl_kill_ring == 0)
    {
      _rl_abort_internal ();
      return 1;
    }

  _rl_set_mark_at_pos (rl_point);
  rl_insert_text (rl_kill_ring[rl_kill_index]);
  return 0;
}

 
int
rl_yank_pop (int count, int key)
{
  int l, n;

  if (((rl_last_func != rl_yank_pop) && (rl_last_func != rl_yank)) ||
      !rl_kill_ring)
    {
      _rl_abort_internal ();
      return 1;
    }

  l = strlen (rl_kill_ring[rl_kill_index]);
  n = rl_point - l;
  if (n >= 0 && STREQN (rl_line_buffer + n, rl_kill_ring[rl_kill_index], l))
    {
      rl_delete_text (n, rl_point);
      rl_point = n;
      rl_kill_index--;
      if (rl_kill_index < 0)
	rl_kill_index = rl_kill_ring_length - 1;
      rl_yank (1, 0);
      return 0;
    }
  else
    {
      _rl_abort_internal ();
      return 1;
    }
}

#if defined (VI_MODE)
int
rl_vi_yank_pop (int count, int key)
{
  int l, n, origpoint;

  if (((rl_last_func != rl_vi_yank_pop) && (rl_last_func != rl_vi_put)) ||
      !rl_kill_ring)
    {
      _rl_abort_internal ();
      return 1;
    }

  l = strlen (rl_kill_ring[rl_kill_index]);
#if 0  
  origpoint = rl_point;
  n = rl_point - l + 1;
#else
  n = rl_point - l;
#endif
  if (n >= 0 && STREQN (rl_line_buffer + n, rl_kill_ring[rl_kill_index], l))
    {
#if 0  
      rl_delete_text (n, n + l);		 
      rl_point = origpoint - l;
#else
      rl_delete_text (n, rl_point);
      rl_point = n;
#endif
      rl_kill_index--;
      if (rl_kill_index < 0)
	rl_kill_index = rl_kill_ring_length - 1;
      rl_vi_put (1, 'p');
      return 0;
    }
  else
    {
      _rl_abort_internal ();
      return 1;
    }
}
#endif  

 
static int
rl_yank_nth_arg_internal (int count, int key, int history_skip)
{
  register HIST_ENTRY *entry;
  char *arg;
  int i, pos;

  pos = where_history ();

  if (history_skip)
    {
      for (i = 0; i < history_skip; i++)
	entry = previous_history ();
    }

  entry = previous_history ();

  history_set_pos (pos);

  if (entry == 0)
    {
      rl_ding ();
      return 1;
    }

  arg = history_arg_extract (count, count, entry->line);
  if (!arg || !*arg)
    {
      rl_ding ();
      FREE (arg);
      return 1;
    }

  rl_begin_undo_group ();

  _rl_set_mark_at_pos (rl_point);

#if defined (VI_MODE)
   
  if (rl_editing_mode == vi_mode && _rl_keymap == vi_movement_keymap)
    {
      rl_vi_append_mode (1, key);
      rl_insert_text (" ");
    }
#endif  

  rl_insert_text (arg);
  xfree (arg);

  rl_end_undo_group ();
  return 0;
}

 
int
rl_yank_nth_arg (int count, int key)
{
  return (rl_yank_nth_arg_internal (count, key, 0));
}

 
int
rl_yank_last_arg (int count, int key)
{
  static int history_skip = 0;
  static int explicit_arg_p = 0;
  static int count_passed = 1;
  static int direction = 1;
  static int undo_needed = 0;
  int retval;

  if (rl_last_func != rl_yank_last_arg)
    {
      history_skip = 0;
      explicit_arg_p = rl_explicit_arg;
      count_passed = count;
      direction = 1;
    }
  else
    {
      if (undo_needed)
	rl_do_undo ();
      if (count < 0)		 
        direction = -direction;
      history_skip += direction;
      if (history_skip < 0)
	history_skip = 0;
    }
 
  if (explicit_arg_p)
    retval = rl_yank_nth_arg_internal (count_passed, key, history_skip);
  else
    retval = rl_yank_nth_arg_internal ('$', key, history_skip);

  undo_needed = retval == 0;
  return retval;
}

 
char *
_rl_bracketed_text (size_t *lenp)
{
  int c;
  size_t len, cap;
  char *buf;

  len = 0;
  buf = xmalloc (cap = 64);
  buf[0] = '\0';

  RL_SETSTATE (RL_STATE_MOREINPUT);
  while ((c = rl_read_key ()) >= 0)
    {
      if (RL_ISSTATE (RL_STATE_MACRODEF))
	_rl_add_macro_char (c);

      if (c == '\r')		 
	c = '\n';

      if (len == cap)
	buf = xrealloc (buf, cap *= 2);

      buf[len++] = c;
      if (len >= BRACK_PASTE_SLEN && c == BRACK_PASTE_LAST &&
	  STREQN (buf + len - BRACK_PASTE_SLEN, BRACK_PASTE_SUFF, BRACK_PASTE_SLEN))
	{
	  len -= BRACK_PASTE_SLEN;
	  break;
	}
    }
  RL_UNSETSTATE (RL_STATE_MOREINPUT);

  if (c >= 0)
    {
      if (len == cap)
	buf = xrealloc (buf, cap + 1);
      buf[len] = '\0';
    }

  if (lenp)
    *lenp = len;
  return (buf);
}

 
int
rl_bracketed_paste_begin (int count, int key)
{
  int retval, c;
  size_t len, cap;
  char *buf;

  buf = _rl_bracketed_text (&len);
  rl_mark = rl_point;
  retval = rl_insert_text (buf) == len ? 0 : 1;
  if (_rl_enable_active_region)
    rl_activate_mark ();

  xfree (buf);
  return (retval);
}

int
_rl_read_bracketed_paste_prefix (int c)
{
  char pbuf[BRACK_PASTE_SLEN+1], *pbpref;
  int key, ind, j;

  pbpref = BRACK_PASTE_PREF;		 
  if (c != pbpref[0])
    return (0);
  pbuf[ind = 0] = c;
  while (ind < BRACK_PASTE_SLEN-1 &&
	 (RL_ISSTATE (RL_STATE_INPUTPENDING|RL_STATE_MACROINPUT) == 0) &&
         _rl_pushed_input_available () == 0 &&
         _rl_input_queued (0))
    {
      key = rl_read_key ();		 
      if (key < 0)
	break;
      pbuf[++ind] = key;
      if (pbuf[ind] != pbpref[ind])
        break;
    }

  if (ind < BRACK_PASTE_SLEN-1)		 
    {
      while (ind >= 0)
	_rl_unget_char (pbuf[ind--]);
      return (key < 0 ? key : 0);
    }
  return (key < 0 ? key : 1);
}

 
int
_rl_bracketed_read_key ()
{
  int c, r;
  char *pbuf;
  size_t pblen;

  RL_SETSTATE(RL_STATE_MOREINPUT);
  c = rl_read_key ();
  RL_UNSETSTATE(RL_STATE_MOREINPUT);

  if (c < 0)
    return -1;

   
  if (_rl_enable_bracketed_paste && c == ESC && (r = _rl_read_bracketed_paste_prefix (c)) == 1)
    {
      pbuf = _rl_bracketed_text (&pblen);
      if (pblen == 0)
	{
	  xfree (pbuf);
	  return 0;		 
	}
      c = (unsigned char)pbuf[0];
      if (pblen > 1)
	{
	  while (--pblen > 0)
	    _rl_unget_char ((unsigned char)pbuf[pblen]);
	}
      xfree (pbuf);
    }

  return c;
}

 
int
_rl_bracketed_read_mbstring (char *mb, int mlen)
{
  int c, r;

  c = _rl_bracketed_read_key ();
  if (c < 0)
    return -1;

#if defined (HANDLE_MULTIBYTE)
  if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
    c = _rl_read_mbstring (c, mb, mlen);
  else
#endif
    mb[0] = c;
  mb[mlen] = '\0';		 

  return c;
}

 
#if defined (_WIN32)
#include <windows.h>

int
rl_paste_from_clipboard (int count, int key)
{
  char *data, *ptr;
  int len;

  if (OpenClipboard (NULL) == 0)
    return (0);

  data = (char *)GetClipboardData (CF_TEXT);
  if (data)
    {
      ptr = strchr (data, '\r');
      if (ptr)
	{
	  len = ptr - data;
	  ptr = (char *)xmalloc (len + 1);
	  ptr[len] = '\0';
	  strncpy (ptr, data, len);
	}
      else
        ptr = data;
      _rl_set_mark_at_pos (rl_point);
      rl_insert_text (ptr);
      if (ptr != data)
	xfree (ptr);
      CloseClipboard ();
    }
  return (0);
}
#endif  
