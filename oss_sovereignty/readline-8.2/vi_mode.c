 

 

#define READLINE_LIBRARY

 
 
 
 
 
#include "rlconf.h"

#if defined (VI_MODE)

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <sys/types.h>

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif  

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include <stdio.h>

 
#include "rldefs.h"
#include "rlmbutil.h"

#include "readline.h"
#include "history.h"

#include "rlprivate.h"
#include "xmalloc.h"

#ifndef member
#define member(c, s) ((c) ? (char *)strchr ((s), (c)) != (char *)NULL : 0)
#endif

 
#if defined (HANDLE_MULTIBYTE)
#define INCREMENT_POS(start) \
  do { \
	if (MB_CUR_MAX == 1 || rl_byte_oriented) \
	  start++; \
	else \
	  start = _rl_find_next_mbchar (rl_line_buffer, start, 1, MB_FIND_ANY); \
  } while (0)
#else  
#define INCREMENT_POS(start)    (start)++
#endif  

 
int _rl_vi_last_command = 'i';	 

_rl_vimotion_cxt *_rl_vimvcxt = 0;

 
int _rl_vi_redoing;

 
static int _rl_vi_doing_insert;

 
static const char * const vi_motion = " hl^$0ftFT;,%wbeWBE|`";

 
static Keymap vi_replace_map;

 
static int vi_replace_count;

 
static int vi_continued_command;
static char *vi_insert_buffer;
static int vi_insert_buffer_size;

static int _rl_vi_last_repeat = 1;
static int _rl_vi_last_arg_sign = 1;
static int _rl_vi_last_motion;
#if defined (HANDLE_MULTIBYTE)
static char _rl_vi_last_search_mbchar[MB_LEN_MAX];
static int _rl_vi_last_search_mblen;
#else
static int _rl_vi_last_search_char;
#endif
static char _rl_vi_last_replacement[MB_LEN_MAX+1];	 

static int _rl_vi_last_key_before_insert;

 
static const char * const vi_textmod = "_*\\AaIiCcDdPpYyRrSsXx~";

 
static int vi_mark_chars['z' - 'a' + 1];

static void _rl_vi_replace_insert (int);
static void _rl_vi_save_replace (void);
static void _rl_vi_stuff_insert (int);
static void _rl_vi_save_insert (UNDO_LIST *);

static void vi_save_insert_buffer (int, int);

static inline void _rl_vi_backup (void);

static int _rl_vi_arg_dispatch (int);
static int rl_digit_loop1 (void);

static int _rl_vi_set_mark (void);
static int _rl_vi_goto_mark (void);

static inline int _rl_vi_advance_point (void);
static inline int _rl_vi_backup_point (void);

static void _rl_vi_append_forward (int);

static int _rl_vi_callback_getchar (char *, int);

#if defined (READLINE_CALLBACKS)
static int _rl_vi_callback_set_mark (_rl_callback_generic_arg *);
static int _rl_vi_callback_goto_mark (_rl_callback_generic_arg *);
static int _rl_vi_callback_change_char (_rl_callback_generic_arg *);
static int _rl_vi_callback_char_search (_rl_callback_generic_arg *);
#endif

static int rl_domove_read_callback (_rl_vimotion_cxt *);
static int rl_domove_motion_callback (_rl_vimotion_cxt *);
static int rl_vi_domove_getchar (_rl_vimotion_cxt *);

static int vi_change_dispatch (_rl_vimotion_cxt *);
static int vi_delete_dispatch (_rl_vimotion_cxt *);
static int vi_yank_dispatch (_rl_vimotion_cxt *);

static int vidomove_dispatch (_rl_vimotion_cxt *);

void
_rl_vi_initialize_line (void)
{
  register int i, n;

  n = sizeof (vi_mark_chars) / sizeof (vi_mark_chars[0]);
  for (i = 0; i < n; i++)
    vi_mark_chars[i] = -1;

  RL_UNSETSTATE(RL_STATE_VICMDONCE);
}

void
_rl_vi_reset_last (void)
{
  _rl_vi_last_command = 'i';
  _rl_vi_last_repeat = 1;
  _rl_vi_last_arg_sign = 1;
  _rl_vi_last_motion = 0;
}

void
_rl_vi_set_last (int key, int repeat, int sign)
{
  _rl_vi_last_command = key;
  _rl_vi_last_repeat = repeat;
  _rl_vi_last_arg_sign = sign;
}

 
void
rl_vi_start_inserting (int key, int repeat, int sign)
{
  _rl_vi_set_last (key, repeat, sign);
  rl_begin_undo_group ();		 
  rl_vi_insertion_mode (1, key);
}

 
int
_rl_vi_textmod_command (int c)
{
  return (member (c, vi_textmod));
}

int
_rl_vi_motion_command (int c)
{
  return (member (c, vi_motion));
}

static void
_rl_vi_replace_insert (int count)
{
  int nchars;

  nchars = strlen (vi_insert_buffer);

  rl_begin_undo_group ();
  while (count--)
     
    _rl_replace_text (vi_insert_buffer, rl_point, rl_point+nchars-1);
  rl_end_undo_group ();
}

static void
_rl_vi_stuff_insert (int count)
{
  rl_begin_undo_group ();
  while (count--)
    rl_insert_text (vi_insert_buffer);
  rl_end_undo_group ();
}

 
int
rl_vi_redo (int count, int c)
{
  int r;

  if (rl_explicit_arg == 0)
    {
      rl_numeric_arg = _rl_vi_last_repeat;
      rl_arg_sign = _rl_vi_last_arg_sign;
    }

  r = 0;
  _rl_vi_redoing = 1;
   
  if (_rl_vi_last_command == 'i' && vi_insert_buffer && *vi_insert_buffer)
    {
      _rl_vi_stuff_insert (count);
       
      if (rl_point > 0)
	_rl_vi_backup ();
    }
  else if (_rl_vi_last_command == 'R' && vi_insert_buffer && *vi_insert_buffer)
    {
      _rl_vi_replace_insert (count);
       
      if (rl_point > 0)
	_rl_vi_backup ();
    }
   
  else if (_rl_vi_last_command == 'I' && vi_insert_buffer && *vi_insert_buffer)
    {
      rl_beg_of_line (1, 'I');
      _rl_vi_stuff_insert (count);
      if (rl_point > 0)
	_rl_vi_backup ();
    }
   
  else if (_rl_vi_last_command == 'a' && vi_insert_buffer && *vi_insert_buffer)
    {
      _rl_vi_append_forward ('a');
      _rl_vi_stuff_insert (count);
      if (rl_point > 0)
	_rl_vi_backup ();
    }
   
  else if (_rl_vi_last_command == 'A' && vi_insert_buffer && *vi_insert_buffer)
    {
      rl_end_of_line (1, 'A');
      _rl_vi_stuff_insert (count);
      if (rl_point > 0)
	_rl_vi_backup ();
    }
  else if (_rl_vi_last_command == '.' && _rl_keymap == vi_movement_keymap)
    {
      rl_ding ();
      r = 0;
    }
  else
    r = _rl_dispatch (_rl_vi_last_command, _rl_keymap);

  _rl_vi_redoing = 0;

  return (r);
}

 
int
rl_vi_undo (int count, int key)
{
  return (rl_undo_command (count, key));
}
    
 
int
rl_vi_yank_arg (int count, int key)
{
   
  if (rl_explicit_arg)
    rl_yank_nth_arg (count - 1, key);
  else
    rl_yank_nth_arg ('$', key);

  return (0);
}

 
int
rl_vi_fetch_history (int count, int c)
{
  return (rl_fetch_history (count, c));
}

 
int
rl_vi_search_again (int count, int key)
{
  switch (key)
    {
    case 'n':
      rl_noninc_reverse_search_again (count, key);
      break;

    case 'N':
      rl_noninc_forward_search_again (count, key);
      break;
    }
  return (0);
}

 
int
rl_vi_search (int count, int key)
{
  switch (key)
    {
    case '?':
      _rl_free_saved_history_line ();
      rl_noninc_forward_search (count, key);
      break;

    case '/':
      _rl_free_saved_history_line ();
      rl_noninc_reverse_search (count, key);
      break;

    default:
      rl_ding ();
      break;
    }
  return (0);
}

 
int
rl_vi_complete (int ignore, int key)
{
  if ((rl_point < rl_end) && (!whitespace (rl_line_buffer[rl_point])))
    {
      if (!whitespace (rl_line_buffer[rl_point + 1]))
	rl_vi_end_word (1, 'E');
      _rl_vi_advance_point ();
    }

  if (key == '*')
    rl_complete_internal ('*');	 
  else if (key == '=')
    rl_complete_internal ('?');	 
  else if (key == '\\')
    rl_complete_internal (TAB);	 
  else
    rl_complete (0, key);

  if (key == '*' || key == '\\')
    rl_vi_start_inserting (key, 1, rl_arg_sign);

  return (0);
}

 
int
rl_vi_tilde_expand (int ignore, int key)
{
  rl_tilde_expand (0, key);
  rl_vi_start_inserting (key, 1, rl_arg_sign);
  return (0);
}

 
int
rl_vi_prev_word (int count, int key)
{
  if (count < 0)
    return (rl_vi_next_word (-count, key));

  if (rl_point == 0)
    {
      rl_ding ();
      return (0);
    }

  if (_rl_uppercase_p (key))
    rl_vi_bWord (count, key);
  else
    rl_vi_bword (count, key);

  return (0);
}

 
int
rl_vi_next_word (int count, int key)
{
  if (count < 0)
    return (rl_vi_prev_word (-count, key));

  if (rl_point >= (rl_end - 1))
    {
      rl_ding ();
      return (0);
    }

  if (_rl_uppercase_p (key))
    rl_vi_fWord (count, key);
  else
    rl_vi_fword (count, key);
  return (0);
}

static inline int
_rl_vi_advance_point (void)
{
  int point;

  point = rl_point;
  if (rl_point < rl_end)
#if defined (HANDLE_MULTIBYTE)
    {
      if (MB_CUR_MAX == 1 || rl_byte_oriented)
	rl_point++;
      else
	{
	  point = rl_point;
	  rl_point = _rl_forward_char_internal (1);
	  if (point == rl_point || rl_point > rl_end)
	    rl_point = rl_end;
	}
    }
#else
    rl_point++;
#endif

  return point;
}

 
static inline void
_rl_vi_backup (void)
{
  if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
    rl_point = _rl_find_prev_mbchar (rl_line_buffer, rl_point, MB_FIND_NONZERO);
  else
    rl_point--;
}

 
static inline int
_rl_vi_backup_point (void)
{
  int point;

  point = rl_point;
  if (rl_point > 0)
#if defined (HANDLE_MULTIBYTE)
    {
      if (MB_CUR_MAX == 1 || rl_byte_oriented)
	rl_point--;
      else
	{
	  point = rl_point;
	  rl_point = _rl_backward_char_internal (1);
	  if (rl_point < 0)
	    rl_point = 0;		 
	}
    }
#else
    rl_point--;
#endif
  return point;
}

 
int
rl_vi_end_word (int count, int key)
{
  if (count < 0)
    {
      rl_ding ();
      return 1;
    }

  if (_rl_uppercase_p (key))
    rl_vi_eWord (count, key);
  else
    rl_vi_eword (count, key);
  return (0);
}

 
int
rl_vi_fWord (int count, int ignore)
{
  while (count-- && rl_point < (rl_end - 1))
    {
       
      while (!whitespace (rl_line_buffer[rl_point]) && rl_point < rl_end)
	_rl_vi_advance_point ();

       
      while (whitespace (rl_line_buffer[rl_point]) && rl_point < rl_end)
	_rl_vi_advance_point ();
    }
  return (0);
}

int
rl_vi_bWord (int count, int ignore)
{
  while (count-- && rl_point > 0)
    {
       
      if (!whitespace (rl_line_buffer[rl_point]) &&
	  whitespace (rl_line_buffer[rl_point - 1]))
	rl_point--;

      while (rl_point > 0 && whitespace (rl_line_buffer[rl_point]))
	_rl_vi_backup_point ();

      if (rl_point > 0)
	{
	  do
	    _rl_vi_backup_point ();
	  while (rl_point > 0 && !whitespace (rl_line_buffer[rl_point]));
	  if (rl_point > 0)	 
	    rl_point++;		

	  if (rl_point < 0)
	    rl_point = 0;
	}
    }
  return (0);
}

int
rl_vi_eWord (int count, int ignore)
{
  int opoint;

  while (count-- && rl_point < (rl_end - 1))
    {
      if (whitespace (rl_line_buffer[rl_point]) == 0)
	_rl_vi_advance_point ();

       
      while (rl_point < rl_end && whitespace (rl_line_buffer[rl_point]))
	_rl_vi_advance_point ();

      if (rl_point && rl_point < rl_end)
	{
	  opoint = rl_point;

	   
	  while (rl_point < rl_end && whitespace (rl_line_buffer[rl_point]))
	    opoint = _rl_vi_advance_point ();	 

	   
	  while (rl_point < rl_end && !whitespace (rl_line_buffer[rl_point]))
	    opoint = _rl_vi_advance_point ();

	   
	  rl_point = opoint;
	}
    }
  return (0);
}

int
rl_vi_fword (int count, int ignore)
{
  int opoint;

  while (count-- && rl_point < (rl_end - 1))
    {
       
      if (_rl_isident (rl_line_buffer[rl_point]))
	{
	  while (_rl_isident (rl_line_buffer[rl_point]) && rl_point < rl_end)
	    _rl_vi_advance_point ();
	}
      else  
	{
	  while (!_rl_isident (rl_line_buffer[rl_point]) &&
		 !whitespace (rl_line_buffer[rl_point]) && rl_point < rl_end)
	    _rl_vi_advance_point ();
	}

      opoint = rl_point;

       
      while (whitespace (rl_line_buffer[rl_point]) && rl_point < rl_end)
	opoint = _rl_vi_advance_point ();
    }
  return (0);
}

int
rl_vi_bword (int count, int ignore)
{
  int opoint;

  while (count-- && rl_point > 0)
    {
      int prev_is_ident, cur_is_ident;

       
      if (!whitespace (rl_line_buffer[rl_point]) &&
	  whitespace (rl_line_buffer[rl_point - 1]))
	if (--rl_point == 0)
	  break;

       
      cur_is_ident = _rl_isident (rl_line_buffer[rl_point]);
      opoint = _rl_vi_backup_point ();
      prev_is_ident = _rl_isident (rl_line_buffer[rl_point]);
      if ((cur_is_ident && !prev_is_ident) || (!cur_is_ident && prev_is_ident))
	;	 
      else
	rl_point = opoint;

      while (rl_point > 0 && whitespace (rl_line_buffer[rl_point]))
	_rl_vi_backup_point ();

      if (rl_point > 0)
	{
	  opoint = rl_point;
	  if (_rl_isident (rl_line_buffer[rl_point]))
	    do
	      opoint = _rl_vi_backup_point ();
	    while (rl_point > 0 && _rl_isident (rl_line_buffer[rl_point]));
	  else
	    do
	      opoint = _rl_vi_backup_point ();
	    while (rl_point > 0 && !_rl_isident (rl_line_buffer[rl_point]) &&
		   !whitespace (rl_line_buffer[rl_point]));

	  if (rl_point > 0)
	    rl_point = opoint;

	  if (rl_point < 0)
	    rl_point = 0;
	}
    }
  return (0);
}

int
rl_vi_eword (int count, int ignore)
{
  int opoint;

  while (count-- && rl_point < (rl_end - 1))
    {
      if (whitespace (rl_line_buffer[rl_point]) == 0)
	_rl_vi_advance_point ();

      while (rl_point < rl_end && whitespace (rl_line_buffer[rl_point]))
	_rl_vi_advance_point ();

      opoint = rl_point;
      if (rl_point < rl_end)
	{
	  if (_rl_isident (rl_line_buffer[rl_point]))
	    do
	      {
		opoint = _rl_vi_advance_point ();
	      }
	    while (rl_point < rl_end && _rl_isident (rl_line_buffer[rl_point]));
	  else
	    do
	      {
		opoint = _rl_vi_advance_point ();
	      }
	    while (rl_point < rl_end && !_rl_isident (rl_line_buffer[rl_point])
		   && !whitespace (rl_line_buffer[rl_point]));
	}
      rl_point = opoint;
    }
  return (0);
}

int
rl_vi_insert_beg (int count, int key)
{
  rl_beg_of_line (1, key);
  rl_vi_insert_mode (1, key);
  return (0);
}

static void
_rl_vi_append_forward (int key)
{
  _rl_vi_advance_point ();
}

int
rl_vi_append_mode (int count, int key)
{
  _rl_vi_append_forward (key);
  rl_vi_start_inserting (key, 1, rl_arg_sign);
  return (0);
}

int
rl_vi_append_eol (int count, int key)
{
  rl_end_of_line (1, key);
  rl_vi_append_mode (1, key);
  return (0);
}

 
int
rl_vi_eof_maybe (int count, int c)
{
  return (rl_newline (1, '\n'));
}

 

 
int
rl_vi_insertion_mode (int count, int key)
{
  _rl_keymap = vi_insertion_keymap;
  _rl_vi_last_key_before_insert = key;
  if (_rl_show_mode_in_prompt)
    _rl_reset_prompt ();
  return (0);
}

int
rl_vi_insert_mode (int count, int key)
{
  rl_vi_start_inserting (key, 1, rl_arg_sign);
  return (0);
}

static void
vi_save_insert_buffer (int start, int len)
{
   
  if (len >= vi_insert_buffer_size)
    {
      vi_insert_buffer_size += (len + 32) - (len % 32);
      vi_insert_buffer = (char *)xrealloc (vi_insert_buffer, vi_insert_buffer_size);
    }
  strncpy (vi_insert_buffer, rl_line_buffer + start, len - 1);
  vi_insert_buffer[len-1] = '\0';
}

static void
_rl_vi_save_replace (void)
{
  int len, start, end;
  UNDO_LIST *up;

  up = rl_undo_list;
  if (up == 0 || up->what != UNDO_END || vi_replace_count <= 0)
    {
      if (vi_insert_buffer_size >= 1)
	vi_insert_buffer[0] = '\0';
      return;
    }
   
  end = rl_point;
  start = end - vi_replace_count + 1;
  len = vi_replace_count + 1;

  if (start < 0)
    {
      len = end + 1;
      start = 0;
    }

  vi_save_insert_buffer (start, len);  
}

static void
_rl_vi_save_insert (UNDO_LIST *up)
{
  int len, start, end;

  if (up == 0 || up->what != UNDO_INSERT)
    {
      if (vi_insert_buffer_size >= 1)
	vi_insert_buffer[0] = '\0';
      return;
    }

  start = up->start;
  end = up->end;
  len = end - start + 1;

  vi_save_insert_buffer (start, len);
}
    
void
_rl_vi_done_inserting (void)
{
  if (_rl_vi_doing_insert)
    {
       
      rl_end_undo_group ();	 
       
      _rl_vi_doing_insert = 0;
      if (_rl_vi_last_key_before_insert == 'R')
	_rl_vi_save_replace ();		 
      else
	_rl_vi_save_insert (rl_undo_list->next);
       
      if (_rl_undo_group_level > 0)
	rl_end_undo_group ();	 
    }
  else
    {
      if (rl_undo_list && (_rl_vi_last_key_before_insert == 'i' ||
			   _rl_vi_last_key_before_insert == 'a' ||
			   _rl_vi_last_key_before_insert == 'I' ||
			   _rl_vi_last_key_before_insert == 'A'))
	_rl_vi_save_insert (rl_undo_list);
       
      else if (_rl_vi_last_key_before_insert == 'C')
	rl_end_undo_group ();
    }

   
  while (_rl_undo_group_level > 0)
    rl_end_undo_group ();
}

int
rl_vi_movement_mode (int count, int key)
{
  if (rl_point > 0)
    rl_backward_char (1, key);

  _rl_keymap = vi_movement_keymap;
  _rl_vi_done_inserting ();

   
  if (RL_ISSTATE (RL_STATE_VICMDONCE) == 0)
    rl_free_undo_list ();

  if (_rl_show_mode_in_prompt)
    _rl_reset_prompt ();

  RL_SETSTATE (RL_STATE_VICMDONCE);
  return (0);
}

int
rl_vi_arg_digit (int count, int c)
{
  if (c == '0' && rl_numeric_arg == 1 && !rl_explicit_arg)
    return (rl_beg_of_line (1, c));
  else
    return (rl_digit_argument (count, c));
}

 
#if defined (HANDLE_MULTIBYTE)
static int
_rl_vi_change_mbchar_case (int count)
{
  WCHAR_T wc;
  char mb[MB_LEN_MAX+1];
  int mlen, p;
  size_t m;
  mbstate_t ps;

  memset (&ps, 0, sizeof (mbstate_t));
  if (_rl_adjust_point (rl_line_buffer, rl_point, &ps) > 0)
    count--;
  while (count-- && rl_point < rl_end)
    {
      m = MBRTOWC (&wc, rl_line_buffer + rl_point, rl_end - rl_point, &ps);
      if (MB_INVALIDCH (m))
	wc = (WCHAR_T)rl_line_buffer[rl_point];
      else if (MB_NULLWCH (m))
	wc = L'\0';
      if (iswupper (wc))
	wc = towlower (wc);
      else if (iswlower (wc))
	wc = towupper (wc);
      else
	{
	   
	  rl_forward_char (1, 0);
	  continue;
	}

       
      if (wc)
	{
	  p = rl_point;
	  mlen = WCRTOMB (mb, wc, &ps);
	  if (mlen >= 0)
	    mb[mlen] = '\0';
	  rl_begin_undo_group ();
	  rl_vi_delete (1, 0);
	  if (rl_point < p)	 
	    _rl_vi_advance_point ();
	  rl_insert_text (mb);
	  rl_end_undo_group ();
	  rl_vi_check ();
	}
      else
	rl_forward_char (1, 0);
    }

  return 0;
}
#endif

int
rl_vi_change_case (int count, int ignore)
{
  int c, p;

   
  if (rl_point >= rl_end)
    return (0);

  c = 0;
#if defined (HANDLE_MULTIBYTE)
  if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
    return (_rl_vi_change_mbchar_case (count));
#endif

  while (count-- && rl_point < rl_end)
    {
      if (_rl_uppercase_p (rl_line_buffer[rl_point]))
	c = _rl_to_lower (rl_line_buffer[rl_point]);
      else if (_rl_lowercase_p (rl_line_buffer[rl_point]))
	c = _rl_to_upper (rl_line_buffer[rl_point]);
      else
	{
	   
	  rl_forward_char (1, c);
	  continue;
	}

       
      if (c)
	{
	  p = rl_point;
	  rl_begin_undo_group ();
	  rl_vi_delete (1, c);
	  if (rl_point < p)	 
	    rl_point++;
	  _rl_insert_char (1, c);
	  rl_end_undo_group ();
	  rl_vi_check ();
	}
      else
	rl_forward_char (1, c);
    }
  return (0);
}

int
rl_vi_put (int count, int key)
{
  if (!_rl_uppercase_p (key) && (rl_point + 1 <= rl_end))
    rl_point = _rl_find_next_mbchar (rl_line_buffer, rl_point, 1, MB_FIND_NONZERO);

  while (count--)
    rl_yank (1, key);

  rl_backward_char (1, key);
  return (0);
}

 
int
rl_vi_check (void)
{
  if (rl_point && rl_point == rl_end)
    _rl_vi_backup ();
  return (0);
}

 
int
rl_vi_column (int count, int key)
{
  if (count > rl_end)
    rl_end_of_line (1, key);
  else
    {
      rl_point = 0;
      rl_point = _rl_forward_char_internal (count - 1);
    }
  return (0);
}

 
static int
_rl_vi_arg_dispatch (int c)
{
  int key;

  key = c;
  if (c >= 0 && _rl_keymap[c].type == ISFUNC && _rl_keymap[c].function == rl_universal_argument)
    {
      rl_numeric_arg *= 4;
      return 1;
    }

  c = UNMETA (c);

  if (_rl_digit_p (c))
    {
      if (rl_explicit_arg)
	rl_numeric_arg = (rl_numeric_arg * 10) + _rl_digit_value (c);
      else
	rl_numeric_arg = _rl_digit_value (c);
      rl_explicit_arg = 1;
      return 1;		 
    }
  else
    {
      rl_clear_message ();
      rl_stuff_char (key);
      return 0;		 
    }
}

 
static int
rl_digit_loop1 (void)
{
  int c, r;

  while (1)
    {
      if (_rl_arg_overflow ())
	return 1;

      c = _rl_arg_getchar ();

      r = _rl_vi_arg_dispatch (c);
      if (r <= 0)
	break;
    }

  RL_UNSETSTATE(RL_STATE_NUMERICARG);
  return (0);
}

 
static void
_rl_mvcxt_init (_rl_vimotion_cxt *m, int op, int key)
{
  m->op = op;
  m->state = m->flags = 0;
  m->ncxt = 0;
  m->numeric_arg = -1;
  m->start = rl_point;
  m->end = rl_end;
  m->key = key;
  m->motion = -1;
}

static _rl_vimotion_cxt *
_rl_mvcxt_alloc (int op, int key)
{
  _rl_vimotion_cxt *m;

  m = xmalloc (sizeof (_rl_vimotion_cxt));
  _rl_mvcxt_init (m, op, key);
  return m;
}

static void
_rl_mvcxt_dispose (_rl_vimotion_cxt *m)
{
  xfree (m);
}

static int
rl_domove_motion_callback (_rl_vimotion_cxt *m)
{
  int c;

  _rl_vi_last_motion = c = m->motion;

   
  rl_extend_line_buffer (rl_end + 1);
  rl_line_buffer[rl_end++] = ' ';
  rl_line_buffer[rl_end] = '\0';

  _rl_dispatch (c, _rl_keymap);

#if defined (READLINE_CALLBACKS)
  if (RL_ISSTATE (RL_STATE_CALLBACK))
    {
       
      if (RL_ISSTATE (RL_STATE_CHARSEARCH))
	return 0;
      else
	return (_rl_vi_domove_motion_cleanup (c, m));
    }
#endif

  return (_rl_vi_domove_motion_cleanup (c, m));
}

int
_rl_vi_domove_motion_cleanup (int c, _rl_vimotion_cxt *m)
{
  int r;

   
  rl_end = m->end;
  rl_line_buffer[rl_end] = '\0';
  _rl_fix_point (0);

   
  if (rl_mark == rl_point)
    {
       
      if (_rl_to_upper (m->key) == 'C' && _rl_vi_motion_command (c))
	return (vidomove_dispatch (m));
      RL_UNSETSTATE (RL_STATE_VIMOTION);
      return (-1);
    }

   
  if ((_rl_to_upper (c) == 'W') && rl_point < rl_end && rl_point > rl_mark &&
      !whitespace (rl_line_buffer[rl_point]))
    rl_point--;		 

   
  if (m->key == 'c' && rl_point >= rl_mark && (_rl_to_upper (c) == 'W'))
    {
       
      while (rl_point > rl_mark && whitespace (rl_line_buffer[rl_point]))
	rl_point--;

       
      if (rl_point == rl_mark)
	_rl_vi_advance_point ();
      else
	{
	   
	  if (rl_point >= 0 && rl_point < (rl_end - 1) && !whitespace (rl_line_buffer[rl_point]))
	    _rl_vi_advance_point ();
	}
    }

  if (rl_mark < rl_point)
    SWAP (rl_point, rl_mark);

#if defined (READLINE_CALLBACKS)
  if (RL_ISSTATE (RL_STATE_CALLBACK))
    (*rl_redisplay_function)();		 
#endif

  r = vidomove_dispatch (m);

  return (r);
}

#define RL_VIMOVENUMARG()	(RL_ISSTATE (RL_STATE_VIMOTION) && RL_ISSTATE (RL_STATE_NUMERICARG))

static int
rl_domove_read_callback (_rl_vimotion_cxt *m)
{
  int c, save;

  c = m->motion;

  if (member (c, vi_motion))
    {
#if defined (READLINE_CALLBACKS)
       
      if (RL_ISSTATE (RL_STATE_CALLBACK) && RL_VIMOVENUMARG())
	RL_UNSETSTATE (RL_STATE_NUMERICARG);
#endif
       
      return (rl_domove_motion_callback (m));
    }
  else if (m->key == c && (m->key == 'd' || m->key == 'y' || m->key == 'c'))
    {
      rl_mark = rl_end;
      rl_beg_of_line (1, c);
      _rl_vi_last_motion = c;
      RL_UNSETSTATE (RL_STATE_VIMOTION);
      return (vidomove_dispatch (m));
    }
#if defined (READLINE_CALLBACKS)
   
   
  else if (_rl_digit_p (c) && RL_ISSTATE (RL_STATE_CALLBACK) && RL_VIMOVENUMARG())
    {
      return (_rl_vi_arg_dispatch (c));
    }
   
  else if (_rl_digit_p (c) && RL_ISSTATE (RL_STATE_CALLBACK) && RL_ISSTATE (RL_STATE_VIMOTION) && (RL_ISSTATE (RL_STATE_NUMERICARG) == 0))
    {
      RL_SETSTATE (RL_STATE_NUMERICARG);
      return (_rl_vi_arg_dispatch (c));
    }
#endif
  else if (_rl_digit_p (c))
    {
       
      save = rl_numeric_arg;
      rl_numeric_arg = _rl_digit_value (c);
      rl_explicit_arg = 1;
      RL_SETSTATE (RL_STATE_NUMERICARG);
      rl_digit_loop1 ();
      rl_numeric_arg *= save;
      c = rl_vi_domove_getchar (m);
      if (c < 0)
	{
	  m->motion = 0;
	  return -1;
	}
      m->motion = c;
      return (rl_domove_motion_callback (m));
    }
  else
    {
      RL_UNSETSTATE (RL_STATE_VIMOTION);
      RL_UNSETSTATE (RL_STATE_NUMERICARG);
      return (1);
    }
}

static int
rl_vi_domove_getchar (_rl_vimotion_cxt *m)
{
  return (_rl_bracketed_read_key ());
}

#if defined (READLINE_CALLBACKS)
int
_rl_vi_domove_callback (_rl_vimotion_cxt *m)
{
  int c, r;

  m->motion = c = rl_vi_domove_getchar (m);
  if (c < 0)
    return 1;		 
  r = rl_domove_read_callback (m);

  return ((r == 0) ? r : 1);	 
}
#endif

 
int
rl_vi_domove (int x, int *ignore)
{
  int r;
  _rl_vimotion_cxt *m;

  m = _rl_vimvcxt;
  *ignore = m->motion = rl_vi_domove_getchar (m);

  if (m->motion < 0)
    {
      m->motion = 0;
      return -1;
    }

  return (rl_domove_read_callback (m));
}

static int
vi_delete_dispatch (_rl_vimotion_cxt *m)
{
   
  if (((strchr (" l|h^0bBFT`", m->motion) == 0) && (rl_point >= m->start)) &&
      (rl_mark < rl_end))
    INCREMENT_POS (rl_mark);

  rl_kill_text (rl_point, rl_mark);
  return (0);
}

int
rl_vi_delete_to (int count, int key)
{
  int c, r;
  _rl_vimotion_cxt *savecxt;

  savecxt = 0;
  if (_rl_vi_redoing)
    {
      savecxt = _rl_vimvcxt;
      _rl_vimvcxt = _rl_mvcxt_alloc (VIM_DELETE, key);
    }
  else if (_rl_vimvcxt)
    _rl_mvcxt_init (_rl_vimvcxt, VIM_DELETE, key);
  else
    _rl_vimvcxt = _rl_mvcxt_alloc (VIM_DELETE, key);

  _rl_vimvcxt->start = rl_point;

  rl_mark = rl_point;
  if (_rl_uppercase_p (key))
    {
      _rl_vimvcxt->motion = '$';
      r = rl_domove_motion_callback (_rl_vimvcxt);
    }
  else if (_rl_vi_redoing && _rl_vi_last_motion != 'd')	 
    {
      _rl_vimvcxt->motion = _rl_vi_last_motion;
      r = rl_domove_motion_callback (_rl_vimvcxt);
    }
  else if (_rl_vi_redoing)		 
    {
      _rl_vimvcxt->motion = _rl_vi_last_motion;
      rl_mark = rl_end;
      rl_beg_of_line (1, key);
      RL_UNSETSTATE (RL_STATE_VIMOTION);
      r = vidomove_dispatch (_rl_vimvcxt);
    }
#if defined (READLINE_CALLBACKS)
  else if (RL_ISSTATE (RL_STATE_CALLBACK))
    {
      RL_SETSTATE (RL_STATE_VIMOTION);
      return (0);
    }
#endif
  else
    r = rl_vi_domove (key, &c);

  if (r < 0)
    {
      rl_ding ();
      r = -1;
    }

  _rl_mvcxt_dispose (_rl_vimvcxt);
  _rl_vimvcxt = savecxt;

  return r;
}

static int
vi_change_dispatch (_rl_vimotion_cxt *m)
{
   
  if (((strchr (" l|hwW^0bBFT`", m->motion) == 0) && (rl_point >= m->start)) &&
      (rl_mark < rl_end))
    INCREMENT_POS (rl_mark);

   
  if ((_rl_to_upper (m->motion) == 'W') && rl_point < m->start)
    rl_point = m->start;

  if (_rl_vi_redoing)
    {
      if (vi_insert_buffer && *vi_insert_buffer)
	rl_begin_undo_group ();
      rl_delete_text (rl_point, rl_mark);
      if (vi_insert_buffer && *vi_insert_buffer)
	{
	  rl_insert_text (vi_insert_buffer);
	  rl_end_undo_group ();
	}
    }
  else
    {
      rl_begin_undo_group ();		 
      rl_kill_text (rl_point, rl_mark);
       
      if (_rl_uppercase_p (m->key) == 0)
	_rl_vi_doing_insert = 1;
       
      rl_vi_start_inserting (m->key, rl_numeric_arg, rl_arg_sign);
    }

  return (0);
}

int
rl_vi_change_to (int count, int key)
{
  int c, r;
  _rl_vimotion_cxt *savecxt;

  savecxt = 0;
  if (_rl_vi_redoing)
    {
      savecxt = _rl_vimvcxt;
      _rl_vimvcxt = _rl_mvcxt_alloc (VIM_CHANGE, key);
    }
  else if (_rl_vimvcxt)
    _rl_mvcxt_init (_rl_vimvcxt, VIM_CHANGE, key);
  else
    _rl_vimvcxt = _rl_mvcxt_alloc (VIM_CHANGE, key);
  _rl_vimvcxt->start = rl_point;

  rl_mark = rl_point;
  if (_rl_uppercase_p (key))
    {
      _rl_vimvcxt->motion = '$';
      r = rl_domove_motion_callback (_rl_vimvcxt);
    }
  else if (_rl_vi_redoing && _rl_vi_last_motion != 'c')	 
    {
      _rl_vimvcxt->motion = _rl_vi_last_motion;
      r = rl_domove_motion_callback (_rl_vimvcxt);
    }
  else if (_rl_vi_redoing)		 
    {
      _rl_vimvcxt->motion = _rl_vi_last_motion;
      rl_mark = rl_end;
      rl_beg_of_line (1, key);
      RL_UNSETSTATE (RL_STATE_VIMOTION);
      r = vidomove_dispatch (_rl_vimvcxt);
    }
#if defined (READLINE_CALLBACKS)
  else if (RL_ISSTATE (RL_STATE_CALLBACK))
    {
      RL_SETSTATE (RL_STATE_VIMOTION);
      return (0);
    }
#endif
  else
    r = rl_vi_domove (key, &c);

  if (r < 0)
    {
      rl_ding ();
      r = -1;	 
    }

  _rl_mvcxt_dispose (_rl_vimvcxt);
  _rl_vimvcxt = savecxt;

  return r;
}

static int
vi_yank_dispatch (_rl_vimotion_cxt *m)
{
   
  if (((strchr (" l|h^0%bBFT`", m->motion) == 0) && (rl_point >= m->start)) &&
      (rl_mark < rl_end))
    INCREMENT_POS (rl_mark);

  rl_begin_undo_group ();
  rl_kill_text (rl_point, rl_mark);
  rl_end_undo_group ();
  rl_do_undo ();
  rl_point = m->start;

  _rl_fix_point (1);

  return (0);
}

int
rl_vi_yank_to (int count, int key)
{
  int c, r;
  _rl_vimotion_cxt *savecxt;

  savecxt = 0;
  if (_rl_vi_redoing)
    {
      savecxt = _rl_vimvcxt;
      _rl_vimvcxt = _rl_mvcxt_alloc (VIM_YANK, key);
    }
  else if (_rl_vimvcxt)
    _rl_mvcxt_init (_rl_vimvcxt, VIM_YANK, key);
  else
    _rl_vimvcxt = _rl_mvcxt_alloc (VIM_YANK, key);
  _rl_vimvcxt->start = rl_point;

  rl_mark = rl_point;
  if (_rl_uppercase_p (key))
    {
      _rl_vimvcxt->motion = '$';
      r = rl_domove_motion_callback (_rl_vimvcxt);
    }
  else if (_rl_vi_redoing && _rl_vi_last_motion != 'y')	 
    {
      _rl_vimvcxt->motion = _rl_vi_last_motion;
      r = rl_domove_motion_callback (_rl_vimvcxt);
    }
  else if (_rl_vi_redoing)			 
    {
      _rl_vimvcxt->motion = _rl_vi_last_motion;
      rl_mark = rl_end;
      rl_beg_of_line (1, key);
      RL_UNSETSTATE (RL_STATE_VIMOTION);
      r = vidomove_dispatch (_rl_vimvcxt);
    }
#if defined (READLINE_CALLBACKS)
  else if (RL_ISSTATE (RL_STATE_CALLBACK))
    {
      RL_SETSTATE (RL_STATE_VIMOTION);
      return (0);
    }
#endif
  else
    r = rl_vi_domove (key, &c);

  if (r < 0)
    {
      rl_ding ();
      r = -1;
    }

  _rl_mvcxt_dispose (_rl_vimvcxt);
  _rl_vimvcxt = savecxt;

  return r;
}

static int
vidomove_dispatch (_rl_vimotion_cxt *m)
{
  int r;

  switch (m->op)
    {
    case VIM_DELETE:
      r = vi_delete_dispatch (m);
      break;
    case VIM_CHANGE:
      r = vi_change_dispatch (m);
      break;
    case VIM_YANK:
      r = vi_yank_dispatch (m);
      break;
    default:
      _rl_errmsg ("vidomove_dispatch: unknown operator %d", m->op);
      r = 1;
      break;
    }

  RL_UNSETSTATE (RL_STATE_VIMOTION);
  return r;
}

int
rl_vi_rubout (int count, int key)
{
  int opoint;

  if (count < 0)
    return (rl_vi_delete (-count, key));

  if (rl_point == 0)
    {
      rl_ding ();
      return 1;
    }

  opoint = rl_point;
  if (count > 1 && MB_CUR_MAX > 1 && rl_byte_oriented == 0)
    rl_backward_char (count, key);
  else if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
    rl_point = _rl_find_prev_mbchar (rl_line_buffer, rl_point, MB_FIND_NONZERO);
  else
    rl_point -= count;

  if (rl_point < 0)
    rl_point = 0;

  rl_kill_text (rl_point, opoint);
  
  return (0);
}

int
rl_vi_delete (int count, int key)
{
  int end;

  if (count < 0)
    return (rl_vi_rubout (-count, key));

  if (rl_end == 0)
    {
      rl_ding ();
      return 1;
    }

  if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
    end = _rl_find_next_mbchar (rl_line_buffer, rl_point, count, MB_FIND_NONZERO);
  else
    end = rl_point + count;

  if (end >= rl_end)
    end = rl_end;

  rl_kill_text (rl_point, end);
  
  if (rl_point > 0 && rl_point == rl_end)
    rl_backward_char (1, key);

  return (0);
}

 

#define vi_unix_word_boundary(c)	(whitespace(c) || ispunct(c))

int
rl_vi_unix_word_rubout (int count, int key)
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
	   

	   
	  if (rl_point > 0 && (rl_line_buffer[rl_point] == 0) &&
		whitespace (rl_line_buffer[rl_point - 1]))
	    while (--rl_point > 0 && whitespace (rl_line_buffer[rl_point]))
	      ;

	   
	  if (rl_point > 0 && (vi_unix_word_boundary (rl_line_buffer[rl_point]) == 0) &&
		vi_unix_word_boundary (rl_line_buffer[rl_point - 1]))
	    rl_point--;

	   
	  if (rl_point > 0 && vi_unix_word_boundary (rl_line_buffer[rl_point]))
	    while (rl_point && vi_unix_word_boundary (rl_line_buffer[rl_point - 1]))
	      rl_point--;
	  else if (rl_point > 0 && vi_unix_word_boundary (rl_line_buffer[rl_point]) == 0)
	    while (rl_point > 0 && (vi_unix_word_boundary (rl_line_buffer[rl_point - 1]) == 0))
	      _rl_vi_backup_point ();
	}

      rl_kill_text (orig_point, rl_point);
    }

  return 0;
}


int
rl_vi_back_to_indent (int count, int key)
{
  rl_beg_of_line (1, key);
  while (rl_point < rl_end && whitespace (rl_line_buffer[rl_point]))
    rl_point++;
  return (0);
}

int
rl_vi_first_print (int count, int key)
{
  return (rl_vi_back_to_indent (1, key));
}

static int _rl_cs_dir, _rl_cs_orig_dir;

#if defined (READLINE_CALLBACKS)
static int
_rl_vi_callback_char_search (_rl_callback_generic_arg *data)
{
  int c;
#if defined (HANDLE_MULTIBYTE)
  c = _rl_vi_last_search_mblen = _rl_read_mbchar (_rl_vi_last_search_mbchar, MB_LEN_MAX);
#else
  RL_SETSTATE(RL_STATE_MOREINPUT);
  c = rl_read_key ();
  RL_UNSETSTATE(RL_STATE_MOREINPUT);
#endif

  if (c <= 0)
    {
      RL_UNSETSTATE (RL_STATE_CHARSEARCH);
      return -1;
    }

#if !defined (HANDLE_MULTIBYTE)
  _rl_vi_last_search_char = c;
#endif

  _rl_callback_func = 0;
  _rl_want_redisplay = 1;
  RL_UNSETSTATE (RL_STATE_CHARSEARCH);

#if defined (HANDLE_MULTIBYTE)
  return (_rl_char_search_internal (data->count, _rl_cs_dir, _rl_vi_last_search_mbchar, _rl_vi_last_search_mblen));
#else
  return (_rl_char_search_internal (data->count, _rl_cs_dir, _rl_vi_last_search_char));
#endif  
}
#endif

int
rl_vi_char_search (int count, int key)
{
  int c;
#if defined (HANDLE_MULTIBYTE)
  static char *target;
  static int tlen;
#else
  static char target;
#endif

  if (key == ';' || key == ',')
    {
      if (_rl_cs_orig_dir == 0)
	return 1;
#if defined (HANDLE_MULTIBYTE)
      if (_rl_vi_last_search_mblen == 0)
	return 1;
#else
      if (_rl_vi_last_search_char == 0)
	return 1;
#endif
      _rl_cs_dir = (key == ';') ? _rl_cs_orig_dir : -_rl_cs_orig_dir;
    }
  else
    {
      switch (key)
	{
	case 't':
	  _rl_cs_orig_dir = _rl_cs_dir = FTO;
	  break;

	case 'T':
	  _rl_cs_orig_dir = _rl_cs_dir = BTO;
	  break;

	case 'f':
	  _rl_cs_orig_dir = _rl_cs_dir = FFIND;
	  break;

	case 'F':
	  _rl_cs_orig_dir = _rl_cs_dir = BFIND;
	  break;
	}

      if (_rl_vi_redoing)
	{
	   
	}
#if defined (READLINE_CALLBACKS)
      else if (RL_ISSTATE (RL_STATE_CALLBACK))
	{
	  _rl_callback_data = _rl_callback_data_alloc (count);
	  _rl_callback_data->i1 = _rl_cs_dir;
	  _rl_callback_data->i2 = key;
	  _rl_callback_func = _rl_vi_callback_char_search;
	  RL_SETSTATE (RL_STATE_CHARSEARCH);
	  return (0);
	}
#endif
      else
	{
#if defined (HANDLE_MULTIBYTE)
	  c = _rl_read_mbchar (_rl_vi_last_search_mbchar, MB_LEN_MAX);
	  if (c <= 0)
	    return -1;
	  _rl_vi_last_search_mblen = c;
#else
	  RL_SETSTATE(RL_STATE_MOREINPUT);
	  c = rl_read_key ();
	  RL_UNSETSTATE(RL_STATE_MOREINPUT);
	  if (c < 0)
	    return -1;
	  _rl_vi_last_search_char = c;
#endif
	}
    }

#if defined (HANDLE_MULTIBYTE)
  target = _rl_vi_last_search_mbchar;
  tlen = _rl_vi_last_search_mblen;
#else
  target = _rl_vi_last_search_char;
#endif

#if defined (HANDLE_MULTIBYTE)
  return (_rl_char_search_internal (count, _rl_cs_dir, target, tlen));
#else
  return (_rl_char_search_internal (count, _rl_cs_dir, target));
#endif
}

 
int
rl_vi_match (int ignore, int key)
{
  int count = 1, brack, pos, tmp, pre;

  pos = rl_point;
  if ((brack = rl_vi_bracktype (rl_line_buffer[rl_point])) == 0)
    {
      if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
	{
	  while ((brack = rl_vi_bracktype (rl_line_buffer[rl_point])) == 0)
	    {
	      pre = rl_point;
	      rl_forward_char (1, key);
	      if (pre == rl_point)
		break;
	    }
	}
      else
	while ((brack = rl_vi_bracktype (rl_line_buffer[rl_point])) == 0 &&
		rl_point < rl_end - 1)
	  rl_forward_char (1, key);

      if (brack <= 0)
	{
	  rl_point = pos;
	  rl_ding ();
	  return 1;
	}
    }

  pos = rl_point;

  if (brack < 0)
    {
      while (count)
	{
	  tmp = pos;
	  if (MB_CUR_MAX == 1 || rl_byte_oriented)
	    pos--;
	  else
	    {
	      pos = _rl_find_prev_mbchar (rl_line_buffer, pos, MB_FIND_ANY);
	      if (tmp == pos)
		pos--;
	    }
	  if (pos >= 0)
	    {
	      int b = rl_vi_bracktype (rl_line_buffer[pos]);
	      if (b == -brack)
		count--;
	      else if (b == brack)
		count++;
	    }
	  else
	    {
	      rl_ding ();
	      return 1;
	    }
	}
    }
  else
    {			 
      while (count)
	{
	  if (MB_CUR_MAX == 1 || rl_byte_oriented)
	    pos++;
	  else
	    pos = _rl_find_next_mbchar (rl_line_buffer, pos, 1, MB_FIND_ANY);

	  if (pos < rl_end)
	    {
	      int b = rl_vi_bracktype (rl_line_buffer[pos]);
	      if (b == -brack)
		count--;
	      else if (b == brack)
		count++;
	    }
	  else
	    {
	      rl_ding ();
	      return 1;
	    }
	}
    }
  rl_point = pos;
  return (0);
}

int
rl_vi_bracktype (int c)
{
  switch (c)
    {
    case '(': return  1;
    case ')': return -1;
    case '[': return  2;
    case ']': return -2;
    case '{': return  3;
    case '}': return -3;
    default:  return  0;
    }
}

static int
_rl_vi_change_char (int count, int c, char *mb)
{
  int p;

  if (c == '\033' || c == CTRL ('C'))
    return -1;

  rl_begin_undo_group ();
  while (count-- && rl_point < rl_end)
    {
      p = rl_point;
      rl_vi_delete (1, c);
      if (rl_point < p)		 
	_rl_vi_append_forward (c);
#if defined (HANDLE_MULTIBYTE)
      if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
	rl_insert_text (mb);
      else
#endif
	_rl_insert_char (1, c);
    }

   
  rl_backward_char (1, c);

  rl_end_undo_group ();

  return (0);
}

static int
_rl_vi_callback_getchar (char *mb, int mlen)
{
  return (_rl_bracketed_read_mbstring (mb, mlen));
}

#if defined (READLINE_CALLBACKS)
static int
_rl_vi_callback_change_char (_rl_callback_generic_arg *data)
{
  int c;
  char mb[MB_LEN_MAX+1];

  c = _rl_vi_callback_getchar (mb, MB_LEN_MAX);
  if (c < 0)
    return -1;

#if defined (HANDLE_MULTIBYTE)
  if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
    strncpy (_rl_vi_last_replacement, mb, MB_LEN_MAX);
  else
#endif
    _rl_vi_last_replacement[0] = c;
  _rl_vi_last_replacement[MB_LEN_MAX] = '\0';	 

  _rl_callback_func = 0;
  _rl_want_redisplay = 1;

  return (_rl_vi_change_char (data->count, c, mb));
}
#endif

int
rl_vi_change_char (int count, int key)
{
  int c;
  char mb[MB_LEN_MAX+1];

  if (_rl_vi_redoing)
    {
      strncpy (mb, _rl_vi_last_replacement, MB_LEN_MAX);
      c = (unsigned char)_rl_vi_last_replacement[0];	 
      mb[MB_LEN_MAX] = '\0';
    }
#if defined (READLINE_CALLBACKS)
  else if (RL_ISSTATE (RL_STATE_CALLBACK))
    {
      _rl_callback_data = _rl_callback_data_alloc (count);
      _rl_callback_func = _rl_vi_callback_change_char;
      return (0);
    }
#endif
  else
    {
      c = _rl_vi_callback_getchar (mb, MB_LEN_MAX);
      if (c < 0)
	return -1;
#ifdef HANDLE_MULTIBYTE
      if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
	strncpy (_rl_vi_last_replacement, mb, MB_LEN_MAX);
      else
#endif
	_rl_vi_last_replacement[0] = c;
      _rl_vi_last_replacement[MB_LEN_MAX] = '\0';	       
    }

  if (c < 0)
    return -1;

  return (_rl_vi_change_char (count, c, mb));
}

int
rl_vi_subst (int count, int key)
{
   
  if (_rl_vi_redoing == 0)
    rl_stuff_char ((key == 'S') ? 'c' : 'l');	 

  return (rl_vi_change_to (count, 'c'));
}

int
rl_vi_overstrike (int count, int key)
{
  if (_rl_vi_doing_insert == 0)
    {
      _rl_vi_doing_insert = 1;
      rl_begin_undo_group ();
    }

  if (count > 0)
    {
      if (_rl_overwrite_char (count, key) != 0)
	return (1);
      vi_replace_count += count;
    }

  return (0);
}

int
rl_vi_overstrike_delete (int count, int key)
{
  int i, s;

  for (i = 0; i < count; i++)
    {
      if (vi_replace_count == 0)
	{
	  rl_ding ();
	  break;
	}
      s = rl_point;

      if (rl_do_undo ())
	vi_replace_count--;		 

      if (rl_point == s)
	rl_backward_char (1, key);
    }

  if (vi_replace_count == 0 && _rl_vi_doing_insert)
    {
      rl_end_undo_group ();
      rl_do_undo ();
      _rl_vi_doing_insert = 0;
    }
  return (0);
}

static int
rl_vi_overstrike_kill_line (int count, int key)
{
  int r, end;

  end = rl_end;
  r = rl_unix_line_discard (count, key);
  vi_replace_count -= end - rl_end;
  return r;
}

static int
rl_vi_overstrike_kill_word (int count, int key)
{
  int r, end;

  end = rl_end;
  r = rl_vi_unix_word_rubout (count, key);
  vi_replace_count -= end - rl_end;
  return r;
}

static int
rl_vi_overstrike_yank (int count, int key)
{
  int r, end;

  end = rl_end;
  r = rl_yank (count, key);
  vi_replace_count += rl_end - end;
  return r;
}

 
static int
rl_vi_overstrike_bracketed_paste (int count, int key)
{
  int r;
  char *pbuf;
  size_t pblen;

  pbuf = _rl_bracketed_text (&pblen);
  if (pblen == 0)
    {
      xfree (pbuf);
      return 0;
    }
  r = pblen;
  while (--r >= 0)
    _rl_unget_char ((unsigned char)pbuf[r]);
  xfree (pbuf);

  while (_rl_pushed_input_available ())
    {
      key = rl_read_key ();
      r = rl_vi_overstrike (1, key);
    }

  return r;
}

int
rl_vi_replace (int count, int key)
{
  int i;

  vi_replace_count = 0;

  if (vi_replace_map == 0)
    {
      vi_replace_map = rl_make_bare_keymap ();

      for (i = 0; i < ' '; i++)
	if (vi_insertion_keymap[i].type == ISFUNC)
	  vi_replace_map[i].function = vi_insertion_keymap[i].function;

      for (i = ' '; i < KEYMAP_SIZE; i++)
	vi_replace_map[i].function = rl_vi_overstrike;

      vi_replace_map[RUBOUT].function = rl_vi_overstrike_delete;

       
      vi_replace_map[ESC].function = rl_vi_movement_mode;
      vi_replace_map[RETURN].function = rl_newline;
      vi_replace_map[NEWLINE].function = rl_newline;

       
      if (vi_insertion_keymap[CTRL ('H')].type == ISFUNC &&
	  vi_insertion_keymap[CTRL ('H')].function == rl_rubout)
	vi_replace_map[CTRL ('H')].function = rl_vi_overstrike_delete;

       
      if (vi_insertion_keymap[CTRL ('U')].type == ISFUNC &&
	  vi_insertion_keymap[CTRL ('U')].function == rl_unix_line_discard)
	vi_replace_map[CTRL ('U')].function = rl_vi_overstrike_kill_line;

       
      if (vi_insertion_keymap[CTRL ('W')].type == ISFUNC &&
	  vi_insertion_keymap[CTRL ('W')].function == rl_vi_unix_word_rubout)
	vi_replace_map[CTRL ('W')].function = rl_vi_overstrike_kill_word;

       
      if (vi_insertion_keymap[CTRL ('Y')].type == ISFUNC &&
	  vi_insertion_keymap[CTRL ('Y')].function == rl_yank)
	vi_replace_map[CTRL ('Y')].function = rl_vi_overstrike_yank;

       
      vi_replace_map[ANYOTHERKEY].type = ISFUNC;
      vi_replace_map[ANYOTHERKEY].function = (rl_command_func_t *)NULL;
    }

  rl_vi_start_inserting (key, 1, rl_arg_sign);

  _rl_vi_last_key_before_insert = 'R';	 
  _rl_keymap = vi_replace_map;

  if (_rl_enable_bracketed_paste)
    rl_bind_keyseq_if_unbound (BRACK_PASTE_PREF, rl_vi_overstrike_bracketed_paste);

  return (0);
}

#if 0
 
int
rl_vi_possible_completions (void)
{
  int save_pos = rl_point;

  if (rl_line_buffer[rl_point] != ' ' && rl_line_buffer[rl_point] != ';')
    {
      while (rl_point < rl_end && rl_line_buffer[rl_point] != ' ' &&
	     rl_line_buffer[rl_point] != ';')
	_rl_vi_advance_point ();
    }
  else if (rl_line_buffer[rl_point - 1] == ';')
    {
      rl_ding ();
      return (0);
    }

  rl_possible_completions ();
  rl_point = save_pos;

  return (0);
}
#endif

 
static int
_rl_vi_set_mark (void)
{
  int ch;

  RL_SETSTATE(RL_STATE_MOREINPUT);
  ch = rl_read_key ();
  RL_UNSETSTATE(RL_STATE_MOREINPUT);

  if (ch < 0 || ch < 'a' || ch > 'z')	 
    {
      rl_ding ();
      return 1;
    }
  ch -= 'a';
  vi_mark_chars[ch] = rl_point;
  return 0;
}

#if defined (READLINE_CALLBACKS)
static int
_rl_vi_callback_set_mark (_rl_callback_generic_arg *data)
{
  _rl_callback_func = 0;
  _rl_want_redisplay = 1;

  return (_rl_vi_set_mark ());
}
#endif

int
rl_vi_set_mark (int count, int key)
{
#if defined (READLINE_CALLBACKS)
  if (RL_ISSTATE (RL_STATE_CALLBACK))
    {
      _rl_callback_data = 0;
      _rl_callback_func = _rl_vi_callback_set_mark;
      return (0);
    }
#endif

  return (_rl_vi_set_mark ());
}

static int
_rl_vi_goto_mark (void)
{
  int ch;

  RL_SETSTATE(RL_STATE_MOREINPUT);
  ch = rl_read_key ();
  RL_UNSETSTATE(RL_STATE_MOREINPUT);

  if (ch == '`')
    {
      rl_point = rl_mark;
      _rl_fix_point (1);
      return 0;
    }
  else if (ch < 0 || ch < 'a' || ch > 'z')	 
    {
      rl_ding ();
      return 1;
    }

  ch -= 'a';
  if (vi_mark_chars[ch] == -1)
    {
      rl_ding ();
      return 1;
    }
  rl_point = vi_mark_chars[ch];
  _rl_fix_point (1);
  return 0;
}

#if defined (READLINE_CALLBACKS)
static int
_rl_vi_callback_goto_mark (_rl_callback_generic_arg *data)
{
  _rl_callback_func = 0;
  _rl_want_redisplay = 1;

  return (_rl_vi_goto_mark ());
}
#endif

int
rl_vi_goto_mark (int count, int key)
{
#if defined (READLINE_CALLBACKS)
  if (RL_ISSTATE (RL_STATE_CALLBACK))
    {
      _rl_callback_data = 0;
      _rl_callback_func = _rl_vi_callback_goto_mark;
      return (0);
    }
#endif

  return (_rl_vi_goto_mark ());
}
#endif  
