 

 

#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include "rlconf.h"

#if defined (READLINE_CALLBACKS)

#include <sys/types.h>

#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif

#include <stdio.h>

 
#include "rldefs.h"
#include "readline.h"
#include "rlprivate.h"
#include "xmalloc.h"

 
_rl_callback_func_t *_rl_callback_func = 0;
_rl_callback_generic_arg *_rl_callback_data = 0;

    
int rl_persistent_signal_handlers = 0;

 
 
 
 
 

 

rl_vcpfunc_t *rl_linefunc;		 
static int in_handler;		 

 
static void
_rl_callback_newline (void)
{
  rl_initialize ();

  if (in_handler == 0)
    {
      in_handler = 1;

      if (rl_prep_term_function)
	(*rl_prep_term_function) (_rl_meta_flag);

#if defined (HANDLE_SIGNALS)
      if (rl_persistent_signal_handlers)
	rl_set_signals ();
#endif
    }

  readline_internal_setup ();
  RL_CHECK_SIGNALS ();
}

 
void
rl_callback_handler_install (const char *prompt, rl_vcpfunc_t *linefunc)
{
  rl_set_prompt (prompt);
  RL_SETSTATE (RL_STATE_CALLBACK);
  rl_linefunc = linefunc;
  _rl_callback_newline ();
}

#if defined (HANDLE_SIGNALS)
#define CALLBACK_READ_RETURN() \
  do { \
    if (rl_persistent_signal_handlers == 0) \
      rl_clear_signals (); \
    return; \
  } while (0)
#else
#define CALLBACK_READ_RETURN() return
#endif

 
void
rl_callback_read_char (void)
{
  char *line;
  int eof, jcode;
  static procenv_t olevel;

  if (rl_linefunc == NULL)
    {
      _rl_errmsg ("readline_callback_read_char() called with no handler!");
      abort ();
    }

  eof = 0;

  memcpy ((void *)olevel, (void *)_rl_top_level, sizeof (procenv_t));
#if defined (HAVE_POSIX_SIGSETJMP)
  jcode = sigsetjmp (_rl_top_level, 0);
#else
  jcode = setjmp (_rl_top_level);
#endif
  if (jcode)
    {
      (*rl_redisplay_function) ();
      _rl_want_redisplay = 0;
      memcpy ((void *)_rl_top_level, (void *)olevel, sizeof (procenv_t));

       
      if (RL_ISSTATE (RL_STATE_TIMEOUT))
	{
	  RL_SETSTATE (RL_STATE_DONE);
	  rl_done = 1;
	}

      CALLBACK_READ_RETURN ();
    }

#if defined (HANDLE_SIGNALS)
   
  if (rl_persistent_signal_handlers == 0)
    rl_set_signals ();
#endif

  do
    {
      RL_CHECK_SIGNALS ();
      if  (RL_ISSTATE (RL_STATE_ISEARCH))
	{
	  eof = _rl_isearch_callback (_rl_iscxt);
	  if (eof == 0 && (RL_ISSTATE (RL_STATE_ISEARCH) == 0) && RL_ISSTATE (RL_STATE_INPUTPENDING))
	    rl_callback_read_char ();

	  CALLBACK_READ_RETURN ();
	}
      else if  (RL_ISSTATE (RL_STATE_NSEARCH))
	{
	  eof = _rl_nsearch_callback (_rl_nscxt);

	  CALLBACK_READ_RETURN ();
	}
#if defined (VI_MODE)
       
      else if (RL_ISSTATE (RL_STATE_CHARSEARCH))
	{
	  int k;

	  k = _rl_callback_data->i2;

	  eof = (*_rl_callback_func) (_rl_callback_data);
	   
	  if (_rl_callback_func == 0)	 
	    {
	      if (_rl_callback_data)
		{
		  _rl_callback_data_dispose (_rl_callback_data);
		  _rl_callback_data = 0;
		}
	    }

	   
	  if (RL_ISSTATE (RL_STATE_VIMOTION))
	    {
	      _rl_vi_domove_motion_cleanup (k, _rl_vimvcxt);
	      _rl_internal_char_cleanup ();
	      CALLBACK_READ_RETURN ();	      
	    }

	  _rl_internal_char_cleanup ();
	}
      else if (RL_ISSTATE (RL_STATE_VIMOTION))
	{
	  eof = _rl_vi_domove_callback (_rl_vimvcxt);
	   
	  if (RL_ISSTATE (RL_STATE_NUMERICARG) == 0)
	    _rl_internal_char_cleanup ();

	  CALLBACK_READ_RETURN ();
	}
#endif
      else if (RL_ISSTATE (RL_STATE_NUMERICARG))
	{
	  eof = _rl_arg_callback (_rl_argcxt);
	  if (eof == 0 && (RL_ISSTATE (RL_STATE_NUMERICARG) == 0) && RL_ISSTATE (RL_STATE_INPUTPENDING))
	    rl_callback_read_char ();
	   
	  else if (RL_ISSTATE (RL_STATE_NUMERICARG) == 0)
	    _rl_internal_char_cleanup ();

	  CALLBACK_READ_RETURN ();
	}
      else if (RL_ISSTATE (RL_STATE_MULTIKEY))
	{
	  eof = _rl_dispatch_callback (_rl_kscxt);	 
	  while ((eof == -1 || eof == -2) && RL_ISSTATE (RL_STATE_MULTIKEY) && _rl_kscxt && (_rl_kscxt->flags & KSEQ_DISPATCHED))
	    eof = _rl_dispatch_callback (_rl_kscxt);
	  if (RL_ISSTATE (RL_STATE_MULTIKEY) == 0)
	    {
	      _rl_internal_char_cleanup ();
	      _rl_want_redisplay = 1;
	    }
	}
      else if (_rl_callback_func)
	{
	   
	  eof = (*_rl_callback_func) (_rl_callback_data);
	   
	  if (_rl_callback_func == 0)
	    {
	      if (_rl_callback_data) 	
		{
		  _rl_callback_data_dispose (_rl_callback_data);
		  _rl_callback_data = 0;
		}
	      _rl_internal_char_cleanup ();
	    }
	}
      else
	eof = readline_internal_char ();

      RL_CHECK_SIGNALS ();
      if (rl_done == 0 && _rl_want_redisplay)
	{
	  (*rl_redisplay_function) ();
	  _rl_want_redisplay = 0;
	}

       
      if (eof > 0)
	{
	  rl_eof_found = eof;
	  RL_SETSTATE(RL_STATE_EOF);
	}

      if (rl_done)
	{
	  line = readline_internal_teardown (eof);

	  if (rl_deprep_term_function)
	    (*rl_deprep_term_function) ();
#if defined (HANDLE_SIGNALS)
	  rl_clear_signals ();
#endif
	  in_handler = 0;
	  if (rl_linefunc)			 
	    (*rl_linefunc) (line);

	   
	  if (rl_line_buffer[0])
	    _rl_init_line_state ();

	   
	  if (in_handler == 0 && rl_linefunc)
	    _rl_callback_newline ();
	}
    }
  while (rl_pending_input || _rl_pushed_input_available () || RL_ISSTATE (RL_STATE_MACROINPUT));

  CALLBACK_READ_RETURN ();
}

 
void
rl_callback_handler_remove (void)
{
  rl_linefunc = NULL;
  RL_UNSETSTATE (RL_STATE_CALLBACK);
  RL_CHECK_SIGNALS ();
  if (in_handler)
    {
      in_handler = 0;
      if (rl_deprep_term_function)
	(*rl_deprep_term_function) ();
#if defined (HANDLE_SIGNALS)
      rl_clear_signals ();
#endif
    }
}

_rl_callback_generic_arg *
_rl_callback_data_alloc (int count)
{
  _rl_callback_generic_arg *arg;

  arg = (_rl_callback_generic_arg *)xmalloc (sizeof (_rl_callback_generic_arg));
  arg->count = count;

  arg->i1 = arg->i2 = 0;

  return arg;
}

void
_rl_callback_data_dispose (_rl_callback_generic_arg *arg)
{
  xfree (arg);
}

 
void
rl_callback_sigcleanup (void)
{
  if (RL_ISSTATE (RL_STATE_CALLBACK) == 0)
    return;

  if (RL_ISSTATE (RL_STATE_ISEARCH))
    _rl_isearch_cleanup (_rl_iscxt, 0);
  else if (RL_ISSTATE (RL_STATE_NSEARCH))
    _rl_nsearch_cleanup (_rl_nscxt, 0);
  else if (RL_ISSTATE (RL_STATE_VIMOTION))
    RL_UNSETSTATE (RL_STATE_VIMOTION);
  else if (RL_ISSTATE (RL_STATE_NUMERICARG))
    {
      _rl_argcxt = 0;
      RL_UNSETSTATE (RL_STATE_NUMERICARG);
    }
  else if (RL_ISSTATE (RL_STATE_MULTIKEY))
    RL_UNSETSTATE (RL_STATE_MULTIKEY);
  if (RL_ISSTATE (RL_STATE_CHARSEARCH))
    RL_UNSETSTATE (RL_STATE_CHARSEARCH);

  _rl_callback_func = 0;
}
#endif
