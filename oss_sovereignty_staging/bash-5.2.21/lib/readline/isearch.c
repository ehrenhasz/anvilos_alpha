 

 
 
 
 
 

 

#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <sys/types.h>

#include <stdio.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif

#include "rldefs.h"
#include "rlmbutil.h"

#include "readline.h"
#include "history.h"

#include "rlprivate.h"
#include "xmalloc.h"

 
char *_rl_isearch_terminators = (char *)NULL;

_rl_search_cxt *_rl_iscxt = 0;

static int rl_search_history (int, int);

static _rl_search_cxt *_rl_isearch_init (int);
static void _rl_isearch_fini (_rl_search_cxt *);

 
 

 
static char *last_isearch_string;
static int last_isearch_string_len;

static char * const default_isearch_terminators = "\033\012";

_rl_search_cxt *
_rl_scxt_alloc (int type, int flags)
{
  _rl_search_cxt *cxt;

  cxt = (_rl_search_cxt *)xmalloc (sizeof (_rl_search_cxt));

  cxt->type = type;
  cxt->sflags = flags;

  cxt->search_string = 0;
  cxt->search_string_size = cxt->search_string_index = 0;

  cxt->lines = 0;
  cxt->allocated_line = 0;
  cxt->hlen = cxt->hindex = 0;

  cxt->save_point = rl_point;
  cxt->save_mark = rl_mark;
  cxt->save_line = where_history ();
  cxt->last_found_line = cxt->save_line;
  cxt->prev_line_found = 0;

  cxt->save_undo_list = 0;

  cxt->keymap = _rl_keymap;
  cxt->okeymap = _rl_keymap;

  cxt->history_pos = 0;
  cxt->direction = 0;

  cxt->prevc = cxt->lastc = 0;

  cxt->sline = 0;
  cxt->sline_len = cxt->sline_index = 0;

  cxt->search_terminators = 0;

  return cxt;
}

void
_rl_scxt_dispose (_rl_search_cxt *cxt, int flags)
{
  FREE (cxt->search_string);
  FREE (cxt->allocated_line);
  FREE (cxt->lines);

  xfree (cxt);
}

 
int
rl_reverse_search_history (int sign, int key)
{
  return (rl_search_history (-sign, key));
}

 
int
rl_forward_search_history (int sign, int key)
{
  return (rl_search_history (sign, key));
}

 
static void
rl_display_search (char *search_string, int flags, int where)
{
  char *message;
  int msglen, searchlen;

  searchlen = (search_string && *search_string) ? strlen (search_string) : 0;

  message = (char *)xmalloc (searchlen + 64);
  msglen = 0;

#if defined (NOTDEF)
  if (where != -1)
    {
      sprintf (message, "[%d]", where + history_base);
      msglen = strlen (message);
    }
#endif  

  message[msglen++] = '(';

  if (flags & SF_FAILED)
    {
      strcpy (message + msglen, "failed ");
      msglen += 7;
    }

  if (flags & SF_REVERSE)
    {
      strcpy (message + msglen, "reverse-");
      msglen += 8;
    }

  strcpy (message + msglen, "i-search)`");
  msglen += 10;

  if (search_string && *search_string)
    {
      strcpy (message + msglen, search_string);
      msglen += searchlen;
    }
  else
    _rl_optimize_redisplay ();

  strcpy (message + msglen, "': ");

  rl_message ("%s", message);
  xfree (message);
#if 0
   
  (*rl_redisplay_function) ();
#endif
}

static _rl_search_cxt *
_rl_isearch_init (int direction)
{
  _rl_search_cxt *cxt;
  register int i;
  HIST_ENTRY **hlist;

  cxt = _rl_scxt_alloc (RL_SEARCH_ISEARCH, 0);
  if (direction < 0)
    cxt->sflags |= SF_REVERSE;

  cxt->search_terminators = _rl_isearch_terminators ? _rl_isearch_terminators
						: default_isearch_terminators;

   
  hlist = history_list ();
  rl_maybe_replace_line ();
  i = 0;
  if (hlist)
    for (i = 0; hlist[i]; i++);

   
  cxt->lines = (char **)xmalloc ((1 + (cxt->hlen = i)) * sizeof (char *));
  for (i = 0; i < cxt->hlen; i++)
    cxt->lines[i] = hlist[i]->line;

  if (_rl_saved_line_for_history)
    cxt->lines[i] = _rl_saved_line_for_history->line;
  else
    {
       
      cxt->allocated_line = (char *)xmalloc (1 + strlen (rl_line_buffer));
      strcpy (cxt->allocated_line, &rl_line_buffer[0]);
      cxt->lines[i] = cxt->allocated_line;
    }

  cxt->hlen++;

   
  cxt->history_pos = cxt->save_line;

  rl_save_prompt ();

   
  cxt->search_string = (char *)xmalloc (cxt->search_string_size = 128);
  cxt->search_string[cxt->search_string_index = 0] = '\0';

   
  cxt->direction = (direction >= 0) ? 1 : -1;

  cxt->sline = rl_line_buffer;
  cxt->sline_len = strlen (cxt->sline);
  cxt->sline_index = rl_point;

  _rl_iscxt = cxt;		 

   
  _rl_init_executing_keyseq ();

  return cxt;
}

static void
_rl_isearch_fini (_rl_search_cxt *cxt)
{
   
  rl_replace_line (cxt->lines[cxt->save_line], 0);

  rl_restore_prompt ();

   
  FREE (last_isearch_string);
  last_isearch_string = cxt->search_string;
  last_isearch_string_len = cxt->search_string_index;
  cxt->search_string = 0;
  cxt->search_string_size = 0;
  cxt->search_string_index = 0;

  if (cxt->last_found_line < cxt->save_line)
    rl_get_previous_history (cxt->save_line - cxt->last_found_line, 0);
  else
    rl_get_next_history (cxt->last_found_line - cxt->save_line, 0);

   
  if (cxt->sline_index < 0)
    {
      if (cxt->last_found_line == cxt->save_line)
	cxt->sline_index = cxt->save_point;
      else
	cxt->sline_index = strlen (rl_line_buffer);
      rl_mark = cxt->save_mark;
      rl_deactivate_mark ();
    }

  rl_point = cxt->sline_index;
   
  _rl_fix_point (0);
  rl_deactivate_mark ();

 
  rl_clear_message ();
}

 
int
_rl_search_getchar (_rl_search_cxt *cxt)
{
  int c;

   
  RL_SETSTATE(RL_STATE_MOREINPUT);
  c = cxt->lastc = rl_read_key ();
  RL_UNSETSTATE(RL_STATE_MOREINPUT);

#if defined (HANDLE_MULTIBYTE)
   
  if (c >= 0 && MB_CUR_MAX > 1 && rl_byte_oriented == 0)
    c = cxt->lastc = _rl_read_mbstring (cxt->lastc, cxt->mb, MB_LEN_MAX);
#endif

  RL_CHECK_SIGNALS ();
  return c;
}

#define ENDSRCH_CHAR(c) \
  ((CTRL_CHAR (c) || META_CHAR (c) || (c) == RUBOUT) && ((c) != CTRL ('G')))

 
int
_rl_isearch_dispatch (_rl_search_cxt *cxt, int c)
{
  int n, wstart, wlen, limit, cval, incr;
  char *paste;
  size_t pastelen;
  int j;
  rl_command_func_t *f;

  f = (rl_command_func_t *)NULL;

  if (c < 0)
    {
      cxt->sflags |= SF_FAILED;
      cxt->history_pos = cxt->last_found_line;
      return -1;
    }

  _rl_add_executing_keyseq (c);

   
  if (_rl_enable_bracketed_paste && c == ESC && strchr (cxt->search_terminators, c) && (n = _rl_nchars_available ()) > (BRACK_PASTE_SLEN-1))
    {
      j = _rl_read_bracketed_paste_prefix (c);
      if (j == 1)
	{
	  cxt->lastc = -7;		 
	  goto opcode_dispatch;	
        }
      else if (_rl_pushed_input_available ())	 
	c = cxt->lastc = rl_read_key ();
      else
	c = cxt->lastc;			 
    }

   
  if (c >= 0 && cxt->keymap[c].type == ISKMAP && strchr (cxt->search_terminators, cxt->lastc) == 0)
    {
       
      if (_rl_keyseq_timeout > 0 &&
	    RL_ISSTATE (RL_STATE_CALLBACK) == 0 &&
	    RL_ISSTATE (RL_STATE_INPUTPENDING) == 0 &&
	    _rl_pushed_input_available () == 0 &&
	    ((Keymap)(cxt->keymap[c].function))[ANYOTHERKEY].function &&
	    _rl_input_queued (_rl_keyseq_timeout*1000) == 0)
	goto add_character;

      cxt->okeymap = cxt->keymap;
      cxt->keymap = FUNCTION_TO_KEYMAP (cxt->keymap, c);
      cxt->sflags |= SF_CHGKMAP;
       
      cxt->prevc = c;
#if defined (HANDLE_MULTIBYTE)
      if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
	{
	  if (cxt->mb[1] == 0)
	    {
	      cxt->pmb[0] = c;		 
	      cxt->pmb[1] = '\0';
	    }
	  else
	    memcpy (cxt->pmb, cxt->mb, sizeof (cxt->pmb));
	}
#endif
      return 1;
    }

add_character:

   
  if (c >= 0 && cxt->keymap[c].type == ISFUNC)
    {
       
#if defined (HANDLE_MULTIBYTE)
      if (MB_CUR_MAX > 1 && rl_byte_oriented == 0 && cxt->mb[1])
	f = rl_function_of_keyseq (cxt->mb, cxt->keymap, (int *)NULL);
      else
#endif
	{
	  f = cxt->keymap[c].function;
	  if (f == rl_do_lowercase_version)
	    f = cxt->keymap[_rl_to_lower (c)].function;
	}

      if (f == rl_reverse_search_history)
	cxt->lastc = (cxt->sflags & SF_REVERSE) ? -1 : -2;
      else if (f == rl_forward_search_history)
	cxt->lastc = (cxt->sflags & SF_REVERSE) ? -2 : -1;
      else if (f == rl_rubout)
	cxt->lastc = -3;
      else if (c == CTRL ('G') || f == rl_abort)
	cxt->lastc = -4;
      else if (c == CTRL ('W') || f == rl_unix_word_rubout)	 
	cxt->lastc = -5;
      else if (c == CTRL ('Y') || f == rl_yank)	 
	cxt->lastc = -6;
      else if (f == rl_bracketed_paste_begin)
	cxt->lastc = -7;
    }

   
  if (cxt->sflags & SF_CHGKMAP)
    {
      cxt->keymap = cxt->okeymap;
      cxt->sflags &= ~SF_CHGKMAP;
       
       
      if (cxt->lastc > 0 && ENDSRCH_CHAR (cxt->prevc))
	{
	  rl_stuff_char (cxt->lastc);
	  rl_execute_next (cxt->prevc);
	   
	  return (0);
	}
       
      else if (cxt->lastc > 0 && cxt->prevc > 0 &&
	       cxt->keymap[cxt->prevc].type == ISKMAP &&
	       (f == 0 || f == rl_insert))
	{
	   
	   
	  rl_execute_next (cxt->lastc);
	   
	  cxt->lastc = cxt->prevc;
#if defined (HANDLE_MULTIBYTE)
	   
	  if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
	    {  
	      if (cxt->pmb[1] == 0)	  
		{
		  cxt->mb[0] = cxt->lastc;	 
		  cxt->mb[1] = '\0';
		}
	      else
		memcpy (cxt->mb, cxt->pmb, sizeof (cxt->mb));
	    }
#endif
	  cxt->prevc = 0;	  
	}
      else if (cxt->lastc > 0 && cxt->prevc > 0 && f && f != rl_insert)
	{
	  _rl_term_executing_keyseq ();		 

	  _rl_pending_command.map = cxt->keymap;
	  _rl_pending_command.count = 1;	 
	  _rl_pending_command.key = cxt->lastc;
	  _rl_pending_command.func = f;
	  _rl_command_to_execute = &_rl_pending_command;

	  return (0);
	}
    }

   
  if (cxt->lastc > 0 && strchr (cxt->search_terminators, cxt->lastc))
    {
       
      if (cxt->lastc == ESC && (_rl_pushed_input_available () || _rl_input_available ()))
	rl_execute_next (ESC);
      return (0);
    }

#if defined (HANDLE_MULTIBYTE)
  if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
    {
      if (cxt->lastc >= 0 && (cxt->mb[0] && cxt->mb[1] == '\0') && ENDSRCH_CHAR (cxt->lastc))
	{
	   
	  rl_execute_next (cxt->lastc);
	  return (0);
	}
    }
  else
#endif
    if (cxt->lastc >= 0 && ENDSRCH_CHAR (cxt->lastc))
      {
	 
	rl_execute_next (cxt->lastc);
	return (0);
      }

  _rl_init_executing_keyseq ();

opcode_dispatch:
   
  switch (cxt->lastc)
    {
     
    case -1:
      if (cxt->search_string_index == 0)
	{
	  if (last_isearch_string)
	    {
	      cxt->search_string_size = 64 + last_isearch_string_len;
	      cxt->search_string = (char *)xrealloc (cxt->search_string, cxt->search_string_size);
	      strcpy (cxt->search_string, last_isearch_string);
	      cxt->search_string_index = last_isearch_string_len;
	      rl_display_search (cxt->search_string, cxt->sflags, -1);
	      break;
	    }
	   
	  return (1);
	}
      else if ((cxt->sflags & SF_REVERSE) && cxt->sline_index >= 0)
	cxt->sline_index--;
      else if (cxt->sline_index != cxt->sline_len)
	cxt->sline_index++;
      else
	rl_ding ();
      break;

     
    case -2:
      cxt->direction = -cxt->direction;
      if (cxt->direction < 0)
	cxt->sflags |= SF_REVERSE;
      else
	cxt->sflags &= ~SF_REVERSE;
      break;

     
    case -3:	 
       
      if (cxt->search_string_index == 0)
	rl_ding ();
      else if (MB_CUR_MAX == 1 || rl_byte_oriented)
	cxt->search_string[--cxt->search_string_index] = '\0';
      else
	{
	  wstart = _rl_find_prev_mbchar (cxt->search_string, cxt->search_string_index, MB_FIND_NONZERO);
	  if (wstart >= 0)
	    cxt->search_string[cxt->search_string_index = wstart] = '\0';
	  else
	    cxt->search_string[cxt->search_string_index = 0] = '\0';
	}

      if (cxt->search_string_index == 0)
	rl_ding ();

      break;

    case -4:	 
      rl_replace_line (cxt->lines[cxt->save_line], 0);
      rl_point = cxt->save_point;
      rl_mark = cxt->save_mark;
      rl_deactivate_mark ();
      rl_restore_prompt();
      rl_clear_message ();

      _rl_fix_point (1);	 
      return -1;

    case -5:	 
       
      wstart = rl_point + cxt->search_string_index;
      if (wstart >= rl_end)
	{
	  rl_ding ();
	  break;
	}

       
      cval = _rl_char_value (rl_line_buffer, wstart);
      if (_rl_walphabetic (cval) == 0)
	{
	  rl_ding ();
	  break;
	}
      n = MB_NEXTCHAR (rl_line_buffer, wstart, 1, MB_FIND_NONZERO);;
      while (n < rl_end)
	{
	  cval = _rl_char_value (rl_line_buffer, n);
	  if (_rl_walphabetic (cval) == 0)
	    break;
	  n = MB_NEXTCHAR (rl_line_buffer, n, 1, MB_FIND_NONZERO);;
	}
      wlen = n - wstart + 1;
      if (cxt->search_string_index + wlen + 1 >= cxt->search_string_size)
	{
	  cxt->search_string_size += wlen + 1;
	  cxt->search_string = (char *)xrealloc (cxt->search_string, cxt->search_string_size);
	}
      for (; wstart < n; wstart++)
	cxt->search_string[cxt->search_string_index++] = rl_line_buffer[wstart];
      cxt->search_string[cxt->search_string_index] = '\0';
      break;

    case -6:	 
       
      wstart = rl_point + cxt->search_string_index;
      if (wstart >= rl_end)
	{
	  rl_ding ();
	  break;
	}
      n = rl_end - wstart + 1;
      if (cxt->search_string_index + n + 1 >= cxt->search_string_size)
	{
	  cxt->search_string_size += n + 1;
	  cxt->search_string = (char *)xrealloc (cxt->search_string, cxt->search_string_size);
	}
      for (n = wstart; n < rl_end; n++)
	cxt->search_string[cxt->search_string_index++] = rl_line_buffer[n];
      cxt->search_string[cxt->search_string_index] = '\0';
      break;

    case -7:	 
      paste = _rl_bracketed_text (&pastelen);
      if (paste == 0 || *paste == 0)
	{
	  xfree (paste);
	  break;
	}
      if (_rl_enable_active_region)
	rl_activate_mark ();
      if (cxt->search_string_index + pastelen + 1 >= cxt->search_string_size)
	{
	  cxt->search_string_size += pastelen + 2;
	  cxt->search_string = (char *)xrealloc (cxt->search_string, cxt->search_string_size);
	}
      memcpy (cxt->search_string + cxt->search_string_index, paste, pastelen);
      cxt->search_string_index += pastelen;
      cxt->search_string[cxt->search_string_index] = '\0';
      xfree (paste);
      break;

     
    default:
#if defined (HANDLE_MULTIBYTE)
      wlen = (cxt->mb[0] == 0 || cxt->mb[1] == 0) ? 1 : RL_STRLEN (cxt->mb);
#else
      wlen = 1;
#endif
      if (cxt->search_string_index + wlen + 1 >= cxt->search_string_size)
	{
	  cxt->search_string_size += 128;	 
	  cxt->search_string = (char *)xrealloc (cxt->search_string, cxt->search_string_size);
	}
#if defined (HANDLE_MULTIBYTE)
      if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
	{
	  int j;

	  if (cxt->mb[0] == 0 || cxt->mb[1] == 0)
	    cxt->search_string[cxt->search_string_index++] = cxt->mb[0];
	  else
	    for (j = 0; j < wlen; )
	      cxt->search_string[cxt->search_string_index++] = cxt->mb[j++];
	}
      else
#endif
	cxt->search_string[cxt->search_string_index++] = cxt->lastc;	 
      cxt->search_string[cxt->search_string_index] = '\0';
      break;
    }

  for (cxt->sflags &= ~(SF_FOUND|SF_FAILED);; )
    {
      if (cxt->search_string_index == 0)
	{
	  cxt->sflags |= SF_FAILED;
	  break;
	}

      limit = cxt->sline_len - cxt->search_string_index + 1;

       
      while ((cxt->sflags & SF_REVERSE) ? (cxt->sline_index >= 0) : (cxt->sline_index < limit))
	{
	  if (STREQN (cxt->search_string, cxt->sline + cxt->sline_index, cxt->search_string_index))
	    {
	      cxt->sflags |= SF_FOUND;
	      break;
	    }
	  else
	    cxt->sline_index += cxt->direction;

	  if (cxt->sline_index < 0)
	    {
	      cxt->sline_index = 0;
	      break;
	    }
	}
      if (cxt->sflags & SF_FOUND)
	break;

       
      do
	{
	   
	  cxt->history_pos += cxt->direction;

	   
	  if ((cxt->sflags & SF_REVERSE) ? (cxt->history_pos < 0) : (cxt->history_pos == cxt->hlen))
	    {
	      cxt->sflags |= SF_FAILED;
	      break;
	    }

	   
	  cxt->sline = cxt->lines[cxt->history_pos];
	  cxt->sline_len = strlen (cxt->sline);
	}
      while ((cxt->prev_line_found && STREQ (cxt->prev_line_found, cxt->lines[cxt->history_pos])) ||
	     (cxt->search_string_index > cxt->sline_len));

      if (cxt->sflags & SF_FAILED)
	{
	   
	  if (cxt->sline_index < 0)
	    cxt->sline_index = 0;
	  break;
	}

       
      cxt->sline_index = (cxt->sflags & SF_REVERSE) ? cxt->sline_len - cxt->search_string_index : 0;
    }

   
  cxt->keymap = cxt->okeymap = _rl_keymap;

  if (cxt->sflags & SF_FAILED)
    {
       
      rl_ding ();
      cxt->history_pos = cxt->last_found_line;
      rl_deactivate_mark ();
      rl_display_search (cxt->search_string, cxt->sflags, (cxt->history_pos == cxt->save_line) ? -1 : cxt->history_pos);
      return 1;
    }

   
  if (cxt->sflags & SF_FOUND)
    {
      cxt->prev_line_found = cxt->lines[cxt->history_pos];
      rl_replace_line (cxt->lines[cxt->history_pos], 0);
      if (_rl_enable_active_region)
	rl_activate_mark ();	
      rl_point = cxt->sline_index;
      if (rl_mark_active_p () && cxt->search_string_index > 0)
	rl_mark = rl_point + cxt->search_string_index;
      cxt->last_found_line = cxt->history_pos;
      rl_display_search (cxt->search_string, cxt->sflags, (cxt->history_pos == cxt->save_line) ? -1 : cxt->history_pos);
    }

  return 1;
}

int
_rl_isearch_cleanup (_rl_search_cxt *cxt, int r)
{
  if (r >= 0)
    _rl_isearch_fini (cxt);
  _rl_scxt_dispose (cxt, 0);
  _rl_iscxt = 0;

  RL_UNSETSTATE(RL_STATE_ISEARCH);

  return (r != 0);
}

 
static int
rl_search_history (int direction, int invoking_key)
{
  _rl_search_cxt *cxt;		 
  int c, r;

  RL_SETSTATE(RL_STATE_ISEARCH);
  cxt = _rl_isearch_init (direction);

  rl_display_search (cxt->search_string, cxt->sflags, -1);

   
  if (RL_ISSTATE (RL_STATE_CALLBACK))
    return (0);

  r = -1;
  for (;;)
    {
      c = _rl_search_getchar (cxt);
       
      r = _rl_isearch_dispatch (cxt, cxt->lastc);
      if (r <= 0)
        break;
    }

   
  return (_rl_isearch_cleanup (cxt, r));
}

#if defined (READLINE_CALLBACKS)
 
int
_rl_isearch_callback (_rl_search_cxt *cxt)
{
  int c, r;

  c = _rl_search_getchar (cxt);
   
  r = _rl_isearch_dispatch (cxt, cxt->lastc);

  return (r <= 0) ? _rl_isearch_cleanup (cxt, r) : 0;
}
#endif
