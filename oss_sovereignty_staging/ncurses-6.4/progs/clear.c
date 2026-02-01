 

 

 

#define USE_LIBTINFO
#include <clear_cmd.h>
#include <tty_settings.h>

MODULE_ID("$Id: clear.c,v 1.24 2021/03/20 18:23:14 tom Exp $")

const char *_nc_progname = "clear";

static GCC_NORETURN void
usage(void)
{
#define KEEP(s) s "\n"
    static const char msg[] =
    {
	KEEP("")
	KEEP("Options:")
	KEEP("  -T TERM     use this instead of $TERM")
	KEEP("  -V          print curses-version")
	KEEP("  -x          do not try to clear scrollback")
    };
#undef KEEP
    (void) fprintf(stderr, "Usage: %s [options]\n", _nc_progname);
    fputs(msg, stderr);
    ExitProgram(EXIT_FAILURE);
}

int
main(
	int argc GCC_UNUSED,
	char *argv[]GCC_UNUSED)
{
    TTY tty_settings;
    int fd;
    int c;
    char *term;
    bool opt_x = FALSE;		 

    _nc_progname = _nc_rootname(argv[0]);
    term = getenv("TERM");

    while ((c = getopt(argc, argv, "T:Vx")) != -1) {
	switch (c) {
	case 'T':
	    use_env(FALSE);
	    use_tioctl(TRUE);
	    term = optarg;
	    break;
	case 'V':
	    puts(curses_version());
	    ExitProgram(EXIT_SUCCESS);
	case 'x':		 
	    opt_x = TRUE;
	    break;
	default:
	    usage();
	     
	}
    }
    if (optind < argc)
	usage();

    fd = save_tty_settings(&tty_settings, FALSE);

    setupterm(term, fd, (int *) 0);

    ExitProgram((clear_cmd(opt_x) == ERR)
		? EXIT_FAILURE
		: EXIT_SUCCESS);
}
