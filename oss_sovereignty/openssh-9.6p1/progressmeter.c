 
 

#include "includes.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "progressmeter.h"
#include "atomicio.h"
#include "misc.h"
#include "utf8.h"

#define DEFAULT_WINSIZE 80
#define MAX_WINSIZE 512
#define PADDING 1		 
#define UPDATE_INTERVAL 1	 
#define STALL_TIME 5		 

 
static int can_output(void);

 
static void sig_winch(int);
static void setscreensize(void);

 
static void sig_alarm(int);

static double start;		 
static double last_update;	 
static const char *file;	 
static off_t start_pos;		 
static off_t end_pos;		 
static off_t cur_pos;		 
static volatile off_t *counter;	 
static long stalled;		 
static int bytes_per_second;	 
static int win_size;		 
static volatile sig_atomic_t win_resized;  
static volatile sig_atomic_t alarm_fired;

 
static const char unit[] = " KMGT";

static int
can_output(void)
{
	return (getpgrp() == tcgetpgrp(STDOUT_FILENO));
}

 
#define STRING_SIZE(v) (((sizeof(v) * 8 * 4) / 10) + 1)

static const char *
format_rate(off_t bytes)
{
	int i;
	static char buf[STRING_SIZE(bytes) * 2 + 16];

	bytes *= 100;
	for (i = 0; bytes >= 100*1000 && unit[i] != 'T'; i++)
		bytes = (bytes + 512) / 1024;
	if (i == 0) {
		i++;
		bytes = (bytes + 512) / 1024;
	}
	snprintf(buf, sizeof(buf), "%3lld.%1lld%c%s",
	    (long long) (bytes + 5) / 100,
	    (long long) (bytes + 5) / 10 % 10,
	    unit[i],
	    i ? "B" : " ");
	return buf;
}

static const char *
format_size(off_t bytes)
{
	int i;
	static char buf[STRING_SIZE(bytes) + 16];

	for (i = 0; bytes >= 10000 && unit[i] != 'T'; i++)
		bytes = (bytes + 512) / 1024;
	snprintf(buf, sizeof(buf), "%4lld%c%s",
	    (long long) bytes,
	    unit[i],
	    i ? "B" : " ");
	return buf;
}

void
refresh_progress_meter(int force_update)
{
	char *buf = NULL, *obuf = NULL;
	off_t transferred;
	double elapsed, now;
	int percent;
	off_t bytes_left;
	int cur_speed;
	int hours, minutes, seconds;
	int file_len, cols;

	if ((!force_update && !alarm_fired && !win_resized) || !can_output())
		return;
	alarm_fired = 0;

	if (win_resized) {
		setscreensize();
		win_resized = 0;
	}

	transferred = *counter - (cur_pos ? cur_pos : start_pos);
	cur_pos = *counter;
	now = monotime_double();
	bytes_left = end_pos - cur_pos;

	if (bytes_left > 0)
		elapsed = now - last_update;
	else {
		elapsed = now - start;
		 
		transferred = end_pos - start_pos;
		bytes_per_second = 0;
	}

	 
	if (elapsed != 0)
		cur_speed = (transferred / elapsed);
	else
		cur_speed = transferred;

#define AGE_FACTOR 0.9
	if (bytes_per_second != 0) {
		bytes_per_second = (bytes_per_second * AGE_FACTOR) +
		    (cur_speed * (1.0 - AGE_FACTOR));
	} else
		bytes_per_second = cur_speed;

	last_update = now;

	 
	if (win_size < 4)
		return;

	 
	file_len = cols = win_size - 36;
	if (file_len > 0) {
		asmprintf(&buf, INT_MAX, &cols, "%-*s", file_len, file);
		 
		if (cols < file_len)
			xextendf(&buf, NULL, "%*s", file_len - cols, "");
	}
	 
	if (end_pos == 0 || cur_pos == end_pos)
		percent = 100;
	else
		percent = ((float)cur_pos / end_pos) * 100;

	 
	xextendf(&buf, NULL, " %3d%% %s %s/s ", percent, format_size(cur_pos),
	    format_rate((off_t)bytes_per_second));

	 
	if (!transferred)
		stalled += elapsed;
	else
		stalled = 0;

	if (stalled >= STALL_TIME)
		xextendf(&buf, NULL, "- stalled -");
	else if (bytes_per_second == 0 && bytes_left)
		xextendf(&buf, NULL, "  --:-- ETA");
	else {
		if (bytes_left > 0)
			seconds = bytes_left / bytes_per_second;
		else
			seconds = elapsed;

		hours = seconds / 3600;
		seconds -= hours * 3600;
		minutes = seconds / 60;
		seconds -= minutes * 60;

		if (hours != 0) {
			xextendf(&buf, NULL, "%d:%02d:%02d",
			    hours, minutes, seconds);
		} else
			xextendf(&buf, NULL, "  %02d:%02d", minutes, seconds);

		if (bytes_left > 0)
			xextendf(&buf, NULL, " ETA");
		else
			xextendf(&buf, NULL, "    ");
	}

	 
	cols = win_size - 1;
	asmprintf(&obuf, INT_MAX, &cols, " %s", buf);
	if (obuf != NULL) {
		*obuf = '\r';  
		atomicio(vwrite, STDOUT_FILENO, obuf, strlen(obuf));
	}
	free(buf);
	free(obuf);
}

static void
sig_alarm(int ignore)
{
	alarm_fired = 1;
	alarm(UPDATE_INTERVAL);
}

void
start_progress_meter(const char *f, off_t filesize, off_t *ctr)
{
	start = last_update = monotime_double();
	file = f;
	start_pos = *ctr;
	end_pos = filesize;
	cur_pos = 0;
	counter = ctr;
	stalled = 0;
	bytes_per_second = 0;

	setscreensize();
	refresh_progress_meter(1);

	ssh_signal(SIGALRM, sig_alarm);
	ssh_signal(SIGWINCH, sig_winch);
	alarm(UPDATE_INTERVAL);
}

void
stop_progress_meter(void)
{
	alarm(0);

	if (!can_output())
		return;

	 
	if (cur_pos != end_pos)
		refresh_progress_meter(1);

	atomicio(vwrite, STDOUT_FILENO, "\n", 1);
}

static void
sig_winch(int sig)
{
	win_resized = 1;
}

static void
setscreensize(void)
{
	struct winsize winsize;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsize) != -1 &&
	    winsize.ws_col != 0) {
		if (winsize.ws_col > MAX_WINSIZE)
			win_size = MAX_WINSIZE;
		else
			win_size = winsize.ws_col;
	} else
		win_size = DEFAULT_WINSIZE;
	win_size += 1;					 
}
