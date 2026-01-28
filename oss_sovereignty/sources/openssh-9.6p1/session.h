


#ifndef SESSION_H
#define SESSION_H

#define TTYSZ 64
typedef struct Session Session;
struct Session {
	int	used;
	int	self;
	int	next_unused;
	struct passwd *pw;
	Authctxt *authctxt;
	pid_t	pid;
	int	forced;

	
	char	*term;
	int	ptyfd, ttyfd, ptymaster;
	u_int	row, col, xpixel, ypixel;
	char	tty[TTYSZ];

	
	u_int	display_number;
	char	*display;
	u_int	screen;
	char	*auth_display;
	char	*auth_proto;
	char	*auth_data;
	int	single_connection;

	int	chanid;
	int	*x11_chanids;
	int	is_subsystem;
	char	*subsys;
	u_int	num_env;
	struct {
		char	*name;
		char	*val;
	} *env;
};

void	 do_authenticated(struct ssh *, Authctxt *);
void	 do_cleanup(struct ssh *, Authctxt *);

int	 session_open(Authctxt *, int);
void	 session_unused(int);
int	 session_input_channel_req(struct ssh *, Channel *, const char *);
void	 session_close_by_pid(struct ssh *ssh, pid_t, int);
void	 session_close_by_channel(struct ssh *, int, int, void *);
void	 session_destroy_all(struct ssh *, void (*)(Session *));
void	 session_pty_cleanup2(Session *);

Session	*session_new(void);
Session	*session_by_tty(char *);
void	 session_close(struct ssh *, Session *);
void	 do_setusercontext(struct passwd *);

const char	*session_get_remote_name_or_ip(struct ssh *, u_int, int);

#endif
