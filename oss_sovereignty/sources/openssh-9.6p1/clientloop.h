




#include <termios.h>

struct ssh;


int	 client_loop(struct ssh *, int, int, int);
int	 client_x11_get_proto(struct ssh *, const char *, const char *,
	    u_int, u_int, char **, char **);
void	 client_global_request_reply_fwd(int, u_int32_t, void *);
void	 client_session2_setup(struct ssh *, int, int, int,
	    const char *, struct termios *, int, struct sshbuf *, char **);
char	 *client_request_tun_fwd(struct ssh *, int, int, int,
    channel_open_fn *, void *);
void	 client_stop_mux(void);


void	*client_new_escape_filter_ctx(int);
void	 client_filter_cleanup(struct ssh *, int, void *);
int	 client_simple_escape_filter(struct ssh *, Channel *, char *, int);


typedef void global_confirm_cb(struct ssh *, int, u_int32_t, void *);
void	 client_register_global_confirm(global_confirm_cb *, void *);


enum confirm_action { CONFIRM_WARN = 0, CONFIRM_CLOSE, CONFIRM_TTY };
void client_expect_confirm(struct ssh *, int, const char *,
    enum confirm_action);


#define SSHMUX_VER			4


#define SSHMUX_COMMAND_OPEN		1	
#define SSHMUX_COMMAND_ALIVE_CHECK	2	
#define SSHMUX_COMMAND_TERMINATE	3	
#define SSHMUX_COMMAND_STDIO_FWD	4	
#define SSHMUX_COMMAND_FORWARD		5	
#define SSHMUX_COMMAND_STOP		6	
#define SSHMUX_COMMAND_CANCEL_FWD	7	
#define SSHMUX_COMMAND_PROXY		8	

void	muxserver_listen(struct ssh *);
int	muxclient(const char *);
void	mux_exit_message(struct ssh *, Channel *, int);
void	mux_tty_alloc_failed(struct ssh *ssh, Channel *);

