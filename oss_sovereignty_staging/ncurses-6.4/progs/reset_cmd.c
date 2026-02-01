 

 

#include <reset_cmd.h>
#include <tty_settings.h>

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#if HAVE_SIZECHANGE
# if !defined(sun) || !TERMIOS
#  if HAVE_SYS_IOCTL_H
#   include <sys/ioctl.h>
#  endif
# endif
#endif

#if NEED_PTEM_H
 
#include <sys/stream.h>
#include <sys/ptem.h>
#endif

MODULE_ID("$Id: reset_cmd.c,v 1.28 2021/10/02 18:08:44 tom Exp $")

 
#ifdef TIOCGSIZE
# define IOCTL_GET_WINSIZE TIOCGSIZE
# define IOCTL_SET_WINSIZE TIOCSSIZE
# define STRUCT_WINSIZE struct ttysize
# define WINSIZE_ROWS(n) n.ts_lines
# define WINSIZE_COLS(n) n.ts_cols
#else
# ifdef TIOCGWINSZ
#  define IOCTL_GET_WINSIZE TIOCGWINSZ
#  define IOCTL_SET_WINSIZE TIOCSWINSZ
#  define STRUCT_WINSIZE struct winsize
#  define WINSIZE_ROWS(n) n.ws_row
#  define WINSIZE_COLS(n) n.ws_col
# endif
#endif

static FILE *my_file;

static bool use_reset = FALSE;	 
static bool use_init = FALSE;	 

static GCC_NORETURN void
failed(const char *msg)
{
    int code = errno;

    (void) fprintf(stderr, "%s: %s: %s\n", _nc_progname, msg, strerror(code));
    restore_tty_settings();
    (void) fprintf(my_file, "\n");
    fflush(my_file);
    ExitProgram(ErrSystem(code));
     
}

static bool
cat_file(char *file)
{
    FILE *fp;
    size_t nr;
    char buf[BUFSIZ];
    bool sent = FALSE;

    if (file != 0) {
	if ((fp = safe_fopen(file, "r")) == 0)
	    failed(file);

	while ((nr = fread(buf, sizeof(char), sizeof(buf), fp)) != 0) {
	    if (fwrite(buf, sizeof(char), nr, my_file) != nr) {
		failed(file);
	    }
	    sent = TRUE;
	}
	fclose(fp);
    }
    return sent;
}

static int
out_char(int c)
{
    return putc(c, my_file);
}

 

 
#if !(defined(CERASE) && defined(CINTR) && defined(CKILL) && defined(CQUIT))
#undef CEOF
#undef CERASE
#undef CINTR
#undef CKILL
#undef CLNEXT
#undef CRPRNT
#undef CQUIT
#undef CSTART
#undef CSTOP
#undef CSUSP
#endif

 
#ifndef CEOF
#define CEOF	CTRL('D')
#endif
#ifndef CERASE
#define CERASE	CTRL('H')
#endif
#ifndef CINTR
#define CINTR	127		 
#endif
#ifndef CKILL
#define CKILL	CTRL('U')
#endif
#ifndef CLNEXT
#define CLNEXT  CTRL('v')
#endif
#ifndef CRPRNT
#define CRPRNT  CTRL('r')
#endif
#ifndef CQUIT
#define CQUIT	CTRL('\\')
#endif
#ifndef CSTART
#define CSTART	CTRL('Q')
#endif
#ifndef CSTOP
#define CSTOP	CTRL('S')
#endif
#ifndef CSUSP
#define CSUSP	CTRL('Z')
#endif

#if defined(_POSIX_VDISABLE)
#define DISABLED(val)   (((_POSIX_VDISABLE != -1) \
		       && ((val) == _POSIX_VDISABLE)) \
		      || ((val) <= 0))
#else
#define DISABLED(val)   ((int)(val) <= 0)
#endif

#define CHK(val, dft)   (unsigned char) (DISABLED(val) ? dft : val)

#define reset_char(item, value) \
    tty_settings->c_cc[item] = CHK(tty_settings->c_cc[item], value)

 
void
reset_tty_settings(int fd, TTY * tty_settings, int noset)
{
    GET_TTY(fd, tty_settings);

#ifdef TERMIOS
#if defined(VDISCARD) && defined(CDISCARD)
    reset_char(VDISCARD, CDISCARD);
#endif
    reset_char(VEOF, CEOF);
    reset_char(VERASE, CERASE);
#if defined(VFLUSH) && defined(CFLUSH)
    reset_char(VFLUSH, CFLUSH);
#endif
    reset_char(VINTR, CINTR);
    reset_char(VKILL, CKILL);
#if defined(VLNEXT) && defined(CLNEXT)
    reset_char(VLNEXT, CLNEXT);
#endif
    reset_char(VQUIT, CQUIT);
#if defined(VREPRINT) && defined(CRPRNT)
    reset_char(VREPRINT, CRPRNT);
#endif
#if defined(VSTART) && defined(CSTART)
    reset_char(VSTART, CSTART);
#endif
#if defined(VSTOP) && defined(CSTOP)
    reset_char(VSTOP, CSTOP);
#endif
#if defined(VSUSP) && defined(CSUSP)
    reset_char(VSUSP, CSUSP);
#endif
#if defined(VWERASE) && defined(CWERASE)
    reset_char(VWERASE, CWERASE);
#endif

    tty_settings->c_iflag &= ~((unsigned) (IGNBRK
					   | PARMRK
					   | INPCK
					   | ISTRIP
					   | INLCR
					   | IGNCR
#ifdef IUCLC
					   | IUCLC
#endif
#ifdef IXANY
					   | IXANY
#endif
					   | IXOFF));

    tty_settings->c_iflag |= (BRKINT
			      | IGNPAR
			      | ICRNL
			      | IXON
#ifdef IMAXBEL
			      | IMAXBEL
#endif
	);

    tty_settings->c_oflag &= ~((unsigned) (0
#ifdef OLCUC
					   | OLCUC
#endif
#ifdef OCRNL
					   | OCRNL
#endif
#ifdef ONOCR
					   | ONOCR
#endif
#ifdef ONLRET
					   | ONLRET
#endif
#ifdef OFILL
					   | OFILL
#endif
#ifdef OFDEL
					   | OFDEL
#endif
#ifdef NLDLY
					   | NLDLY
#endif
#ifdef CRDLY
					   | CRDLY
#endif
#ifdef TABDLY
					   | TABDLY
#endif
#ifdef BSDLY
					   | BSDLY
#endif
#ifdef VTDLY
					   | VTDLY
#endif
#ifdef FFDLY
					   | FFDLY
#endif
			       ));

    tty_settings->c_oflag |= (OPOST
#ifdef ONLCR
			      | ONLCR
#endif
	);

    tty_settings->c_cflag &= ~((unsigned) (CSIZE
					   | CSTOPB
					   | PARENB
					   | PARODD
					   | CLOCAL));
    tty_settings->c_cflag |= (CS8 | CREAD);
    tty_settings->c_lflag &= ~((unsigned) (ECHONL
					   | NOFLSH
#ifdef TOSTOP
					   | TOSTOP
#endif
#ifdef ECHOPTR
					   | ECHOPRT
#endif
#ifdef XCASE
					   | XCASE
#endif
			       ));

    tty_settings->c_lflag |= (ISIG
			      | ICANON
			      | ECHO
			      | ECHOE
			      | ECHOK
#ifdef ECHOCTL
			      | ECHOCTL
#endif
#ifdef ECHOKE
			      | ECHOKE
#endif
	);
#endif

    if (!noset) {
	SET_TTY(fd, tty_settings);
    }
}

 
static int
default_erase(void)
{
    int result;

    if (over_strike
	&& VALID_STRING(key_backspace)
	&& strlen(key_backspace) == 1) {
	result = key_backspace[0];
    } else {
	result = CERASE;
    }

    return result;
}

 
void
set_control_chars(TTY * tty_settings, int my_erase, int my_intr, int my_kill)
{
#if defined(EXP_WIN32_DRIVER)
     
    (void) tty_settings;
    (void) my_erase;
    (void) my_intr;
    (void) my_kill;
#else
    if (DISABLED(tty_settings->c_cc[VERASE]) || my_erase >= 0) {
	tty_settings->c_cc[VERASE] = UChar((my_erase >= 0)
					   ? my_erase
					   : default_erase());
    }

    if (DISABLED(tty_settings->c_cc[VINTR]) || my_intr >= 0) {
	tty_settings->c_cc[VINTR] = UChar((my_intr >= 0)
					  ? my_intr
					  : CINTR);
    }

    if (DISABLED(tty_settings->c_cc[VKILL]) || my_kill >= 0) {
	tty_settings->c_cc[VKILL] = UChar((my_kill >= 0)
					  ? my_kill
					  : CKILL);
    }
#endif
}

 
void
set_conversions(TTY * tty_settings)
{
#if defined(EXP_WIN32_DRIVER)
     
#else
#ifdef ONLCR
    tty_settings->c_oflag |= ONLCR;
#endif
    tty_settings->c_iflag |= ICRNL;
    tty_settings->c_lflag |= ECHO;
#ifdef OXTABS
    tty_settings->c_oflag |= OXTABS;
#endif  

     
    if (VALID_STRING(newline) && newline[0] == '\n' && !newline[1]) {
	 
#ifdef ONLCR
	tty_settings->c_oflag &= ~((unsigned) ONLCR);
#endif
	tty_settings->c_iflag &= ~((unsigned) ICRNL);
    }
#ifdef OXTABS
     
    if (VALID_STRING(set_tab) && VALID_STRING(clear_all_tabs))
	tty_settings->c_oflag &= ~OXTABS;
#endif  
    tty_settings->c_lflag |= (ECHOE | ECHOK);
#endif
}

static bool
sent_string(const char *s)
{
    bool sent = FALSE;
    if (VALID_STRING(s)) {
	tputs(s, 0, out_char);
	sent = TRUE;
    }
    return sent;
}

static bool
to_left_margin(void)
{
    if (VALID_STRING(carriage_return)) {
	sent_string(carriage_return);
    } else {
	out_char('\r');
    }
    return TRUE;
}

 
static bool
reset_tabstops(int wide)
{
    if ((init_tabs != 8)
	&& VALID_NUMERIC(init_tabs)
	&& VALID_STRING(set_tab)
	&& VALID_STRING(clear_all_tabs)) {
	int c;

	to_left_margin();
	tputs(clear_all_tabs, 0, out_char);
	if (init_tabs > 1) {
	    if (init_tabs > wide)
		init_tabs = (short) wide;
	    for (c = init_tabs; c < wide; c += init_tabs) {
		fprintf(my_file, "%*s", init_tabs, " ");
		tputs(set_tab, 0, out_char);
	    }
	    to_left_margin();
	}
	return (TRUE);
    }
    return (FALSE);
}

 
bool
send_init_strings(int fd GCC_UNUSED, TTY * old_settings)
{
    int i;
    bool need_flush = FALSE;

    (void) old_settings;
#ifdef TAB3
    if (old_settings != 0 &&
	old_settings->c_oflag & (TAB3 | ONLCR | OCRNL | ONLRET)) {
	old_settings->c_oflag &= (TAB3 | ONLCR | OCRNL | ONLRET);
	SET_TTY(fd, old_settings);
    }
#endif
    if (use_reset || use_init) {
	if (VALID_STRING(init_prog)) {
	    IGNORE_RC(system(init_prog));
	}

	need_flush |= sent_string((use_reset && (reset_1string != 0))
				  ? reset_1string
				  : init_1string);

	need_flush |= sent_string((use_reset && (reset_2string != 0))
				  ? reset_2string
				  : init_2string);

	if (VALID_STRING(clear_margins)) {
	    need_flush |= sent_string(clear_margins);
	} else
#if defined(set_lr_margin)
	if (VALID_STRING(set_lr_margin)) {
	    need_flush |= sent_string(TIPARM_2(set_lr_margin, 0, columns - 1));
	} else
#endif
#if defined(set_left_margin_parm) && defined(set_right_margin_parm)
	    if (VALID_STRING(set_left_margin_parm)
		&& VALID_STRING(set_right_margin_parm)) {
	    need_flush |= sent_string(TIPARM_1(set_left_margin_parm, 0));
	    need_flush |= sent_string(TIPARM_1(set_right_margin_parm,
					       columns - 1));
	} else
#endif
	    if (VALID_STRING(set_left_margin)
		&& VALID_STRING(set_right_margin)) {
	    need_flush |= to_left_margin();
	    need_flush |= sent_string(set_left_margin);
	    if (VALID_STRING(parm_right_cursor)) {
		need_flush |= sent_string(TIPARM_1(parm_right_cursor,
						   columns - 1));
	    } else {
		for (i = 0; i < columns - 1; i++) {
		    out_char(' ');
		    need_flush = TRUE;
		}
	    }
	    need_flush |= sent_string(set_right_margin);
	    need_flush |= to_left_margin();
	}

	need_flush |= reset_tabstops(columns);

	need_flush |= cat_file((use_reset && reset_file) ? reset_file : init_file);

	need_flush |= sent_string((use_reset && (reset_3string != 0))
				  ? reset_3string
				  : init_3string);
    }

    return need_flush;
}

 
static void
show_tty_change(TTY * old_settings,
		TTY * new_settings,
		const char *name,
		int which,
		unsigned def)
{
    unsigned older = 0, newer = 0;
    char *p;

#if defined(EXP_WIN32_DRIVER)
     
    (void) old_settings;
    (void) new_settings;
    (void) name;
    (void) which;
    (void) def;
#else
    newer = new_settings->c_cc[which];
    older = old_settings->c_cc[which];

    if (older == newer && older == def)
	return;
#endif
    (void) fprintf(stderr, "%s %s ", name, older == newer ? "is" : "set to");

    if (DISABLED(newer)) {
	(void) fprintf(stderr, "undef.\n");
	 
    } else if (newer == 0177) {
	(void) fprintf(stderr, "delete.\n");
    } else if ((p = key_backspace) != 0
	       && newer == (unsigned char) p[0]
	       && p[1] == '\0') {
	(void) fprintf(stderr, "backspace.\n");
    } else if (newer < 040) {
	newer ^= 0100;
	(void) fprintf(stderr, "control-%c (^%c).\n", UChar(newer), UChar(newer));
    } else
	(void) fprintf(stderr, "%c.\n", UChar(newer));
}

 

void
reset_start(FILE *fp, bool is_reset, bool is_init)
{
    my_file = fp;
    use_reset = is_reset;
    use_init = is_init;
}

void
reset_flush(void)
{
    if (my_file != 0)
	fflush(my_file);
}

void
print_tty_chars(TTY * old_settings, TTY * new_settings)
{
#if defined(EXP_WIN32_DRIVER)
     
#else
    show_tty_change(old_settings, new_settings, "Erase", VERASE, CERASE);
    show_tty_change(old_settings, new_settings, "Kill", VKILL, CKILL);
    show_tty_change(old_settings, new_settings, "Interrupt", VINTR, CINTR);
#endif
}

#if HAVE_SIZECHANGE
 
void
set_window_size(int fd, short *high, short *wide)
{
    STRUCT_WINSIZE win;
    (void) ioctl(fd, IOCTL_GET_WINSIZE, &win);
    if (WINSIZE_ROWS(win) == 0 &&
	WINSIZE_COLS(win) == 0) {
	if (*high > 0 && *wide > 0) {
	    WINSIZE_ROWS(win) = (unsigned short) *high;
	    WINSIZE_COLS(win) = (unsigned short) *wide;
	    (void) ioctl(fd, IOCTL_SET_WINSIZE, &win);
	}
    } else if (WINSIZE_ROWS(win) > 0 &&
	       WINSIZE_COLS(win) > 0) {
	*high = (short) WINSIZE_ROWS(win);
	*wide = (short) WINSIZE_COLS(win);
    }
}
#endif
