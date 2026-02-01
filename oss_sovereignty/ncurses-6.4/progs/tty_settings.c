 

 

#define USE_LIBTINFO
#include <tty_settings.h>

#include <fcntl.h>

MODULE_ID("$Id: tty_settings.c,v 1.7 2021/10/08 23:53:32 tom Exp $")

static int my_fd;
static TTY original_settings;
static bool can_restore = FALSE;

static void
failed(const char *msg)
{
    int code = errno;

    (void) fprintf(stderr, "%s: %s: %s\n", _nc_progname, msg, strerror(code));
    restore_tty_settings();
    (void) fprintf(stderr, "\n");
    ExitProgram(ErrSystem(code));
     
}

static bool
get_tty_settings(int fd, TTY * tty_settings)
{
    bool success = TRUE;
    my_fd = fd;
    if (fd < 0 || GET_TTY(my_fd, tty_settings) < 0) {
	success = FALSE;
    }
    return success;
}

 
int
save_tty_settings(TTY * tty_settings, bool need_tty)
{
    if (!get_tty_settings(STDERR_FILENO, tty_settings) &&
	!get_tty_settings(STDOUT_FILENO, tty_settings) &&
	!get_tty_settings(STDIN_FILENO, tty_settings)) {
	if (need_tty) {
	    int fd = open("/dev/tty", O_RDWR);
	    if (!get_tty_settings(fd, tty_settings)) {
		failed("terminal attributes");
	    }
	} else {
	    my_fd = fileno(stdout);
	}
    } else {
	can_restore = TRUE;
	original_settings = *tty_settings;
    }
    return my_fd;
}

void
restore_tty_settings(void)
{
    if (can_restore)
	SET_TTY(my_fd, &original_settings);
}

 
void
update_tty_settings(TTY * old_settings, TTY * new_settings)
{
    if (memcmp(new_settings, old_settings, sizeof(TTY))) {
	SET_TTY(my_fd, new_settings);
    }
}
