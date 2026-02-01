 

 

 

#include <curses.priv.h>

#ifndef CUR
#define CUR SP_TERMTYPE
#endif

#include <ctype.h>
#include <termcap.h>		 
#include <tic.h>

MODULE_ID("$Id: lib_tputs.c,v 1.109 2022/07/21 23:26:34 tom Exp $")

NCURSES_EXPORT_VAR(char) PC = 0;               
NCURSES_EXPORT_VAR(NCURSES_OSPEED) ospeed = 0;         

NCURSES_EXPORT_VAR(int) _nc_nulls_sent = 0;    

#if NCURSES_NO_PADDING
NCURSES_EXPORT(void)
_nc_set_no_padding(SCREEN *sp)
{
    bool no_padding = (getenv("NCURSES_NO_PADDING") != 0);

    if (sp)
	sp->_no_padding = no_padding;
    else
	_nc_prescreen._no_padding = no_padding;

    TR(TRACE_CHARPUT | TRACE_MOVE, ("padding will%s be used",
				    GetNoPadding(sp) ? " not" : ""));
}
#endif

#if NCURSES_SP_FUNCS
#define SetOutCh(func) if (SP_PARM) SP_PARM->_outch = func; else _nc_prescreen._outch = func
#define GetOutCh()     (SP_PARM ? SP_PARM->_outch : _nc_prescreen._outch)
#else
#define SetOutCh(func) static_outch = func
#define GetOutCh()     static_outch
static NCURSES_SP_OUTC static_outch = NCURSES_SP_NAME(_nc_outch);
#endif

NCURSES_EXPORT(int)
NCURSES_SP_NAME(delay_output) (NCURSES_SP_DCLx int ms)
{
    T((T_CALLED("delay_output(%p,%d)"), (void *) SP_PARM, ms));

    if (!HasTInfoTerminal(SP_PARM))
	returnCode(ERR);

    if (no_pad_char) {
	NCURSES_SP_NAME(_nc_flush) (NCURSES_SP_ARG);
	napms(ms);
    } else {
	NCURSES_SP_OUTC my_outch = GetOutCh();
	register int nullcount;

	nullcount = (ms * _nc_baudrate(ospeed)) / (BAUDBYTE * 1000);
	for (_nc_nulls_sent += nullcount; nullcount > 0; nullcount--)
	    my_outch(NCURSES_SP_ARGx PC);
	if (my_outch == NCURSES_SP_NAME(_nc_outch))
	    NCURSES_SP_NAME(_nc_flush) (NCURSES_SP_ARG);
    }

    returnCode(OK);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
delay_output(int ms)
{
    return NCURSES_SP_NAME(delay_output) (CURRENT_SCREEN, ms);
}
#endif

NCURSES_EXPORT(void)
NCURSES_SP_NAME(_nc_flush) (NCURSES_SP_DCL0)
{
    T((T_CALLED("_nc_flush(%p)"), (void *) SP_PARM));
    if (SP_PARM != 0 && SP_PARM->_ofd >= 0) {
	TR(TRACE_CHARPUT, ("ofd:%d inuse:%lu buffer:%p",
			   SP_PARM->_ofd,
			   (unsigned long) SP_PARM->out_inuse,
			   SP_PARM->out_buffer));
	if (SP_PARM->out_inuse) {
	    char *buf = SP_PARM->out_buffer;
	    size_t amount = SP_PARM->out_inuse;

	    TR(TRACE_CHARPUT, ("flushing %ld/%ld bytes",
			       (unsigned long) amount, _nc_outchars));
	    while (amount) {
		ssize_t res = write(SP_PARM->_ofd, buf, amount);
		if (res > 0) {
		     
		    amount -= (size_t) res;
		    buf += res;
		} else if (errno == EAGAIN) {
		    continue;
		} else if (errno == EINTR) {
		    continue;
		} else {
		    break;	 
		}
	    }
	} else if (SP_PARM->out_buffer == 0) {
	    TR(TRACE_CHARPUT, ("flushing stdout"));
	    fflush(stdout);
	}
    } else {
	TR(TRACE_CHARPUT, ("flushing stdout"));
	fflush(stdout);
    }
    if (SP_PARM != 0)
	SP_PARM->out_inuse = 0;
    returnVoid;
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(void)
_nc_flush(void)
{
    NCURSES_SP_NAME(_nc_flush) (CURRENT_SCREEN);
}
#endif

NCURSES_EXPORT(int)
NCURSES_SP_NAME(_nc_outch) (NCURSES_SP_DCLx int ch)
{
    int rc = OK;

    COUNT_OUTCHARS(1);

    if (HasTInfoTerminal(SP_PARM)
	&& SP_PARM != 0) {
	if (SP_PARM->out_buffer != 0) {
	    if (SP_PARM->out_inuse + 1 >= SP_PARM->out_limit)
		NCURSES_SP_NAME(_nc_flush) (NCURSES_SP_ARG);
	    SP_PARM->out_buffer[SP_PARM->out_inuse++] = (char) ch;
	} else {
	    char tmp = (char) ch;
	     
	    if (write(fileno(NC_OUTPUT(SP_PARM)), &tmp, (size_t) 1) == -1)
		rc = ERR;
	}
    } else {
	char tmp = (char) ch;
	if (write(fileno(stdout), &tmp, (size_t) 1) == -1)
	    rc = ERR;
    }
    return rc;
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
_nc_outch(int ch)
{
    return NCURSES_SP_NAME(_nc_outch) (CURRENT_SCREEN, ch);
}
#endif

 
NCURSES_EXPORT(int)
NCURSES_SP_NAME(_nc_putchar) (NCURSES_SP_DCLx int ch)
{
    (void) SP_PARM;
    return putchar(ch);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
_nc_putchar(int ch)
{
    return putchar(ch);
}
#endif

 
NCURSES_EXPORT(int)
NCURSES_SP_NAME(putp) (NCURSES_SP_DCLx const char *string)
{
    return NCURSES_SP_NAME(tputs) (NCURSES_SP_ARGx
				   string, 1, NCURSES_SP_NAME(_nc_putchar));
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
putp(const char *string)
{
    return NCURSES_SP_NAME(putp) (CURRENT_SCREEN, string);
}
#endif

 
NCURSES_EXPORT(int)
NCURSES_SP_NAME(_nc_putp) (NCURSES_SP_DCLx
			   const char *name GCC_UNUSED,
			   const char *string)
{
    int rc = ERR;

    if (string != 0) {
	TPUTS_TRACE(name);
	rc = NCURSES_SP_NAME(tputs) (NCURSES_SP_ARGx
				     string, 1, NCURSES_SP_NAME(_nc_outch));
    }
    return rc;
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
_nc_putp(const char *name, const char *string)
{
    return NCURSES_SP_NAME(_nc_putp) (CURRENT_SCREEN, name, string);
}
#endif

NCURSES_EXPORT(int)
NCURSES_SP_NAME(tputs) (NCURSES_SP_DCLx
			const char *string,
			int affcnt,
			NCURSES_SP_OUTC outc)
{
    NCURSES_SP_OUTC my_outch = GetOutCh();
    bool always_delay = FALSE;
    bool normal_delay = FALSE;
    int number;
#if BSD_TPUTS
    int trailpad;
#endif  

#ifdef TRACE
    if (USE_TRACEF(TRACE_TPUTS)) {
	char addrbuf[32];
	TR_FUNC_BFR(1);

	if (outc == NCURSES_SP_NAME(_nc_outch)) {
	    _nc_STRCPY(addrbuf, "_nc_outch", sizeof(addrbuf));
	} else {
	    _nc_SPRINTF(addrbuf, _nc_SLIMIT(sizeof(addrbuf)) "%s",
			TR_FUNC_ARG(0, outc));
	}
	if (_nc_tputs_trace) {
	    _tracef("tputs(%s = %s, %d, %s) called", _nc_tputs_trace,
		    _nc_visbuf(string), affcnt, addrbuf);
	} else {
	    _tracef("tputs(%s, %d, %s) called", _nc_visbuf(string), affcnt, addrbuf);
	}
	TPUTS_TRACE(NULL);
	_nc_unlock_global(tracef);
    }
#endif  

    if (!VALID_STRING(string))
	return ERR;

    if (SP_PARM != 0 && HasTInfoTerminal(SP_PARM)) {
	if (
#if NCURSES_SP_FUNCS
	       (SP_PARM != 0 && SP_PARM->_term == 0)
#else
	       cur_term == 0
#endif
	    ) {
	    always_delay = FALSE;
	    normal_delay = TRUE;
	} else {
	    always_delay = (string == bell) || (string == flash_screen);
	    normal_delay =
		!xon_xoff
		&& padding_baud_rate
#if NCURSES_NO_PADDING
		&& !GetNoPadding(SP_PARM)
#endif
		&& (_nc_baudrate(ospeed) >= padding_baud_rate);
	}
    }
#if BSD_TPUTS
     
    trailpad = 0;
    if (isdigit(UChar(*string))) {
	while (isdigit(UChar(*string))) {
	    trailpad = trailpad * 10 + (*string - '0');
	    string++;
	}
	trailpad *= 10;
	if (*string == '.') {
	    string++;
	    if (isdigit(UChar(*string))) {
		trailpad += (*string - '0');
		string++;
	    }
	    while (isdigit(UChar(*string)))
		string++;
	}

	if (*string == '*') {
	    trailpad *= affcnt;
	    string++;
	}
    }
#endif  

    SetOutCh(outc);		 
    while (*string) {
	if (*string != '$')
	    (*outc) (NCURSES_SP_ARGx *string);
	else {
	    string++;
	    if (*string != '<') {
		(*outc) (NCURSES_SP_ARGx '$');
		if (*string)
		    (*outc) (NCURSES_SP_ARGx *string);
	    } else {
		bool mandatory;

		string++;
		if ((!isdigit(UChar(*string)) && *string != '.')
		    || !strchr(string, '>')) {
		    (*outc) (NCURSES_SP_ARGx '$');
		    (*outc) (NCURSES_SP_ARGx '<');
		    continue;
		}

		number = 0;
		while (isdigit(UChar(*string))) {
		    number = number * 10 + (*string - '0');
		    string++;
		}
		number *= 10;
		if (*string == '.') {
		    string++;
		    if (isdigit(UChar(*string))) {
			number += (*string - '0');
			string++;
		    }
		    while (isdigit(UChar(*string)))
			string++;
		}

		mandatory = FALSE;
		while (*string == '*' || *string == '/') {
		    if (*string == '*') {
			number *= affcnt;
			string++;
		    } else {	 
			mandatory = TRUE;
			string++;
		    }
		}

		if (number > 0
		    && (always_delay
			|| normal_delay
			|| mandatory))
		    NCURSES_SP_NAME(delay_output) (NCURSES_SP_ARGx number / 10);

	    }			 
	}			 

	if (*string == '\0')
	    break;

	string++;
    }

#if BSD_TPUTS
     
    if (trailpad > 0
	&& (always_delay || normal_delay))
	NCURSES_SP_NAME(delay_output) (NCURSES_SP_ARGx trailpad / 10);
#endif  

    SetOutCh(my_outch);
    return OK;
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
_nc_outc_wrapper(SCREEN *sp, int c)
{
    if (0 == sp) {
	return fputc(c, stdout);
    } else {
	return sp->jump(c);
    }
}

NCURSES_EXPORT(int)
tputs(const char *string, int affcnt, int (*outc) (int))
{
    SetSafeOutcWrapper(outc);
    return NCURSES_SP_NAME(tputs) (NCURSES_SP_ARGx string, affcnt, _nc_outc_wrapper);
}
#endif
