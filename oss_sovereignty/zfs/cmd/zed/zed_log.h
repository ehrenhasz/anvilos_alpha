#ifndef	ZED_LOG_H
#define	ZED_LOG_H
#include <syslog.h>
void zed_log_init(const char *identity);
void zed_log_fini(void);
void zed_log_pipe_open(void);
void zed_log_pipe_close_reads(void);
void zed_log_pipe_close_writes(void);
void zed_log_pipe_wait(void);
void zed_log_stderr_open(int priority);
void zed_log_stderr_close(void);
void zed_log_syslog_open(int facility);
void zed_log_syslog_close(void);
void zed_log_msg(int priority, const char *fmt, ...);
__attribute__((format(printf, 1, 2), __noreturn__))
void zed_log_die(const char *fmt, ...);
#endif	 
