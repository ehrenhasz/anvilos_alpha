#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>
#include "zed_log.h"
#define	ZED_LOG_MAX_LOG_LEN	1024
static struct {
	unsigned do_stderr:1;
	unsigned do_syslog:1;
	const char *identity;
	int priority;
	int pipe_fd[2];
} _ctx;
void
zed_log_init(const char *identity)
{
	if (identity) {
		const char *p = strrchr(identity, '/');
		_ctx.identity = (p != NULL) ? p + 1 : identity;
	} else {
		_ctx.identity = NULL;
	}
	_ctx.pipe_fd[0] = -1;
	_ctx.pipe_fd[1] = -1;
}
void
zed_log_fini(void)
{
	zed_log_stderr_close();
	zed_log_syslog_close();
}
void
zed_log_pipe_open(void)
{
	if ((_ctx.pipe_fd[0] != -1) || (_ctx.pipe_fd[1] != -1))
		zed_log_die("Invalid use of zed_log_pipe_open in PID %d",
		    (int)getpid());
	if (pipe(_ctx.pipe_fd) < 0)
		zed_log_die("Failed to create daemonize pipe in PID %d: %s",
		    (int)getpid(), strerror(errno));
}
void
zed_log_pipe_close_reads(void)
{
	if (_ctx.pipe_fd[0] < 0)
		zed_log_die(
		    "Invalid use of zed_log_pipe_close_reads in PID %d",
		    (int)getpid());
	if (close(_ctx.pipe_fd[0]) < 0)
		zed_log_die(
		    "Failed to close reads on daemonize pipe in PID %d: %s",
		    (int)getpid(), strerror(errno));
	_ctx.pipe_fd[0] = -1;
}
void
zed_log_pipe_close_writes(void)
{
	if (_ctx.pipe_fd[1] < 0)
		zed_log_die(
		    "Invalid use of zed_log_pipe_close_writes in PID %d",
		    (int)getpid());
	if (close(_ctx.pipe_fd[1]) < 0)
		zed_log_die(
		    "Failed to close writes on daemonize pipe in PID %d: %s",
		    (int)getpid(), strerror(errno));
	_ctx.pipe_fd[1] = -1;
}
void
zed_log_pipe_wait(void)
{
	ssize_t n;
	char c;
	if (_ctx.pipe_fd[0] < 0)
		zed_log_die("Invalid use of zed_log_pipe_wait in PID %d",
		    (int)getpid());
	for (;;) {
		n = read(_ctx.pipe_fd[0], &c, sizeof (c));
		if (n < 0) {
			if (errno == EINTR)
				continue;
			zed_log_die(
			    "Failed to read from daemonize pipe in PID %d: %s",
			    (int)getpid(), strerror(errno));
		}
		if (n == 0) {
			break;
		}
	}
}
void
zed_log_stderr_open(int priority)
{
	_ctx.do_stderr = 1;
	_ctx.priority = priority;
}
void
zed_log_stderr_close(void)
{
	if (_ctx.do_stderr)
		_ctx.do_stderr = 0;
}
void
zed_log_syslog_open(int facility)
{
	_ctx.do_syslog = 1;
	openlog(_ctx.identity, LOG_NDELAY | LOG_PID, facility);
}
void
zed_log_syslog_close(void)
{
	if (_ctx.do_syslog) {
		_ctx.do_syslog = 0;
		closelog();
	}
}
static void
_zed_log_aux(int priority, const char *fmt, va_list vargs)
{
	char buf[ZED_LOG_MAX_LOG_LEN];
	int n;
	if (!fmt)
		return;
	n = vsnprintf(buf, sizeof (buf), fmt, vargs);
	if ((n < 0) || (n >= sizeof (buf))) {
		buf[sizeof (buf) - 2] = '+';
		buf[sizeof (buf) - 1] = '\0';
	}
	if (_ctx.do_syslog)
		syslog(priority, "%s", buf);
	if (_ctx.do_stderr && (priority <= _ctx.priority))
		fprintf(stderr, "%s\n", buf);
}
void
zed_log_msg(int priority, const char *fmt, ...)
{
	va_list vargs;
	if (fmt) {
		va_start(vargs, fmt);
		_zed_log_aux(priority, fmt, vargs);
		va_end(vargs);
	}
}
void
zed_log_die(const char *fmt, ...)
{
	va_list vargs;
	if (fmt) {
		va_start(vargs, fmt);
		_zed_log_aux(LOG_ERR, fmt, vargs);
		va_end(vargs);
	}
	exit(EXIT_FAILURE);
}
