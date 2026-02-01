 

 

 

#ifdef __EMX__
#  include <io.h>
#  define  INCL_DOS
#  define  INCL_VIO
#  define  INCL_KBD
#  define  INCL_MOU
#  define  INCL_DOSPROCESS
#  include <os2.h>		 
#endif

#include <curses.priv.h>

#ifndef CUR
#define CUR SP_TERMTYPE
#endif

MODULE_ID("$Id: lib_mouse.c,v 1.197 2022/08/13 14:13:12 tom Exp $")

#include <tic.h>

#if USE_GPM_SUPPORT
#include <linux/keyboard.h>	 

#ifdef HAVE_LIBDL
 
#include <dlfcn.h>

#ifdef RTLD_NOW
#define my_RTLD RTLD_NOW
#else
#ifdef RTLD_LAZY
#define my_RTLD RTLD_LAZY
#else
make an error
#endif
#endif				 
#endif				 

#endif				 

#if USE_SYSMOUSE
#undef buttons			 
#undef mouse_info		 
#include <osreldate.h>
#if defined(__DragonFly_version) || (defined(__FreeBSD__) && (__FreeBSD_version >= 400017))
#include <sys/consio.h>
#include <sys/fbio.h>
#else
#include <machine/console.h>
#endif
#endif				 

#if USE_KLIBC_MOUSE
#include <sys/socket.h>
#define pipe(handles) socketpair(AF_LOCAL, SOCK_STREAM, 0, handles)
#define DosWrite(hfile, pbuffer, cbwrite, pcbactual) \
		write(hfile, pbuffer, cbwrite)
#define DosExit(action, result )	 
#define DosCreateThread(ptid, pfn, param, flag, cbStack) \
		(*(ptid) = _beginthread(pfn, NULL, cbStack, \
					(void *)param), (*(ptid) == -1))
#endif

#define MY_TRACE TRACE_ICALLS|TRACE_IEVENT

#define	MASK_RELEASE(x)		(mmask_t) NCURSES_MOUSE_MASK(x, 001)
#define	MASK_PRESS(x)		(mmask_t) NCURSES_MOUSE_MASK(x, 002)
#define	MASK_CLICK(x)		(mmask_t) NCURSES_MOUSE_MASK(x, 004)
#define	MASK_DOUBLE_CLICK(x)	(mmask_t) NCURSES_MOUSE_MASK(x, 010)
#define	MASK_TRIPLE_CLICK(x)	(mmask_t) NCURSES_MOUSE_MASK(x, 020)
#define	MASK_RESERVED_EVENT(x)	(mmask_t) NCURSES_MOUSE_MASK(x, 040)

#if NCURSES_MOUSE_VERSION == 1

#define BUTTON_CLICKED        (BUTTON1_CLICKED        | BUTTON2_CLICKED        | BUTTON3_CLICKED        | BUTTON4_CLICKED)
#define BUTTON_PRESSED        (BUTTON1_PRESSED        | BUTTON2_PRESSED        | BUTTON3_PRESSED        | BUTTON4_PRESSED)
#define BUTTON_RELEASED       (BUTTON1_RELEASED       | BUTTON2_RELEASED       | BUTTON3_RELEASED       | BUTTON4_RELEASED)
#define BUTTON_DOUBLE_CLICKED (BUTTON1_DOUBLE_CLICKED | BUTTON2_DOUBLE_CLICKED | BUTTON3_DOUBLE_CLICKED | BUTTON4_DOUBLE_CLICKED)
#define BUTTON_TRIPLE_CLICKED (BUTTON1_TRIPLE_CLICKED | BUTTON2_TRIPLE_CLICKED | BUTTON3_TRIPLE_CLICKED | BUTTON4_TRIPLE_CLICKED)

#define MAX_BUTTONS  4

#else

#define BUTTON_CLICKED        (BUTTON1_CLICKED        | BUTTON2_CLICKED        | BUTTON3_CLICKED        | BUTTON4_CLICKED        | BUTTON5_CLICKED)
#define BUTTON_PRESSED        (BUTTON1_PRESSED        | BUTTON2_PRESSED        | BUTTON3_PRESSED        | BUTTON4_PRESSED        | BUTTON5_PRESSED)
#define BUTTON_RELEASED       (BUTTON1_RELEASED       | BUTTON2_RELEASED       | BUTTON3_RELEASED       | BUTTON4_RELEASED       | BUTTON5_RELEASED)
#define BUTTON_DOUBLE_CLICKED (BUTTON1_DOUBLE_CLICKED | BUTTON2_DOUBLE_CLICKED | BUTTON3_DOUBLE_CLICKED | BUTTON4_DOUBLE_CLICKED | BUTTON5_DOUBLE_CLICKED)
#define BUTTON_TRIPLE_CLICKED (BUTTON1_TRIPLE_CLICKED | BUTTON2_TRIPLE_CLICKED | BUTTON3_TRIPLE_CLICKED | BUTTON4_TRIPLE_CLICKED | BUTTON5_TRIPLE_CLICKED)

#if NCURSES_MOUSE_VERSION == 2
#define MAX_BUTTONS  5
#else
#define MAX_BUTTONS  11
#endif

#endif

#define INVALID_EVENT	-1
#define NORMAL_EVENT	0

#define ValidEvent(ep) ((ep)->id != INVALID_EVENT)
#define Invalidate(ep) (ep)->id = INVALID_EVENT

#if USE_GPM_SUPPORT

#ifndef LIBGPM_SONAME
#define LIBGPM_SONAME "libgpm.so"
#endif

#define GET_DLSYM(name) (my_##name = (TYPE_##name) dlsym(sp->_dlopen_gpm, #name))

#endif				 

static bool _nc_mouse_parse(SCREEN *, int);
static void _nc_mouse_resume(SCREEN *);
static void _nc_mouse_wrap(SCREEN *);

 

#define FirstEV(sp)	((sp)->_mouse_events)
#define LastEV(sp)	((sp)->_mouse_events + EV_MAX - 1)

#undef  NEXT
#define NEXT(ep)	((ep >= LastEV(SP_PARM)) \
			 ? FirstEV(SP_PARM) \
			 : ep + 1)

#undef  PREV
#define PREV(ep)	((ep <= FirstEV(SP_PARM)) \
			 ? LastEV(SP_PARM) \
			 : ep - 1)

#define IndexEV(sp, ep)	(ep - FirstEV(sp))

#define RunParams(sp, eventp, runp) \
		(long) IndexEV(sp, runp), \
		(long) (IndexEV(sp, eventp) + (EV_MAX - 1)) % EV_MAX

#ifdef TRACE
static void
_trace_slot(SCREEN *sp, const char *tag)
{
    MEVENT *ep;

    _tracef("%s", tag);

    for (ep = FirstEV(sp); ep <= LastEV(sp); ep++)
	_tracef("mouse event queue slot %ld = %s",
		(long) IndexEV(sp, ep),
		_nc_tracemouse(sp, ep));
}
#endif

#if USE_EMX_MOUSE

#  define TOP_ROW          0
#  define LEFT_COL         0

#  define M_FD(sp) sp->_mouse_fd

static void
write_event(SCREEN *sp, int down, int button, int x, int y)
{
    char buf[6];
    unsigned long ignore;

    _nc_STRCPY(buf, "\033[M", sizeof(buf));	 
    buf[3] = ' ' + (button - 1) + (down ? 0 : 0x40);
    buf[4] = ' ' + x - LEFT_COL + 1;
    buf[5] = ' ' + y - TOP_ROW + 1;
    DosWrite(sp->_emxmouse_wfd, buf, 6, &ignore);
}

static void
#if USE_KLIBC_MOUSE
mouse_server(void *param)
#else
mouse_server(unsigned long param)
#endif
{
    SCREEN *sp = (SCREEN *) param;
    unsigned short fWait = MOU_WAIT;
     
    MOUEVENTINFO mouev;
    HMOU hmou;
    unsigned short mask = MOUSE_BN1_DOWN | MOUSE_BN2_DOWN | MOUSE_BN3_DOWN;
    int nbuttons = 3;
    int oldstate = 0;
    char err[80];
    unsigned long rc;

     
    if (MouOpen(NULL, &hmou) == 0) {
	rc = MouSetEventMask(&mask, hmou);
	if (rc) {		 
	    mask = MOUSE_BN1_DOWN | MOUSE_BN2_DOWN;
	    rc = MouSetEventMask(&mask, hmou);
	    nbuttons = 2;
	}
	if (rc == 0 && MouDrawPtr(hmou) == 0) {
	    for (;;) {
		 
		rc = MouReadEventQue(&mouev, &fWait, hmou);
		if (rc) {
		    _nc_SPRINTF(err, _nc_SLIMIT(sizeof(err))
				"Error reading mouse queue, rc=%lu.\r\n", rc);
		    break;
		}
		if (!sp->_emxmouse_activated)
		    goto finish;

		 
		if ((mouev.fs ^ oldstate) & MOUSE_BN1_DOWN)
		    write_event(sp, mouev.fs & MOUSE_BN1_DOWN,
				sp->_emxmouse_buttons[1], mouev.col, mouev.row);
		if ((mouev.fs ^ oldstate) & MOUSE_BN2_DOWN)
		    write_event(sp, mouev.fs & MOUSE_BN2_DOWN,
				sp->_emxmouse_buttons[3], mouev.col, mouev.row);
		if ((mouev.fs ^ oldstate) & MOUSE_BN3_DOWN)
		    write_event(sp, mouev.fs & MOUSE_BN3_DOWN,
				sp->_emxmouse_buttons[2], mouev.col, mouev.row);

	      finish:
		oldstate = mouev.fs;
	    }
	} else {
	    _nc_SPRINTF(err, _nc_SLIMIT(sizeof(err))
			"Error setting event mask, buttons=%d, rc=%lu.\r\n",
			nbuttons, rc);
	}

	DosWrite(2, err, strlen(err), &rc);
	MouClose(hmou);
    }
    DosExit(EXIT_THREAD, 0L);
}

#endif  

#if USE_SYSMOUSE
static void
sysmouse_server(SCREEN *sp)
{
    struct mouse_info the_mouse;
    MEVENT *work;

    the_mouse.operation = MOUSE_GETINFO;
    if (sp != 0
	&& sp->_mouse_fd >= 0
	&& sp->_sysmouse_tail < FIFO_SIZE
	&& ioctl(sp->_mouse_fd, CONS_MOUSECTL, &the_mouse) != -1) {

	if (sp->_sysmouse_head > sp->_sysmouse_tail) {
	    sp->_sysmouse_tail = 0;
	    sp->_sysmouse_head = 0;
	}
	work = &(sp->_sysmouse_fifo[sp->_sysmouse_tail]);
	memset(work, 0, sizeof(*work));
	work->id = NORMAL_EVENT;	 

	sp->_sysmouse_old_buttons = sp->_sysmouse_new_buttons;
	sp->_sysmouse_new_buttons = the_mouse.u.data.buttons & 0x7;

	if (sp->_sysmouse_new_buttons) {
	    if (sp->_sysmouse_new_buttons & 1)
		work->bstate |= BUTTON1_PRESSED;
	    if (sp->_sysmouse_new_buttons & 2)
		work->bstate |= BUTTON2_PRESSED;
	    if (sp->_sysmouse_new_buttons & 4)
		work->bstate |= BUTTON3_PRESSED;
	} else {
	    if (sp->_sysmouse_old_buttons & 1)
		work->bstate |= BUTTON1_RELEASED;
	    if (sp->_sysmouse_old_buttons & 2)
		work->bstate |= BUTTON2_RELEASED;
	    if (sp->_sysmouse_old_buttons & 4)
		work->bstate |= BUTTON3_RELEASED;
	}

	 
	the_mouse.operation = MOUSE_HIDE;
	ioctl(sp->_mouse_fd, CONS_MOUSECTL, &the_mouse);
	the_mouse.operation = MOUSE_SHOW;
	ioctl(sp->_mouse_fd, CONS_MOUSECTL, &the_mouse);

	 
	if (sp->_sysmouse_new_buttons != sp->_sysmouse_old_buttons) {
	    sp->_sysmouse_tail += 1;
	}
	work->x = the_mouse.u.data.x / sp->_sysmouse_char_width;
	work->y = the_mouse.u.data.y / sp->_sysmouse_char_height;
    }
}

static void
handle_sysmouse(int sig GCC_UNUSED)
{
    sysmouse_server(CURRENT_SCREEN);
}
#endif  

#ifndef USE_TERM_DRIVER
#define xterm_kmous "\033[M"

static void
init_xterm_mouse(SCREEN *sp)
{
    sp->_mouse_type = M_XTERM;
    sp->_mouse_format = MF_X10;
    sp->_mouse_xtermcap = tigetstr("XM");
    if (VALID_STRING(sp->_mouse_xtermcap)) {
	char *code = strstr(sp->_mouse_xtermcap, "[?");
	if (code != 0) {
	    code += 2;
	    while ((*code >= '0') && (*code <= '9')) {
		char *next = code;
		while ((*next >= '0') && (*next <= '9')) {
		    ++next;
		}
		if (!strncmp(code, "1006", (size_t) (next - code))) {
		    sp->_mouse_format = MF_SGR1006;
		}
#ifdef EXP_XTERM_1005
		if (!strncmp(code, "1005", (size_t) (next - code))) {
		    sp->_mouse_format = MF_XTERM_1005;
		}
#endif
		if (*next == ';') {
		    while (*next == ';') {
			++next;
		    }
		    code = next;
		} else {
		    break;
		}
	    }
	}
    } else {
	int code = tigetnum("XM");
	switch (code) {
#ifdef EXP_XTERM_1005
	case 1005:
	     
	    sp->_mouse_xtermcap = "\033[?1005;1000%?%p1%{1}%=%th%el%;";
	    sp->_mouse_format = MF_XTERM_1005;
	    break;
#endif
	case 1006:
	     
	    sp->_mouse_xtermcap = "\033[?1006;1000%?%p1%{1}%=%th%el%;";
	    sp->_mouse_format = MF_SGR1006;
	    break;
	default:
	    sp->_mouse_xtermcap = "\033[?1000%?%p1%{1}%=%th%el%;";
	    break;
	}
    }
}
#endif

static void
enable_xterm_mouse(SCREEN *sp, int enable)
{
#if USE_EMX_MOUSE
    sp->_emxmouse_activated = enable;
#else
    NCURSES_PUTP2("xterm-mouse", TIPARM_1(sp->_mouse_xtermcap, enable));
#endif
    sp->_mouse_active = enable;
}

#if USE_GPM_SUPPORT
static bool
allow_gpm_mouse(SCREEN *sp GCC_UNUSED)
{
    bool result = FALSE;

#if USE_WEAK_SYMBOLS
     
    if ((Gpm_Wgetch) != 0) {
	if (!sp->_mouse_gpm_loaded) {
	    T(("GPM library was already dlopen'd, not by us"));
	}
    } else
#endif
	 
    if (NC_ISATTY(fileno(stdout))) {
	const char *list = getenv("NCURSES_GPM_TERMS");
	const char *env = getenv("TERM");
	if (list != 0) {
	    if (env != 0) {
		result = _nc_name_match(list, env, "|:");
	    }
	} else {
	     
	    if (env != 0 && strstr(env, "linux") != 0) {
		result = TRUE;
	    }
	}
    }
    return result;
}

#ifdef HAVE_LIBDL
static void
unload_gpm_library(SCREEN *sp)
{
    if (sp->_dlopen_gpm != 0) {
	T(("unload GPM library"));
	sp->_mouse_gpm_loaded = FALSE;
	sp->_mouse_fd = -1;
    }
}

static void
load_gpm_library(SCREEN *sp)
{
    sp->_mouse_gpm_found = FALSE;

     
    if (sp->_dlopen_gpm != 0) {
	sp->_mouse_gpm_found = TRUE;
	sp->_mouse_gpm_loaded = TRUE;
    } else if ((sp->_dlopen_gpm = dlopen(LIBGPM_SONAME, my_RTLD)) != 0) {
#if (defined(__GNUC__) && (__GNUC__ >= 5)) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
	if (GET_DLSYM(gpm_fd) == 0 ||
	    GET_DLSYM(Gpm_Open) == 0 ||
	    GET_DLSYM(Gpm_Close) == 0 ||
	    GET_DLSYM(Gpm_GetEvent) == 0) {
#if (defined(__GNUC__) && (__GNUC__ >= 5)) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
	    T(("GPM initialization failed: %s", dlerror()));
	    unload_gpm_library(sp);
	    dlclose(sp->_dlopen_gpm);
	    sp->_dlopen_gpm = 0;
	} else {
	    sp->_mouse_gpm_found = TRUE;
	    sp->_mouse_gpm_loaded = TRUE;
	}
    }
}
#endif  

static bool
enable_gpm_mouse(SCREEN *sp, bool enable)
{
    bool result;

    T((T_CALLED("enable_gpm_mouse(%d)"), enable));

    if (enable && !sp->_mouse_active) {
#ifdef HAVE_LIBDL
	if (sp->_mouse_gpm_found && !sp->_mouse_gpm_loaded) {
	    load_gpm_library(sp);
	}
#endif
	if (sp->_mouse_gpm_loaded) {
	    int code;

	     
	    sp->_mouse_gpm_connect.eventMask = GPM_DOWN | GPM_UP;
	    sp->_mouse_gpm_connect.defaultMask =
		(unsigned short) (~(sp->_mouse_gpm_connect.eventMask | GPM_HARD));
	    sp->_mouse_gpm_connect.minMod = 0;
	    sp->_mouse_gpm_connect.maxMod =
		(unsigned short) (~((1 << KG_SHIFT) |
				    (1 << KG_SHIFTL) |
				    (1 << KG_SHIFTR)));
	     
	    code = my_Gpm_Open(&sp->_mouse_gpm_connect, 0);
	    result = (code >= 0);

	     
	    if (code == -2) {
		my_Gpm_Close();
	    }
	} else {
	    result = FALSE;
	}
	sp->_mouse_active = result;
	T(("GPM open %s", result ? "succeeded" : "failed"));
    } else {
	if (!enable && sp->_mouse_active) {
	     
	    my_Gpm_Close();
	    sp->_mouse_active = FALSE;
	    T(("GPM closed"));
	}
	result = enable;
    }
#ifdef HAVE_LIBDL
    if (!result) {
	unload_gpm_library(sp);
    }
#endif
    returnBool(result);
}
#endif  

static void
initialize_mousetype(SCREEN *sp)
{
    T((T_CALLED("initialize_mousetype()")));

     
#if USE_GPM_SUPPORT
    if (allow_gpm_mouse(sp)) {
	if (!sp->_mouse_gpm_loaded) {
#ifdef HAVE_LIBDL
	    load_gpm_library(sp);
#else  
	    sp->_mouse_gpm_found = TRUE;
	    sp->_mouse_gpm_loaded = TRUE;
#endif
	}

	 
	if (sp->_mouse_gpm_found && enable_gpm_mouse(sp, TRUE)) {
	    sp->_mouse_type = M_GPM;
	    sp->_mouse_fd = *(my_gpm_fd);
	    T(("GPM mouse_fd %d", sp->_mouse_fd));
	    returnVoid;
	}
    }
#endif  

     
#if USE_EMX_MOUSE
    if (!sp->_emxmouse_thread
	&& strstr(SP_TERMTYPE term_names, "xterm") == 0
	&& NonEmpty(key_mouse)) {
	int handles[2];

	if (pipe(handles) < 0) {
	    perror("mouse pipe error");
	    returnVoid;
	} else {
	    int rc;

	    if (!sp->_emxmouse_buttons[0]) {
		const char *s = getenv("MOUSE_BUTTONS_123");

		sp->_emxmouse_buttons[0] = 1;
		if (s && strlen(s) >= 3) {
		    sp->_emxmouse_buttons[1] = s[0] - '0';
		    sp->_emxmouse_buttons[2] = s[1] - '0';
		    sp->_emxmouse_buttons[3] = s[2] - '0';
		} else {
		    sp->_emxmouse_buttons[1] = 1;
		    sp->_emxmouse_buttons[2] = 3;
		    sp->_emxmouse_buttons[3] = 2;
		}
	    }
	    sp->_emxmouse_wfd = handles[1];
	    M_FD(sp) = handles[0];
	     
	    setmode(handles[0], O_BINARY);
	    setmode(handles[1], O_BINARY);
	     
	    rc = DosCreateThread((unsigned long *) &sp->_emxmouse_thread,
				 mouse_server, (long) sp, 0, 8192);
	    if (rc) {
		printf("mouse thread error %d=%#x", rc, rc);
	    } else {
		sp->_mouse_type = M_XTERM;
	    }
	    returnVoid;
	}
    }
#endif  

#if USE_SYSMOUSE
    {
	static char dev_tty[] = "/dev/tty";
	struct mouse_info the_mouse;
	char *the_device = 0;

	if (NC_ISATTY(sp->_ifd))
	    the_device = ttyname(sp->_ifd);
	if (the_device == 0)
	    the_device = dev_tty;

	sp->_mouse_fd = open(the_device, O_RDWR);

	if (sp->_mouse_fd >= 0) {
	     
	    signal(SIGUSR2, SIG_IGN);
	    the_mouse.operation = MOUSE_MODE;
	    the_mouse.u.mode.mode = 0;
	    the_mouse.u.mode.signal = SIGUSR2;
	    if (ioctl(sp->_mouse_fd, CONS_MOUSECTL, &the_mouse) != -1) {
		signal(SIGUSR2, handle_sysmouse);
		the_mouse.operation = MOUSE_SHOW;
		ioctl(sp->_mouse_fd, CONS_MOUSECTL, &the_mouse);

#if defined(FBIO_MODEINFO) || defined(CONS_MODEINFO)	 
		{
#ifndef FBIO_GETMODE		 
#define FBIO_GETMODE    CONS_GET
#define FBIO_MODEINFO   CONS_MODEINFO
#endif  
		    video_info_t the_video;

		    if (ioctl(sp->_mouse_fd,
			      FBIO_GETMODE,
			      &the_video.vi_mode) != -1
			&& ioctl(sp->_mouse_fd,
				 FBIO_MODEINFO,
				 &the_video) != -1) {
			sp->_sysmouse_char_width = the_video.vi_cwidth;
			sp->_sysmouse_char_height = the_video.vi_cheight;
		    }
		}
#endif  

		if (sp->_sysmouse_char_width <= 0)
		    sp->_sysmouse_char_width = 8;
		if (sp->_sysmouse_char_height <= 0)
		    sp->_sysmouse_char_height = 16;
		sp->_mouse_type = M_SYSMOUSE;
		returnVoid;
	    }
	}
    }
#endif  

#ifdef USE_TERM_DRIVER
    CallDriver(sp, td_initmouse);
#else
     
    if (NonEmpty(key_mouse)) {
	init_xterm_mouse(sp);
    } else if (strstr(SP_TERMTYPE term_names, "xterm") != 0) {
	if (_nc_add_to_try(&(sp->_keytry), xterm_kmous, KEY_MOUSE) == OK)
	    init_xterm_mouse(sp);
    }
#endif

    returnVoid;
}

static bool
_nc_mouse_init(SCREEN *sp)
 
{
    bool result = FALSE;

    if (sp != 0) {
	if (!sp->_mouse_initialized) {
	    int i;

	    sp->_mouse_initialized = TRUE;

	    TR(MY_TRACE, ("_nc_mouse_init() called"));

	    sp->_mouse_eventp = FirstEV(sp);
	    for (i = 0; i < EV_MAX; i++)
		Invalidate(sp->_mouse_events + i);

	    initialize_mousetype(sp);

	    T(("_nc_mouse_init() set mousetype to %d", sp->_mouse_type));
	}
	result = sp->_mouse_initialized;
    }
    return result;
}

 
static bool
_nc_mouse_event(SCREEN *sp)
{
    MEVENT *eventp = sp->_mouse_eventp;
    bool result = FALSE;

    (void) eventp;

    switch (sp->_mouse_type) {
    case M_XTERM:
	 
#if USE_EMX_MOUSE
	{
	    char kbuf[3];

	    int i, res = read(M_FD(sp), &kbuf, 3);	 
	    if (res != 3)
		printf("Got %d chars instead of 3 for prefix.\n", res);
	    for (i = 0; i < res; i++) {
		if (kbuf[i] != key_mouse[i])
		    printf("Got char %d instead of %d for prefix.\n",
			   (int) kbuf[i], (int) key_mouse[i]);
	    }
	    result = TRUE;
	}
#endif  
	break;

#if USE_GPM_SUPPORT
    case M_GPM:
	if (sp->_mouse_fd >= 0) {
	     
	    Gpm_Event ev;

	    switch (my_Gpm_GetEvent(&ev)) {
	    case 0:
		 
		sp->_mouse_fd = -1;
		break;
	    case 1:
		 
		eventp->id = NORMAL_EVENT;

		eventp->bstate = 0;
		switch (ev.type & 0x0f) {
		case (GPM_DOWN):
		    if (ev.buttons & GPM_B_LEFT)
			eventp->bstate |= BUTTON1_PRESSED;
		    if (ev.buttons & GPM_B_MIDDLE)
			eventp->bstate |= BUTTON2_PRESSED;
		    if (ev.buttons & GPM_B_RIGHT)
			eventp->bstate |= BUTTON3_PRESSED;
		    break;
		case (GPM_UP):
		    if (ev.buttons & GPM_B_LEFT)
			eventp->bstate |= BUTTON1_RELEASED;
		    if (ev.buttons & GPM_B_MIDDLE)
			eventp->bstate |= BUTTON2_RELEASED;
		    if (ev.buttons & GPM_B_RIGHT)
			eventp->bstate |= BUTTON3_RELEASED;
		    break;
		default:
		    eventp->bstate |= REPORT_MOUSE_POSITION;
		    break;
		}

		eventp->x = ev.x - 1;
		eventp->y = ev.y - 1;
		eventp->z = 0;

		 
		sp->_mouse_eventp = NEXT(eventp);
		result = TRUE;
		break;
	    }
	}
	break;
#endif

#if USE_SYSMOUSE
    case M_SYSMOUSE:
	if (sp->_sysmouse_head < sp->_sysmouse_tail) {
	    *eventp = sp->_sysmouse_fifo[sp->_sysmouse_head];

	     
	    sp->_sysmouse_head += 1;
	    if (sp->_sysmouse_head == sp->_sysmouse_tail) {
		sp->_sysmouse_tail = 0;
		sp->_sysmouse_head = 0;
	    }

	     
	    sp->_mouse_eventp = eventp = NEXT(eventp);
	    result = TRUE;
	}
	break;
#endif  

#ifdef USE_TERM_DRIVER
    case M_TERM_DRIVER:
	while (sp->_drv_mouse_head < sp->_drv_mouse_tail) {
	    *eventp = sp->_drv_mouse_fifo[sp->_drv_mouse_head];

	     
	    sp->_drv_mouse_head += 1;
	    if (sp->_drv_mouse_head == sp->_drv_mouse_tail) {
		sp->_drv_mouse_tail = 0;
		sp->_drv_mouse_head = 0;
	    }

	     
	    sp->_mouse_eventp = eventp = NEXT(eventp);
	    result = TRUE;
	}
	break;
#endif

    case M_NONE:
	break;
    }

    return result;		 
}

#if USE_EMX_MOUSE
#define PRESS_POSITION(n) \
    do { \
	    eventp->bstate = MASK_PRESS(n); \
	    sp->_mouse_bstate |= MASK_PRESS(n); \
	    if (button & 0x40) { \
		    eventp->bstate = MASK_RELEASE(n); \
		    sp->_mouse_bstate &= ~MASK_PRESS(n); \
	    } \
    } while (0)
#else
#define PRESS_POSITION(n) \
    do { \
	    eventp->bstate = (mmask_t) ((sp->_mouse_bstate & MASK_PRESS(n)) \
				    ? REPORT_MOUSE_POSITION \
				    : MASK_PRESS(n)); \
	    sp->_mouse_bstate |= MASK_PRESS(n); \
    } while (0)
#endif

static bool
handle_wheel(SCREEN *sp, MEVENT * eventp, int button, int wheel)
{
    bool result = TRUE;

    switch (button & 3) {
    case 0:
	if (wheel) {
	    eventp->bstate = MASK_PRESS(4);
	     
	} else {
	    PRESS_POSITION(1);
	}
	break;
    case 1:
	if (wheel) {
#if NCURSES_MOUSE_VERSION >= 2
	    eventp->bstate = MASK_PRESS(5);
	     
#else
	     
	    eventp->bstate = REPORT_MOUSE_POSITION;
#endif
	} else {
	    PRESS_POSITION(2);
	}
	break;
    case 2:
	PRESS_POSITION(3);
	break;
    default:
	 
	eventp->bstate = REPORT_MOUSE_POSITION;
	result = FALSE;
	break;
    }
    return result;
}

static bool
decode_X10_bstate(SCREEN *sp, MEVENT * eventp, unsigned intro)
{
    bool result;
    int button = 0;
    int wheel = (intro & 96) == 96;

    eventp->bstate = 0;

    if (intro >= 96) {
	if (intro >= 160) {
	    button = (int) (intro - 152);	 
	} else {
	    button = (int) (intro - 92);	 
	}
    } else {
	button = (intro & 3);
    }

    if (button > MAX_BUTTONS) {
	eventp->bstate = REPORT_MOUSE_POSITION;
    } else if (!handle_wheel(sp, eventp, (int) intro, wheel)) {

	 
	if (sp->_mouse_bstate & BUTTON_PRESSED) {
	    int b;

	    eventp->bstate = BUTTON_RELEASED;
	    for (b = 1; b <= MAX_BUTTONS; ++b) {
		if (!(sp->_mouse_bstate & MASK_PRESS(b)))
		    eventp->bstate &= ~MASK_RELEASE(b);
	    }
	    sp->_mouse_bstate = 0;
	} else {
	     
	    eventp->bstate = REPORT_MOUSE_POSITION;
	}
    }

    if (intro & 4) {
	eventp->bstate |= BUTTON_SHIFT;
    }
    if (intro & 8) {
	eventp->bstate |= BUTTON_ALT;
    }
    if (intro & 16) {
	eventp->bstate |= BUTTON_CTRL;
    }
    result = (eventp->bstate & REPORT_MOUSE_POSITION) ? TRUE : FALSE;
    return result;
}

 
static bool
decode_xterm_X10(SCREEN *sp, MEVENT * eventp)
{
#define MAX_KBUF 3
    unsigned char kbuf[MAX_KBUF + 1];
    size_t grabbed;
    int res;
    bool result;

    _nc_set_read_thread(TRUE);
    for (grabbed = 0; grabbed < MAX_KBUF; grabbed += (size_t) res) {

	 
	res = (int) read(
#if USE_EMX_MOUSE
			    (M_FD(sp) >= 0) ? M_FD(sp) : sp->_ifd,
#else
			    sp->_ifd,
#endif
			    kbuf + grabbed, (size_t) (MAX_KBUF - (int) grabbed));
	if (res == -1)
	    break;
    }
    _nc_set_read_thread(FALSE);
    kbuf[MAX_KBUF] = '\0';

    TR(TRACE_IEVENT,
       ("_nc_mouse_inline sees the following xterm data: '%s'", kbuf));

     
    eventp->id = NORMAL_EVENT;

    result = decode_X10_bstate(sp, eventp, kbuf[0]);

    eventp->x = (kbuf[1] - ' ') - 1;
    eventp->y = (kbuf[2] - ' ') - 1;

    return result;
}

#ifdef EXP_XTERM_1005
 
static bool
decode_xterm_1005(SCREEN *sp, MEVENT * eventp)
{
    char kbuf[80];
    size_t grabbed;
    size_t limit = (sizeof(kbuf) - 1);
    unsigned coords[2];
    bool result;

    coords[0] = 0;
    coords[1] = 0;

    _nc_set_read_thread(TRUE);
    for (grabbed = 0; grabbed < limit;) {
	int res;

	res = (int) read(
#if USE_EMX_MOUSE
			    (M_FD(sp) >= 0) ? M_FD(sp) : sp->_ifd,
#else
			    sp->_ifd,
#endif
			    (kbuf + grabbed), (size_t) 1);
	if (res == -1)
	    break;
	grabbed += (size_t) res;
	if (grabbed > 1) {
	    size_t check = 1;
	    int n;

	    for (n = 0; n < 2; ++n) {
		int rc;

		if (check >= grabbed)
		    break;
		rc = _nc_conv_to_utf32(&coords[n], kbuf + check, (unsigned)
				       (grabbed - check));
		if (!rc)
		    break;
		check += (size_t) rc;
	    }
	    if (n >= 2)
		break;
	}
    }
    _nc_set_read_thread(FALSE);

    TR(TRACE_IEVENT,
       ("_nc_mouse_inline sees the following xterm data: %s",
	_nc_visbufn(kbuf, (int) grabbed)));

     
    eventp->id = NORMAL_EVENT;

    result = decode_X10_bstate(sp, eventp, UChar(kbuf[0]));

    eventp->x = (int) (coords[0] - ' ') - 1;
    eventp->y = (int) (coords[1] - ' ') - 1;

    return result;
}
#endif  

 
#define isInter(c) ((c) >= 0x20 && (c) <= 0x2f)
#define isParam(c) ((c) >= 0x30 && (c) <= 0x3f)
#define isFinal(c) ((c) >= 0x40 && (c) <= 0x7e)

#define MAX_PARAMS 9

typedef struct {
    int nerror;			 
    int nparam;			 
    int params[MAX_PARAMS];
    int final;			 
} SGR_DATA;

static bool
read_SGR(SCREEN *sp, SGR_DATA * result)
{
    char kbuf[80];		 
    int grabbed = 0;
    int ch = 0;
    int now = -1;
    int marker = 1;

    memset(result, 0, sizeof(*result));
    _nc_set_read_thread(TRUE);

    do {
	int res;

	res = (int) read(
#if USE_EMX_MOUSE
			    (M_FD(sp) >= 0) ? M_FD(sp) : sp->_ifd,
#else
			    sp->_ifd,
#endif
			    (kbuf + grabbed), (size_t) 1);
	if (res == -1)
	    break;
	if ((grabbed + MAX_KBUF) >= (int) sizeof(kbuf)) {
	    result->nerror++;
	    break;
	}
	ch = UChar(kbuf[grabbed]);
	kbuf[grabbed + 1] = 0;
	switch (ch) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	    if (marker) {
		++now;
		result->nparam = (now + 1);
	    }
	    marker = 0;
	    result->params[now] = (result->params[now] * 10) + (ch - '0');
	    break;
	case ';':
	    if (marker) {
		++now;
		result->nparam = (now + 1);
	    }
	    marker = 1;
	    break;
	default:
	    if (ch < 32 || ch > 126) {
		 
		result->nerror++;
		continue;
	    } else if (isFinal(ch)) {
		if (marker) {
		    result->nparam++;
		}
		result->final = ch;
	    } else {
		result->nerror++;
	    }
	    break;
	}
	++grabbed;
    } while (!isFinal(ch));
    _nc_set_read_thread(FALSE);

    kbuf[++grabbed] = 0;
    TR(TRACE_IEVENT,
       ("_nc_mouse_inline sees the following xterm data: '%s'", kbuf));
    return (grabbed > 0) && (result->nerror == 0);
}

static bool
decode_xterm_SGR1006(SCREEN *sp, MEVENT * eventp)
{
    SGR_DATA data;
    bool result = FALSE;
    if (read_SGR(sp, &data)) {
	int b = data.params[0];
	int b3 = 1 + (b & 3);
	int wheel = ((b & 64) == 64);

	if (b >= 132) {
	    b3 = MAX_BUTTONS + 1;
	} else if (b >= 128) {
	    b3 = (b - 120);	 
	} else if (b >= 64) {
	    b3 = (b - 60);	 
	}

	eventp->id = NORMAL_EVENT;
	if (data.final == 'M') {
	    (void) handle_wheel(sp, eventp, b, wheel);
	} else if (b3 > MAX_BUTTONS) {
	    eventp->bstate = REPORT_MOUSE_POSITION;
	} else {
	    mmask_t pressed = (mmask_t) NCURSES_MOUSE_MASK(b3, NCURSES_BUTTON_PRESSED);
	    mmask_t release = (mmask_t) NCURSES_MOUSE_MASK(b3, NCURSES_BUTTON_RELEASED);
	    if (sp->_mouse_bstate & pressed) {
		eventp->bstate = release;
		sp->_mouse_bstate &= ~pressed;
	    } else {
		eventp->bstate = REPORT_MOUSE_POSITION;
	    }
	}
	if (b & 4) {
	    eventp->bstate |= BUTTON_SHIFT;
	}
	if (b & 8) {
	    eventp->bstate |= BUTTON_ALT;
	}
	if (b & 16) {
	    eventp->bstate |= BUTTON_CTRL;
	}
	result = (eventp->bstate & REPORT_MOUSE_POSITION) ? TRUE : FALSE;
	eventp->x = (data.params[1] ? (data.params[1] - 1) : 0);
	eventp->y = (data.params[2] ? (data.params[2] - 1) : 0);
    }
    return result;
}

static bool
_nc_mouse_inline(SCREEN *sp)
 
{
    bool result = FALSE;
    MEVENT *eventp = sp->_mouse_eventp;

    TR(MY_TRACE, ("_nc_mouse_inline() called"));

    if (sp->_mouse_type == M_XTERM) {
	switch (sp->_mouse_format) {
	case MF_X10:
	    result = decode_xterm_X10(sp, eventp);
	    break;
	case MF_SGR1006:
	    result = decode_xterm_SGR1006(sp, eventp);
	    break;
#ifdef EXP_XTERM_1005
	case MF_XTERM_1005:
	    result = decode_xterm_1005(sp, eventp);
	    break;
#endif
	}

	TR(MY_TRACE,
	   ("_nc_mouse_inline: primitive mouse-event %s has slot %ld",
	    _nc_tracemouse(sp, eventp),
	    (long) IndexEV(sp, eventp)));

	 
	sp->_mouse_eventp = NEXT(eventp);

	if (!result) {
	     
	    if (eventp->bstate & BUTTON_PRESSED) {
		int b;

		for (b = 4; b <= MAX_BUTTONS; ++b) {
		    if ((eventp->bstate & MASK_PRESS(b))) {
			result = TRUE;
			break;
		    }
		}
	    }
	}
    }

    return (result);
}

static void
mouse_activate(SCREEN *sp, int on)
{
    if (!on && !sp->_mouse_initialized)
	return;

    if (!_nc_mouse_init(sp))
	return;

    if (on) {
	sp->_mouse_bstate = 0;
	switch (sp->_mouse_type) {
	case M_XTERM:
#if NCURSES_EXT_FUNCS
	    NCURSES_SP_NAME(keyok) (NCURSES_SP_ARGx KEY_MOUSE, on);
#endif
	    TPUTS_TRACE("xterm mouse initialization");
	    enable_xterm_mouse(sp, 1);
	    break;
#if USE_GPM_SUPPORT
	case M_GPM:
	    if (enable_gpm_mouse(sp, TRUE)) {
		sp->_mouse_fd = *(my_gpm_fd);
		T(("GPM mouse_fd %d", sp->_mouse_fd));
	    }
	    break;
#endif
#if USE_SYSMOUSE
	case M_SYSMOUSE:
	    signal(SIGUSR2, handle_sysmouse);
	    sp->_mouse_active = TRUE;
	    break;
#endif
#ifdef USE_TERM_DRIVER
	case M_TERM_DRIVER:
	    sp->_mouse_active = TRUE;
	    break;
#endif
	case M_NONE:
	    return;
	}
	 
	sp->_mouse_event = _nc_mouse_event;
	sp->_mouse_inline = _nc_mouse_inline;
	sp->_mouse_parse = _nc_mouse_parse;
	sp->_mouse_resume = _nc_mouse_resume;
	sp->_mouse_wrap = _nc_mouse_wrap;
    } else {

	switch (sp->_mouse_type) {
	case M_XTERM:
	    TPUTS_TRACE("xterm mouse deinitialization");
	    enable_xterm_mouse(sp, 0);
	    break;
#if USE_GPM_SUPPORT
	case M_GPM:
	    enable_gpm_mouse(sp, FALSE);
	    break;
#endif
#if USE_SYSMOUSE
	case M_SYSMOUSE:
	    signal(SIGUSR2, SIG_IGN);
	    sp->_mouse_active = FALSE;
	    break;
#endif
#ifdef USE_TERM_DRIVER
	case M_TERM_DRIVER:
	    sp->_mouse_active = FALSE;
	    break;
#endif
	case M_NONE:
	    return;
	}
    }
    NCURSES_SP_NAME(_nc_flush) (NCURSES_SP_ARG);
}

 

static bool
_nc_mouse_parse(SCREEN *sp, int runcount)
 
{
    MEVENT *eventp = sp->_mouse_eventp;
    MEVENT *next, *ep;
    MEVENT *first_valid = NULL;
    MEVENT *first_invalid = NULL;
    int n;
    int b;
    bool merge;
    bool endLoop;

    TR(MY_TRACE, ("_nc_mouse_parse(%d) called", runcount));

     

     
    ep = eventp;
    for (n = runcount; n < EV_MAX; n++) {
	Invalidate(ep);
	ep = NEXT(ep);
    }

#ifdef TRACE
    if (USE_TRACEF(TRACE_IEVENT)) {
	_trace_slot(sp, "before mouse press/release merge:");
	_tracef("_nc_mouse_parse: run starts at %ld, ends at %ld, count %d",
		RunParams(sp, eventp, ep),
		runcount);
	_nc_unlock_global(tracef);
    }
#endif  

     
    endLoop = FALSE;
    while (!endLoop) {
	next = NEXT(ep);
	if (next == eventp) {
	     
	    endLoop = TRUE;
	} else {

#define MASK_CHANGED(x) (!(ep->bstate & MASK_PRESS(x)) \
		      == !(next->bstate & MASK_RELEASE(x)))

	    if (ValidEvent(ep) && ValidEvent(next)
		&& ep->x == next->x && ep->y == next->y
		&& (ep->bstate & BUTTON_PRESSED)
		&& (!(next->bstate & BUTTON_PRESSED))) {
		bool changed = TRUE;

		for (b = 1; b <= MAX_BUTTONS; ++b) {
		    if (!MASK_CHANGED(b)) {
			changed = FALSE;
			break;
		    }
		}

		if (changed) {
		    merge = FALSE;
		    for (b = 1; b <= MAX_BUTTONS; ++b) {
			if ((sp->_mouse_mask2 & MASK_CLICK(b))
			    && (ep->bstate & MASK_PRESS(b))) {
			    next->bstate &= ~MASK_RELEASE(b);
			    next->bstate |= MASK_CLICK(b);
			    merge = TRUE;
			}
		    }
		    if (merge) {
			Invalidate(ep);
		    }
		}
	    }
	}

	 
	if (!ValidEvent(ep)) {
	    if ((first_valid != NULL) && (first_invalid == NULL)) {
		first_invalid = ep;
	    }
	} else {
	    if (first_valid == NULL) {
		first_valid = ep;
	    } else if (first_invalid != NULL) {
		*first_invalid = *ep;
		Invalidate(ep);
		first_invalid = NEXT(first_invalid);
	    }
	}

	ep = next;
    }

    if (first_invalid != NULL) {
	eventp = first_invalid;
    }
#ifdef TRACE
    if (USE_TRACEF(TRACE_IEVENT)) {
	_trace_slot(sp, "before mouse click merge:");
	if (first_valid == NULL) {
	    _tracef("_nc_mouse_parse: no valid event");
	} else {
	    _tracef("_nc_mouse_parse: run starts at %ld, ends at %ld, count %d",
		    RunParams(sp, eventp, first_valid),
		    runcount);
	    _nc_unlock_global(tracef);
	}
    }
#endif  

     
    first_invalid = NULL;
    endLoop = (first_valid == NULL);
    ep = first_valid;
    while (!endLoop) {
	next = NEXT(ep);

	if (next == eventp) {
	     
	    endLoop = TRUE;
	} else if (!ValidEvent(next)) {
	    continue;
	} else {
	     
	    if ((ep->bstate & BUTTON_CLICKED)
		&& (next->bstate & BUTTON_CLICKED)) {
		merge = FALSE;
		for (b = 1; b <= MAX_BUTTONS; ++b) {
		    if ((sp->_mouse_mask2 & MASK_DOUBLE_CLICK(b))
			&& (ep->bstate & MASK_CLICK(b))
			&& (next->bstate & MASK_CLICK(b))) {
			next->bstate &= ~MASK_CLICK(b);
			next->bstate |= MASK_DOUBLE_CLICK(b);
			merge = TRUE;
		    }
		}
		if (merge) {
		    Invalidate(ep);
		}
	    }

	     
	    if ((ep->bstate & BUTTON_DOUBLE_CLICKED)
		&& (next->bstate & BUTTON_CLICKED)) {
		merge = FALSE;
		for (b = 1; b <= MAX_BUTTONS; ++b) {
		    if ((sp->_mouse_mask2 & MASK_TRIPLE_CLICK(b))
			&& (ep->bstate & MASK_DOUBLE_CLICK(b))
			&& (next->bstate & MASK_CLICK(b))) {
			next->bstate &= ~MASK_CLICK(b);
			next->bstate |= MASK_TRIPLE_CLICK(b);
			merge = TRUE;
		    }
		}
		if (merge) {
		    Invalidate(ep);
		}
	    }
	}

	 
	if (!(ep->bstate & sp->_mouse_mask2)) {
	    Invalidate(ep);
	}

	 
	if (!ValidEvent(ep)) {
	    if (ep == first_valid) {
		first_valid = next;
	    } else if (first_invalid == NULL) {
		first_invalid = ep;
	    }
	} else if (first_invalid != NULL) {
	    *first_invalid = *ep;
	    Invalidate(ep);
	    first_invalid = NEXT(first_invalid);
	}

	ep = next;
    }

    if (first_invalid == NULL) {
	first_invalid = eventp;
    }
    sp->_mouse_eventp = first_invalid;

#ifdef TRACE
    if (first_valid != NULL) {
	if (USE_TRACEF(TRACE_IEVENT)) {
	    _trace_slot(sp, "after mouse event queue compaction:");
	    _tracef("_nc_mouse_parse: run starts at %ld, ends at %ld, count %d",
		    RunParams(sp, first_invalid, first_valid),
		    runcount);
	    _nc_unlock_global(tracef);
	}
	for (ep = first_valid; ep != first_invalid; ep = NEXT(ep)) {
	    if (ValidEvent(ep))
		TR(MY_TRACE,
		   ("_nc_mouse_parse: returning composite mouse event %s at slot %ld",
		    _nc_tracemouse(sp, ep),
		    (long) IndexEV(sp, ep)));
	}
    }
#endif  

     
    ep = PREV(first_invalid);
    return ValidEvent(ep) && ((ep->bstate & sp->_mouse_mask) != 0);
}

static void
_nc_mouse_wrap(SCREEN *sp)
 
{
    TR(MY_TRACE, ("_nc_mouse_wrap() called"));

    switch (sp->_mouse_type) {
    case M_XTERM:
	if (sp->_mouse_mask)
	    mouse_activate(sp, FALSE);
	break;
#if USE_GPM_SUPPORT
	 
    case M_GPM:
	if (sp->_mouse_mask)
	    mouse_activate(sp, FALSE);
	break;
#endif
#if USE_SYSMOUSE
    case M_SYSMOUSE:
	mouse_activate(sp, FALSE);
	break;
#endif
#ifdef USE_TERM_DRIVER
    case M_TERM_DRIVER:
	mouse_activate(sp, FALSE);
	break;
#endif
    case M_NONE:
	break;
    }
}

static void
_nc_mouse_resume(SCREEN *sp)
 
{
    TR(MY_TRACE, ("_nc_mouse_resume() called"));

    switch (sp->_mouse_type) {
    case M_XTERM:
	 
	if (sp->_mouse_mask)
	    mouse_activate(sp, TRUE);
	break;

#if USE_GPM_SUPPORT
    case M_GPM:
	 
	if (sp->_mouse_mask)
	    mouse_activate(sp, TRUE);
	break;
#endif

#if USE_SYSMOUSE
    case M_SYSMOUSE:
	mouse_activate(sp, TRUE);
	break;
#endif

#ifdef USE_TERM_DRIVER
    case M_TERM_DRIVER:
	mouse_activate(sp, TRUE);
	break;
#endif

    case M_NONE:
	break;
    }
}

 

NCURSES_EXPORT(int)
NCURSES_SP_NAME(getmouse) (NCURSES_SP_DCLx MEVENT * aevent)
{
    int result = ERR;
    MEVENT *eventp;

    T((T_CALLED("getmouse(%p,%p)"), (void *) SP_PARM, (void *) aevent));

    if ((aevent != 0) &&
	(SP_PARM != 0) &&
	(SP_PARM->_mouse_type != M_NONE) &&
	(eventp = SP_PARM->_mouse_eventp) != 0) {
	 
	MEVENT *prev = PREV(eventp);

	 
	while (ValidEvent(prev) && (!(prev->bstate & SP_PARM->_mouse_mask2))) {
	    Invalidate(prev);
	    prev = PREV(prev);
	}
	if (ValidEvent(prev)) {
	     
	    *aevent = *prev;

	    TR(TRACE_IEVENT, ("getmouse: returning event %s from slot %ld",
			      _nc_tracemouse(SP_PARM, prev),
			      (long) IndexEV(SP_PARM, prev)));

	    Invalidate(prev);	 
	    SP_PARM->_mouse_eventp = prev;
	    result = OK;
	} else {
	     
	    aevent->bstate = 0;
	    Invalidate(aevent);
	    aevent->x = 0;
	    aevent->y = 0;
	    aevent->z = 0;
	}
    }
    returnCode(result);
}

#if NCURSES_SP_FUNCS
 
NCURSES_EXPORT(int)
getmouse(MEVENT * aevent)
{
    return NCURSES_SP_NAME(getmouse) (CURRENT_SCREEN, aevent);
}
#endif

NCURSES_EXPORT(int)
NCURSES_SP_NAME(ungetmouse) (NCURSES_SP_DCLx MEVENT * aevent)
{
    int result = ERR;
    MEVENT *eventp;

    T((T_CALLED("ungetmouse(%p,%p)"), (void *) SP_PARM, (void *) aevent));

    if (aevent != 0 &&
	SP_PARM != 0 &&
	(eventp = SP_PARM->_mouse_eventp) != 0) {

	 
	*eventp = *aevent;

	 
	SP_PARM->_mouse_eventp = NEXT(eventp);

	 
	result = NCURSES_SP_NAME(ungetch) (NCURSES_SP_ARGx KEY_MOUSE);
    }
    returnCode(result);
}

#if NCURSES_SP_FUNCS
 
NCURSES_EXPORT(int)
ungetmouse(MEVENT * aevent)
{
    return NCURSES_SP_NAME(ungetmouse) (CURRENT_SCREEN, aevent);
}
#endif

NCURSES_EXPORT(mmask_t)
NCURSES_SP_NAME(mousemask) (NCURSES_SP_DCLx mmask_t newmask, mmask_t * oldmask)
 
{
    mmask_t result = 0;

    T((T_CALLED("mousemask(%p,%#lx,%p)"),
       (void *) SP_PARM,
       (unsigned long) newmask,
       (void *) oldmask));

    if (SP_PARM != 0) {
	if (oldmask)
	    *oldmask = SP_PARM->_mouse_mask;

	if (newmask || SP_PARM->_mouse_initialized) {
	    _nc_mouse_init(SP_PARM);

	    if (SP_PARM->_mouse_type != M_NONE) {
		int b;

		result = newmask &
		    (REPORT_MOUSE_POSITION
		     | BUTTON_ALT
		     | BUTTON_CTRL
		     | BUTTON_SHIFT
		     | BUTTON_PRESSED
		     | BUTTON_RELEASED
		     | BUTTON_CLICKED
		     | BUTTON_DOUBLE_CLICKED
		     | BUTTON_TRIPLE_CLICKED);

		mouse_activate(SP_PARM, (bool) (result != 0));

		SP_PARM->_mouse_mask = result;
		SP_PARM->_mouse_mask2 = result;

		 
		for (b = 1; b <= MAX_BUTTONS; ++b) {
		    if (SP_PARM->_mouse_mask2 & MASK_TRIPLE_CLICK(b))
			SP_PARM->_mouse_mask2 |= MASK_DOUBLE_CLICK(b);
		    if (SP_PARM->_mouse_mask2 & MASK_DOUBLE_CLICK(b))
			SP_PARM->_mouse_mask2 |= MASK_CLICK(b);
		    if (SP_PARM->_mouse_mask2 & MASK_CLICK(b))
			SP_PARM->_mouse_mask2 |= (MASK_PRESS(b) |
						  MASK_RELEASE(b));
		}
	    }
	}
    }
    returnMMask(result);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(mmask_t)
mousemask(mmask_t newmask, mmask_t * oldmask)
{
    return NCURSES_SP_NAME(mousemask) (CURRENT_SCREEN, newmask, oldmask);
}
#endif

NCURSES_EXPORT(bool)
wenclose(const WINDOW *win, int y, int x)
 
{
    bool result = FALSE;

    T((T_CALLED("wenclose(%p,%d,%d)"), (const void *) win, y, x));

    if (win != 0) {
	y -= win->_yoffset;
	result = ((win->_begy <= y &&
		   win->_begx <= x &&
		   (win->_begx + win->_maxx) >= x &&
		   (win->_begy + win->_maxy) >= y) ? TRUE : FALSE);
    }
    returnBool(result);
}

NCURSES_EXPORT(int)
NCURSES_SP_NAME(mouseinterval) (NCURSES_SP_DCLx int maxclick)
 
{
    int oldval;

    T((T_CALLED("mouseinterval(%p,%d)"), (void *) SP_PARM, maxclick));

    if (SP_PARM != 0) {
	oldval = SP_PARM->_maxclick;
	if (maxclick >= 0)
	    SP_PARM->_maxclick = maxclick;
    } else {
	oldval = DEFAULT_MAXCLICK;
    }

    returnCode(oldval);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
mouseinterval(int maxclick)
{
    return NCURSES_SP_NAME(mouseinterval) (CURRENT_SCREEN, maxclick);
}
#endif

 
NCURSES_EXPORT(bool)
_nc_has_mouse(SCREEN *sp)
{
    return (((0 == sp) || (sp->_mouse_type == M_NONE)) ? FALSE : TRUE);
}

NCURSES_EXPORT(bool)
NCURSES_SP_NAME(has_mouse) (NCURSES_SP_DCL0)
{
    return _nc_has_mouse(SP_PARM);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(bool)
has_mouse(void)
{
    return _nc_has_mouse(CURRENT_SCREEN);
}
#endif

NCURSES_EXPORT(bool)
wmouse_trafo(const WINDOW *win, int *pY, int *pX, bool to_screen)
{
    bool result = FALSE;

    T((T_CALLED("wmouse_trafo(%p,%p,%p,%d)"),
       (const void *) win,
       (void *) pY,
       (void *) pX,
       to_screen));

    if (win && pY && pX) {
	int y = *pY;
	int x = *pX;

	if (to_screen) {
	    y += win->_begy + win->_yoffset;
	    x += win->_begx;
	    if (wenclose(win, y, x))
		result = TRUE;
	} else {
	    if (wenclose(win, y, x)) {
		y -= (win->_begy + win->_yoffset);
		x -= win->_begx;
		result = TRUE;
	    }
	}
	if (result) {
	    *pX = x;
	    *pY = y;
	}
    }
    returnBool(result);
}
