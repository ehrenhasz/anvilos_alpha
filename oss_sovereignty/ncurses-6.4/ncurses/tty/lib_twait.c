 

 

 

#include <curses.priv.h>

#if defined __HAIKU__ && defined __BEOS__
#undef __BEOS__
#endif

#ifdef __BEOS__
#undef false
#undef true
#include <OS.h>
#endif

#if USE_KLIBC_KBD
#define INCL_KBD
#include <os2.h>
#endif

#if USE_FUNC_POLL
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# endif
#elif HAVE_SELECT
# if HAVE_SYS_TIME_H && HAVE_SYS_TIME_SELECT
#  include <sys/time.h>
# endif
# if HAVE_SYS_SELECT_H
#  include <sys/select.h>
# endif
#endif
#if HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#undef CUR

MODULE_ID("$Id: lib_twait.c,v 1.75 2020/02/29 15:46:00 anonymous.maarten Exp $")

static long
_nc_gettime(TimeType * t0, int first)
{
    long res;

#if PRECISE_GETTIME
    TimeType t1;
    gettimeofday(&t1, (struct timezone *) 0);
    if (first) {
	*t0 = t1;
	res = 0;
    } else {
	 
	if (t0->tv_usec > t1.tv_usec) {
	    t1.tv_usec += 1000000;	 
	    t1.tv_sec--;
	}
	res = (t1.tv_sec - t0->tv_sec) * 1000
	    + (t1.tv_usec - t0->tv_usec) / 1000;
    }
#else
    time_t t1 = time((time_t *) 0);
    if (first) {
	*t0 = t1;
    }
    res = (long) ((t1 - *t0) * 1000);
#endif
    TR(TRACE_IEVENT, ("%s time: %ld msec", first ? "get" : "elapsed", res));
    return res;
}

#ifdef NCURSES_WGETCH_EVENTS
NCURSES_EXPORT(int)
_nc_eventlist_timeout(_nc_eventlist * evl)
{
    int event_delay = -1;

    if (evl != 0) {
	int n;

	for (n = 0; n < evl->count; ++n) {
	    _nc_event *ev = evl->events[n];

	    if (ev->type == _NC_EVENT_TIMEOUT_MSEC) {
		event_delay = (int) ev->data.timeout_msec;
		if (event_delay < 0)
		    event_delay = INT_MAX;	 
	    }
	}
    }
    return event_delay;
}
#endif  

#if (USE_FUNC_POLL || HAVE_SELECT)
#  define MAYBE_UNUSED
#else
#  define MAYBE_UNUSED GCC_UNUSED
#endif

#if (USE_FUNC_POLL || HAVE_SELECT)
#  define MAYBE_UNUSED
#else
#  define MAYBE_UNUSED GCC_UNUSED
#endif

 
NCURSES_EXPORT(int)
_nc_timed_wait(SCREEN *sp MAYBE_UNUSED,
	       int mode MAYBE_UNUSED,
	       int milliseconds,
	       int *timeleft
	       EVENTLIST_2nd(_nc_eventlist * evl))
{
    int count;
    int result = TW_NONE;
    TimeType t0;
#if (USE_FUNC_POLL || HAVE_SELECT)
    int fd;
#endif

#ifdef NCURSES_WGETCH_EVENTS
    int timeout_is_event = 0;
    int n;
#endif

#if USE_FUNC_POLL
#define MIN_FDS 2
    struct pollfd fd_list[MIN_FDS];
    struct pollfd *fds = fd_list;
#elif defined(__BEOS__)
#elif HAVE_SELECT
    fd_set set;
#endif

#if USE_KLIBC_KBD
    fd_set saved_set;
    KBDKEYINFO ki;
    struct timeval tv;
#endif

    long starttime, returntime;

#ifdef NCURSES_WGETCH_EVENTS
    (void) timeout_is_event;
#endif

    TR(TRACE_IEVENT, ("start twait: %d milliseconds, mode: %d",
		      milliseconds, mode));

#ifdef NCURSES_WGETCH_EVENTS
    if (mode & TW_EVENT) {
	int event_delay = _nc_eventlist_timeout(evl);

	if (event_delay >= 0
	    && (milliseconds >= event_delay || milliseconds < 0)) {
	    milliseconds = event_delay;
	    timeout_is_event = 1;
	}
    }
#endif

#if PRECISE_GETTIME && HAVE_NANOSLEEP
  retry:
#endif
    starttime = _nc_gettime(&t0, TRUE);

    count = 0;
    (void) count;

#ifdef NCURSES_WGETCH_EVENTS
    if ((mode & TW_EVENT) && evl)
	evl->result_flags = 0;
#endif

#if USE_FUNC_POLL
    memset(fd_list, 0, sizeof(fd_list));

#ifdef NCURSES_WGETCH_EVENTS
    if ((mode & TW_EVENT) && evl) {
	if (fds == fd_list)
	    fds = typeMalloc(struct pollfd, MIN_FDS + evl->count);
	if (fds == 0)
	    return TW_NONE;
    }
#endif

    if (mode & TW_INPUT) {
	fds[count].fd = sp->_ifd;
	fds[count].events = POLLIN;
	count++;
    }
    if ((mode & TW_MOUSE)
	&& (fd = sp->_mouse_fd) >= 0) {
	fds[count].fd = fd;
	fds[count].events = POLLIN;
	count++;
    }
#ifdef NCURSES_WGETCH_EVENTS
    if ((mode & TW_EVENT) && evl) {
	for (n = 0; n < evl->count; ++n) {
	    _nc_event *ev = evl->events[n];

	    if (ev->type == _NC_EVENT_FILE
		&& (ev->data.fev.flags & _NC_EVENT_FILE_READABLE)) {
		fds[count].fd = ev->data.fev.fd;
		fds[count].events = POLLIN;
		count++;
	    }
	}
    }
#endif

    result = poll(fds, (size_t) count, milliseconds);

#ifdef NCURSES_WGETCH_EVENTS
    if ((mode & TW_EVENT) && evl) {
	int c;

	if (!result)
	    count = 0;

	for (n = 0; n < evl->count; ++n) {
	    _nc_event *ev = evl->events[n];

	    if (ev->type == _NC_EVENT_FILE
		&& (ev->data.fev.flags & _NC_EVENT_FILE_READABLE)) {
		ev->data.fev.result = 0;
		for (c = 0; c < count; c++)
		    if (fds[c].fd == ev->data.fev.fd
			&& fds[c].revents & POLLIN) {
			ev->data.fev.result |= _NC_EVENT_FILE_READABLE;
			evl->result_flags |= _NC_EVENT_FILE_READABLE;
		    }
	    } else if (ev->type == _NC_EVENT_TIMEOUT_MSEC
		       && !result && timeout_is_event) {
		evl->result_flags |= _NC_EVENT_TIMEOUT_MSEC;
	    }
	}
    }
#endif

#elif defined(__BEOS__)
     
    result = TW_NONE;
    if (mode & TW_INPUT) {
	int step = (milliseconds < 0) ? 0 : 5000;
	bigtime_t d;
	bigtime_t useconds = milliseconds * 1000;
	int n, howmany;

	if (useconds <= 0)	 
	    useconds = 1;

	for (d = 0; d < useconds; d += step) {
	    n = 0;
	    howmany = ioctl(0, 'ichr', &n);
	    if (howmany >= 0 && n > 0) {
		result = 1;
		break;
	    }
	    if (useconds > 1 && step > 0) {
		snooze(step);
		milliseconds -= (step / 1000);
		if (milliseconds <= 0) {
		    milliseconds = 0;
		    break;
		}
	    }
	}
    } else if (milliseconds > 0) {
	snooze(milliseconds * 1000);
	milliseconds = 0;
    }
#elif HAVE_SELECT
     
    FD_ZERO(&set);

#if !USE_KLIBC_KBD
    if (mode & TW_INPUT) {
	FD_SET(sp->_ifd, &set);
	count = sp->_ifd + 1;
    }
#endif
    if ((mode & TW_MOUSE)
	&& (fd = sp->_mouse_fd) >= 0) {
	FD_SET(fd, &set);
	count = max(fd, count) + 1;
    }
#ifdef NCURSES_WGETCH_EVENTS
    if ((mode & TW_EVENT) && evl) {
	for (n = 0; n < evl->count; ++n) {
	    _nc_event *ev = evl->events[n];

	    if (ev->type == _NC_EVENT_FILE
		&& (ev->data.fev.flags & _NC_EVENT_FILE_READABLE)) {
		FD_SET(ev->data.fev.fd, &set);
		count = max(ev->data.fev.fd + 1, count);
	    }
	}
    }
#endif

#if USE_KLIBC_KBD
    for (saved_set = set;; set = saved_set) {
	if ((mode & TW_INPUT)
	    && (sp->_extended_key
		|| (KbdPeek(&ki, 0) == 0
		    && (ki.fbStatus & KBDTRF_FINAL_CHAR_IN)))) {
	    FD_ZERO(&set);
	    FD_SET(sp->_ifd, &set);
	    result = 1;
	    break;
	}

	tv.tv_sec = 0;
	tv.tv_usec = (milliseconds == 0) ? 0 : (10 * 1000);

	if ((result = select(count, &set, NULL, NULL, &tv)) != 0)
	    break;

	 
	if (milliseconds >= 0 && _nc_gettime(&t0, FALSE) >= milliseconds) {
	    result = 0;
	    break;
	}
    }
#else
    if (milliseconds >= 0) {
	struct timeval ntimeout;
	ntimeout.tv_sec = milliseconds / 1000;
	ntimeout.tv_usec = (milliseconds % 1000) * 1000;
	result = select(count, &set, NULL, NULL, &ntimeout);
    } else {
	result = select(count, &set, NULL, NULL, NULL);
    }
#endif

#ifdef NCURSES_WGETCH_EVENTS
    if ((mode & TW_EVENT) && evl) {
	evl->result_flags = 0;
	for (n = 0; n < evl->count; ++n) {
	    _nc_event *ev = evl->events[n];

	    if (ev->type == _NC_EVENT_FILE
		&& (ev->data.fev.flags & _NC_EVENT_FILE_READABLE)) {
		ev->data.fev.result = 0;
		if (FD_ISSET(ev->data.fev.fd, &set)) {
		    ev->data.fev.result |= _NC_EVENT_FILE_READABLE;
		    evl->result_flags |= _NC_EVENT_FILE_READABLE;
		}
	    } else if (ev->type == _NC_EVENT_TIMEOUT_MSEC
		       && !result && timeout_is_event)
		evl->result_flags |= _NC_EVENT_TIMEOUT_MSEC;
	}
    }
#endif

#endif  

    returntime = _nc_gettime(&t0, FALSE);

    if (milliseconds >= 0)
	milliseconds -= (int) (returntime - starttime);

#ifdef NCURSES_WGETCH_EVENTS
    if (evl) {
	evl->result_flags = 0;
	for (n = 0; n < evl->count; ++n) {
	    _nc_event *ev = evl->events[n];

	    if (ev->type == _NC_EVENT_TIMEOUT_MSEC) {
		long diff = (returntime - starttime);
		if (ev->data.timeout_msec <= diff)
		    ev->data.timeout_msec = 0;
		else
		    ev->data.timeout_msec -= diff;
	    }

	}
    }
#endif

#if PRECISE_GETTIME && HAVE_NANOSLEEP
     
    if (result == 0 && milliseconds > 100) {
	napms(100);		 
	milliseconds -= 100;
	goto retry;
    }
#endif

     
    if (timeleft)
	*timeleft = milliseconds;

    TR(TRACE_IEVENT, ("end twait: returned %d (%d), remaining time %d msec",
		      result, errno, milliseconds));

     
    if (result != 0) {
	if (result > 0) {
	    result = 0;
#if USE_FUNC_POLL
	    for (count = 0; count < MIN_FDS; count++) {
		if ((mode & (1 << count))
		    && (fds[count].revents & POLLIN)) {
		    result |= (1 << count);
		}
	    }
#elif defined(__BEOS__)
	    result = TW_INPUT;	 
#elif HAVE_SELECT
	    if ((mode & TW_MOUSE)
		&& (fd = sp->_mouse_fd) >= 0
		&& FD_ISSET(fd, &set))
		result |= TW_MOUSE;
	    if ((mode & TW_INPUT)
		&& FD_ISSET(sp->_ifd, &set))
		result |= TW_INPUT;
#endif
	} else
	    result = 0;
    }
#ifdef NCURSES_WGETCH_EVENTS
    if ((mode & TW_EVENT) && evl && evl->result_flags)
	result |= TW_EVENT;
#endif

#if USE_FUNC_POLL
#ifdef NCURSES_WGETCH_EVENTS
    if (fds != fd_list)
	free((char *) fds);
#endif
#endif

    return (result);
}
