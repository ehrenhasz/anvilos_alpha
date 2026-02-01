 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_tracebits.c,v 1.31 2020/11/14 23:38:11 tom Exp $")

#if HAVE_SYS_TERMIO_H
#include <sys/termio.h>		 
#endif

#ifdef __EMX__
#include <io.h>
#endif

 
#ifndef TOSTOP
#define TOSTOP 0
#endif

#ifndef IEXTEN
#define IEXTEN 0
#endif

#ifndef ONLCR
#define ONLCR 0
#endif

#ifndef OCRNL
#define OCRNL 0
#endif

#ifndef ONOCR
#define ONOCR 0
#endif

#ifndef ONLRET
#define ONLRET 0
#endif

#ifdef TRACE

#if defined(EXP_WIN32_DRIVER)
#define BITNAMELEN 36
#else
#define BITNAMELEN 8
#endif

typedef struct {
    unsigned int val;
    const char name[BITNAMELEN];
} BITNAMES;

#define TRACE_BUF_SIZE(num) (_nc_globals.tracebuf_ptr[num].size)

static void
lookup_bits(char *buf, const BITNAMES * table, const char *label, unsigned int val)
{
    const BITNAMES *sp;

    _nc_STRCAT(buf, label, TRACE_BUF_SIZE(0));
    _nc_STRCAT(buf, ": {", TRACE_BUF_SIZE(0));
    for (sp = table; sp->name[0]; sp++)
	if (sp->val != 0
	    && (val & sp->val) == sp->val) {
	    _nc_STRCAT(buf, sp->name, TRACE_BUF_SIZE(0));
	    _nc_STRCAT(buf, ", ", TRACE_BUF_SIZE(0));
	}
    if (buf[strlen(buf) - 2] == ',')
	buf[strlen(buf) - 2] = '\0';
    _nc_STRCAT(buf, "} ", TRACE_BUF_SIZE(0));
}

NCURSES_EXPORT(char *)
_nc_trace_ttymode(const TTY * tty)
 
{
    char *buf;

#ifdef TERMIOS
#define DATA(name)        { name, { #name } }
#define DATA2(name,name2) { name, { #name2 } }
#define DATAX()           { 0,    { "" } }
    static const BITNAMES iflags[] =
    {
	DATA(BRKINT),
	DATA(IGNBRK),
	DATA(IGNPAR),
	DATA(PARMRK),
	DATA(INPCK),
	DATA(ISTRIP),
	DATA(INLCR),
	DATA(IGNCR),
	DATA(ICRNL),
	DATA(IXON),
	DATA(IXOFF),
	DATAX()
#define ALLIN	(BRKINT|IGNBRK|IGNPAR|PARMRK|INPCK|ISTRIP|INLCR|IGNCR|ICRNL|IXON|IXOFF)
    }, oflags[] =
    {
	DATA(OPOST),
	DATA2(OFLAGS_TABS, XTABS),
	DATA(ONLCR),
	DATA(OCRNL),
	DATA(ONOCR),
	DATA(ONLRET),
	DATAX()
#define ALLOUT	(OPOST|OFLAGS_TABS|ONLCR|OCRNL|ONOCR|ONLRET|OFLAGS_TABS)
    }, cflags[] =
    {
	DATA(CLOCAL),
	DATA(CREAD),
	DATA(CSTOPB),
#if !defined(CS5) || !defined(CS8)
	DATA(CSIZE),
#endif
	DATA(HUPCL),
	DATA(PARENB),
	DATA2(PARODD | PARENB, PARODD),
	DATAX()
#define ALLCTRL	(CLOCAL|CREAD|CSIZE|CSTOPB|HUPCL|PARENB|PARODD)
    }, lflags[] =
    {
	DATA(ECHO),
	DATA2(ECHOE | ECHO, ECHOE),
	DATA2(ECHOK | ECHO, ECHOK),
	DATA(ECHONL),
	DATA(ICANON),
	DATA(ISIG),
	DATA(NOFLSH),
	DATA(TOSTOP),
	DATA(IEXTEN),
	DATAX()
#define ALLLOCAL	(ECHO|ECHONL|ICANON|ISIG|NOFLSH|TOSTOP|IEXTEN)
    };

    buf = _nc_trace_buf(0,
			8 + sizeof(iflags) +
			8 + sizeof(oflags) +
			8 + sizeof(cflags) +
			8 + sizeof(lflags) +
			8);
    if (buf != 0) {

	if (tty->c_iflag & ALLIN)
	    lookup_bits(buf, iflags, "iflags", tty->c_iflag);

	if (tty->c_oflag & ALLOUT)
	    lookup_bits(buf, oflags, "oflags", tty->c_oflag);

	if (tty->c_cflag & ALLCTRL)
	    lookup_bits(buf, cflags, "cflags", tty->c_cflag);

#if defined(CS5) && defined(CS8)
	{
	    static const struct {
		int value;
		const char name[5];
	    } csizes[] = {
#define CS_DATA(name) { name, { #name " " } }
		CS_DATA(CS5),
#ifdef CS6
		    CS_DATA(CS6),
#endif
#ifdef CS7
		    CS_DATA(CS7),
#endif
		    CS_DATA(CS8),
	    };
	    const char *result = "CSIZE? ";
	    int value = (int) (tty->c_cflag & CSIZE);
	    unsigned n;

	    if (value != 0) {
		for (n = 0; n < SIZEOF(csizes); n++) {
		    if (csizes[n].value == value) {
			result = csizes[n].name;
			break;
		    }
		}
	    }
	    _nc_STRCAT(buf, result, TRACE_BUF_SIZE(0));
	}
#endif

	if (tty->c_lflag & ALLLOCAL)
	    lookup_bits(buf, lflags, "lflags", tty->c_lflag);
    }
#elif defined(EXP_WIN32_DRIVER)
#define DATA(name)        { name, { #name } }
    static const BITNAMES dwFlagsOut[] =
    {
	DATA(ENABLE_PROCESSED_OUTPUT),
	DATA(ENABLE_WRAP_AT_EOL_OUTPUT),
	DATA(ENABLE_VIRTUAL_TERMINAL_PROCESSING),
	DATA(DISABLE_NEWLINE_AUTO_RETURN),
	DATA(ENABLE_LVB_GRID_WORLDWIDE)
    };
    static const BITNAMES dwFlagsIn[] =
    {
	DATA(ENABLE_PROCESSED_INPUT),
	DATA(ENABLE_LINE_INPUT),
	DATA(ENABLE_ECHO_INPUT),
	DATA(ENABLE_MOUSE_INPUT),
	DATA(ENABLE_INSERT_MODE),
	DATA(ENABLE_QUICK_EDIT_MODE),
	DATA(ENABLE_EXTENDED_FLAGS),
	DATA(ENABLE_AUTO_POSITION),
	DATA(ENABLE_VIRTUAL_TERMINAL_INPUT)
    };

    buf = _nc_trace_buf(0,
			8 + sizeof(dwFlagsOut) +
			8 + sizeof(dwFlagsIn));
    if (buf != 0) {
	lookup_bits(buf, dwFlagsIn, "dwIn", tty->dwFlagIn);
	lookup_bits(buf, dwFlagsOut, "dwOut", tty->dwFlagOut);
    }
#else
     
#ifndef EVENP
#define EVENP 0
#endif
#ifndef LCASE
#define LCASE 0
#endif
#ifndef LLITOUT
#define LLITOUT 0
#endif
#ifndef ODDP
#define ODDP 0
#endif
#ifndef TANDEM
#define TANDEM 0
#endif

    static const BITNAMES cflags[] =
    {
	DATA(CBREAK),
	DATA(CRMOD),
	DATA(ECHO),
	DATA(EVENP),
	DATA(LCASE),
	DATA(LLITOUT),
	DATA(ODDP),
	DATA(RAW),
	DATA(TANDEM),
	DATA(XTABS),
	DATAX()
#define ALLCTRL	(CBREAK|CRMOD|ECHO|EVENP|LCASE|LLITOUT|ODDP|RAW|TANDEM|XTABS)
    };

    buf = _nc_trace_buf(0,
			8 + sizeof(cflags));
    if (buf != 0) {
	if (tty->sg_flags & ALLCTRL) {
	    lookup_bits(buf, cflags, "cflags", tty->sg_flags);
	}
    }
#endif
    return (buf);
}

NCURSES_EXPORT(char *)
_nc_tracebits(void)
{
    return _nc_trace_ttymode(&(cur_term->Nttyb));
}
#else
EMPTY_MODULE(_nc_empty_lib_tracebits)
#endif  
