 

 

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

 
#include "rldefs.h"

#ifdef __MSDOS__
#  include <pc.h>
#endif

#include "rltty.h"
#if defined (HAVE_SYS_IOCTL_H)
#  include <sys/ioctl.h>		 
#endif
#include "tcap.h"

 
#include "readline.h"
#include "history.h"

#include "rlprivate.h"
#include "rlshell.h"
#include "xmalloc.h"

#if defined (__MINGW32__)
#  include <windows.h>
#  include <wincon.h>

static void _win_get_screensize (int *, int *);
#endif

#if defined (__EMX__)
static void _emx_get_screensize (int *, int *);
#endif

 
int rl_prefer_env_winsize = 0;

 
int rl_change_environment = 1;

 
 
 
 
 

#ifndef __MSDOS__
static char *term_buffer = (char *)NULL;
static char *term_string_buffer = (char *)NULL;
#endif

static int tcap_initialized;

#if !defined (__linux__) && !defined (NCURSES_VERSION)
#  if defined (__EMX__) || defined (NEED_EXTERN_PC)
extern 
#  endif  
char PC, *BC, *UP;
#endif  

 
char *_rl_term_clreol;
char *_rl_term_clrpag;
char *_rl_term_clrscroll;
char *_rl_term_cr;
char *_rl_term_backspace;
char *_rl_term_goto;
char *_rl_term_pc;

 
int _rl_terminal_can_insert = 0;

 
char *_rl_term_im;
char *_rl_term_ei;
char *_rl_term_ic;
char *_rl_term_ip;
char *_rl_term_IC;

 
char *_rl_term_dc;
char *_rl_term_DC;

 
char *_rl_term_forward_char;

 
char *_rl_term_up;

 
static char *_rl_visible_bell;

 
int _rl_term_autowrap = -1;

 
static int term_has_meta;

 
static char *_rl_term_mm;
static char *_rl_term_mo;

 
static char *_rl_term_so;
static char *_rl_term_se;

 
static char *_rl_term_ku;
static char *_rl_term_kd;
static char *_rl_term_kr;
static char *_rl_term_kl;

 
static char *_rl_term_ks;
static char *_rl_term_ke;

 
static char *_rl_term_kh;
static char *_rl_term_kH;
static char *_rl_term_at7;	 

 
static char *_rl_term_kD;

 
static char *_rl_term_kI;

 
static char *_rl_term_kP;
static char *_rl_term_kN;

 
static char *_rl_term_vs;	 
static char *_rl_term_ve;	 

 
char *_rl_active_region_start_color = NULL;
char *_rl_active_region_end_color = NULL;

 
#ifdef TGETENT_BROKEN
#  define TGETENT_SUCCESS 0
#else
#  define TGETENT_SUCCESS 1
#endif
#ifdef TGETFLAG_BROKEN
#  define TGETFLAG_SUCCESS 0
#else
#  define TGETFLAG_SUCCESS 1
#endif
#define TGETFLAG(cap)	(tgetflag (cap) == TGETFLAG_SUCCESS)

static void bind_termcap_arrow_keys (Keymap);

 
int _rl_screenwidth, _rl_screenheight, _rl_screenchars;

 
int _rl_enable_keypad;

 
int _rl_enable_meta = 1;

#if defined (__EMX__)
static void
_emx_get_screensize (int *swp, int *shp)
{
  int sz[2];

  _scrsize (sz);

  if (swp)
    *swp = sz[0];
  if (shp)
    *shp = sz[1];
}
#endif

#if defined (__MINGW32__)
static void
_win_get_screensize (int *swp, int *shp)
{
  HANDLE hConOut;
  CONSOLE_SCREEN_BUFFER_INFO scr;

  hConOut = GetStdHandle (STD_OUTPUT_HANDLE);
  if (hConOut != INVALID_HANDLE_VALUE)
    {
      if (GetConsoleScreenBufferInfo (hConOut, &scr))
	{
	  *swp = scr.dwSize.X;
	  *shp = scr.srWindow.Bottom - scr.srWindow.Top + 1;
	}
    }
}
#endif

 
void
_rl_get_screen_size (int tty, int ignore_env)
{
  char *ss;
#if defined (TIOCGWINSZ)
  struct winsize window_size;
#endif  
  int wr, wc;

  wr = wc = -1;
#if defined (TIOCGWINSZ)
  if (ioctl (tty, TIOCGWINSZ, &window_size) == 0)
    {
      wc = (int) window_size.ws_col;
      wr = (int) window_size.ws_row;
    }
#endif  

#if defined (__EMX__)
  _emx_get_screensize (&wc, &wr);
#elif defined (__MINGW32__)
  _win_get_screensize (&wc, &wr);
#endif

  if (ignore_env || rl_prefer_env_winsize == 0)
    {
      _rl_screenwidth = wc;
      _rl_screenheight = wr;
    }
  else
    _rl_screenwidth = _rl_screenheight = -1;

   
  if (_rl_screenwidth <= 0)
    {
      if (ignore_env == 0 && (ss = sh_get_env_value ("COLUMNS")))
	_rl_screenwidth = atoi (ss);

      if (_rl_screenwidth <= 0)
        _rl_screenwidth = wc;

#if defined (__DJGPP__)
      if (_rl_screenwidth <= 0)
	_rl_screenwidth = ScreenCols ();
#else
      if (_rl_screenwidth <= 0 && term_string_buffer)
	_rl_screenwidth = tgetnum ("co");
#endif
    }

   
  if (_rl_screenheight <= 0)
    {
      if (ignore_env == 0 && (ss = sh_get_env_value ("LINES")))
	_rl_screenheight = atoi (ss);

      if (_rl_screenheight <= 0)
        _rl_screenheight = wr;

#if defined (__DJGPP__)
      if (_rl_screenheight <= 0)
	_rl_screenheight = ScreenRows ();
#else
      if (_rl_screenheight <= 0 && term_string_buffer)
	_rl_screenheight = tgetnum ("li");
#endif
    }

   
  if (_rl_screenwidth <= 1)
    _rl_screenwidth = 80;

  if (_rl_screenheight <= 0)
    _rl_screenheight = 24;

   
  if (rl_change_environment)
    sh_set_lines_and_columns (_rl_screenheight, _rl_screenwidth);

  if (_rl_term_autowrap == 0)
    _rl_screenwidth--;

  _rl_screenchars = _rl_screenwidth * _rl_screenheight;
}

void
_rl_set_screen_size (int rows, int cols)
{
  if (_rl_term_autowrap == -1)
    _rl_init_terminal_io (rl_terminal_name);

  if (rows > 0)
    _rl_screenheight = rows;
  if (cols > 0)
    {
      _rl_screenwidth = cols;
      if (_rl_term_autowrap == 0)
	_rl_screenwidth--;
    }

  if (rows > 0 || cols > 0)
    _rl_screenchars = _rl_screenwidth * _rl_screenheight;
}

void
rl_set_screen_size (int rows, int cols)
{
  _rl_set_screen_size (rows, cols);
}

void
rl_get_screen_size (int *rows, int *cols)
{
  if (rows)
    *rows = _rl_screenheight;
  if (cols)
    *cols = _rl_screenwidth;
}

void
rl_reset_screen_size (void)
{
  _rl_get_screen_size (fileno (rl_instream), 0);
}

void
_rl_sigwinch_resize_terminal (void)
{
  _rl_get_screen_size (fileno (rl_instream), 1);
}
	
void
rl_resize_terminal (void)
{
  int width, height;

  width = _rl_screenwidth;
  height = _rl_screenheight;
  _rl_get_screen_size (fileno (rl_instream), 1);
  if (_rl_echoing_p && (width != _rl_screenwidth || height != _rl_screenheight))
    {
      if (CUSTOM_REDISPLAY_FUNC ())
	rl_forced_update_display ();
      else if (RL_ISSTATE(RL_STATE_REDISPLAYING) == 0)
	_rl_redisplay_after_sigwinch ();
    }
}

struct _tc_string {
     const char * const tc_var;
     char **tc_value;
};

 
static const struct _tc_string tc_strings[] =
{
  { "@7", &_rl_term_at7 },
  { "DC", &_rl_term_DC },
  { "E3", &_rl_term_clrscroll },
  { "IC", &_rl_term_IC },
  { "ce", &_rl_term_clreol },
  { "cl", &_rl_term_clrpag },
  { "cr", &_rl_term_cr },
  { "dc", &_rl_term_dc },
  { "ei", &_rl_term_ei },
  { "ic", &_rl_term_ic },
  { "im", &_rl_term_im },
  { "kD", &_rl_term_kD },	 
  { "kH", &_rl_term_kH },	 
  { "kI", &_rl_term_kI },	 
  { "kN", &_rl_term_kN },	 
  { "kP", &_rl_term_kP },	 
  { "kd", &_rl_term_kd },
  { "ke", &_rl_term_ke },	 
  { "kh", &_rl_term_kh },	 
  { "kl", &_rl_term_kl },
  { "kr", &_rl_term_kr },
  { "ks", &_rl_term_ks },	 
  { "ku", &_rl_term_ku },
  { "le", &_rl_term_backspace },
  { "mm", &_rl_term_mm },
  { "mo", &_rl_term_mo },
  { "nd", &_rl_term_forward_char },
  { "pc", &_rl_term_pc },
  { "se", &_rl_term_se },
  { "so", &_rl_term_so },
  { "up", &_rl_term_up },
  { "vb", &_rl_visible_bell },
  { "vs", &_rl_term_vs },
  { "ve", &_rl_term_ve },
};

#define NUM_TC_STRINGS (sizeof (tc_strings) / sizeof (struct _tc_string))

 
static void
get_term_capabilities (char **bp)
{
#if !defined (__DJGPP__)	 
  register int i;

  for (i = 0; i < NUM_TC_STRINGS; i++)
    *(tc_strings[i].tc_value) = tgetstr ((char *)tc_strings[i].tc_var, bp);
#endif
  tcap_initialized = 1;
}

int
_rl_init_terminal_io (const char *terminal_name)
{
  const char *term;
  char *buffer;
  int tty, tgetent_ret, dumbterm, reset_region_colors;

  term = terminal_name ? terminal_name : sh_get_env_value ("TERM");
  _rl_term_clrpag = _rl_term_cr = _rl_term_clreol = _rl_term_clrscroll = (char *)NULL;
  tty = rl_instream ? fileno (rl_instream) : 0;

  if (term == 0)
    term = "dumb";

  dumbterm = STREQ (term, "dumb");

  reset_region_colors = 1;

#ifdef __MSDOS__
  _rl_term_im = _rl_term_ei = _rl_term_ic = _rl_term_IC = (char *)NULL;
  _rl_term_up = _rl_term_dc = _rl_term_DC = _rl_visible_bell = (char *)NULL;
  _rl_term_ku = _rl_term_kd = _rl_term_kl = _rl_term_kr = (char *)NULL;
  _rl_term_mm = _rl_term_mo = (char *)NULL;
  _rl_terminal_can_insert = term_has_meta = _rl_term_autowrap = 0;
  _rl_term_cr = "\r";
  _rl_term_backspace = (char *)NULL;
  _rl_term_goto = _rl_term_pc = _rl_term_ip = (char *)NULL;
  _rl_term_ks = _rl_term_ke =_rl_term_vs = _rl_term_ve = (char *)NULL;
  _rl_term_kh = _rl_term_kH = _rl_term_at7 = _rl_term_kI = (char *)NULL;
  _rl_term_kN = _rl_term_kP = (char *)NULL;
  _rl_term_so = _rl_term_se = (char *)NULL;
#if defined(HACK_TERMCAP_MOTION)
  _rl_term_forward_char = (char *)NULL;
#endif

  _rl_get_screen_size (tty, 0);
#else   
   
  if (CUSTOM_REDISPLAY_FUNC())
    {
      tgetent_ret = -1;
    }
  else
    {
      if (term_string_buffer == 0)
	term_string_buffer = (char *)xmalloc(2032);

      if (term_buffer == 0)
	term_buffer = (char *)xmalloc(4080);

      buffer = term_string_buffer;

      tgetent_ret = tgetent (term_buffer, term);
    }

  if (tgetent_ret != TGETENT_SUCCESS)
    {
      FREE (term_string_buffer);
      FREE (term_buffer);
      buffer = term_buffer = term_string_buffer = (char *)NULL;

      _rl_term_autowrap = 0;	 

       
      if (_rl_screenwidth <= 0 || _rl_screenheight <= 0)
	{
#if defined (__EMX__)
	  _emx_get_screensize (&_rl_screenwidth, &_rl_screenheight);
	  _rl_screenwidth--;
#else  
	  _rl_get_screen_size (tty, 0);
#endif  
	}

       
      if (_rl_screenwidth <= 0 || _rl_screenheight <= 0)
        {
	  _rl_screenwidth = 79;
	  _rl_screenheight = 24;
        }

       
      _rl_screenchars = _rl_screenwidth * _rl_screenheight;
      _rl_term_cr = "\r";
      _rl_term_im = _rl_term_ei = _rl_term_ic = _rl_term_IC = (char *)NULL;
      _rl_term_up = _rl_term_dc = _rl_term_DC = _rl_visible_bell = (char *)NULL;
      _rl_term_ku = _rl_term_kd = _rl_term_kl = _rl_term_kr = (char *)NULL;
      _rl_term_kh = _rl_term_kH = _rl_term_kI = _rl_term_kD = (char *)NULL;
      _rl_term_ks = _rl_term_ke = _rl_term_at7 = (char *)NULL;
      _rl_term_kN = _rl_term_kP = (char *)NULL;
      _rl_term_mm = _rl_term_mo = (char *)NULL;
      _rl_term_ve = _rl_term_vs = (char *)NULL;
      _rl_term_forward_char = (char *)NULL;
      _rl_term_so = _rl_term_se = (char *)NULL;
      _rl_terminal_can_insert = term_has_meta = 0;

       
      _rl_enable_bracketed_paste = 0;

       
      _rl_enable_active_region = 0;
      _rl_reset_region_color (0, NULL);
      _rl_reset_region_color (1, NULL);
    
       
      PC = '\0';
      BC = _rl_term_backspace = "\b";
      UP = _rl_term_up;

      return 0;
    }

  get_term_capabilities (&buffer);

   
  PC = _rl_term_pc ? *_rl_term_pc : 0;
  BC = _rl_term_backspace;
  UP = _rl_term_up;

  if (_rl_term_cr == 0)
    _rl_term_cr = "\r";

  _rl_term_autowrap = TGETFLAG ("am") && TGETFLAG ("xn");

   
  if (_rl_screenwidth <= 0 || _rl_screenheight <= 0)
    _rl_get_screen_size (tty, 0);

   
  _rl_terminal_can_insert = (_rl_term_IC || _rl_term_im || _rl_term_ic);

   
  term_has_meta = TGETFLAG ("km");
  if (term_has_meta == 0)
    _rl_term_mm = _rl_term_mo = (char *)NULL;
#endif  

   

  bind_termcap_arrow_keys (emacs_standard_keymap);

#if defined (VI_MODE)
  bind_termcap_arrow_keys (vi_movement_keymap);
  bind_termcap_arrow_keys (vi_insertion_keymap);
#endif  

   
  if (dumbterm)
    _rl_enable_bracketed_paste = _rl_enable_active_region = 0;

  if (reset_region_colors)
    {
      _rl_reset_region_color (0, _rl_term_so);
      _rl_reset_region_color (1, _rl_term_se);
    }

  return 0;
}

 
static void
bind_termcap_arrow_keys (Keymap map)
{
  Keymap xkeymap;

  xkeymap = _rl_keymap;
  _rl_keymap = map;

  rl_bind_keyseq_if_unbound (_rl_term_ku, rl_get_previous_history);
  rl_bind_keyseq_if_unbound (_rl_term_kd, rl_get_next_history);
  rl_bind_keyseq_if_unbound (_rl_term_kr, rl_forward_char);
  rl_bind_keyseq_if_unbound (_rl_term_kl, rl_backward_char);

  rl_bind_keyseq_if_unbound (_rl_term_kh, rl_beg_of_line);	 
  rl_bind_keyseq_if_unbound (_rl_term_at7, rl_end_of_line);	 

  rl_bind_keyseq_if_unbound (_rl_term_kD, rl_delete);
  rl_bind_keyseq_if_unbound (_rl_term_kI, rl_overwrite_mode);	 

  rl_bind_keyseq_if_unbound (_rl_term_kN, rl_history_search_forward);	 
  rl_bind_keyseq_if_unbound (_rl_term_kP, rl_history_search_backward);	 

  _rl_keymap = xkeymap;
}

char *
rl_get_termcap (const char *cap)
{
  register int i;

  if (tcap_initialized == 0)
    return ((char *)NULL);
  for (i = 0; i < NUM_TC_STRINGS; i++)
    {
      if (tc_strings[i].tc_var[0] == cap[0] && strcmp (tc_strings[i].tc_var, cap) == 0)
        return *(tc_strings[i].tc_value);
    }
  return ((char *)NULL);
}

 
int
rl_reset_terminal (const char *terminal_name)
{
  _rl_screenwidth = _rl_screenheight = 0;
  _rl_init_terminal_io (terminal_name);
  return 0;
}

 
#ifdef _MINIX
void
_rl_output_character_function (int c)
{
  putc (c, _rl_out_stream);
}
#else  
int
_rl_output_character_function (int c)
{
  return putc (c, _rl_out_stream);
}
#endif  

 
void
_rl_output_some_chars (const char *string, int count)
{
  fwrite (string, 1, count, _rl_out_stream);
}

 
int
_rl_backspace (int count)
{
  register int i;

#ifndef __MSDOS__
  if (_rl_term_backspace)
    for (i = 0; i < count; i++)
      tputs (_rl_term_backspace, 1, _rl_output_character_function);
  else
#endif
    for (i = 0; i < count; i++)
      putc ('\b', _rl_out_stream);
  return 0;
}

 
int
rl_crlf (void)
{
#if defined (NEW_TTY_DRIVER) || defined (__MINT__)
  if (_rl_term_cr)
    tputs (_rl_term_cr, 1, _rl_output_character_function);
#endif  
  putc ('\n', _rl_out_stream);
  return 0;
}

void
_rl_cr (void)
{
#if defined (__MSDOS__)
  putc ('\r', rl_outstream);
#else
  tputs (_rl_term_cr, 1, _rl_output_character_function);
#endif
}

 
int
rl_ding (void)
{
  if (_rl_echoing_p)
    {
      switch (_rl_bell_preference)
        {
	case NO_BELL:
	default:
	  break;
	case VISIBLE_BELL:
	  if (_rl_visible_bell)
	    {
#ifdef __DJGPP__
	      ScreenVisualBell ();
#else
	      tputs (_rl_visible_bell, 1, _rl_output_character_function);
#endif
	      break;
	    }
	   
	case AUDIBLE_BELL:
	  fprintf (stderr, "\007");
	  fflush (stderr);
	  break;
        }
      return (0);
    }
  return (-1);
}

 
 
 
 
 

void
_rl_standout_on (void)
{
#ifndef __MSDOS__
  if (_rl_term_so && _rl_term_se)
    tputs (_rl_term_so, 1, _rl_output_character_function);
#endif
}

void
_rl_standout_off (void)
{
#ifndef __MSDOS__
  if (_rl_term_so && _rl_term_se)
    tputs (_rl_term_se, 1, _rl_output_character_function);
#endif
}

 
 
 
 
 

 
int
_rl_reset_region_color (int which, const char *value)
{
  int len;

  if (which == 0)
    {
      xfree (_rl_active_region_start_color);
      if (value && *value)
	{
	  _rl_active_region_start_color = (char *)xmalloc (2 * strlen (value) + 1);
	  rl_translate_keyseq (value, _rl_active_region_start_color, &len);
	  _rl_active_region_start_color[len] = '\0';
	}
      else
	_rl_active_region_start_color = NULL;
    }
  else
    {
      xfree (_rl_active_region_end_color);
      if (value && *value)
	{
	  _rl_active_region_end_color = (char *)xmalloc (2 * strlen (value) + 1);
	  rl_translate_keyseq (value, _rl_active_region_end_color, &len);
	  _rl_active_region_end_color[len] = '\0';
	}
      else
	_rl_active_region_end_color = NULL;
    }

  return 0;
}

void
_rl_region_color_on (void)
{
#ifndef __MSDOS__
  if (_rl_active_region_start_color && _rl_active_region_end_color)
    tputs (_rl_active_region_start_color, 1, _rl_output_character_function);
#endif
}

void
_rl_region_color_off (void)
{
#ifndef __MSDOS__
  if (_rl_active_region_start_color && _rl_active_region_end_color)
    tputs (_rl_active_region_end_color, 1, _rl_output_character_function);
#endif
}

 
 
 
 
 

static int enabled_meta = 0;	 

void
_rl_enable_meta_key (void)
{
#if !defined (__DJGPP__)
  if (term_has_meta && _rl_term_mm)
    {
      tputs (_rl_term_mm, 1, _rl_output_character_function);
      enabled_meta = 1;
    }
#endif
}

void
_rl_disable_meta_key (void)
{
#if !defined (__DJGPP__)
  if (term_has_meta && _rl_term_mo && enabled_meta)
    {
      tputs (_rl_term_mo, 1, _rl_output_character_function);
      enabled_meta = 0;
    }
#endif
}

void
_rl_control_keypad (int on)
{
#if !defined (__DJGPP__)
  if (on && _rl_term_ks)
    tputs (_rl_term_ks, 1, _rl_output_character_function);
  else if (!on && _rl_term_ke)
    tputs (_rl_term_ke, 1, _rl_output_character_function);
#endif
}

 
 
 
 
 

 
void
_rl_set_cursor (int im, int force)
{
#ifndef __MSDOS__
  if (_rl_term_ve && _rl_term_vs)
    {
      if (force || im != rl_insert_mode)
	{
	  if (im == RL_IM_OVERWRITE)
	    tputs (_rl_term_vs, 1, _rl_output_character_function);
	  else
	    tputs (_rl_term_ve, 1, _rl_output_character_function);
	}
    }
#endif
}
