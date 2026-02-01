 

 

#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <sys/types.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif  

#include "posixstat.h"

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif  

#include <stdio.h>

#ifdef __MSDOS__
#  include <pc.h>
#endif

 
#include "rldefs.h"
#include "rlmbutil.h"

 
#include "tcap.h"

 
#include "readline.h"
#include "history.h"

#include "rlprivate.h"
#include "xmalloc.h"

#if !defined (strchr) && !defined (__STDC__)
extern char *strchr (), *strrchr ();
#endif  

static void putc_face (int, int, char *);
static void puts_face (const char *, const char *, int);
static void norm_face (char *, int);

static void update_line (char *, char *, char *, char *, int, int, int, int);
static void space_to_eol (int);
static void delete_chars (int);
static void insert_some_chars (char *, int, int);
static void open_some_spaces (int);
static void cr (void);
static void redraw_prompt (char *);
static void _rl_move_cursor_relative (int, const char *, const char *);

 
#define PMT_MULTILINE	0x01

static char *expand_prompt (char *, int, int *, int *, int *, int *);

#define DEFAULT_LINE_BUFFER_SIZE	1024

 
struct line_state
  {
    char *line;
    char *lface;
    int *lbreaks;
    int lbsize;
#if defined (HANDLE_MULTIBYTE)
    int wbsize;
    int *wrapped_line;
#endif
  };

 
static struct line_state line_state_array[2];
static struct line_state *line_state_visible = &line_state_array[0];
static struct line_state *line_state_invisible = &line_state_array[1];
static int line_structures_initialized = 0;

 
#define inv_lbreaks	(line_state_invisible->lbreaks)
#define inv_lbsize	(line_state_invisible->lbsize)
#define vis_lbreaks	(line_state_visible->lbreaks)
#define vis_lbsize	(line_state_visible->lbsize)

#define visible_line	(line_state_visible->line)
#define vis_face	(line_state_visible->lface)
#define invisible_line	(line_state_invisible->line)
#define inv_face	(line_state_invisible->lface)

#if defined (HANDLE_MULTIBYTE)
static int _rl_col_width (const char *, int, int, int);
#else
#  define _rl_col_width(l, s, e, f)	(((e) <= (s)) ? 0 : (e) - (s))
#endif

 
#define CR_FASTER(new, cur) (((new) + 1) < ((cur) - (new)))

 
 
#define PROMPT_ENDING_INDEX \
  ((MB_CUR_MAX > 1 && rl_byte_oriented == 0) ? prompt_physical_chars : prompt_last_invisible+1)

#define FACE_NORMAL	'0'
#define FACE_STANDOUT	'1'
#define FACE_INVALID	((char)1)
  
 
 
 
 
 

 

 

 

 
rl_voidfunc_t *rl_redisplay_function = rl_redisplay;

 
 
int rl_display_fixed = 0;

 
char *rl_display_prompt = (char *)NULL;

 
char *_rl_emacs_mode_str;
int _rl_emacs_modestr_len;

char *_rl_vi_ins_mode_str;
int _rl_vi_ins_modestr_len;

char *_rl_vi_cmd_mode_str;
int _rl_vi_cmd_modestr_len;

 

 
int _rl_suppress_redisplay = 0;
int _rl_want_redisplay = 0;

 
 
int _rl_last_c_pos = 0;
int _rl_last_v_pos = 0;

 
int _rl_vis_botlin = 0;

static int _rl_quick_redisplay = 0;

 
static int cpos_adjusted;

 
static int cpos_buffer_position;

 
static int displaying_prompt_first_line;
 
static int prompt_multibyte_chars;

static int _rl_inv_botlin = 0;

 
 
static int last_lmargin;

 
static char *msg_buf = 0;
static int msg_bufsiz = 0;

 
static int forced_display;

 
static int line_size  = 0;

 
static int horizontal_scrolling_autoset = 0;	 

 

static char *local_prompt, *local_prompt_prefix;
static int local_prompt_len;
static int prompt_prefix_length;
 
static int prompt_visible_length;

 
static int visible_wrap_offset;

 
static int wrap_offset;

 
static int prompt_last_invisible;

 
static int visible_first_line_len;

 
static int prompt_invis_chars_first_line;

static int prompt_last_screen_line;

static int prompt_physical_chars;

 
static int *local_prompt_newlines;

 
static int modmark;

static int line_totbytes;

 

 

static char *saved_local_prompt;
static char *saved_local_prefix;
static int *saved_local_prompt_newlines;

static int saved_last_invisible;
static int saved_visible_length;
static int saved_prefix_length;
static int saved_local_length;
static int saved_invis_chars_first_line;
static int saved_physical_chars;

 

static char *
prompt_modestr (int *lenp)
{
  if (rl_editing_mode == emacs_mode)
    {
      if (lenp)
	*lenp = _rl_emacs_mode_str ? _rl_emacs_modestr_len : RL_EMACS_MODESTR_DEFLEN;
      return _rl_emacs_mode_str ? _rl_emacs_mode_str : RL_EMACS_MODESTR_DEFAULT;
    }
  else if (_rl_keymap == vi_insertion_keymap)
    {
      if (lenp)
	*lenp = _rl_vi_ins_mode_str ? _rl_vi_ins_modestr_len : RL_VI_INS_MODESTR_DEFLEN;
      return _rl_vi_ins_mode_str ? _rl_vi_ins_mode_str : RL_VI_INS_MODESTR_DEFAULT;		 
    }
  else
    {
      if (lenp)
	*lenp = _rl_vi_cmd_mode_str ? _rl_vi_cmd_modestr_len : RL_VI_CMD_MODESTR_DEFLEN;
      return _rl_vi_cmd_mode_str ? _rl_vi_cmd_mode_str : RL_VI_CMD_MODESTR_DEFAULT;		 
    }
}

 

 	

 

 
#define APPROX_DIV(n, d)	(((n) < (d)) ? 1 : ((n) / (d)) + 1)

static char *
expand_prompt (char *pmt, int flags, int *lp, int *lip, int *niflp, int *vlp)
{
  char *r, *ret, *p, *igstart, *nprompt, *ms;
  int l, rl, last, ignoring, ninvis, invfl, invflset, ind, pind, physchars;
  int mlen, newlines, newlines_guess, bound, can_add_invis;
  int mb_cur_max;

   
  ms = (((pmt == rl_prompt) ^ (flags & PMT_MULTILINE)) && _rl_show_mode_in_prompt) ? prompt_modestr (&mlen) : 0;
  if (ms)
    {
      l = strlen (pmt);
      nprompt = (char *)xmalloc (l + mlen + 1);
      memcpy (nprompt, ms, mlen);
      strcpy (nprompt + mlen, pmt);
    }
  else
    nprompt = pmt;

  can_add_invis = 0;
  mb_cur_max = MB_CUR_MAX;

  if (_rl_screenwidth == 0)
    _rl_get_screen_size (0, 0);	 

   
  if ((mb_cur_max <= 1 || rl_byte_oriented) && strchr (nprompt, RL_PROMPT_START_IGNORE) == 0)
    {
      l = strlen (nprompt);
      if (l < (_rl_screenwidth > 0 ? _rl_screenwidth : 80))
        {
	  r = (nprompt == pmt) ? savestring (pmt) : nprompt;
	  if (lp)
	    *lp = l;
	  if (lip)
	    *lip = 0;
	  if (niflp)
	    *niflp = 0;
	  if (vlp)
	    *vlp = l;

	  local_prompt_newlines = (int *) xrealloc (local_prompt_newlines, sizeof (int) * 2);
	  local_prompt_newlines[0] = 0;
	  local_prompt_newlines[1] = -1;

	  return r;
        }
    }

  l = strlen (nprompt);			 
  r = ret = (char *)xmalloc (l + 1);

   
  newlines_guess = (_rl_screenwidth > 0) ? APPROX_DIV(l,  _rl_screenwidth) : APPROX_DIV(l, 80);
  local_prompt_newlines = (int *) xrealloc (local_prompt_newlines, sizeof (int) * (newlines_guess + 1));
  local_prompt_newlines[newlines = 0] = 0;
  for (rl = 1; rl <= newlines_guess; rl++)
    local_prompt_newlines[rl] = -1;

  rl = physchars = 0;	 
  invfl = 0;		 
  invflset = 0;		 
  igstart = 0;		 

  for (ignoring = last = ninvis = 0, p = nprompt; p && *p; p++)
    {
       
      if (ignoring == 0 && *p == RL_PROMPT_START_IGNORE)		 
	{
	  ignoring = 1;
	  igstart = p;
	  continue;
	}
      else if (ignoring && *p == RL_PROMPT_END_IGNORE)
	{
	  ignoring = 0;
	   
	  if (can_add_invis)
	    {
	      local_prompt_newlines[newlines] = r - ret;
	       
	      if (invflset && newlines == 1)
		invfl = ninvis;
	    }
	  if (p != (igstart + 1))
	    last = r - ret - 1;
	  continue;
	}
      else
	{
#if defined (HANDLE_MULTIBYTE)
	  if (mb_cur_max > 1 && rl_byte_oriented == 0)
	    {
	      pind = p - nprompt;
	      ind = _rl_find_next_mbchar (nprompt, pind, 1, MB_FIND_NONZERO);
	      l = ind - pind;
	      while (l--)
	        *r++ = *p++;
	      if (!ignoring)
		{
		   
		  rl += ind - pind;
		  physchars += _rl_col_width (nprompt, pind, ind, 0);
		}
	      else
		ninvis += ind - pind;
	      p--;			 
	    }
	  else
#endif
	    {
	      *r++ = *p;
	      if (!ignoring)
		{
		  rl++;			 
		  physchars++;
		}
	      else
		ninvis++;		 
	    }

	  if (invflset == 0 && physchars >= _rl_screenwidth)
	    {
	      invfl = ninvis;
	      invflset = 1;
	    }

	  if (physchars >= (bound = (newlines + 1) * _rl_screenwidth) && local_prompt_newlines[newlines+1] == -1)
	    {
	      int new;
	      if (physchars > bound)		 
		{
#if defined (HANDLE_MULTIBYTE)
		  *r = '\0';	 
		  if (mb_cur_max > 1 && rl_byte_oriented == 0)
		    new = _rl_find_prev_mbchar (ret, r - ret, MB_FIND_ANY);
		  else
#endif
		    new = r - ret - (physchars - bound);	 
		}
	      else
	        new = r - ret;
	      local_prompt_newlines[++newlines] = new;
	    }

	   
	  if (ignoring == 0)
	    can_add_invis = (physchars == bound); 
	}
    }

  if (rl <= _rl_screenwidth)
    invfl = ninvis;

  *r = '\0';
  if (lp)
    *lp = rl;
  if (lip)
    *lip = last;
  if (niflp)
    *niflp = invfl;
  if  (vlp)
    *vlp = physchars;

  if (nprompt != pmt)
    xfree (nprompt);

  return ret;
}

 
char *
_rl_strip_prompt (char *pmt)
{
  char *ret;

  ret = expand_prompt (pmt, 0, (int *)NULL, (int *)NULL, (int *)NULL, (int *)NULL);
  return ret;
}

void
_rl_reset_prompt (void)
{
  rl_visible_prompt_length = rl_expand_prompt (rl_prompt);
}

 
int
rl_expand_prompt (char *prompt)
{
  char *p, *t;
  int c;

   
  FREE (local_prompt);
  FREE (local_prompt_prefix);

  local_prompt = local_prompt_prefix = (char *)0;
  local_prompt_len = 0;
  prompt_last_invisible = prompt_invis_chars_first_line = 0;
  prompt_visible_length = prompt_physical_chars = 0;

  if (prompt == 0 || *prompt == 0)
    return (0);

  p = strrchr (prompt, '\n');
  if (p == 0)
    {
       
      local_prompt = expand_prompt (prompt, 0, &prompt_visible_length,
					       &prompt_last_invisible,
					       &prompt_invis_chars_first_line,
					       &prompt_physical_chars);
      local_prompt_prefix = (char *)0;
      local_prompt_len = local_prompt ? strlen (local_prompt) : 0;
      return (prompt_visible_length);
    }
  else
    {
       
      t = ++p;
      c = *t; *t = '\0';
       
      local_prompt_prefix = expand_prompt (prompt, PMT_MULTILINE,
						   &prompt_prefix_length,
						   (int *)NULL,
						   (int *)NULL,
						   (int *)NULL);
      *t = c;

      local_prompt = expand_prompt (p, PMT_MULTILINE,
				       &prompt_visible_length,
				       &prompt_last_invisible,
				       &prompt_invis_chars_first_line,
				       &prompt_physical_chars);
      local_prompt_len = local_prompt ? strlen (local_prompt) : 0;
      return (prompt_prefix_length);
    }
}

 
static void
realloc_line (int minsize)
{
  int minimum_size;
  int newsize, delta;

  minimum_size = DEFAULT_LINE_BUFFER_SIZE;
  if (minsize < minimum_size)
    minsize = minimum_size;
  if (minsize <= _rl_screenwidth)	 
    minsize = _rl_screenwidth + 1;
  if (line_size >= minsize)
    return;

  newsize = minimum_size;
  while (newsize < minsize)
    newsize *= 2;

  visible_line = (char *)xrealloc (visible_line, newsize);
  vis_face = (char *)xrealloc (vis_face, newsize);

  invisible_line = (char *)xrealloc (invisible_line, newsize);
  inv_face = (char *)xrealloc (inv_face, newsize);

  delta = newsize - line_size;  
  memset (visible_line + line_size, 0, delta);
  memset (vis_face + line_size, FACE_NORMAL, delta);
  memset (invisible_line + line_size, 1, delta);
  memset (inv_face + line_size, FACE_INVALID, delta);

  line_size = newsize;
}

 
static void
init_line_structures (int minsize)
{
  if (invisible_line == 0)	 
    {
      if (line_size > minsize)
	minsize = line_size;
    }
   realloc_line (minsize); 

  if (vis_lbreaks == 0)
    {
       
      inv_lbsize = vis_lbsize = 256;

#if defined (HANDLE_MULTIBYTE)
      line_state_visible->wbsize = vis_lbsize;
      line_state_visible->wrapped_line = (int *)xmalloc (line_state_visible->wbsize * sizeof (int));

      line_state_invisible->wbsize = inv_lbsize;
      line_state_invisible->wrapped_line = (int *)xmalloc (line_state_invisible->wbsize * sizeof (int));
#endif

      inv_lbreaks = (int *)xmalloc (inv_lbsize * sizeof (int));
      vis_lbreaks = (int *)xmalloc (vis_lbsize * sizeof (int));
      inv_lbreaks[0] = vis_lbreaks[0] = 0;
    }

  line_structures_initialized = 1;
}

 
static void		 
invis_addc (int *outp, char c, char face)
{
  realloc_line (*outp + 1);
  invisible_line[*outp] = c;
  inv_face[*outp] = face;
  *outp += 1;
}

static void
invis_adds (int *outp, const char *str, int n, char face)
{
  int i;

  for (i = 0; i < n; i++)
    invis_addc (outp, str[i], face);
}

static void
invis_nul (int *outp)
{
  invis_addc (outp, '\0', 0);
  *outp -= 1;
}

static void
set_active_region (int *beg, int *end)
{
  if (rl_point >= 0 && rl_point <= rl_end && rl_mark >= 0 && rl_mark <= rl_end)
    {
      *beg = (rl_mark < rl_point) ? rl_mark : rl_point;
      *end = (rl_mark < rl_point) ? rl_point : rl_mark;
    }
}

 
void
_rl_optimize_redisplay (void)
{
  if (_rl_vis_botlin == 0)
    _rl_quick_redisplay = 1;
}  

 
void
rl_redisplay (void)
{
  int in, out, c, linenum, cursor_linenum;
  int inv_botlin, lb_botlin, lb_linenum, o_cpos;
  int newlines, lpos, temp, n0, num, prompt_lines_estimate;
  char *prompt_this_line;
  char cur_face;
  int hl_begin, hl_end;
  int mb_cur_max = MB_CUR_MAX;
#if defined (HANDLE_MULTIBYTE)
  WCHAR_T wc;
  size_t wc_bytes;
  int wc_width;
  mbstate_t ps;
  int _rl_wrapped_multicolumn = 0;
#endif

  if (_rl_echoing_p == 0)
    return;

   
  _rl_block_sigint ();  
  RL_SETSTATE (RL_STATE_REDISPLAYING);

  cur_face = FACE_NORMAL;
   
  hl_begin = hl_end = -1;

  if (rl_mark_active_p ())
    set_active_region (&hl_begin, &hl_end);

  if (!rl_display_prompt)
    rl_display_prompt = "";

  if (line_structures_initialized == 0)
    {
      init_line_structures (0);
      rl_on_new_line ();
    }
  else if (line_size <= _rl_screenwidth)
    init_line_structures (_rl_screenwidth + 1);

   
  if (_rl_screenheight <= 1)
    {
      if (_rl_horizontal_scroll_mode == 0)
	 horizontal_scrolling_autoset = 1;
      _rl_horizontal_scroll_mode = 1;
    }
  else if (horizontal_scrolling_autoset)
    _rl_horizontal_scroll_mode = 0;

   
  cpos_buffer_position = -1;

  prompt_multibyte_chars = prompt_visible_length - prompt_physical_chars;

  out = inv_botlin = 0;

   
  modmark = 0;
  if (_rl_mark_modified_lines && current_history () && rl_undo_list)
    {
      invis_addc (&out, '*', cur_face);
      invis_nul (&out);
      modmark = 1;
    }

   
  if (visible_line[0] != invisible_line[0])
    rl_display_fixed = 0;

   
   
  if (rl_display_prompt == rl_prompt || local_prompt)
    {
      if (local_prompt_prefix && forced_display)
	_rl_output_some_chars (local_prompt_prefix, strlen (local_prompt_prefix));

      if (local_prompt_len > 0)
	invis_adds (&out, local_prompt, local_prompt_len, cur_face);
      invis_nul (&out);
      wrap_offset = local_prompt_len - prompt_visible_length;
    }
  else
    {
      int pmtlen;
      prompt_this_line = strrchr (rl_display_prompt, '\n');
      if (!prompt_this_line)
	prompt_this_line = rl_display_prompt;
      else
	{
	  prompt_this_line++;
	  pmtlen = prompt_this_line - rl_display_prompt;	 
	  if (forced_display)
	    {
	      _rl_output_some_chars (rl_display_prompt, pmtlen);
	       
	      if (pmtlen < 2 || prompt_this_line[-2] != '\r')
		cr ();
	    }
	}

      prompt_physical_chars = pmtlen = strlen (prompt_this_line);	 
      invis_adds (&out, prompt_this_line, pmtlen, cur_face);
      invis_nul (&out);
      wrap_offset = prompt_invis_chars_first_line = 0;
    }

#if defined (HANDLE_MULTIBYTE)
#define CHECK_INV_LBREAKS() \
      do { \
	if (newlines >= (inv_lbsize - 2)) \
	  { \
	    inv_lbsize *= 2; \
	    inv_lbreaks = (int *)xrealloc (inv_lbreaks, inv_lbsize * sizeof (int)); \
	  } \
	if (newlines >= (line_state_invisible->wbsize - 2)) \
	  { \
	    line_state_invisible->wbsize *= 2; \
	    line_state_invisible->wrapped_line = (int *)xrealloc (line_state_invisible->wrapped_line, line_state_invisible->wbsize * sizeof(int)); \
	  } \
      } while (0)
#else
#define CHECK_INV_LBREAKS() \
      do { \
	if (newlines >= (inv_lbsize - 2)) \
	  { \
	    inv_lbsize *= 2; \
	    inv_lbreaks = (int *)xrealloc (inv_lbreaks, inv_lbsize * sizeof (int)); \
	  } \
      } while (0)
#endif  

#if defined (HANDLE_MULTIBYTE)	  
#define CHECK_LPOS() \
      do { \
	lpos++; \
	if (lpos >= _rl_screenwidth) \
	  { \
	    if (newlines >= (inv_lbsize - 2)) \
	      { \
		inv_lbsize *= 2; \
		inv_lbreaks = (int *)xrealloc (inv_lbreaks, inv_lbsize * sizeof (int)); \
	      } \
	    inv_lbreaks[++newlines] = out; \
	    if (newlines >= (line_state_invisible->wbsize - 2)) \
	      { \
		line_state_invisible->wbsize *= 2; \
		line_state_invisible->wrapped_line = (int *)xrealloc (line_state_invisible->wrapped_line, line_state_invisible->wbsize * sizeof(int)); \
	      } \
	    line_state_invisible->wrapped_line[newlines] = _rl_wrapped_multicolumn; \
	    lpos = 0; \
	  } \
      } while (0)
#else
#define CHECK_LPOS() \
      do { \
	lpos++; \
	if (lpos >= _rl_screenwidth) \
	  { \
	    if (newlines >= (inv_lbsize - 2)) \
	      { \
		inv_lbsize *= 2; \
		inv_lbreaks = (int *)xrealloc (inv_lbreaks, inv_lbsize * sizeof (int)); \
	      } \
	    inv_lbreaks[++newlines] = out; \
	    lpos = 0; \
	  } \
      } while (0)
#endif

   
  inv_lbreaks[newlines = 0] = 0;
   
  lpos = prompt_physical_chars + modmark;

#if defined (HANDLE_MULTIBYTE)
  memset (line_state_invisible->wrapped_line, 0, line_state_invisible->wbsize * sizeof (int));
  num = 0;
#endif

   

   
  prompt_lines_estimate = lpos / _rl_screenwidth;

   
  if (lpos >= _rl_screenwidth)
    {
      temp = 0;

       
      while (local_prompt_newlines[newlines+1] != -1)
	{
	  temp = local_prompt_newlines[newlines+1];
	  inv_lbreaks[++newlines] = temp;
	}  

       
      if (mb_cur_max > 1 && rl_byte_oriented == 0 && prompt_multibyte_chars > 0)
        lpos = _rl_col_width (local_prompt, temp, local_prompt_len, 1) - (wrap_offset - prompt_invis_chars_first_line);
      else
        lpos -= (_rl_screenwidth * newlines);
    }

  prompt_last_screen_line = newlines;

   
  lb_linenum = 0;
#if defined (HANDLE_MULTIBYTE)
  in = 0;
  if (mb_cur_max > 1 && rl_byte_oriented == 0)
    {
      memset (&ps, 0, sizeof (mbstate_t));
      if (_rl_utf8locale && UTF8_SINGLEBYTE(rl_line_buffer[0]))
	{
	  wc = (WCHAR_T)rl_line_buffer[0];
	  wc_bytes = 1;
	}
      else
	wc_bytes = MBRTOWC (&wc, rl_line_buffer, rl_end, &ps);
    }
  else
    wc_bytes = 1;
  while (in < rl_end)
#else
  for (in = 0; in < rl_end; in++)
#endif
    {
      if (in == hl_begin)
	cur_face = FACE_STANDOUT;
      else if (in == hl_end)
	cur_face = FACE_NORMAL;

      c = (unsigned char)rl_line_buffer[in];

#if defined (HANDLE_MULTIBYTE)
      if (mb_cur_max > 1 && rl_byte_oriented == 0)
	{
	  if (MB_INVALIDCH (wc_bytes))
	    {
	       
	      wc_bytes = 1;
	       
	      wc_width = 1;
	      memset (&ps, 0, sizeof (mbstate_t));
	    }
	  else if (MB_NULLWCH (wc_bytes))
	    break;			 
	  else
	    {
	      temp = WCWIDTH (wc);
	      wc_width = (temp >= 0) ? temp : 1;
	    }
	}
#endif

      if (in == rl_point)
	{
	  cpos_buffer_position = out;
	  lb_linenum = newlines;
	}

#if defined (HANDLE_MULTIBYTE)
      if (META_CHAR (c) && _rl_output_meta_chars == 0)	 
#else
      if (META_CHAR (c))
#endif
	{
	  if (_rl_output_meta_chars == 0)
	    {
	      char obuf[5];
	      int olen;

	      olen = sprintf (obuf, "\\%o", c);
	  
	      if (lpos + olen >= _rl_screenwidth)
		{
		  temp = _rl_screenwidth - lpos;
		  CHECK_INV_LBREAKS ();
		  inv_lbreaks[++newlines] = out + temp;
#if defined (HANDLE_MULTIBYTE)
		  line_state_invisible->wrapped_line[newlines] = _rl_wrapped_multicolumn;
#endif
		  lpos = olen - temp;
		}
	      else
		lpos += olen;

	      for (temp = 0; temp < olen; temp++)
		{
		  invis_addc (&out, obuf[temp], cur_face);
		  CHECK_LPOS ();
		}
	    }
	  else
	    {
	      invis_addc (&out, c, cur_face);
	      CHECK_LPOS();
	    }
	}
#if defined (DISPLAY_TABS)
      else if (c == '\t')
	{
	  register int newout;

	  newout = out + 8 - lpos % 8;
	  temp = newout - out;
	  if (lpos + temp >= _rl_screenwidth)
	    {
	      register int temp2;
	      temp2 = _rl_screenwidth - lpos;
	      CHECK_INV_LBREAKS ();
	      inv_lbreaks[++newlines] = out + temp2;
#if defined (HANDLE_MULTIBYTE)
	      line_state_invisible->wrapped_line[newlines] = _rl_wrapped_multicolumn;
#endif
	      lpos = temp - temp2;
	      while (out < newout)
		invis_addc (&out, ' ', cur_face);
	    }
	  else
	    {
	      while (out < newout)
		invis_addc (&out, ' ', cur_face);
	      lpos += temp;
	    }
	}
#endif
      else if (c == '\n' && _rl_horizontal_scroll_mode == 0 && _rl_term_up && *_rl_term_up)
	{
	  invis_addc (&out, '\0', cur_face);
	  CHECK_INV_LBREAKS ();
	  inv_lbreaks[++newlines] = out;
#if defined (HANDLE_MULTIBYTE)
	  line_state_invisible->wrapped_line[newlines] = _rl_wrapped_multicolumn;
#endif
	  lpos = 0;
	}
      else if (CTRL_CHAR (c) || c == RUBOUT)
	{
	  invis_addc (&out, '^', cur_face);
	  CHECK_LPOS();
	  invis_addc (&out, CTRL_CHAR (c) ? UNCTRL (c) : '?', cur_face);
	  CHECK_LPOS();
	}
      else
	{
#if defined (HANDLE_MULTIBYTE)
	  if (mb_cur_max > 1 && rl_byte_oriented == 0)
	    {
	      register int i;

	      _rl_wrapped_multicolumn = 0;

	      if (_rl_screenwidth < lpos + wc_width)
		for (i = lpos; i < _rl_screenwidth; i++)
		  {
		     
		    invis_addc (&out, ' ', cur_face);
		    _rl_wrapped_multicolumn++;
		    CHECK_LPOS();
		  }
	      if (in == rl_point)
		{
		  cpos_buffer_position = out;
		  lb_linenum = newlines;
		}
	      for (i = in; i < in+wc_bytes; i++)
		invis_addc (&out, rl_line_buffer[i], cur_face);
	      for (i = 0; i < wc_width; i++)
		CHECK_LPOS();
	    }
	  else
	    {
	      invis_addc (&out, c, cur_face);
	      CHECK_LPOS();
	    }
#else
	  invis_addc (&out, c, cur_face);
	  CHECK_LPOS();
#endif
	}

#if defined (HANDLE_MULTIBYTE)
      if (mb_cur_max > 1 && rl_byte_oriented == 0)
	{
	  in += wc_bytes;
	  if (_rl_utf8locale && UTF8_SINGLEBYTE(rl_line_buffer[in]))
	    {
	      wc = (WCHAR_T)rl_line_buffer[in];
	      wc_bytes = 1;
	      memset (&ps, 0, sizeof (mbstate_t));	 
	    }
	  else
	    wc_bytes = MBRTOWC (&wc, rl_line_buffer + in, rl_end - in, &ps);
	}
      else
        in++;
#endif
    }
  invis_nul (&out);
  line_totbytes = out;
  if (cpos_buffer_position < 0)
    {
      cpos_buffer_position = out;
      lb_linenum = newlines;
    }

   
  if (_rl_quick_redisplay && newlines > 0)
    _rl_quick_redisplay = 0;

  inv_botlin = lb_botlin = _rl_inv_botlin = newlines;
  CHECK_INV_LBREAKS ();
  inv_lbreaks[newlines+1] = out;
#if defined (HANDLE_MULTIBYTE)
   
  line_state_invisible->wrapped_line[newlines+1] = _rl_wrapped_multicolumn;
#endif
  cursor_linenum = lb_linenum;

   

   

   
  displaying_prompt_first_line = 1;
  if (_rl_horizontal_scroll_mode == 0 && _rl_term_up && *_rl_term_up)
    {
      int nleft, pos, changed_screen_line, tx;

      if (!rl_display_fixed || forced_display)
	{
	  forced_display = 0;

	   
	  if (out >= _rl_screenchars)
	    {
#if defined (HANDLE_MULTIBYTE)
	      if (mb_cur_max > 1 && rl_byte_oriented == 0)
		out = _rl_find_prev_mbchar (invisible_line, _rl_screenchars, MB_FIND_ANY);
	      else
#endif
		out = _rl_screenchars - 1;
	    }

	   

#define INVIS_FIRST()	(prompt_physical_chars > _rl_screenwidth ? prompt_invis_chars_first_line : wrap_offset)
#define WRAP_OFFSET(line, offset)  ((line == 0) \
					? (offset ? INVIS_FIRST() : 0) \
					: ((line == prompt_last_screen_line) ? wrap_offset-prompt_invis_chars_first_line : 0))
#define W_OFFSET(line, offset) ((line) == 0 ? offset : 0)
#define VIS_LLEN(l)	((l) > _rl_vis_botlin ? 0 : (vis_lbreaks[l+1] - vis_lbreaks[l]))
#define INV_LLEN(l)	(inv_lbreaks[l+1] - inv_lbreaks[l])
#define VIS_CHARS(line) (visible_line + vis_lbreaks[line])
#define VIS_FACE(line) (vis_face + vis_lbreaks[line])
#define VIS_LINE(line) ((line) > _rl_vis_botlin) ? "" : VIS_CHARS(line)
#define VIS_LINE_FACE(line) ((line) > _rl_vis_botlin) ? "" : VIS_FACE(line)
#define INV_LINE(line) (invisible_line + inv_lbreaks[line])
#define INV_LINE_FACE(line) (inv_face + inv_lbreaks[line])

#define OLD_CPOS_IN_PROMPT() (cpos_adjusted == 0 && \
			_rl_last_c_pos != o_cpos && \
			_rl_last_c_pos > wrap_offset && \
			o_cpos < prompt_last_invisible)


	   
	  if (rl_mark_active_p () && inv_botlin > _rl_screenheight)
	    {
	      int extra;

	      extra = inv_botlin - _rl_screenheight;
	      for (linenum = 0; linenum <= extra; linenum++)
		norm_face (INV_LINE_FACE(linenum), INV_LLEN (linenum));
	    }

	   
	  for (linenum = 0; linenum <= inv_botlin; linenum++)
	    {
	       
	      o_cpos = _rl_last_c_pos;
	      cpos_adjusted = 0;
	      update_line (VIS_LINE(linenum), VIS_LINE_FACE(linenum),
			   INV_LINE(linenum), INV_LINE_FACE(linenum),
			   linenum,
			   VIS_LLEN(linenum), INV_LLEN(linenum), inv_botlin);

	       
	      if (linenum == 0 && (mb_cur_max > 1 && rl_byte_oriented == 0) && OLD_CPOS_IN_PROMPT())
		_rl_last_c_pos -= prompt_invis_chars_first_line;	 
	      else if (cpos_adjusted == 0 &&
			linenum == prompt_last_screen_line &&
			prompt_physical_chars > _rl_screenwidth &&
			(mb_cur_max > 1 && rl_byte_oriented == 0) &&
			_rl_last_c_pos != o_cpos &&
			_rl_last_c_pos > (prompt_last_invisible - _rl_screenwidth - prompt_invis_chars_first_line))	 
		 
		 
		_rl_last_c_pos -= (wrap_offset-prompt_invis_chars_first_line);

	       
	      if (linenum == 0 &&
		  inv_botlin == 0 && _rl_last_c_pos == out &&
		  (wrap_offset > visible_wrap_offset) &&
		  (_rl_last_c_pos < visible_first_line_len))
		{
		  if (mb_cur_max > 1 && rl_byte_oriented == 0)
		    nleft = _rl_screenwidth - _rl_last_c_pos;
		  else
		    nleft = _rl_screenwidth + wrap_offset - _rl_last_c_pos;
		  if (nleft)
		    _rl_clear_to_eol (nleft);
		}
#if 0
	       
	      else if (linenum == 0 &&
		       linenum == prompt_last_screen_line &&
		       _rl_last_c_pos == out &&
		       _rl_last_c_pos < visible_first_line_len &&
		       visible_wrap_offset &&
		       visible_wrap_offset != wrap_offset)
		{
		  if (mb_cur_max > 1 && rl_byte_oriented == 0)
		    nleft = _rl_screenwidth - _rl_last_c_pos;
		  else
		    nleft = _rl_screenwidth + wrap_offset - _rl_last_c_pos;
		  if (nleft)
		    _rl_clear_to_eol (nleft);
		}
#endif

	       
	      if (linenum == 0)
		visible_first_line_len = (inv_botlin > 0) ? inv_lbreaks[1] : out - wrap_offset;
	    }

	   
	  if (_rl_vis_botlin > inv_botlin)
	    {
	      char *tt;
	      for (; linenum <= _rl_vis_botlin; linenum++)
		{
		  tt = VIS_CHARS (linenum);
		  _rl_move_vert (linenum);
		  _rl_move_cursor_relative (0, tt, VIS_FACE(linenum));
		  _rl_clear_to_eol
		    ((linenum == _rl_vis_botlin) ? strlen (tt) : _rl_screenwidth);
		}
	    }
	  _rl_vis_botlin = inv_botlin;

	   
	  changed_screen_line = _rl_last_v_pos != cursor_linenum;
	  if (changed_screen_line)
	    {
	      _rl_move_vert (cursor_linenum);
	       
	      if ((mb_cur_max == 1 || rl_byte_oriented) && cursor_linenum == 0 && wrap_offset)
		_rl_last_c_pos += wrap_offset;
	    }

	   

	   
	   
	  nleft = prompt_visible_length + wrap_offset;
	  if (cursor_linenum == 0 && wrap_offset > 0 && _rl_last_c_pos > 0 &&
	      _rl_last_c_pos < PROMPT_ENDING_INDEX && local_prompt)
	    {
	      _rl_cr ();
	      if (modmark)
		_rl_output_some_chars ("*", 1);

	      _rl_output_some_chars (local_prompt, nleft);
	      if (mb_cur_max > 1 && rl_byte_oriented == 0)
		_rl_last_c_pos = _rl_col_width (local_prompt, 0, nleft, 1) - wrap_offset + modmark;
	      else
		_rl_last_c_pos = nleft + modmark;
	    }

	   
	  pos = inv_lbreaks[cursor_linenum];
	   
	  nleft = cpos_buffer_position - pos;

	   

	   
	  if (wrap_offset && cursor_linenum == 0 && nleft < _rl_last_c_pos)
	    {
	       
	      if (mb_cur_max > 1 && rl_byte_oriented == 0)
		tx = _rl_col_width (&visible_line[pos], 0, nleft, 1) - visible_wrap_offset;
	      else
		tx = nleft;
	      if (tx >= 0 && _rl_last_c_pos > tx)
		{
	          _rl_backspace (_rl_last_c_pos - tx);	 
	          _rl_last_c_pos = tx;
		}
	    }

	   
	  if (mb_cur_max > 1 && rl_byte_oriented == 0)
	    _rl_move_cursor_relative (nleft, &invisible_line[pos], &inv_face[pos]);
	  else if (nleft != _rl_last_c_pos)
	    _rl_move_cursor_relative (nleft, &invisible_line[pos], &inv_face[pos]);
	}
    }
  else				 
    {
#define M_OFFSET(margin, offset) ((margin) == 0 ? offset : 0)
      int lmargin, ndisp, nleft, phys_c_pos, t;

       
      _rl_last_v_pos = 0;

       

       
      ndisp = cpos_buffer_position - wrap_offset;
      nleft  = prompt_visible_length + wrap_offset;
       
      phys_c_pos = cpos_buffer_position - (last_lmargin ? last_lmargin : wrap_offset);
      t = _rl_screenwidth / 3;

       

       
      if (phys_c_pos > _rl_screenwidth - 2)
	{
	  lmargin = cpos_buffer_position - (2 * t);
	  if (lmargin < 0)
	    lmargin = 0;
	   
	  if (wrap_offset && lmargin > 0 && lmargin < nleft)
	    lmargin = nleft;
	}
      else if (ndisp < _rl_screenwidth - 2)		 
	lmargin = 0;
      else if (phys_c_pos < 1)
	{
	   
	  lmargin = ((cpos_buffer_position - 1) / t) * t;	 
	  if (wrap_offset && lmargin > 0 && lmargin < nleft)
	    lmargin = nleft;
	}
      else
	lmargin = last_lmargin;

      displaying_prompt_first_line = lmargin < nleft;

       
      if (lmargin > 0)
	invisible_line[lmargin] = '<';

       
      t = lmargin + M_OFFSET (lmargin, wrap_offset) + _rl_screenwidth;
      if (t > 0 && t < out)
	invisible_line[t - 1] = '>';

      if (rl_display_fixed == 0 || forced_display || lmargin != last_lmargin)
	{
	  forced_display = 0;
	  o_cpos = _rl_last_c_pos;
	  cpos_adjusted = 0;
	  update_line (&visible_line[last_lmargin], &vis_face[last_lmargin],
		       &invisible_line[lmargin], &inv_face[lmargin],
		       0,
		       _rl_screenwidth + visible_wrap_offset,
		       _rl_screenwidth + (lmargin ? 0 : wrap_offset),
		       0);

	  if ((mb_cur_max > 1 && rl_byte_oriented == 0) &&
		displaying_prompt_first_line && OLD_CPOS_IN_PROMPT())
	    _rl_last_c_pos -= prompt_invis_chars_first_line;	 

	   
	  t = _rl_last_c_pos - M_OFFSET (lmargin, wrap_offset);
	  if ((M_OFFSET (lmargin, wrap_offset) > visible_wrap_offset) &&
	      (_rl_last_c_pos == out) && displaying_prompt_first_line &&
	      t < visible_first_line_len)
	    {
	      nleft = _rl_screenwidth - t;
	      _rl_clear_to_eol (nleft);
	    }
	  visible_first_line_len = out - lmargin - M_OFFSET (lmargin, wrap_offset);
	  if (visible_first_line_len > _rl_screenwidth)
	    visible_first_line_len = _rl_screenwidth;

	  _rl_move_cursor_relative (cpos_buffer_position - lmargin, &invisible_line[lmargin], &inv_face[lmargin]);
	  last_lmargin = lmargin;
	}
    }
  fflush (rl_outstream);

   
  {
    struct line_state *vtemp = line_state_visible;

    line_state_visible = line_state_invisible;
    line_state_invisible = vtemp;

    rl_display_fixed = 0;
     
    if (_rl_horizontal_scroll_mode && last_lmargin)
      visible_wrap_offset = 0;
    else
      visible_wrap_offset = wrap_offset;

    _rl_quick_redisplay = 0;
  }

  RL_UNSETSTATE (RL_STATE_REDISPLAYING);
  _rl_release_sigint ();
}

static void
putc_face (int c, int face, char *cur_face)
{
  char cf;
  cf = *cur_face;
  if (cf != face)
    {
      if (cf != FACE_NORMAL && cf != FACE_STANDOUT)
	return;
      if (face != FACE_NORMAL && face != FACE_STANDOUT)
	return;
      if (face == FACE_STANDOUT && cf == FACE_NORMAL)
	_rl_region_color_on ();
      if (face == FACE_NORMAL && cf == FACE_STANDOUT)
	_rl_region_color_off ();
      *cur_face = face;
    }
  if (c != EOF)
    putc (c, rl_outstream);
}

static void
puts_face (const char *str, const char *face, int n)
{
  int i;
  char cur_face;

  for (cur_face = FACE_NORMAL, i = 0; i < n; i++)
    putc_face ((unsigned char) str[i], face[i], &cur_face);
  putc_face (EOF, FACE_NORMAL, &cur_face);
}

static void
norm_face (char *face, int n)
{
  memset (face, FACE_NORMAL, n);
}

#define ADJUST_CPOS(x) do { _rl_last_c_pos -= (x) ; cpos_adjusted = 1; } while (0)

 
static void
update_line (char *old, char *old_face, char *new, char *new_face, int current_line, int omax, int nmax, int inv_botlin)
{
  char *ofd, *ols, *oe, *nfd, *nls, *ne;
  char *ofdf, *nfdf, *olsf, *nlsf;
  int temp, lendiff, wsatend, od, nd, twidth, o_cpos;
  int current_invis_chars;
  int col_lendiff, col_temp;
  int bytes_to_insert;
  int mb_cur_max = MB_CUR_MAX;
#if defined (HANDLE_MULTIBYTE)
  mbstate_t ps_new, ps_old;
  int new_offset, old_offset;
#endif

   
  if (mb_cur_max > 1 && rl_byte_oriented == 0)
    temp = _rl_last_c_pos;
  else
    temp = _rl_last_c_pos - WRAP_OFFSET (_rl_last_v_pos, visible_wrap_offset);
  if (temp == _rl_screenwidth && _rl_term_autowrap && !_rl_horizontal_scroll_mode
	&& _rl_last_v_pos == current_line - 1)
    {
       
       
#if defined (HANDLE_MULTIBYTE)
      if (mb_cur_max > 1 && rl_byte_oriented == 0)
	{
	  WCHAR_T wc;
	  mbstate_t ps;
	  int oldwidth, newwidth;
	  int oldbytes, newbytes;
	  size_t ret;

	   
	   
	   
	  if (current_line < line_state_invisible->wbsize && line_state_invisible->wrapped_line[current_line] > 0)
	    _rl_clear_to_eol (line_state_invisible->wrapped_line[current_line]);

	   
	  memset (&ps, 0, sizeof (mbstate_t));
	  ret = MBRTOWC (&wc, old, mb_cur_max, &ps);
	  oldbytes = ret;
	  if (MB_INVALIDCH (ret))
	    {
	      oldwidth = 1;
	      oldbytes = 1;
	    }
	  else if (MB_NULLWCH (ret))
	    oldwidth = 0;
	  else
	    oldwidth = WCWIDTH (wc);
	  if (oldwidth < 0)
	    oldwidth = 1;

	   
	  memset (&ps, 0, sizeof (mbstate_t));
	  ret = MBRTOWC (&wc, new, mb_cur_max, &ps);
	  newbytes = ret;
	  if (MB_INVALIDCH (ret))
	    {
	      newwidth = 1;
	      newbytes = 1;
	    }
	  else if (MB_NULLWCH (ret))
	    newwidth = 0;
	  else
	    newwidth = WCWIDTH (wc);
	  if (newwidth < 0)
	    newwidth = 1;

	   
	  while (newbytes < nmax && newwidth < oldwidth)
	    {
	      int t;

	      ret = MBRTOWC (&wc, new+newbytes, mb_cur_max, &ps);
	      if (MB_INVALIDCH (ret))
		{
		  newwidth += 1;
		  newbytes += 1;
		}
	      else if (MB_NULLWCH (ret))
	        break;
	      else
		{
		  t = WCWIDTH (wc);
		  newwidth += (t >= 0) ? t : 1;
		  newbytes += ret;
		}
	    }
	   
	  while (oldbytes < omax && oldwidth < newwidth)
	    {
	      int t;

	      ret = MBRTOWC (&wc, old+oldbytes, mb_cur_max, &ps);
	      if (MB_INVALIDCH (ret))
		{
		  oldwidth += 1;
		  oldbytes += 1;
		}
	      else if (MB_NULLWCH (ret))
	        break;
	      else
		{
		  t = WCWIDTH (wc);
		  oldwidth += (t >= 0) ? t : 1;
		  oldbytes += ret;
		}
	    }
	   
	  if (newwidth > 0)
	    {
	      int count, i, j;
	      char *optr;

	      puts_face (new, new_face, newbytes);
	      _rl_last_c_pos = newwidth;
	      _rl_last_v_pos++;

	       
	      if (newwidth != oldwidth || newbytes > oldbytes)
		{
		  oe = old + omax;
		  ne = new + nmax;
		  nd = newbytes;
		  nfd = new + nd;
		  ofdf = old_face + oldbytes;
		  nfdf = new_face + newbytes;

		  goto dumb_update;
		}
	      if (oldbytes != 0 && newbytes != 0)
		{
		   
		   

		   
		  if (oldbytes != newbytes)
		    {
		      memmove (old+newbytes, old+oldbytes, strlen (old+oldbytes) + 1);
		      memmove (old_face+newbytes, old_face+oldbytes, strlen (old+oldbytes) + 1);
		    }
		  memcpy (old, new, newbytes);
		  memcpy (old_face, new_face, newbytes);
		  j = newbytes - oldbytes;
		  omax += j;
		   
		  for (i = current_line+1; j != 0 && i <= inv_botlin+1 && i <=_rl_vis_botlin+1; i++)
		    vis_lbreaks[i] += j;
		}
	    }
	  else
	    {
	      putc (' ', rl_outstream);
	      _rl_last_c_pos = 1;
	      _rl_last_v_pos++;
	      if (old[0] && new[0])
		{
		  old[0] = new[0];
		  old_face[0] = new_face[0];
		}
	    }
	}
      else
#endif
	{
	  if (new[0])
	    puts_face (new, new_face, 1);
	  else
	    putc (' ', rl_outstream);
	  _rl_last_c_pos = 1;
	  _rl_last_v_pos++;
	  if (old[0] && new[0])
	    {
	      old[0] = new[0];
	      old_face[0] = new_face[0];
	    }
	}
    }

   
  if (_rl_quick_redisplay)
    {
      nfd = new;
      nfdf = new_face;
      ofd = old;
      ofdf = old_face;
      for (od = 0, oe = ofd; od < omax && *oe; oe++, od++);
      for (nd = 0, ne = nfd; nd < nmax && *ne; ne++, nd++);
      od = nd = 0;
      _rl_move_cursor_relative (0, old, old_face);

      bytes_to_insert = ne - nfd;
      if (bytes_to_insert < local_prompt_len)	 
	goto dumb_update;

       
      _rl_output_some_chars (nfd, local_prompt_len);
      if (mb_cur_max > 1 && rl_byte_oriented == 0)
	_rl_last_c_pos = prompt_physical_chars;
      else
	_rl_last_c_pos = local_prompt_len;

      bytes_to_insert -= local_prompt_len;
      if (bytes_to_insert > 0)
	{
	  puts_face (new+local_prompt_len, nfdf+local_prompt_len, bytes_to_insert);
	  if (mb_cur_max > 1 && rl_byte_oriented)
	    _rl_last_c_pos += _rl_col_width (new, local_prompt_len, ne-new, 1);
	  else
	    _rl_last_c_pos += bytes_to_insert;
	}

       
      if (nmax < omax)
	goto clear_rest_of_line;
      else if ((nmax - W_OFFSET(current_line, wrap_offset)) < (omax - W_OFFSET (current_line, visible_wrap_offset)))
	goto clear_rest_of_line;
      else
	return;
    }

   
#if defined (HANDLE_MULTIBYTE)
  if (mb_cur_max > 1 && rl_byte_oriented == 0)
    {
       
      temp = (omax < nmax) ? omax : nmax;
      if (memcmp (old, new, temp) == 0 && memcmp (old_face, new_face, temp) == 0)
	{
	  new_offset = old_offset = temp;	 
	  ofd = old + temp;
	  ofdf = old_face + temp;
	  nfd = new + temp;
	  nfdf = new_face + temp;
	}
      else
	{      
	  memset (&ps_new, 0, sizeof(mbstate_t));
	  memset (&ps_old, 0, sizeof(mbstate_t));

	   
	  if (omax == nmax && memcmp (new, old, omax) == 0 && memcmp (new_face, old_face, omax) == 0)
	    {
	      old_offset = omax;
	      new_offset = nmax;
	      ofd = old + omax;
	      ofdf = old_face + omax;
	      nfd = new + nmax;
	      nfdf = new_face + nmax;
	    }
	  else
	    {
	       
	      new_offset = old_offset = 0;
	      for (ofd = old, ofdf = old_face, nfd = new, nfdf = new_face;
		    (ofd - old < omax) && *ofd &&
		    _rl_compare_chars(old, old_offset, &ps_old, new, new_offset, &ps_new) &&
		    *ofdf == *nfdf; )
		{
		  old_offset = _rl_find_next_mbchar (old, old_offset, 1, MB_FIND_ANY);
		  new_offset = _rl_find_next_mbchar (new, new_offset, 1, MB_FIND_ANY);

		  ofd = old + old_offset;
		  ofdf = old_face + old_offset;
		  nfd = new + new_offset;
		  nfdf = new_face + new_offset;
		}
	    }
	}
    }
  else
#endif
  for (ofd = old, ofdf = old_face, nfd = new, nfdf = new_face;
       (ofd - old < omax) && *ofd && (*ofd == *nfd) && (*ofdf == *nfdf);
       ofd++, nfd++, ofdf++, nfdf++)
    ;

   
  for (od = ofd - old, oe = ofd; od < omax && *oe; oe++, od++);
  for (nd = nfd - new, ne = nfd; nd < nmax && *ne; ne++, nd++);

   
  if (ofd == oe && nfd == ne)
    return;

#if defined (HANDLE_MULTIBYTE)
  if (mb_cur_max > 1 && rl_byte_oriented == 0 && _rl_utf8locale)
    {
      WCHAR_T wc;
      mbstate_t ps = { 0 };
      int t;

       
      t = MBRTOWC (&wc, ofd, mb_cur_max, &ps);
      if (t > 0 && UNICODE_COMBINING_CHAR (wc) && WCWIDTH (wc) == 0)
	{
	  old_offset = _rl_find_prev_mbchar (old, ofd - old, MB_FIND_ANY);
	  new_offset = _rl_find_prev_mbchar (new, nfd - new, MB_FIND_ANY);
	  ofd = old + old_offset;	 
	  ofdf = old_face + old_offset;
	  nfd = new + new_offset;
	  nfdf = new_face + new_offset;
	}
    }
#endif

  wsatend = 1;			 

#if defined (HANDLE_MULTIBYTE)
   
  if (mb_cur_max > 1 && rl_byte_oriented == 0)
    {
      ols = old + _rl_find_prev_mbchar (old, oe - old, MB_FIND_ANY);
      olsf = old_face + (ols - old);
      nls = new + _rl_find_prev_mbchar (new, ne - new, MB_FIND_ANY);
      nlsf = new_face + (nls - new);

      while ((ols > ofd) && (nls > nfd))
	{
	  memset (&ps_old, 0, sizeof (mbstate_t));
	  memset (&ps_new, 0, sizeof (mbstate_t));

	  if (_rl_compare_chars (old, ols - old, &ps_old, new, nls - new, &ps_new) == 0 ||
		*olsf != *nlsf)
	    break;

	  if (*ols == ' ')
	    wsatend = 0;

	  ols = old + _rl_find_prev_mbchar (old, ols - old, MB_FIND_ANY);
	  olsf = old_face + (ols - old);
	  nls = new + _rl_find_prev_mbchar (new, nls - new, MB_FIND_ANY);
	  nlsf = new_face + (nls - new);
	}
    }
  else
    {
#endif  
  ols = oe - 1;			 
  olsf = old_face + (ols - old);
  nls = ne - 1;
  nlsf = new_face + (nls - new);
  while ((ols > ofd) && (nls > nfd) && (*ols == *nls) && (*olsf == *nlsf))
    {
      if (*ols != ' ')
	wsatend = 0;
      ols--; olsf--;
      nls--; nlsf--;
    }
#if defined (HANDLE_MULTIBYTE)
    }
#endif

  if (wsatend)
    {
      ols = oe;
      olsf = old_face + (ols - old);
      nls = ne;
      nlsf = new_face + (nls - new);
    }
#if defined (HANDLE_MULTIBYTE)
   
  else if (_rl_compare_chars (ols, 0, NULL, nls, 0, NULL) == 0 || *olsf != *nlsf)
#else
  else if (*ols != *nls || *olsf != *nlsf)
#endif
    {
      if (*ols)			 
	{
	  if (mb_cur_max > 1 && rl_byte_oriented == 0)
	    ols = old + _rl_find_next_mbchar (old, ols - old, 1, MB_FIND_ANY);
	  else
	    ols++;
	}
      if (*nls)
	{
	  if (mb_cur_max > 1 && rl_byte_oriented == 0)
	    nls = new + _rl_find_next_mbchar (new, nls - new, 1, MB_FIND_ANY);
	  else
	    nls++;
	}
      olsf = old_face + (ols - old);
      nlsf = new_face + (nls - new);
    }

   
  current_invis_chars = W_OFFSET (current_line, wrap_offset);
  if (_rl_last_v_pos != current_line)
    {
      _rl_move_vert (current_line);
       
      if (current_line == 0)
	visible_wrap_offset = prompt_invis_chars_first_line;	 
#if 0		 
      else if (current_line == prompt_last_screen_line && wrap_offset > prompt_invis_chars_first_line)
	visible_wrap_offset = wrap_offset - prompt_invis_chars_first_line
#endif
      if ((mb_cur_max == 1 || rl_byte_oriented) && current_line == 0 && visible_wrap_offset)
	_rl_last_c_pos += visible_wrap_offset;
    }

   

  lendiff = local_prompt_len;
  if (lendiff > nmax)
    lendiff = nmax;
  od = ofd - old;	 
  nd = nfd - new;	 
  if (current_line == 0 && !_rl_horizontal_scroll_mode &&
      _rl_term_cr && lendiff > prompt_visible_length && _rl_last_c_pos > 0 &&
      (((od > 0 || nd > 0) && (od <= prompt_last_invisible || nd <= prompt_last_invisible)) ||
		((od >= lendiff) && _rl_last_c_pos < PROMPT_ENDING_INDEX)))
    {
      _rl_cr ();
      if (modmark)
	_rl_output_some_chars ("*", 1);
      _rl_output_some_chars (local_prompt, lendiff);
      if (mb_cur_max > 1 && rl_byte_oriented == 0)
	{
	   
	  if (lendiff == local_prompt_len)
	    _rl_last_c_pos = prompt_physical_chars + modmark;
	  else
	     
	    _rl_last_c_pos = _rl_col_width (local_prompt, 0, lendiff, 1) - wrap_offset + modmark;
	  cpos_adjusted = 1;
	}
      else
	_rl_last_c_pos = lendiff + modmark;

       
      if ((od <= prompt_last_invisible || nd <= prompt_last_invisible) &&
          omax == nmax &&
	  lendiff > (ols-old) && lendiff > (nls-new))
	return;

       
      if ((od <= prompt_last_invisible || nd <= prompt_last_invisible))
	{
	  nfd = new + lendiff;	 
	  nfdf = new_face + lendiff;
	  nd = lendiff;

	   
dumb_update:
	  temp = ne - nfd;
	  if (temp > 0)
	    {
	      puts_face (nfd, nfdf, temp);
	      if (mb_cur_max > 1 && rl_byte_oriented == 0)
		{
		  _rl_last_c_pos += _rl_col_width (new, nd, ne - new, 1);
		   
		  if (wrap_offset > prompt_invis_chars_first_line &&
		      current_line == prompt_last_screen_line &&
		      prompt_physical_chars > _rl_screenwidth &&
		      _rl_horizontal_scroll_mode == 0)
		    ADJUST_CPOS (wrap_offset - prompt_invis_chars_first_line);

		   
		  else if (current_line == 0 &&
			   nfd == new &&
			   prompt_invis_chars_first_line &&
			   local_prompt_len <= temp &&
			   wrap_offset >= prompt_invis_chars_first_line &&
			   _rl_horizontal_scroll_mode == 0)
		    ADJUST_CPOS (prompt_invis_chars_first_line);
		}
	      else
		_rl_last_c_pos += temp;
	    }
	   
	  if (nmax < omax)
	    goto clear_rest_of_line;	 
	  else if ((nmax - W_OFFSET(current_line, wrap_offset)) < (omax - W_OFFSET (current_line, visible_wrap_offset)))
	    goto clear_rest_of_line;
	  else
	    return;
	}
    }

  o_cpos = _rl_last_c_pos;

   
  _rl_move_cursor_relative (od, old, old_face);

#if defined (HANDLE_MULTIBYTE)
   
  if (current_line == 0 && mb_cur_max > 1 && rl_byte_oriented == 0 &&
      (_rl_last_c_pos > 0 || o_cpos > 0) &&
      _rl_last_c_pos == prompt_physical_chars)
    cpos_adjusted = 1;
#endif

   
  lendiff = (nls - nfd) - (ols - ofd);
  if (mb_cur_max > 1 && rl_byte_oriented == 0)
    {
      int newchars, newwidth, newind;
      int oldchars, oldwidth, oldind;

      newchars = nls - new;
      oldchars = ols - old;

       
      if (current_line == 0 && nfd == new && newchars > prompt_last_invisible &&
	  newchars <= local_prompt_len &&
	  local_prompt_len <= nmax &&
	  current_invis_chars != visible_wrap_offset)
	{
	  while (newchars < nmax && oldchars < omax &&  newchars < local_prompt_len)
	    {
#if defined (HANDLE_MULTIBYTE)
	      newind = _rl_find_next_mbchar (new, newchars, 1, MB_FIND_NONZERO);
	      oldind = _rl_find_next_mbchar (old, oldchars, 1, MB_FIND_NONZERO);

	      nls += newind - newchars;
	      ols += oldind - oldchars;

	      newchars = newind;
	      oldchars = oldind;
#else
	      nls++; ols++;
	      newchars++; oldchars++;
#endif
	    }
	  newwidth = (newchars == local_prompt_len) ? prompt_physical_chars + wrap_offset
	  					    : _rl_col_width (new, 0, nls - new, 1);
	   
	  lendiff = (nls - nfd) - (ols - ofd);

	  nlsf = new_face + (nls - new);
	  olsf = old_face + (ols - old);
	}
      else
	newwidth = _rl_col_width (new, nfd - new, nls - new, 1);

      oldwidth = _rl_col_width (old, ofd - old, ols - old, 1);

      col_lendiff = newwidth - oldwidth;
    }
  else
    col_lendiff = lendiff;

   
     
   
  if (current_line == 0 && current_invis_chars != visible_wrap_offset)
    {
      if (mb_cur_max > 1 && rl_byte_oriented == 0)
	{
	  lendiff += visible_wrap_offset - current_invis_chars;
	  col_lendiff += visible_wrap_offset - current_invis_chars;
	}
      else
	{
	  lendiff += visible_wrap_offset - current_invis_chars;
	  col_lendiff = lendiff;
	}
    }

   
   
  temp = ne - nfd;
  if (mb_cur_max > 1 && rl_byte_oriented == 0)
    col_temp = _rl_col_width (new, nfd - new, ne - new, 1);
  else
    col_temp = temp;

   
  bytes_to_insert = nls - nfd;

   
  if (col_lendiff > 0)	 
    {
       
      int gl = current_line >= _rl_vis_botlin && inv_botlin > _rl_vis_botlin;

       
      if (lendiff < 0)
	{
	  puts_face (nfd, nfdf, temp);
	  _rl_last_c_pos += col_temp;
	   
	  if (current_line == 0 && displaying_prompt_first_line && wrap_offset && ((nfd - new) <= prompt_last_invisible))
	    ADJUST_CPOS (wrap_offset);	 
	  return;
	}
       
      else if (_rl_terminal_can_insert && ((2 * col_temp) >= col_lendiff || _rl_term_IC) && (!_rl_term_autowrap || !gl))
	{
	   
	   
	  if (*ols && ((_rl_horizontal_scroll_mode &&
			_rl_last_c_pos == 0 &&
			lendiff > prompt_visible_length &&
			current_invis_chars > 0) == 0) &&
		      (((mb_cur_max > 1 && rl_byte_oriented == 0) &&
		        current_line == 0 && wrap_offset &&
		        ((nfd - new) <= prompt_last_invisible) &&
		        (col_lendiff < prompt_visible_length)) == 0) &&
		      (visible_wrap_offset >= current_invis_chars))
	    {
	      open_some_spaces (col_lendiff);
	      puts_face (nfd, nfdf, bytes_to_insert);
	      if (mb_cur_max > 1 && rl_byte_oriented == 0)
		_rl_last_c_pos += _rl_col_width (nfd, 0, bytes_to_insert, 1);
	      else
		_rl_last_c_pos += bytes_to_insert;
	    }
	  else if ((mb_cur_max == 1 || rl_byte_oriented != 0) && *ols == 0 && lendiff > 0)
	    {
	       
	      puts_face (nfd, nfdf, temp);
	      _rl_last_c_pos += col_temp;
	      return;
	    }
	  else	 
	    {
	      puts_face (nfd, nfdf, temp);
	      _rl_last_c_pos += col_temp;
	       
	      if ((mb_cur_max > 1 && rl_byte_oriented == 0) && current_line == 0 && displaying_prompt_first_line && wrap_offset && ((nfd - new) <= prompt_last_invisible))
		ADJUST_CPOS (wrap_offset);	 
	      return;
	    }

	  if (bytes_to_insert > lendiff)
	    {
	       
	      if ((mb_cur_max > 1 && rl_byte_oriented == 0) && current_line == 0 && displaying_prompt_first_line && wrap_offset && ((nfd - new) <= prompt_last_invisible))
		ADJUST_CPOS (wrap_offset);	 
	    }
	}
      else
	{
	   
	  puts_face (nfd, nfdf, temp);
	  _rl_last_c_pos += col_temp;
	   
	   
	  if ((mb_cur_max > 1 && rl_byte_oriented == 0) &&
		current_line == prompt_last_screen_line && wrap_offset &&
		displaying_prompt_first_line &&
		wrap_offset != prompt_invis_chars_first_line &&
		((nfd-new) < (prompt_last_invisible-(current_line*_rl_screenwidth+prompt_invis_chars_first_line))))
	    ADJUST_CPOS (wrap_offset - prompt_invis_chars_first_line);

	   
	  if ((mb_cur_max > 1 && rl_byte_oriented == 0) &&
		current_line == 0 && wrap_offset &&
		displaying_prompt_first_line &&
		wrap_offset == prompt_invis_chars_first_line &&
		visible_wrap_offset != current_invis_chars &&
		visible_wrap_offset != prompt_invis_chars_first_line &&
		((nfd-new) < prompt_last_invisible))
	    ADJUST_CPOS (prompt_invis_chars_first_line);
	}
    }
  else				 
    {
       
      if (_rl_term_dc && (2 * col_temp) >= -col_lendiff)
	{
	   
	  if (_rl_horizontal_scroll_mode && _rl_last_c_pos == 0 &&
	      displaying_prompt_first_line &&
	      -lendiff == visible_wrap_offset)
	    col_lendiff = 0;

	   
	  if (_rl_horizontal_scroll_mode && displaying_prompt_first_line == 0 &&
		col_lendiff && _rl_last_c_pos < -col_lendiff)
	    col_lendiff = 0;

	  if (col_lendiff)
	    delete_chars (-col_lendiff);  

	   
	  if (bytes_to_insert > 0)
	    {
	       
	      puts_face (nfd, nfdf, bytes_to_insert);
	      if (mb_cur_max > 1 && rl_byte_oriented == 0)
		{
		   
		  _rl_last_c_pos += _rl_col_width (nfd, 0, bytes_to_insert, 1);
		  if (current_line == 0 && wrap_offset &&
			displaying_prompt_first_line &&
			prompt_invis_chars_first_line &&
			_rl_last_c_pos >= prompt_invis_chars_first_line &&
			((nfd - new) <= prompt_last_invisible))
		    ADJUST_CPOS (prompt_invis_chars_first_line);

#if 1
#ifdef HANDLE_MULTIBYTE
		   
		   
		  if (_rl_last_c_pos == _rl_screenwidth &&
			line_state_invisible->wrapped_line[current_line+1] &&
			nfd[bytes_to_insert-1] != ' ')
		    line_state_invisible->wrapped_line[current_line+1] = 0;
#endif
#endif
		}
	      else
		_rl_last_c_pos += bytes_to_insert;

	       
	      if (_rl_horizontal_scroll_mode && ((oe-old) > (ne-new)))
		{
		  _rl_move_cursor_relative (ne-new, new, new_face);
		  goto clear_rest_of_line;
		}
	    }
	}
       
      else
	{
	  if (temp > 0)
	    {
	       
	      puts_face (nfd, nfdf, temp);
	      _rl_last_c_pos += col_temp;		 
	      if (mb_cur_max > 1 && rl_byte_oriented == 0)
		{
		  if (current_line == 0 && wrap_offset &&
			displaying_prompt_first_line &&
			_rl_last_c_pos > wrap_offset &&
			((nfd - new) <= prompt_last_invisible))
		    ADJUST_CPOS (wrap_offset);	 
		}
	    }
clear_rest_of_line:
	  lendiff = (oe - old) - (ne - new);
	  if (mb_cur_max > 1 && rl_byte_oriented == 0)
	    col_lendiff = _rl_col_width (old, 0, oe - old, 1) - _rl_col_width (new, 0, ne - new, 1);
	  else
	    col_lendiff = lendiff;

	   
	  if (col_lendiff && ((mb_cur_max == 1 || rl_byte_oriented) || (_rl_last_c_pos < _rl_screenwidth)))
	    {	  
	      if (_rl_term_autowrap && current_line < inv_botlin)
		space_to_eol (col_lendiff);
	      else
		_rl_clear_to_eol (col_lendiff);
	    }
	}
    }
}

 
int
rl_on_new_line (void)
{
  if (visible_line)
    visible_line[0] = '\0';

  _rl_last_c_pos = _rl_last_v_pos = 0;
  _rl_vis_botlin = last_lmargin = 0;
  if (vis_lbreaks)
    vis_lbreaks[0] = vis_lbreaks[1] = 0;
  visible_wrap_offset = 0;
  return 0;
}

 
int
rl_clear_visible_line (void)
{
  int curr_line;

   
  _rl_cr ();
  _rl_last_c_pos = 0;

   
  _rl_move_vert (_rl_vis_botlin);

   
  for (curr_line = _rl_last_v_pos; curr_line >= 0; curr_line--)
    {
      _rl_move_vert (curr_line);
      _rl_clear_to_eol (_rl_screenwidth);
      _rl_cr ();		 
    }

  return 0;
}

 
int
rl_on_new_line_with_prompt (void)
{
  int prompt_size, i, l, real_screenwidth, newlines;
  char *prompt_last_line, *lprompt;

   
  prompt_size = strlen (rl_prompt) + 1;
  init_line_structures (prompt_size);

   
  lprompt = local_prompt ? local_prompt : rl_prompt;
  strcpy (visible_line, lprompt);
  strcpy (invisible_line, lprompt);

   
  prompt_last_line = strrchr (rl_prompt, '\n');
  if (!prompt_last_line)
    prompt_last_line = rl_prompt;

  l = strlen (prompt_last_line);
  if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
    _rl_last_c_pos = _rl_col_width (prompt_last_line, 0, l, 1);	 
  else
    _rl_last_c_pos = l;

   
  real_screenwidth = _rl_screenwidth + (_rl_term_autowrap ? 0 : 1);
  _rl_last_v_pos = l / real_screenwidth;
   
  if (l > 0 && (l % real_screenwidth) == 0)
    _rl_output_some_chars ("\n", 1);
  last_lmargin = 0;

  newlines = 0; i = 0;
  while (i <= l)
    {
      _rl_vis_botlin = newlines;
      vis_lbreaks[newlines++] = i;
      i += real_screenwidth;
    }
  vis_lbreaks[newlines] = l;
  visible_wrap_offset = 0;

  rl_display_prompt = rl_prompt;	 

  return 0;
}

 
int
rl_forced_update_display (void)
{
  register char *temp;

  if (visible_line)
    {
      temp = visible_line;
      while (*temp)
	*temp++ = '\0';
    }
  rl_on_new_line ();
  forced_display++;
  (*rl_redisplay_function) ();
  return 0;
}

 
void
rl_redraw_prompt_last_line (void)
{
  char *t;

  t = strrchr (rl_display_prompt, '\n');
  if (t)
    redraw_prompt (++t);
  else
    rl_forced_update_display ();
}

 
static void
_rl_move_cursor_relative (int new, const char *data, const char *dataf)
{
  register int i;
  int woff;			 
  int cpos, dpos;		 
  int adjust;
  int in_invisline;
  int mb_cur_max = MB_CUR_MAX;

  woff = WRAP_OFFSET (_rl_last_v_pos, wrap_offset);
  cpos = _rl_last_c_pos;

  if (cpos == 0 && cpos == new)
    return;

#if defined (HANDLE_MULTIBYTE)
   
  if (mb_cur_max > 1 && rl_byte_oriented == 0)
    {
      adjust = 1;
       
       
      if (new == local_prompt_len && memcmp (data, local_prompt, new) == 0)
	{
	  dpos = prompt_physical_chars;
	  cpos_adjusted = 1;
	  adjust = 0;
	}
       
      else if (new > local_prompt_len && local_prompt && memcmp (data, local_prompt, local_prompt_len) == 0)
	{
	  dpos = prompt_physical_chars + _rl_col_width (data, local_prompt_len, new, 1);
	  cpos_adjusted = 1;
	  adjust = 0;
	}
      else
        dpos = _rl_col_width (data, 0, new, 1);

      if (displaying_prompt_first_line == 0)
	adjust = 0;

       
      in_invisline = 0;
      if (data > invisible_line && data < invisible_line+inv_lbreaks[_rl_inv_botlin+1])
	in_invisline = data - invisible_line;

       
       
      if (adjust && ((new > prompt_last_invisible) ||		 
		     (new+in_invisline > prompt_last_invisible) ||	 
	  (prompt_physical_chars >= _rl_screenwidth &&		 
	   _rl_last_v_pos == prompt_last_screen_line &&
	   wrap_offset >= woff && dpos >= woff &&
	   new > (prompt_last_invisible-(vis_lbreaks[_rl_last_v_pos])-wrap_offset))))
	    
	{
	  dpos -= woff;
	   
	  cpos_adjusted = 1;
	}
    }
  else
#endif
    dpos = new;

   
  if (cpos == dpos)
    return;

   
   
#if defined (HANDLE_MULTIBYTE)
  if (mb_cur_max > 1 && rl_byte_oriented == 0)
    i = _rl_last_c_pos;
  else
#endif
  i = _rl_last_c_pos - woff;
  if (dpos == 0 || CR_FASTER (dpos, _rl_last_c_pos) ||
      (_rl_term_autowrap && i == _rl_screenwidth))
    {
      _rl_cr ();
      cpos = _rl_last_c_pos = 0;
    }

  if (cpos < dpos)
    {
       

       

       
      if (mb_cur_max > 1 && rl_byte_oriented == 0)
	{
	  if (_rl_term_forward_char)
	    {
	      for (i = cpos; i < dpos; i++)
	        tputs (_rl_term_forward_char, 1, _rl_output_character_function);
	    }
	  else
	    {
	      _rl_cr ();
	      puts_face (data, dataf, new);
	    }
	}
      else
	puts_face (data + cpos, dataf + cpos, new - cpos);
    }

#if defined (HANDLE_MULTIBYTE)
   
#endif
  else if (cpos > dpos)
    _rl_backspace (cpos - dpos);

  _rl_last_c_pos = dpos;
}

 
void
_rl_move_vert (int to)
{
  register int delta, i;

  if (_rl_last_v_pos == to || to > _rl_screenheight)
    return;

  if ((delta = to - _rl_last_v_pos) > 0)
    {
      for (i = 0; i < delta; i++)
	putc ('\n', rl_outstream);
      _rl_cr ();
      _rl_last_c_pos = 0;
    }
  else
    {			 
#ifdef __DJGPP__
      int row, col;

      fflush (rl_outstream);
      ScreenGetCursor (&row, &col);
      ScreenSetCursor (row + delta, col);
      i = -delta;
#else
      if (_rl_term_up && *_rl_term_up)
	for (i = 0; i < -delta; i++)
	  tputs (_rl_term_up, 1, _rl_output_character_function);
#endif  
    }

  _rl_last_v_pos = to;		 
}

 
int
rl_show_char (int c)
{
  int n = 1;
  if (META_CHAR (c) && (_rl_output_meta_chars == 0))
    {
      fprintf (rl_outstream, "M-");
      n += 2;
      c = UNMETA (c);
    }

#if defined (DISPLAY_TABS)
  if ((CTRL_CHAR (c) && c != '\t') || c == RUBOUT)
#else
  if (CTRL_CHAR (c) || c == RUBOUT)
#endif  
    {
      fprintf (rl_outstream, "C-");
      n += 2;
      c = CTRL_CHAR (c) ? UNCTRL (c) : '?';
    }

  putc (c, rl_outstream);
  fflush (rl_outstream);
  return n;
}

int
rl_character_len (int c, int pos)
{
  unsigned char uc;

  uc = (unsigned char)c;

  if (META_CHAR (uc))
    return ((_rl_output_meta_chars == 0) ? 4 : 1);

  if (uc == '\t')
    {
#if defined (DISPLAY_TABS)
      return (((pos | 7) + 1) - pos);
#else
      return (2);
#endif  
    }

  if (CTRL_CHAR (c) || c == RUBOUT)
    return (2);

  return ((ISPRINT (uc)) ? 1 : 2);
}
 
static int msg_saved_prompt = 0;

#if defined (USE_VARARGS)
int
#if defined (PREFER_STDARG)
rl_message (const char *format, ...)
#else
rl_message (va_alist)
     va_dcl
#endif
{
  va_list args;
#if defined (PREFER_VARARGS)
  char *format;
#endif
#if defined (HAVE_VSNPRINTF)
  int bneed;
#endif

#if defined (PREFER_STDARG)
  va_start (args, format);
#else
  va_start (args);
  format = va_arg (args, char *);
#endif

  if (msg_buf == 0)
    msg_buf = xmalloc (msg_bufsiz = 128);

#if defined (HAVE_VSNPRINTF)
  bneed = vsnprintf (msg_buf, msg_bufsiz, format, args);
  if (bneed >= msg_bufsiz - 1)
    {
      msg_bufsiz = bneed + 1;
      msg_buf = xrealloc (msg_buf, msg_bufsiz);
      va_end (args);

#if defined (PREFER_STDARG)
      va_start (args, format);
#else
      va_start (args);
      format = va_arg (args, char *);
#endif
      vsnprintf (msg_buf, msg_bufsiz - 1, format, args);
    }
#else
  vsprintf (msg_buf, format, args);
  msg_buf[msg_bufsiz - 1] = '\0';	 
#endif
  va_end (args);

  if (saved_local_prompt == 0)
    {
      rl_save_prompt ();
      msg_saved_prompt = 1;
    }
  else if (local_prompt != saved_local_prompt)
    {
      FREE (local_prompt);
      FREE (local_prompt_prefix);
      local_prompt = (char *)NULL;
    }
  rl_display_prompt = msg_buf;
  local_prompt = expand_prompt (msg_buf, 0, &prompt_visible_length,
					    &prompt_last_invisible,
					    &prompt_invis_chars_first_line,
					    &prompt_physical_chars);
  local_prompt_prefix = (char *)NULL;
  local_prompt_len = local_prompt ? strlen (local_prompt) : 0;
  (*rl_redisplay_function) ();

  return 0;
}
#else  
int
rl_message (format, arg1, arg2)
     char *format;
{
  if (msg_buf == 0)
    msg_buf = xmalloc (msg_bufsiz = 128);

  sprintf (msg_buf, format, arg1, arg2);
  msg_buf[msg_bufsiz - 1] = '\0';	 

  rl_display_prompt = msg_buf;
  if (saved_local_prompt == 0)
    {
      rl_save_prompt ();
      msg_saved_prompt = 1;
    }
  else if (local_prompt != saved_local_prompt)
    {
      FREE (local_prompt);
      FREE (local_prompt_prefix);
      local_prompt = (char *)NULL;
    }
  local_prompt = expand_prompt (msg_buf, 0, &prompt_visible_length,
					    &prompt_last_invisible,
					    &prompt_invis_chars_first_line,
					    &prompt_physical_chars);
  local_prompt_prefix = (char *)NULL;
  local_prompt_len = local_prompt ? strlen (local_prompt) : 0;
  (*rl_redisplay_function) ();
      
  return 0;
}
#endif  

 
int
rl_clear_message (void)
{
  rl_display_prompt = rl_prompt;
  if (msg_saved_prompt)
    {
      rl_restore_prompt ();
      msg_saved_prompt = 0;
    }
  (*rl_redisplay_function) ();
  return 0;
}

int
rl_reset_line_state (void)
{
  rl_on_new_line ();

  rl_display_prompt = rl_prompt ? rl_prompt : "";
  forced_display = 1;
  return 0;
}

 
void
rl_save_prompt (void)
{
  saved_local_prompt = local_prompt;
  saved_local_prefix = local_prompt_prefix;
  saved_prefix_length = prompt_prefix_length;
  saved_local_length = local_prompt_len;
  saved_last_invisible = prompt_last_invisible;
  saved_visible_length = prompt_visible_length;
  saved_invis_chars_first_line = prompt_invis_chars_first_line;
  saved_physical_chars = prompt_physical_chars;
  saved_local_prompt_newlines = local_prompt_newlines;

  local_prompt = local_prompt_prefix = (char *)0;
  local_prompt_len = 0;
  local_prompt_newlines = (int *)0;

  prompt_last_invisible = prompt_visible_length = prompt_prefix_length = 0;
  prompt_invis_chars_first_line = prompt_physical_chars = 0;
}

void
rl_restore_prompt (void)
{
  FREE (local_prompt);
  FREE (local_prompt_prefix);
  FREE (local_prompt_newlines);

  local_prompt = saved_local_prompt;
  local_prompt_prefix = saved_local_prefix;
  local_prompt_len = saved_local_length;
  local_prompt_newlines = saved_local_prompt_newlines;

  prompt_prefix_length = saved_prefix_length;
  prompt_last_invisible = saved_last_invisible;
  prompt_visible_length = saved_visible_length;
  prompt_invis_chars_first_line = saved_invis_chars_first_line;
  prompt_physical_chars = saved_physical_chars;

   
  saved_local_prompt = saved_local_prefix = (char *)0;
  saved_local_length = 0;
  saved_last_invisible = saved_visible_length = saved_prefix_length = 0;
  saved_invis_chars_first_line = saved_physical_chars = 0;
  saved_local_prompt_newlines = 0;
}

char *
_rl_make_prompt_for_search (int pchar)
{
  int len;
  char *pmt, *p;

  rl_save_prompt ();

   
  p = rl_prompt ? strrchr (rl_prompt, '\n') : 0;
  if (p == 0)
    {
      len = (rl_prompt && *rl_prompt) ? strlen (rl_prompt) : 0;
      pmt = (char *)xmalloc (len + 2);
      if (len)
	strcpy (pmt, rl_prompt);
      pmt[len] = pchar;
      pmt[len+1] = '\0';
    }
  else
    {
      p++;
      len = strlen (p);
      pmt = (char *)xmalloc (len + 2);
      if (len)
	strcpy (pmt, p);
      pmt[len] = pchar;
      pmt[len+1] = '\0';
    }  

   
  prompt_physical_chars = saved_physical_chars + 1;
  return pmt;
}

 
void
_rl_erase_at_end_of_line (int l)
{
  register int i;

  _rl_backspace (l);
  for (i = 0; i < l; i++)
    putc (' ', rl_outstream);
  _rl_backspace (l);
  for (i = 0; i < l; i++)
    visible_line[--_rl_last_c_pos] = '\0';
  rl_display_fixed++;
}

 
void
_rl_clear_to_eol (int count)
{
#ifndef __MSDOS__
  if (_rl_term_clreol)
    tputs (_rl_term_clreol, 1, _rl_output_character_function);
  else
#endif
    if (count)
      space_to_eol (count);
}

 
static void
space_to_eol (int count)
{
  register int i;

  for (i = 0; i < count; i++)
    putc (' ', rl_outstream);

  _rl_last_c_pos += count;
}

void
_rl_clear_screen (int clrscr)
{
#if defined (__DJGPP__)
  ScreenClear ();
  ScreenSetCursor (0, 0);
#else
  if (_rl_term_clrpag)
    {
      tputs (_rl_term_clrpag, 1, _rl_output_character_function);
      if (clrscr && _rl_term_clrscroll)
	tputs (_rl_term_clrscroll, 1, _rl_output_character_function);
    }
  else
    rl_crlf ();
#endif  
}

 
static void
insert_some_chars (char *string, int count, int col)
{
  open_some_spaces (col);
  _rl_output_some_chars (string, count);
}

 
static void
open_some_spaces (int col)
{
#if !defined (__MSDOS__) && (!defined (__MINGW32__) || defined (NCURSES_VERSION))
  char *buffer;
  register int i;

   
  if (_rl_term_IC)
    {
      buffer = tgoto (_rl_term_IC, 0, col);
      tputs (buffer, 1, _rl_output_character_function);
    }
  else if (_rl_term_im && *_rl_term_im)
    {
      tputs (_rl_term_im, 1, _rl_output_character_function);
       
      for (i = col; i--; )
	_rl_output_character_function (' ');
       
      if (_rl_term_ei && *_rl_term_ei)
	tputs (_rl_term_ei, 1, _rl_output_character_function);
       
      _rl_backspace (col);
    }
  else if (_rl_term_ic && *_rl_term_ic)
    {
       
      for (i = col; i--; )
	tputs (_rl_term_ic, 1, _rl_output_character_function);
    }
#endif  
}

 
static void
delete_chars (int count)
{
  if (count > _rl_screenwidth)	 
    return;

#if !defined (__MSDOS__) && (!defined (__MINGW32__) || defined (NCURSES_VERSION))
  if (_rl_term_DC && *_rl_term_DC)
    {
      char *buffer;
      buffer = tgoto (_rl_term_DC, count, count);
      tputs (buffer, count, _rl_output_character_function);
    }
  else
    {
      if (_rl_term_dc && *_rl_term_dc)
	while (count--)
	  tputs (_rl_term_dc, 1, _rl_output_character_function);
    }
#endif  
}

void
_rl_update_final (void)
{
  int full_lines, woff, botline_length;

  if (line_structures_initialized == 0)
    return;

  full_lines = 0;
   
  if (_rl_vis_botlin && _rl_last_c_pos == 0 &&
	visible_line[vis_lbreaks[_rl_vis_botlin]] == 0)
    {
      _rl_vis_botlin--;
      full_lines = 1;
    }
  _rl_move_vert (_rl_vis_botlin);
  woff = W_OFFSET(_rl_vis_botlin, wrap_offset);
  botline_length = VIS_LLEN(_rl_vis_botlin) - woff;
   
  if (full_lines && _rl_term_autowrap && botline_length == _rl_screenwidth)
    {
      char *last_line, *last_face;

       
      last_line = &visible_line[vis_lbreaks[_rl_vis_botlin]];  
      last_face = &vis_face[vis_lbreaks[_rl_vis_botlin]];  
      cpos_buffer_position = -1;	 
      _rl_move_cursor_relative (_rl_screenwidth - 1 + woff, last_line, last_face);	 
      _rl_clear_to_eol (0);
      puts_face (&last_line[_rl_screenwidth - 1 + woff],
		 &last_face[_rl_screenwidth - 1 + woff], 1);
    }
  _rl_vis_botlin = 0;
  if (botline_length > 0 || _rl_last_c_pos > 0)
    rl_crlf ();
  fflush (rl_outstream);
  rl_display_fixed++;
}

 
static void
cr (void)
{
  _rl_cr ();
  _rl_last_c_pos = 0;
}

 
static void
redraw_prompt (char *t)
{
  char *oldp;

  oldp = rl_display_prompt;
  rl_save_prompt ();

  rl_display_prompt = t;
  local_prompt = expand_prompt (t, PMT_MULTILINE,
				   &prompt_visible_length,
				   &prompt_last_invisible,
				   &prompt_invis_chars_first_line,
				   &prompt_physical_chars);
  local_prompt_prefix = (char *)NULL;
  local_prompt_len = local_prompt ? strlen (local_prompt) : 0;

  rl_forced_update_display ();

  rl_display_prompt = oldp;
  rl_restore_prompt();
}
      
 
void
_rl_redisplay_after_sigwinch (void)
{
  char *t;

   
  if (_rl_term_cr)
    {
      rl_clear_visible_line ();
      if (_rl_last_v_pos > 0)
	_rl_move_vert (0);
    }
  else
    rl_crlf ();

  if (_rl_screenwidth < prompt_visible_length)
    _rl_reset_prompt ();		 

   
  t = strrchr (rl_display_prompt, '\n');
  if (t)
    redraw_prompt (++t);
  else
    rl_forced_update_display ();
}

void
_rl_clean_up_for_exit (void)
{
  if (_rl_echoing_p)
    {
      if (_rl_vis_botlin > 0)	 
	_rl_move_vert (_rl_vis_botlin);
      _rl_vis_botlin = 0;
      fflush (rl_outstream);
      rl_restart_output (1, 0);
    }
}

void
_rl_erase_entire_line (void)
{
  cr ();
  _rl_clear_to_eol (0);
  cr ();
  fflush (rl_outstream);
}

void
_rl_ttyflush (void)
{
  fflush (rl_outstream);
}

 
int
_rl_current_display_line (void)
{
  int ret, nleft;

   
  if (rl_display_prompt == rl_prompt)
    nleft = _rl_last_c_pos - _rl_screenwidth - rl_visible_prompt_length;
  else
    nleft = _rl_last_c_pos - _rl_screenwidth;

  if (nleft > 0)
    ret = 1 + nleft / _rl_screenwidth;
  else
    ret = 0;

  return ret;
}

void
_rl_refresh_line (void)
{
  rl_clear_visible_line ();
  rl_redraw_prompt_last_line ();
  rl_keep_mark_active ();
}

#if defined (HANDLE_MULTIBYTE)
 
static int
_rl_col_width (const char *str, int start, int end, int flags)
{
  WCHAR_T wc;
  mbstate_t ps;
  int tmp, point, width, max;

  if (end <= start)
    return 0;
  if (MB_CUR_MAX == 1 || rl_byte_oriented)
     
    return (end - start);

  memset (&ps, 0, sizeof (mbstate_t));

  point = 0;
  max = end;

   
   
  if (flags && start == 0 && end == local_prompt_len && memcmp (str, local_prompt, local_prompt_len) == 0)
    return (prompt_physical_chars + wrap_offset);
   
  else if (flags && start == 0 && local_prompt_len > 0 && end > local_prompt_len && local_prompt && memcmp (str, local_prompt, local_prompt_len) == 0)
    {
      tmp = prompt_physical_chars + wrap_offset;
       
      tmp += _rl_col_width (str, local_prompt_len, end, flags);
      return (tmp);
    }

  while (point < start)
    {
      if (_rl_utf8locale && UTF8_SINGLEBYTE(str[point]))
	{
	  memset (&ps, 0, sizeof (mbstate_t));
	  tmp = 1;
	}
      else
	tmp = mbrlen (str + point, max, &ps);
      if (MB_INVALIDCH ((size_t)tmp))
	{
	   
	  point++;
	  max--;

	   
	  memset (&ps, 0, sizeof (mbstate_t));
	}
      else if (MB_NULLWCH (tmp))
	break;		 
      else
	{
	  point += tmp;
	  max -= tmp;
	}
    }

   
  width = point - start;

  while (point < end)
    {
      if (_rl_utf8locale && UTF8_SINGLEBYTE(str[point]))
	{
	  tmp = 1;
	  wc = (WCHAR_T) str[point];
	}
      else
	tmp = MBRTOWC (&wc, str + point, max, &ps);
      if (MB_INVALIDCH ((size_t)tmp))
	{
	   
	  point++;
	  max--;

	   
	  width++;

	   
	  memset (&ps, 0, sizeof (mbstate_t));
	}
      else if (MB_NULLWCH (tmp))
	break;			 
      else
	{
	  point += tmp;
	  max -= tmp;
	  tmp = WCWIDTH(wc);
	  width += (tmp >= 0) ? tmp : 1;
	}
    }

  width += point - end;

  return width;
}
#endif  
