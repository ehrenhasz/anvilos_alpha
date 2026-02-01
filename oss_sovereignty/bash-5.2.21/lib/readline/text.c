 

 

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

#if defined (__EMX__)
#  define INCL_DOSPROCESS
#  include <os2.h>
#endif  

 
#include "readline.h"
#include "history.h"

#include "rlprivate.h"
#include "rlshell.h"
#include "xmalloc.h"

 
static int rl_change_case (int, int);
static int _rl_char_search (int, int, int);

#if defined (READLINE_CALLBACKS)
static int _rl_insert_next_callback (_rl_callback_generic_arg *);
static int _rl_char_search_callback (_rl_callback_generic_arg *);
#endif

 
#define TEXT_COUNT_MAX	1024

int _rl_optimize_typeahead = 1;	 

 
 
 
 
 

 
int
rl_insert_text (const char *string)
{
  register int i, l;

  l = (string && *string) ? strlen (string) : 0;
  if (l == 0)
    return 0;

  if (rl_end + l >= rl_line_buffer_len)
    rl_extend_line_buffer (rl_end + l);

  for (i = rl_end; i >= rl_point; i--)
    rl_line_buffer[i + l] = rl_line_buffer[i];

  strncpy (rl_line_buffer + rl_point, string, l);

   
  if (_rl_doing_an_undo == 0)
    {
       
      if ((l == 1) &&
	  rl_undo_list &&
	  (rl_undo_list->what == UNDO_INSERT) &&
	  (rl_undo_list->end == rl_point) &&
	  (rl_undo_list->end - rl_undo_list->start < 20))
	rl_undo_list->end++;
      else
	rl_add_undo (UNDO_INSERT, rl_point, rl_point + l, (char *)NULL);
    }
  rl_point += l;
  rl_end += l;
  rl_line_buffer[rl_end] = '\0';
  return l;
}

 
int
rl_delete_text (int from, int to)
{
  register char *text;
  register int diff, i;

   
  if (from > to)
    SWAP (from, to);

   
  if (to > rl_end)
    {
      to = rl_end;
      if (from > to)
	from = to;
    }
  if (from < 0)
    from = 0;

  text = rl_copy_text (from, to);

   
  diff = to - from;
  for (i = from; i < rl_end - diff; i++)
    rl_line_buffer[i] = rl_line_buffer[i + diff];

   
  if (_rl_doing_an_undo == 0)
    rl_add_undo (UNDO_DELETE, from, to, text);
  else
    xfree (text);

  rl_end -= diff;
  rl_line_buffer[rl_end] = '\0';
  _rl_fix_mark ();
  return (diff);
}

 

#define _RL_FIX_POINT(x) \
	do { \
	if (x > rl_end) \
	  x = rl_end; \
	else if (x < 0) \
	  x = 0; \
	} while (0)

void
_rl_fix_point (int fix_mark_too)
{
  _RL_FIX_POINT (rl_point);
  if (fix_mark_too)
    _RL_FIX_POINT (rl_mark);
}

void
_rl_fix_mark (void)
{
  _RL_FIX_POINT (rl_mark);
}
#undef _RL_FIX_POINT

 
int
_rl_replace_text (const char *text, int start, int end)
{
  int n;

  n = 0;
  rl_begin_undo_group ();
  if (start <= end)
    rl_delete_text (start, end + 1);
  rl_point = start;
  if (*text)
    n = rl_insert_text (text);
  rl_end_undo_group ();

  return n;
}

 
void
rl_replace_line (const char *text, int clear_undo)
{
  int len;

  len = strlen (text);
  if (len >= rl_line_buffer_len)
    rl_extend_line_buffer (len);
  strcpy (rl_line_buffer, text);
  rl_end = len;

  if (clear_undo)
    rl_free_undo_list ();

  _rl_fix_point (1);
}

 
 
 
 
 

 

 

 
 
 
 
 

 

 
int
rl_forward_byte (int count, int key)
{
  if (count < 0)
    return (rl_backward_byte (-count, key));

  if (count > 0)
    {
      int end, lend;

      end = rl_point + count;
#if defined (VI_MODE)
      lend = rl_end > 0 ? rl_end - (VI_COMMAND_MODE()) : rl_end;
#else
      lend = rl_end;
#endif

      if (end > lend)
	{
	  rl_point = lend;
	  rl_ding ();
	}
      else
	rl_point = end;
    }

  if (rl_end < 0)
    rl_end = 0;

  return 0;
}

int
_rl_forward_char_internal (int count)
{
  int point;

#if defined (HANDLE_MULTIBYTE)
  point = _rl_find_next_mbchar (rl_line_buffer, rl_point, count, MB_FIND_NONZERO);

#if defined (VI_MODE)
  if (point >= rl_end && VI_COMMAND_MODE())
    point = _rl_find_prev_mbchar (rl_line_buffer, rl_end, MB_FIND_NONZERO);
#endif

    if (rl_end < 0)
      rl_end = 0;
#else
  point = rl_point + count;
#endif

  if (point > rl_end)
    point = rl_end;
  return (point);
}

int
_rl_backward_char_internal (int count)
{
  int point;

  point = rl_point;
#if defined (HANDLE_MULTIBYTE)
  if (count > 0)
    {
      while (count > 0 && point > 0)
	{
	  point = _rl_find_prev_mbchar (rl_line_buffer, point, MB_FIND_NONZERO);
	  count--;
	}
      if (count > 0)
        return 0;	 
    }
#else
  if (count > 0)
    point -= count;
#endif

  if (point < 0)
    point = 0;
  return (point);
}

#if defined (HANDLE_MULTIBYTE)
 
int
rl_forward_char (int count, int key)
{
  int point;

  if (MB_CUR_MAX == 1 || rl_byte_oriented)
    return (rl_forward_byte (count, key));

  if (count < 0)
    return (rl_backward_char (-count, key));

  if (count > 0)
    {
      if (rl_point == rl_end && EMACS_MODE())
	{
	  rl_ding ();
	  return 0;
	}

      point = _rl_forward_char_internal (count);

      if (rl_point == point)
	rl_ding ();

      rl_point = point;
    }

  return 0;
}
#else  
int
rl_forward_char (int count, int key)
{
  return (rl_forward_byte (count, key));
}
#endif  
  
 
int
rl_forward (int count, int key)
{
  return (rl_forward_char (count, key));
}

 
int
rl_backward_byte (int count, int key)
{
  if (count < 0)
    return (rl_forward_byte (-count, key));

  if (count > 0)
    {
      if (rl_point < count)
	{
	  rl_point = 0;
	  rl_ding ();
	}
      else
	rl_point -= count;
    }

  if (rl_point < 0)
    rl_point = 0;

  return 0;
}

#if defined (HANDLE_MULTIBYTE)
 
int
rl_backward_char (int count, int key)
{
  int point;

  if (MB_CUR_MAX == 1 || rl_byte_oriented)
    return (rl_backward_byte (count, key));

  if (count < 0)
    return (rl_forward_char (-count, key));

  if (count > 0)
    {
      point = rl_point;

      while (count > 0 && point > 0)
	{
	  point = _rl_find_prev_mbchar (rl_line_buffer, point, MB_FIND_NONZERO);
	  count--;
	}
      if (count > 0)
	{
	  rl_point = 0;
	  rl_ding ();
	}
      else
        rl_point = point;
    }

  return 0;
}
#else
int
rl_backward_char (int count, int key)
{
  return (rl_backward_byte (count, key));
}
#endif

 
int
rl_backward (int count, int key)
{
  return (rl_backward_char (count, key));
}

 
int
rl_beg_of_line (int count, int key)
{
  rl_point = 0;
  return 0;
}

 
int
rl_end_of_line (int count, int key)
{
  rl_point = rl_end;
  return 0;
}

 
int
rl_forward_word (int count, int key)
{
  int c;

  if (count < 0)
    return (rl_backward_word (-count, key));

  while (count)
    {
      if (rl_point > rl_end)
	rl_point = rl_end;
      if (rl_point == rl_end)
	return 0;

       
      c = _rl_char_value (rl_line_buffer, rl_point);

      if (_rl_walphabetic (c) == 0)
	{
	  rl_point = MB_NEXTCHAR (rl_line_buffer, rl_point, 1, MB_FIND_NONZERO);
	  while (rl_point < rl_end)
	    {
	      c = _rl_char_value (rl_line_buffer, rl_point);
	      if (_rl_walphabetic (c))
		break;
	      rl_point = MB_NEXTCHAR (rl_line_buffer, rl_point, 1, MB_FIND_NONZERO);
	    }
	}

      if (rl_point > rl_end)
	rl_point = rl_end;
      if (rl_point == rl_end)
	return 0;

      rl_point = MB_NEXTCHAR (rl_line_buffer, rl_point, 1, MB_FIND_NONZERO);
      while (rl_point < rl_end)
	{
	  c = _rl_char_value (rl_line_buffer, rl_point);
	  if (_rl_walphabetic (c) == 0)
	    break;
	  rl_point = MB_NEXTCHAR (rl_line_buffer, rl_point, 1, MB_FIND_NONZERO);
	}

      --count;
    }

  return 0;
}

 
int
rl_backward_word (int count, int key)
{
  int c, p;

  if (count < 0)
    return (rl_forward_word (-count, key));

  while (count)
    {
      if (rl_point == 0)
	return 0;

       

      p = MB_PREVCHAR (rl_line_buffer, rl_point, MB_FIND_NONZERO);
      c = _rl_char_value (rl_line_buffer, p);

      if (_rl_walphabetic (c) == 0)
	{
	  rl_point = p;
	  while (rl_point > 0)
	    {
	      p = MB_PREVCHAR (rl_line_buffer, rl_point, MB_FIND_NONZERO);
	      c = _rl_char_value (rl_line_buffer, p);
	      if (_rl_walphabetic (c))
		break;
	      rl_point = p;
	    }
	}

      while (rl_point)
	{
	  p = MB_PREVCHAR (rl_line_buffer, rl_point, MB_FIND_NONZERO);
	  c = _rl_char_value (rl_line_buffer, p);	  
	  if (_rl_walphabetic (c) == 0)
	    break;
	  else
	    rl_point = p;
	}

      --count;
    }

  return 0;
}

 
int
rl_refresh_line (int ignore1, int ignore2)
{
  _rl_refresh_line ();
  rl_display_fixed = 1;
  return 0;
}

 
int
rl_clear_screen (int count, int key)
{
  if (rl_explicit_arg)
    {
      rl_refresh_line (count, key);
      return 0;
    }

  _rl_clear_screen (0);		 
  rl_keep_mark_active ();
  rl_forced_update_display ();
  rl_display_fixed = 1;

  return 0;
}

int
rl_clear_display (int count, int key)
{
  _rl_clear_screen (1);		 
  rl_forced_update_display ();
  rl_display_fixed = 1;

  return 0;
}

int
rl_previous_screen_line (int count, int key)
{
  int c;

  c = _rl_term_autowrap ? _rl_screenwidth : (_rl_screenwidth + 1);
  return (rl_backward_char (c, key));
}

int
rl_next_screen_line (int count, int key)
{
  int c;

  c = _rl_term_autowrap ? _rl_screenwidth : (_rl_screenwidth + 1);
  return (rl_forward_char (c, key));
}

int
rl_skip_csi_sequence (int count, int key)
{
  int ch;

  RL_SETSTATE (RL_STATE_MOREINPUT);
  do
    ch = rl_read_key ();
  while (ch >= 0x20 && ch < 0x40);
  RL_UNSETSTATE (RL_STATE_MOREINPUT);

  return (ch < 0);
}

int
rl_arrow_keys (int count, int key)
{
  int ch;

  RL_SETSTATE(RL_STATE_MOREINPUT);
  ch = rl_read_key ();
  RL_UNSETSTATE(RL_STATE_MOREINPUT);
  if (ch < 0)
    return (1);

  switch (_rl_to_upper (ch))
    {
    case 'A':
      rl_get_previous_history (count, ch);
      break;

    case 'B':
      rl_get_next_history (count, ch);
      break;

    case 'C':
      if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
	rl_forward_char (count, ch);
      else
	rl_forward_byte (count, ch);
      break;

    case 'D':
      if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
	rl_backward_char (count, ch);
      else
	rl_backward_byte (count, ch);
      break;

    default:
      rl_ding ();
    }

  return 0;
}

 
 
 
 
 

#ifdef HANDLE_MULTIBYTE
static char pending_bytes[MB_LEN_MAX];
static int pending_bytes_length = 0;
static mbstate_t ps = {0};
#endif

 
int
_rl_insert_char (int count, int c)
{
  register int i;
  char *string;
#ifdef HANDLE_MULTIBYTE
  int string_size;
  char incoming[MB_LEN_MAX + 1];
  int incoming_length = 0;
  mbstate_t ps_back;
  static int stored_count = 0;
#endif

  if (count <= 0)
    return 0;

#if defined (HANDLE_MULTIBYTE)
  if (MB_CUR_MAX == 1 || rl_byte_oriented)
    {
      incoming[0] = c;
      incoming[1] = '\0';
      incoming_length = 1;
    }
  else if (_rl_utf8locale && (c & 0x80) == 0)
    {
      incoming[0] = c;
      incoming[1] = '\0';
      incoming_length = 1;
    }
  else
    {
      WCHAR_T wc;
      size_t ret;

      if (stored_count <= 0)
	stored_count = count;
      else
	count = stored_count;

      ps_back = ps;
      pending_bytes[pending_bytes_length++] = c;
      ret = MBRTOWC (&wc, pending_bytes, pending_bytes_length, &ps);

      if (ret == (size_t)-2)
	{
	   
	  ps = ps_back;
	  return 1;
	}
      else if (ret == (size_t)-1)
	{
	   
	  incoming[0] = pending_bytes[0];
	  incoming[1] = '\0';
	  incoming_length = 1;
	  pending_bytes_length--;
	  memmove (pending_bytes, pending_bytes + 1, pending_bytes_length);
	   
	  memset (&ps, 0, sizeof (mbstate_t));
	}
      else if (ret == (size_t)0)
	{
	  incoming[0] = '\0';
	  incoming_length = 0;
	  pending_bytes_length--;
	   
	  memset (&ps, 0, sizeof (mbstate_t));
	}
      else if (ret == 1)
	{
	  incoming[0] = pending_bytes[0];
	  incoming[incoming_length = 1] = '\0';
	  pending_bytes_length = 0;
	}
      else
	{
	   
	  memcpy (incoming, pending_bytes, pending_bytes_length);
	  incoming[pending_bytes_length] = '\0';
	  incoming_length = pending_bytes_length;
	  pending_bytes_length = 0;
	}
    }
#endif  
	  
   
  if (count > 1 && count <= TEXT_COUNT_MAX)
    {
#if defined (HANDLE_MULTIBYTE)
      string_size = count * incoming_length;
      string = (char *)xmalloc (1 + string_size);

      i = 0;
      while (i < string_size)
	{
	  if (incoming_length == 1)
	    string[i++] = *incoming;
	  else
	    {
	      strncpy (string + i, incoming, incoming_length);
	      i += incoming_length;
	    }
	}
      incoming_length = 0;
      stored_count = 0;
#else  
      string = (char *)xmalloc (1 + count);

      for (i = 0; i < count; i++)
	string[i] = c;
#endif  

      string[i] = '\0';
      rl_insert_text (string);
      xfree (string);

      return 0;
    }

  if (count > TEXT_COUNT_MAX)
    {
      int decreaser;
#if defined (HANDLE_MULTIBYTE)
      string_size = incoming_length * TEXT_COUNT_MAX;
      string = (char *)xmalloc (1 + string_size);

      i = 0;
      while (i < string_size)
	{
	  if (incoming_length == 1)
	    string[i++] = *incoming;
	  else
	    {
	      strncpy (string + i, incoming, incoming_length);
	      i += incoming_length;
	    }
	}

      while (count)
	{
	  decreaser = (count > TEXT_COUNT_MAX) ? TEXT_COUNT_MAX : count;
	  string[decreaser*incoming_length] = '\0';
	  rl_insert_text (string);
	  count -= decreaser;
	}

      xfree (string);
      incoming_length = 0;
      stored_count = 0;
#else  
      char str[TEXT_COUNT_MAX+1];

      for (i = 0; i < TEXT_COUNT_MAX; i++)
	str[i] = c;

      while (count)
	{
	  decreaser = (count > TEXT_COUNT_MAX ? TEXT_COUNT_MAX : count);
	  str[decreaser] = '\0';
	  rl_insert_text (str);
	  count -= decreaser;
	}
#endif  

      return 0;
    }

  if (MB_CUR_MAX == 1 || rl_byte_oriented)
    {
       
      if ((RL_ISSTATE (RL_STATE_MACROINPUT) == 0) && _rl_pushed_input_available ())
	_rl_insert_typein (c);
      else
	{
	   
	  char str[2];

	  str[1] = '\0';
	  str[0] = c;
	  rl_insert_text (str);
	}
    }
#if defined (HANDLE_MULTIBYTE)
  else
    {
      rl_insert_text (incoming);
      stored_count = 0;
    }
#endif

  return 0;
}

 
int
_rl_overwrite_char (int count, int c)
{
  int i;
#if defined (HANDLE_MULTIBYTE)
  char mbkey[MB_LEN_MAX];
  int k;

   
  k = 1;
  if (count > 0 && MB_CUR_MAX > 1 && rl_byte_oriented == 0)
    k = _rl_read_mbstring (c, mbkey, MB_LEN_MAX);
  if (k < 0)
    return 1;
#endif

  rl_begin_undo_group ();

  for (i = 0; i < count; i++)
    {
#if defined (HANDLE_MULTIBYTE)
      if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
	rl_insert_text (mbkey);
      else
#endif
	_rl_insert_char (1, c);

      if (rl_point < rl_end)
	rl_delete (1, c);
    }

  rl_end_undo_group ();

  return 0;
}

int
rl_insert (int count, int c)
{
  int r, n, x;

  r = (rl_insert_mode == RL_IM_INSERT) ? _rl_insert_char (count, c) : _rl_overwrite_char (count, c);

   
  x = 0;
  n = (unsigned short)-2;
  while (_rl_optimize_typeahead &&
	 rl_num_chars_to_read == 0 &&
	 (RL_ISSTATE (RL_STATE_INPUTPENDING|RL_STATE_MACROINPUT) == 0) &&
	 _rl_pushed_input_available () == 0 &&
	 _rl_input_queued (0) &&
	 (n = rl_read_key ()) > 0 &&
	 _rl_keymap[(unsigned char)n].type == ISFUNC &&
	 _rl_keymap[(unsigned char)n].function == rl_insert)
    {
      r = (rl_insert_mode == RL_IM_INSERT) ? _rl_insert_char (1, n) : _rl_overwrite_char (1, n);
       
      n = (unsigned short)-2;
      x++;		 
      if (r == 1)	 
	continue;
      if (rl_done || r != 0)
	break;
    }

  if (n != (unsigned short)-2)		 
    {
       
      rl_last_func = rl_insert; 
      _rl_reset_argument ();
      rl_executing_keyseq[rl_key_sequence_length = 0] = '\0';
      r = rl_execute_next (n);
    }

  return r;
}

 
static int
_rl_insert_next (int count)
{
  int c;

  RL_SETSTATE(RL_STATE_MOREINPUT);
  c = rl_read_key ();
  RL_UNSETSTATE(RL_STATE_MOREINPUT);

  if (c < 0)
    return 1;

  if (RL_ISSTATE (RL_STATE_MACRODEF))
    _rl_add_macro_char (c);

#if defined (HANDLE_SIGNALS)
  if (RL_ISSTATE (RL_STATE_CALLBACK) == 0)
    _rl_restore_tty_signals ();
#endif

  return (_rl_insert_char (count, c));  
}

#if defined (READLINE_CALLBACKS)
static int
_rl_insert_next_callback (_rl_callback_generic_arg *data)
{
  int count, r;

  count = data->count;
  r = 0;

  if (count < 0)
    {
      data->count++;
      r = _rl_insert_next (1);
      _rl_want_redisplay = 1;
       
      if (data->count < 0 && r == 0)
	return r;
      count = 0;	 
    }

   
  _rl_callback_func = 0;
  _rl_want_redisplay = 1;

  if (count == 0)
    return r;

  return _rl_insert_next (count);
}
#endif
  
int
rl_quoted_insert (int count, int key)
{
   
#if defined (HANDLE_SIGNALS)
  if (RL_ISSTATE (RL_STATE_CALLBACK) == 0)
    _rl_disable_tty_signals ();
#endif

#if defined (READLINE_CALLBACKS)
  if (RL_ISSTATE (RL_STATE_CALLBACK))
    {
      _rl_callback_data = _rl_callback_data_alloc (count);
      _rl_callback_func = _rl_insert_next_callback;
      return (0);
    }
#endif

   
  if (count < 0)
    {
      int r;

      do
	r = _rl_insert_next (1);
      while (r == 0 && ++count < 0);
      return r;
    }

  return _rl_insert_next (count);
}

 
int
rl_tab_insert (int count, int key)
{
  return (_rl_insert_char (count, '\t'));
}

 
int
rl_newline (int count, int key)
{
  if (rl_mark_active_p ())
    {
      rl_deactivate_mark ();
      (*rl_redisplay_function) ();
      _rl_want_redisplay = 0;
    }

  rl_done = 1;

  if (_rl_history_preserve_point)
    _rl_history_saved_point = (rl_point == rl_end) ? -1 : rl_point;

  RL_SETSTATE(RL_STATE_DONE);

#if defined (VI_MODE)
  if (rl_editing_mode == vi_mode)
    {
      _rl_vi_done_inserting ();
      if (_rl_vi_textmod_command (_rl_vi_last_command) == 0)	 
	_rl_vi_reset_last ();
    }
#endif  

   
  if (rl_erase_empty_line && rl_point == 0 && rl_end == 0)
    return 0;

  if (_rl_echoing_p)
    _rl_update_final ();
  return 0;
}

 
int
rl_do_lowercase_version (int ignore1, int ignore2)
{
  return 99999;		 
}

 
int
_rl_overwrite_rubout (int count, int key)
{
  int opoint;
  int i, l;

  if (rl_point == 0)
    {
      rl_ding ();
      return 1;
    }

  opoint = rl_point;

   
  for (i = l = 0; i < count; i++)
    {
      rl_backward_char (1, key);
      l += rl_character_len (rl_line_buffer[rl_point], rl_point);	 
    }

  rl_begin_undo_group ();

  if (count > 1 || rl_explicit_arg)
    rl_kill_text (opoint, rl_point);
  else
    rl_delete_text (opoint, rl_point);

   
  if (rl_point < rl_end)
    {
      opoint = rl_point;
      _rl_insert_char (l, ' ');
      rl_point = opoint;
    }

  rl_end_undo_group ();

  return 0;
}
  
 
int
rl_rubout (int count, int key)
{
  if (count < 0)
    return (rl_delete (-count, key));

  if (!rl_point)
    {
      rl_ding ();
      return 1;
    }

  if (rl_insert_mode == RL_IM_OVERWRITE)
    return (_rl_overwrite_rubout (count, key));

  return (_rl_rubout_char (count, key));
}

int
_rl_rubout_char (int count, int key)
{
  int orig_point;
  unsigned char c;

   
  if (count < 0)
    return (rl_delete (-count, key));

  if (rl_point == 0)
    {
      rl_ding ();
      return 1;
    }

  orig_point = rl_point;
  if (count > 1 || rl_explicit_arg)
    {
      rl_backward_char (count, key);
      rl_kill_text (orig_point, rl_point);
    }
  else if (MB_CUR_MAX == 1 || rl_byte_oriented)
    {
      c = rl_line_buffer[--rl_point];
      rl_delete_text (rl_point, orig_point);
       
      if (rl_point == rl_end && ISPRINT ((unsigned char)c) && _rl_last_c_pos)
	{
	  int l;
	  l = rl_character_len (c, rl_point);
	  _rl_erase_at_end_of_line (l);
	}
    }
  else
    {
      rl_point = _rl_find_prev_mbchar (rl_line_buffer, rl_point, MB_FIND_NONZERO);
      rl_delete_text (rl_point, orig_point);
    }

  return 0;
}

 
int
rl_delete (int count, int key)
{
  int xpoint;

  if (count < 0)
    return (_rl_rubout_char (-count, key));

  if (rl_point == rl_end)
    {
      rl_ding ();
      return 1;
    }

  if (count > 1 || rl_explicit_arg)
    {
      xpoint = rl_point;
      if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
	rl_forward_char (count, key);
      else
	rl_forward_byte (count, key);

      rl_kill_text (xpoint, rl_point);
      rl_point = xpoint;
    }
  else
    {
      xpoint = MB_NEXTCHAR (rl_line_buffer, rl_point, 1, MB_FIND_NONZERO);
      rl_delete_text (rl_point, xpoint);
    }
  return 0;
}

       
int
rl_rubout_or_delete (int count, int key)
{
  if (rl_end != 0 && rl_point == rl_end)
    return (_rl_rubout_char (count, key));
  else
    return (rl_delete (count, key));
}  

 
int
rl_delete_horizontal_space (int count, int ignore)
{
  int start;

  while (rl_point && whitespace (rl_line_buffer[rl_point - 1]))
    rl_point--;

  start = rl_point;

  while (rl_point < rl_end && whitespace (rl_line_buffer[rl_point]))
    rl_point++;

  if (start != rl_point)
    {
      rl_delete_text (start, rl_point);
      rl_point = start;
    }

  if (rl_point < 0)
    rl_point = 0;

  return 0;
}

 
int
rl_delete_or_show_completions (int count, int key)
{
  if (rl_end != 0 && rl_point == rl_end)
    return (rl_possible_completions (count, key));
  else
    return (rl_delete (count, key));
}

#ifndef RL_COMMENT_BEGIN_DEFAULT
#define RL_COMMENT_BEGIN_DEFAULT "#"
#endif

 
int
rl_insert_comment (int count, int key)
{
  char *rl_comment_text;
  int rl_comment_len;

  rl_beg_of_line (1, key);
  rl_comment_text = _rl_comment_begin ? _rl_comment_begin : RL_COMMENT_BEGIN_DEFAULT;

  if (rl_explicit_arg == 0)
    rl_insert_text (rl_comment_text);
  else
    {
      rl_comment_len = strlen (rl_comment_text);
      if (STREQN (rl_comment_text, rl_line_buffer, rl_comment_len))
	rl_delete_text (rl_point, rl_point + rl_comment_len);
      else
	rl_insert_text (rl_comment_text);
    }

  (*rl_redisplay_function) ();
  rl_newline (1, '\n');

  return (0);
}

 
 
 
 
 

 
#define UpCase 1
#define DownCase 2
#define CapCase 3

 
int
rl_upcase_word (int count, int key)
{
  return (rl_change_case (count, UpCase));
}

 
int
rl_downcase_word (int count, int key)
{
  return (rl_change_case (count, DownCase));
}

 
int
rl_capitalize_word (int count, int key)
{
 return (rl_change_case (count, CapCase));
}

 
static int
rl_change_case (int count, int op)
{
  int start, next, end;
  int inword, nc, nop;
  WCHAR_T c;
#if defined (HANDLE_MULTIBYTE)
  WCHAR_T wc, nwc;
  char mb[MB_LEN_MAX+1];
  int mlen;
  size_t m;
  mbstate_t mps;
#endif

  start = rl_point;
  rl_forward_word (count, 0);
  end = rl_point;

  if (op != UpCase && op != DownCase && op != CapCase)
    {
      rl_ding ();
      return 1;
    }

  if (count < 0)
    SWAP (start, end);

#if defined (HANDLE_MULTIBYTE)
  memset (&mps, 0, sizeof (mbstate_t));
#endif

   
  rl_modifying (start, end);

  inword = 0;
  while (start < end)
    {
      c = _rl_char_value (rl_line_buffer, start);
       
      next = MB_NEXTCHAR (rl_line_buffer, start, 1, MB_FIND_NONZERO);

      if (_rl_walphabetic (c) == 0)
	{
	  inword = 0;
	  start = next;
	  continue;
	}

      if (op == CapCase)
	{
	  nop = inword ? DownCase : UpCase;
	  inword = 1;
	}
      else
	nop = op;
       
      if (MB_CUR_MAX == 1 || rl_byte_oriented)
	{
	  nc = (nop == UpCase) ? _rl_to_upper (c) : _rl_to_lower (c);
	  rl_line_buffer[start] = nc;
	}
#if defined (HANDLE_MULTIBYTE)
      else
	{
	  m = MBRTOWC (&wc, rl_line_buffer + start, end - start, &mps);
	  if (MB_INVALIDCH (m))
	    wc = (WCHAR_T)rl_line_buffer[start];
	  else if (MB_NULLWCH (m))
	    wc = L'\0';
	  nwc = (nop == UpCase) ? _rl_to_wupper (wc) : _rl_to_wlower (wc);
	  if  (nwc != wc)	 
	    {
	      char *s, *e;
	      mbstate_t ts;

	      memset (&ts, 0, sizeof (mbstate_t));
	      mlen = WCRTOMB (mb, nwc, &ts);
	      if (mlen < 0)
		{
		  nwc = wc;
		  memset (&ts, 0, sizeof (mbstate_t));
		  mlen = WCRTOMB (mb, nwc, &ts);
		  if (mlen < 0)		 
		    strncpy (mb, rl_line_buffer + start, mlen = m);
		}
	      if (mlen > 0)
		mb[mlen] = '\0';
	       
	       
	      s = rl_line_buffer + start;
	      e = rl_line_buffer + rl_end;
	      if (m == mlen)
		memcpy (s, mb, mlen);
	      else if (m > mlen)
		{
		  memcpy (s, mb, mlen);
		  memmove (s + mlen, s + m, (e - s) - m);
		  next -= m - mlen;	 
		  end -= m - mlen;	 
		  rl_end -= m - mlen;	 
		  rl_line_buffer[rl_end] = 0;
		}
	      else if (m < mlen)
		{
		  rl_extend_line_buffer (rl_end + mlen + (e - s) - m + 2);
		  s = rl_line_buffer + start;	 
		  e = rl_line_buffer + rl_end;
		  memmove (s + mlen, s + m, (e - s) - m);
		  memcpy (s, mb, mlen);
		  next += mlen - m;	 
		  end += mlen - m;	 
		  rl_end += mlen - m;	 
		  rl_line_buffer[rl_end] = 0;
		}
	    }
	}
#endif

      start = next;
    }

  rl_point = end;
  return 0;
}

 
 
 
 
 

 
int
rl_transpose_words (int count, int key)
{
  char *word1, *word2;
  int w1_beg, w1_end, w2_beg, w2_end;
  int orig_point, orig_end;

  orig_point = rl_point;
  orig_end = rl_end;

  if (!count)
    return 0;

   
  rl_forward_word (count, key);
  w2_end = rl_point;
  rl_backward_word (1, key);
  w2_beg = rl_point;
  rl_backward_word (count, key);
  w1_beg = rl_point;
  rl_forward_word (1, key);
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
  rl_end = orig_end;		 

   
  rl_end_undo_group ();
  xfree (word1);
  xfree (word2);

  return 0;
}

 
int
rl_transpose_chars (int count, int key)
{
#if defined (HANDLE_MULTIBYTE)
  char *dummy;
  int i;
#else
  char dummy[2];
#endif
  int char_length, prev_point;

  if (count == 0)
    return 0;

  if (!rl_point || rl_end < 2)
    {
      rl_ding ();
      return 1;
    }

  rl_begin_undo_group ();

  if (rl_point == rl_end)
    {
      rl_point = MB_PREVCHAR (rl_line_buffer, rl_point, MB_FIND_NONZERO);
      count = 1;
    }

  prev_point = rl_point;
  rl_point = MB_PREVCHAR (rl_line_buffer, rl_point, MB_FIND_NONZERO);

#if defined (HANDLE_MULTIBYTE)
  char_length = prev_point - rl_point;
  dummy = (char *)xmalloc (char_length + 1);
  for (i = 0; i < char_length; i++)
    dummy[i] = rl_line_buffer[rl_point + i];
  dummy[i] = '\0';
#else
  dummy[0] = rl_line_buffer[rl_point];
  dummy[char_length = 1] = '\0';
#endif

  rl_delete_text (rl_point, rl_point + char_length);

  rl_point = _rl_find_next_mbchar (rl_line_buffer, rl_point, count, MB_FIND_NONZERO);

  _rl_fix_point (0);
  rl_insert_text (dummy);
  rl_end_undo_group ();

#if defined (HANDLE_MULTIBYTE)
  xfree (dummy);
#endif

  return 0;
}

 
 
 
 
 

int
#if defined (HANDLE_MULTIBYTE)
_rl_char_search_internal (int count, int dir, char *smbchar, int len)
#else
_rl_char_search_internal (int count, int dir, int schar)
#endif
{
  int pos, inc;
#if defined (HANDLE_MULTIBYTE)
  int prepos;
#endif

  if (dir == 0)
    return 1;

  pos = rl_point;
  inc = (dir < 0) ? -1 : 1;
  while (count)
    {
      if ((dir < 0 && pos <= 0) || (dir > 0 && pos >= rl_end))
	{
	  rl_ding ();
	  return 1;
	}

#if defined (HANDLE_MULTIBYTE)
      pos = (inc > 0) ? _rl_find_next_mbchar (rl_line_buffer, pos, 1, MB_FIND_ANY)
		      : _rl_find_prev_mbchar (rl_line_buffer, pos, MB_FIND_ANY);
#else
      pos += inc;
#endif
      do
	{
#if defined (HANDLE_MULTIBYTE)
	  if (_rl_is_mbchar_matched (rl_line_buffer, pos, rl_end, smbchar, len))
#else
	  if (rl_line_buffer[pos] == schar)
#endif
	    {
	      count--;
	      if (dir < 0)
	        rl_point = (dir == BTO) ? _rl_find_next_mbchar (rl_line_buffer, pos, 1, MB_FIND_ANY)
					: pos;
	      else
		rl_point = (dir == FTO) ? _rl_find_prev_mbchar (rl_line_buffer, pos, MB_FIND_ANY)
					: pos;
	      break;
	    }
#if defined (HANDLE_MULTIBYTE)
	  prepos = pos;
#endif
	}
#if defined (HANDLE_MULTIBYTE)
      while ((dir < 0) ? (pos = _rl_find_prev_mbchar (rl_line_buffer, pos, MB_FIND_ANY)) != prepos
		       : (pos = _rl_find_next_mbchar (rl_line_buffer, pos, 1, MB_FIND_ANY)) != prepos);
#else
      while ((dir < 0) ? pos-- : ++pos < rl_end);
#endif
    }
  return (0);
}

 
#if defined (HANDLE_MULTIBYTE)
static int
_rl_char_search (int count, int fdir, int bdir)
{
  char mbchar[MB_LEN_MAX];
  int mb_len;

  mb_len = _rl_read_mbchar (mbchar, MB_LEN_MAX);

  if (mb_len <= 0)
    return 1;

  if (count < 0)
    return (_rl_char_search_internal (-count, bdir, mbchar, mb_len));
  else
    return (_rl_char_search_internal (count, fdir, mbchar, mb_len));
}
#else  
static int
_rl_char_search (int count, int fdir, int bdir)
{
  int c;

  c = _rl_bracketed_read_key ();
  if (c < 0)
    return 1;

  if (count < 0)
    return (_rl_char_search_internal (-count, bdir, c));
  else
    return (_rl_char_search_internal (count, fdir, c));
}
#endif  

#if defined (READLINE_CALLBACKS)
static int
_rl_char_search_callback (data)
     _rl_callback_generic_arg *data;
{
  _rl_callback_func = 0;
  _rl_want_redisplay = 1;

  return (_rl_char_search (data->count, data->i1, data->i2));
}
#endif

int
rl_char_search (int count, int key)
{
#if defined (READLINE_CALLBACKS)
  if (RL_ISSTATE (RL_STATE_CALLBACK))
    {
      _rl_callback_data = _rl_callback_data_alloc (count);
      _rl_callback_data->i1 = FFIND;
      _rl_callback_data->i2 = BFIND;
      _rl_callback_func = _rl_char_search_callback;
      return (0);
    }
#endif
  
  return (_rl_char_search (count, FFIND, BFIND));
}

int
rl_backward_char_search (int count, int key)
{
#if defined (READLINE_CALLBACKS)
  if (RL_ISSTATE (RL_STATE_CALLBACK))
    {
      _rl_callback_data = _rl_callback_data_alloc (count);
      _rl_callback_data->i1 = BFIND;
      _rl_callback_data->i2 = FFIND;
      _rl_callback_func = _rl_char_search_callback;
      return (0);
    }
#endif

  return (_rl_char_search (count, BFIND, FFIND));
}

 
 
 
 
 

 
int
_rl_set_mark_at_pos (int position)
{
  if (position < 0 || position > rl_end)
    return 1;

  rl_mark = position;
  return 0;
}

 
int
rl_set_mark (int count, int key)
{
  return (_rl_set_mark_at_pos (rl_explicit_arg ? count : rl_point));
}

 
int
rl_exchange_point_and_mark (int count, int key)
{
  if (rl_mark > rl_end)
    rl_mark = -1;

  if (rl_mark < 0)
    {
      rl_ding ();
      rl_mark = 0;		 
      return 1;
    }
  else
    {
      SWAP (rl_point, rl_mark);
      rl_activate_mark ();
    }

  return 0;
}

 

 
static int mark_active = 0;

 
int _rl_keep_mark_active;

void
rl_keep_mark_active (void)
{
  _rl_keep_mark_active++;
}

void
rl_activate_mark (void)
{
  mark_active = 1;
  rl_keep_mark_active ();
}

void
rl_deactivate_mark (void)
{
  mark_active = 0;
}

int
rl_mark_active_p (void)
{
  return (mark_active);
}
