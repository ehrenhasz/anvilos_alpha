 

 

 

 
 

#include <reset_cmd.h>
#include <termcap.h>
#include <transform.h>
#include <tty_settings.h>

#if HAVE_GETTTYNAM
#include <ttyent.h>
#endif
#ifdef NeXT
char *ttyname(int fd);
#endif

MODULE_ID("$Id: tset.c,v 1.131 2021/12/04 23:02:13 tom Exp $")

#ifndef environ
extern char **environ;
#endif

const char *_nc_progname = "tset";

#define LOWERCASE(c) ((isalpha(UChar(c)) && isupper(UChar(c))) ? tolower(UChar(c)) : (c))

static GCC_NORETURN void exit_error(void);

static int
CaselessCmp(const char *a, const char *b)
{				 
    while (*a && *b) {
	int cmp = LOWERCASE(*a) - LOWERCASE(*b);
	if (cmp != 0)
	    break;
	a++, b++;
    }
    return LOWERCASE(*a) - LOWERCASE(*b);
}

static GCC_NORETURN void
exit_error(void)
{
    restore_tty_settings();
    (void) fprintf(stderr, "\n");
    fflush(stderr);
    ExitProgram(EXIT_FAILURE);
     
}

static GCC_NORETURN void
err(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    (void) fprintf(stderr, "%s: ", _nc_progname);
    (void) vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit_error();
     
}

static GCC_NORETURN void
failed(const char *msg)
{
    char temp[BUFSIZ];
    size_t len = strlen(_nc_progname) + 2;

    if ((int) len < (int) sizeof(temp) - 12) {
	_nc_STRCPY(temp, _nc_progname, sizeof(temp));
	_nc_STRCAT(temp, ": ", sizeof(temp));
    } else {
	_nc_STRCPY(temp, "tset: ", sizeof(temp));
    }
    _nc_STRNCAT(temp, msg, sizeof(temp), sizeof(temp) - strlen(temp) - 2);
    perror(temp);
    exit_error();
     
}

 
static const char *
askuser(const char *dflt)
{
    static char answer[256];

     
    clearerr(stdin);
    if (feof(stdin) || ferror(stdin)) {
	(void) fprintf(stderr, "\n");
	exit_error();
	 
    }

    for (;;) {
	char *p;

	if (dflt)
	    (void) fprintf(stderr, "Terminal type? [%s] ", dflt);
	else
	    (void) fprintf(stderr, "Terminal type? ");
	(void) fflush(stderr);

	if (fgets(answer, sizeof(answer), stdin) == 0) {
	    if (dflt == 0) {
		exit_error();
		 
	    }
	    return (dflt);
	}

	if ((p = strchr(answer, '\n')) != 0)
	    *p = '\0';
	if (answer[0])
	    return (answer);
	if (dflt != 0)
	    return (dflt);
    }
}

 

 
#define	GT		0x01
#define	EQ		0x02
#define	LT		0x04
#define	NOT		0x08
#define	GE		(GT | EQ)
#define	LE		(LT | EQ)

typedef struct map {
    struct map *next;		 
    const char *porttype;	 
    const char *type;		 
    int conditional;		 
    int speed;			 
} MAP;

static MAP *cur, *maplist;

#define DATA(name,value) { { name }, value }

typedef struct speeds {
    const char string[8];
    int speed;
} SPEEDS;

#if defined(EXP_WIN32_DRIVER)
static const SPEEDS speeds[] =
{
    {"0", 0}
};
#else
static const SPEEDS speeds[] =
{
    DATA("0", B0),
    DATA("50", B50),
    DATA("75", B75),
    DATA("110", B110),
    DATA("134", B134),
    DATA("134.5", B134),
    DATA("150", B150),
    DATA("200", B200),
    DATA("300", B300),
    DATA("600", B600),
    DATA("1200", B1200),
    DATA("1800", B1800),
    DATA("2400", B2400),
    DATA("4800", B4800),
    DATA("9600", B9600),
     
#ifdef B19200
    DATA("19200", B19200),
#endif
#ifdef B38400
    DATA("38400", B38400),
#endif
#ifdef B19200
    DATA("19200", B19200),
#endif
#ifdef B38400
    DATA("38400", B38400),
#endif
#ifdef B19200
    DATA("19200", B19200),
#else
#ifdef EXTA
    DATA("19200", EXTA),
#endif
#endif
#ifdef B38400
    DATA("38400", B38400),
#else
#ifdef EXTB
    DATA("38400", EXTB),
#endif
#endif
#ifdef B57600
    DATA("57600", B57600),
#endif
#ifdef B76800
    DATA("76800", B57600),
#endif
#ifdef B115200
    DATA("115200", B115200),
#endif
#ifdef B153600
    DATA("153600", B153600),
#endif
#ifdef B230400
    DATA("230400", B230400),
#endif
#ifdef B307200
    DATA("307200", B307200),
#endif
#ifdef B460800
    DATA("460800", B460800),
#endif
#ifdef B500000
    DATA("500000", B500000),
#endif
#ifdef B576000
    DATA("576000", B576000),
#endif
#ifdef B921600
    DATA("921600", B921600),
#endif
#ifdef B1000000
    DATA("1000000", B1000000),
#endif
#ifdef B1152000
    DATA("1152000", B1152000),
#endif
#ifdef B1500000
    DATA("1500000", B1500000),
#endif
#ifdef B2000000
    DATA("2000000", B2000000),
#endif
#ifdef B2500000
    DATA("2500000", B2500000),
#endif
#ifdef B3000000
    DATA("3000000", B3000000),
#endif
#ifdef B3500000
    DATA("3500000", B3500000),
#endif
#ifdef B4000000
    DATA("4000000", B4000000),
#endif
};
#undef DATA
#endif

static int
tbaudrate(char *rate)
{
    const SPEEDS *sp = 0;
    size_t n;

     
    if (*rate == 'B')
	++rate;

    for (n = 0; n < SIZEOF(speeds); ++n) {
	if (n > 0 && (speeds[n].speed <= speeds[n - 1].speed)) {
	     
	    break;
	}
	if (!CaselessCmp(rate, speeds[n].string)) {
	    sp = speeds + n;
	    break;
	}
    }
    if (sp == 0)
	err("unknown baud rate %s", rate);
    return (sp->speed);
}

 
static void
add_mapping(const char *port, char *arg)
{
    MAP *mapp;
    char *copy, *p;
    const char *termp;
    char *base = 0;

    copy = strdup(arg);
    mapp = typeMalloc(MAP, 1);
    if (copy == 0 || mapp == 0)
	failed("malloc");

    assert(copy != 0);
    assert(mapp != 0);

    mapp->next = 0;
    if (maplist == 0)
	cur = maplist = mapp;
    else {
	cur->next = mapp;
	cur = mapp;
    }

    mapp->porttype = arg;
    mapp->conditional = 0;

    arg = strpbrk(arg, "><@=!:");

    if (arg == 0) {		 
	mapp->type = mapp->porttype;
	mapp->porttype = 0;
	goto done;
    }

    if (arg == mapp->porttype)	 
	termp = mapp->porttype = 0;
    else
	termp = base = arg;

    for (;; ++arg) {		 
	switch (*arg) {
	case '<':
	    if (mapp->conditional & GT)
		goto badmopt;
	    mapp->conditional |= LT;
	    break;
	case '>':
	    if (mapp->conditional & LT)
		goto badmopt;
	    mapp->conditional |= GT;
	    break;
	case '@':
	case '=':		 
	    mapp->conditional |= EQ;
	    break;
	case '!':
	    mapp->conditional |= NOT;
	    break;
	default:
	    goto next;
	}
    }

  next:
    if (*arg == ':') {
	if (mapp->conditional)
	    goto badmopt;
	++arg;
    } else {			 
	arg = strchr(p = arg, ':');
	if (arg == 0)
	    goto badmopt;
	*arg++ = '\0';
	mapp->speed = tbaudrate(p);
    }

    mapp->type = arg;

     
    if (termp != 0)
	*base = '\0';

     
    if (mapp->conditional & NOT)
	mapp->conditional = ~mapp->conditional & (EQ | GT | LT);

     
  done:
    if (port) {
	if (mapp->porttype) {
	  badmopt:
	    err("illegal -m option format: %s", copy);
	}
	mapp->porttype = port;
    }
    free(copy);
#ifdef MAPDEBUG
    (void) printf("port: %s\n", mapp->porttype ? mapp->porttype : "ANY");
    (void) printf("type: %s\n", mapp->type);
    (void) printf("conditional: ");
    p = "";
    if (mapp->conditional & GT) {
	(void) printf("GT");
	p = "/";
    }
    if (mapp->conditional & EQ) {
	(void) printf("%sEQ", p);
	p = "/";
    }
    if (mapp->conditional & LT)
	(void) printf("%sLT", p);
    (void) printf("\nspeed: %d\n", mapp->speed);
#endif
}

 
static const char *
mapped(const char *type)
{
    MAP *mapp;
    int match;

    for (mapp = maplist; mapp; mapp = mapp->next)
	if (mapp->porttype == 0 || !strcmp(mapp->porttype, type)) {
	    switch (mapp->conditional) {
	    case 0:		 
		match = TRUE;
		break;
	    case EQ:
		match = ((int) ospeed == mapp->speed);
		break;
	    case GE:
		match = ((int) ospeed >= mapp->speed);
		break;
	    case GT:
		match = ((int) ospeed > mapp->speed);
		break;
	    case LE:
		match = ((int) ospeed <= mapp->speed);
		break;
	    case LT:
		match = ((int) ospeed < mapp->speed);
		break;
	    default:
		match = FALSE;
	    }
	    if (match)
		return (mapp->type);
	}
     
    return (type);
}

 

 
static const char *
get_termcap_entry(int fd, char *userarg)
{
    int errret;
    char *p;
    const char *ttype;
#if HAVE_PATH_TTYS
#if HAVE_GETTTYNAM
    struct ttyent *t;
#else
    FILE *fp;
#endif
    char *ttypath;
#endif  

    (void) fd;

    if (userarg) {
	ttype = userarg;
	goto found;
    }

     
    if ((ttype = getenv("TERM")) != 0)
	goto map;

#if HAVE_PATH_TTYS
    if ((ttypath = ttyname(fd)) != 0) {
	p = _nc_basename(ttypath);
#if HAVE_GETTTYNAM
	 
	if ((t = getttynam(p))) {
	    ttype = t->ty_type;
	    goto map;
	}
#else
	if ((fp = fopen("/etc/ttytype", "r")) != 0
	    || (fp = fopen("/etc/ttys", "r")) != 0) {
	    char buffer[BUFSIZ];
	    char *s, *t, *d;

	    while (fgets(buffer, sizeof(buffer) - 1, fp) != 0) {
		for (s = buffer, t = d = 0; *s; s++) {
		    if (isspace(UChar(*s)))
			*s = '\0';
		    else if (t == 0)
			t = s;
		    else if (d == 0 && s != buffer && s[-1] == '\0')
			d = s;
		}
		if (t != 0 && d != 0 && !strcmp(d, p)) {
		    ttype = strdup(t);
		    fclose(fp);
		    goto map;
		}
	    }
	    fclose(fp);
	}
#endif  
    }
#endif  

     
    ttype = "unknown";

  map:ttype = mapped(ttype);

     
  found:
    if ((p = getenv("TERMCAP")) != 0 && !_nc_is_abs_path(p)) {
	 
	int n;
	for (n = 0; environ[n] != 0; n++) {
	    if (!strncmp("TERMCAP=", environ[n], (size_t) 8)) {
		while ((environ[n] = environ[n + 1]) != 0) {
		    n++;
		}
		break;
	    }
	}
    }

     
    if (ttype[0] == '?') {
	if (ttype[1] != '\0')
	    ttype = askuser(ttype + 1);
	else
	    ttype = askuser(0);
    }
     
    while (setupterm((NCURSES_CONST char *) ttype, fd, &errret)
	   != OK) {
	if (errret == 0) {
	    (void) fprintf(stderr, "%s: unknown terminal type %s\n",
			   _nc_progname, ttype);
	    ttype = 0;
	} else {
	    (void) fprintf(stderr,
			   "%s: can't initialize terminal type %s (error %d)\n",
			   _nc_progname, ttype, errret);
	    ttype = 0;
	}
	ttype = askuser(ttype);
    }
#if BROKEN_LINKER
    tgetflag("am");		 
#endif
    return (ttype);
}

 

 
static void
obsolete(char **argv)
{
    for (; *argv; ++argv) {
	char *parm = argv[0];

	if (parm[0] == '-' && parm[1] == '\0') {
	    argv[0] = strdup("-q");
	    continue;
	}

	if ((parm[0] != '-')
	    || (argv[1] && argv[1][0] != '-')
	    || (parm[1] != 'e' && parm[1] != 'i' && parm[1] != 'k')
	    || (parm[2] != '\0'))
	    continue;
	switch (argv[0][1]) {
	case 'e':
	    argv[0] = strdup("-e^H");
	    break;
	case 'i':
	    argv[0] = strdup("-i^C");
	    break;
	case 'k':
	    argv[0] = strdup("-k^U");
	    break;
	}
    }
}

static void
print_shell_commands(const char *ttype)
{
    const char *p;
    int len;
    char *var;
    char *leaf;
     
    if ((var = getenv("SHELL")) != 0
	&& ((len = (int) strlen(leaf = _nc_basename(var))) >= 3)
	&& !strcmp(leaf + len - 3, "csh"))
	p = "set noglob;\nsetenv TERM %s;\nunset noglob;\n";
    else
	p = "TERM=%s;\n";
    (void) printf(p, ttype);
}

static void
usage(void)
{
#define SKIP(s)			 
#define KEEP(s) s "\n"
    static const char msg[] =
    {
	KEEP("")
	KEEP("Options:")
	SKIP("  -a arpanet  (obsolete)")
	KEEP("  -c          set control characters")
	SKIP("  -d dialup   (obsolete)")
	KEEP("  -e ch       erase character")
	KEEP("  -I          no initialization strings")
	KEEP("  -i ch       interrupt character")
	KEEP("  -k ch       kill character")
	KEEP("  -m mapping  map identifier to type")
	SKIP("  -p plugboard (obsolete)")
	KEEP("  -Q          do not output control key settings")
	KEEP("  -q          display term only, do no changes")
	KEEP("  -r          display term on stderr")
	SKIP("  -S          (obsolete)")
	KEEP("  -s          output TERM set command")
	KEEP("  -V          print curses-version")
	KEEP("  -w          set window-size")
	KEEP("")
	KEEP("If neither -c/-w are given, both are assumed.")
    };
#undef KEEP
#undef SKIP
    (void) fprintf(stderr, "Usage: %s [options] [terminal]\n", _nc_progname);
    fputs(msg, stderr);
    ExitProgram(EXIT_FAILURE);
     
}

static char
arg_to_char(void)
{
    return (char) ((optarg[0] == '^' && optarg[1] != '\0')
		   ? ((optarg[1] == '?') ? '\177' : CTRL(optarg[1]))
		   : optarg[0]);
}

int
main(int argc, char **argv)
{
    int ch, noinit, noset, quiet, Sflag, sflag, showterm;
    const char *ttype;
    int terasechar = -1;	 
    int intrchar = -1;		 
    int tkillchar = -1;		 
    int my_fd;
    bool opt_c = FALSE;		 
    bool opt_w = FALSE;		 
    TTY mode, oldmode;

    _nc_progname = _nc_rootname(*argv);
    obsolete(argv);
    noinit = noset = quiet = Sflag = sflag = showterm = 0;
    while ((ch = getopt(argc, argv, "a:cd:e:Ii:k:m:p:qQrSsVw")) != -1) {
	switch (ch) {
	case 'c':		 
	    opt_c = TRUE;
	    break;
	case 'a':		 
	    add_mapping("arpanet", optarg);
	    break;
	case 'd':		 
	    add_mapping("dialup", optarg);
	    break;
	case 'e':		 
	    terasechar = arg_to_char();
	    break;
	case 'I':		 
	    noinit = 1;
	    break;
	case 'i':		 
	    intrchar = arg_to_char();
	    break;
	case 'k':		 
	    tkillchar = arg_to_char();
	    break;
	case 'm':		 
	    add_mapping(0, optarg);
	    break;
	case 'p':		 
	    add_mapping("plugboard", optarg);
	    break;
	case 'Q':		 
	    quiet = 1;
	    break;
	case 'q':		 
	    noset = 1;
	    break;
	case 'r':		 
	    showterm = 1;
	    break;
	case 'S':		 
	    Sflag = 1;
	    break;
	case 's':		 
	    sflag = 1;
	    break;
	case 'V':		 
	    puts(curses_version());
	    ExitProgram(EXIT_SUCCESS);
	case 'w':		 
	    opt_w = TRUE;
	    break;
	case '?':
	default:
	    usage();
	}
    }

    argc -= optind;
    argv += optind;

    if (argc > 1)
	usage();

    if (!opt_c && !opt_w)
	opt_c = opt_w = TRUE;

    my_fd = save_tty_settings(&mode, TRUE);
    oldmode = mode;
#ifdef TERMIOS
    ospeed = (NCURSES_OSPEED) cfgetospeed(&mode);
#elif defined(EXP_WIN32_DRIVER)
    ospeed = 0;
#else
    ospeed = (NCURSES_OSPEED) mode.sg_ospeed;
#endif

    if (same_program(_nc_progname, PROG_RESET)) {
	reset_start(stderr, TRUE, FALSE);
	reset_tty_settings(my_fd, &mode, noset);
    } else {
	reset_start(stderr, FALSE, TRUE);
    }

    ttype = get_termcap_entry(my_fd, *argv);

    if (!noset) {
#if HAVE_SIZECHANGE
	if (opt_w) {
	    set_window_size(my_fd, &lines, &columns);
	}
#endif
	if (opt_c) {
	    set_control_chars(&mode, terasechar, intrchar, tkillchar);
	    set_conversions(&mode);

	    if (!noinit) {
		if (send_init_strings(my_fd, &oldmode)) {
		    (void) putc('\r', stderr);
		    (void) fflush(stderr);
		    (void) napms(1000);		 
		}
	    }

	    update_tty_settings(&oldmode, &mode);
	}
    }

    if (noset) {
	(void) printf("%s\n", ttype);
    } else {
	if (showterm)
	    (void) fprintf(stderr, "Terminal type is %s.\n", ttype);
	 
	if (!quiet) {
	    print_tty_chars(&oldmode, &mode);
	}
    }

    if (Sflag)
	err("The -S option is not supported under terminfo.");

    if (sflag) {
	print_shell_commands(ttype);
    }

    ExitProgram(EXIT_SUCCESS);
}
