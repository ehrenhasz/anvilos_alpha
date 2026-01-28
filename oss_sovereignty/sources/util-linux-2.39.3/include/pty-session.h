
#ifndef UTIL_LINUX_PTY_SESSION_H
#define UTIL_LINUX_PTY_SESSION_H

#include <pty.h>
#include <termios.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <sys/signalfd.h>


struct ul_pty_callbacks {
	
	void (*child_wait)(void *, pid_t);

	
	void (*child_die)(void *, pid_t, int);

	
	void (*child_sigstop)(void *, pid_t);

	
	int (*mainloop)(void *);

	
	int (*log_stream_activity)(void *, int, char *, size_t);

	
	int (*log_signal)(void *, struct signalfd_siginfo *, void *);

	
	int (*flush_logs)(void *);
};

struct ul_pty {
	struct termios	stdin_attrs;	
	int		master;		
	int		slave;		
	int		sigfd;		
	int		poll_timeout;
	struct winsize	win;		
	sigset_t	orgsig;		

	int		delivered_signal;

	struct ul_pty_callbacks	callbacks;
	void			*callback_data;

	pid_t		child;

	struct timeval	next_callback_time;

	struct ul_pty_child_buffer {
		struct ul_pty_child_buffer *next;
		char buf[BUFSIZ];
		size_t size, cursor;
		unsigned int final_input:1;	
	} *child_buffer_head, *child_buffer_tail, *free_buffers;

	unsigned int isterm:1,		
		     slave_echo:1;	
};

void ul_pty_init_debug(int mask);
struct ul_pty *ul_new_pty(int is_stdin_tty);
void ul_free_pty(struct ul_pty *pty);

void ul_pty_slave_echo(struct ul_pty *pty, int enable);
int ul_pty_get_delivered_signal(struct ul_pty *pty);

void ul_pty_set_callback_data(struct ul_pty *pty, void *data);
void ul_pty_set_child(struct ul_pty *pty, pid_t child);

struct ul_pty_callbacks *ul_pty_get_callbacks(struct ul_pty *pty);
int ul_pty_is_running(struct ul_pty *pty);
int ul_pty_setup(struct ul_pty *pty);
int ul_pty_signals_setup(struct ul_pty *pty);
void ul_pty_cleanup(struct ul_pty *pty);
int ul_pty_chownmod_slave(struct ul_pty *pty, uid_t uid, gid_t gid, mode_t mode);
void ul_pty_init_slave(struct ul_pty *pty);
int ul_pty_proxy_master(struct ul_pty *pty);

void ul_pty_set_mainloop_time(struct ul_pty *pty, struct timeval *tv);
int ul_pty_get_childfd(struct ul_pty *pty);
void ul_pty_wait_for_child(struct ul_pty *pty);
pid_t ul_pty_get_child(struct ul_pty *pty);
void ul_pty_write_eof_to_child(struct ul_pty *pty);

#endif 
