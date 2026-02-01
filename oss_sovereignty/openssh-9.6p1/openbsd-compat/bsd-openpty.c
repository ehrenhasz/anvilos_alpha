 

 

 

#include "includes.h"
#if !defined(HAVE_OPENPTY)

#include <sys/types.h>

#include <stdlib.h>

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#ifdef HAVE_UTIL_H
# include <util.h>
#endif  

#ifdef HAVE_PTY_H
# include <pty.h>
#endif
#if defined(HAVE_DEV_PTMX) && defined(HAVE_SYS_STROPTS_H)
# include <sys/stropts.h>
#endif

#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "misc.h"
#include "log.h"

#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif

#if defined(HAVE_DEV_PTMX) && !defined(HAVE__GETPTY)
static int
openpty_streams(int *amaster, int *aslave)
{
	 
	int ptm;
	char *pts;
	sshsig_t old_signal;

	if ((ptm = open("/dev/ptmx", O_RDWR | O_NOCTTY)) == -1)
		return (-1);

	 
	old_signal = ssh_signal(SIGCHLD, SIG_DFL);
	if (grantpt(ptm) < 0)
		return (-1);
	ssh_signal(SIGCHLD, old_signal);

	if (unlockpt(ptm) < 0)
		return (-1);

	if ((pts = ptsname(ptm)) == NULL)
		return (-1);
	*amaster = ptm;

	 
	if ((*aslave = open(pts, O_RDWR | O_NOCTTY)) == -1) {
		close(*amaster);
		return (-1);
	}

# if defined(I_FIND) && defined(__SVR4)
	 
	if (ioctl(*aslave, I_FIND, "ptem") != 0)
		return 0;
# endif

	 
	ioctl(*aslave, I_PUSH, "ptem");
	ioctl(*aslave, I_PUSH, "ldterm");
# ifndef __hpux
	ioctl(*aslave, I_PUSH, "ttcompat");
# endif  
	return (0);
}
#endif

int
openpty(int *amaster, int *aslave, char *name, struct termios *termp,
   struct winsize *winp)
{
#if defined(HAVE__GETPTY)
	 
	char *slave;

	if ((slave = _getpty(amaster, O_RDWR, 0622, 0)) == NULL)
		return (-1);

	 
	if ((*aslave = open(slave, O_RDWR | O_NOCTTY)) == -1) {
		close(*amaster);
		return (-1);
	}
	return (0);

#elif defined(HAVE_DEV_PTMX)

#ifdef SSHD_ACQUIRES_CTTY
	 
	int r, fd;
	static int junk_ptyfd = -1, junk_ttyfd;

	r = openpty_streams(amaster, aslave);
	if (junk_ptyfd == -1 && (fd = open(_PATH_TTY, O_RDWR|O_NOCTTY)) >= 0) {
		close(fd);
		junk_ptyfd = *amaster;
		junk_ttyfd = *aslave;
		debug("STREAMS bug workaround pty %d tty %d name %s",
		    junk_ptyfd, junk_ttyfd, ttyname(junk_ttyfd));
        } else
		return r;
#endif

	return openpty_streams(amaster, aslave);

#elif defined(HAVE_DEV_PTS_AND_PTC)
	 
	const char *ttname;

	if ((*amaster = open("/dev/ptc", O_RDWR | O_NOCTTY)) == -1)
		return (-1);
	if ((ttname = ttyname(*amaster)) == NULL)
		return (-1);
	if ((*aslave = open(ttname, O_RDWR | O_NOCTTY)) == -1) {
		close(*amaster);
		return (-1);
	}
	return (0);

#else
	 
	char ptbuf[64], ttbuf[64];
	int i;
	const char *ptymajors = "pqrstuvwxyzabcdefghijklmno"
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	const char *ptyminors = "0123456789abcdef";
	int num_minors = strlen(ptyminors);
	int num_ptys = strlen(ptymajors) * num_minors;
	struct termios tio;

	for (i = 0; i < num_ptys; i++) {
		snprintf(ptbuf, sizeof(ptbuf), "/dev/pty%c%c",
		    ptymajors[i / num_minors], ptyminors[i % num_minors]);
		snprintf(ttbuf, sizeof(ttbuf), "/dev/tty%c%c",
		    ptymajors[i / num_minors], ptyminors[i % num_minors]);

		if ((*amaster = open(ptbuf, O_RDWR | O_NOCTTY)) == -1) {
			 
			snprintf(ptbuf, sizeof(ptbuf), "/dev/ptyp%d", i);
			snprintf(ttbuf, sizeof(ttbuf), "/dev/ttyp%d", i);
			if ((*amaster = open(ptbuf, O_RDWR | O_NOCTTY)) == -1)
				continue;
		}

		 
		if ((*aslave = open(ttbuf, O_RDWR | O_NOCTTY)) == -1) {
			close(*amaster);
			return (-1);
		}
		 
		if (tcgetattr(*amaster, &tio) != -1) {
			tio.c_lflag |= (ECHO | ISIG | ICANON);
			tio.c_oflag |= (OPOST | ONLCR);
			tio.c_iflag |= ICRNL;
			tcsetattr(*amaster, TCSANOW, &tio);
		}

		return (0);
	}
	return (-1);
#endif
}

#endif  

