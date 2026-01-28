




#ifndef CHANNEL_H
#define CHANNEL_H


#define SSH_CHANNEL_X11_LISTENER	1	
#define SSH_CHANNEL_PORT_LISTENER	2	
#define SSH_CHANNEL_OPENING		3	
#define SSH_CHANNEL_OPEN		4	
#define SSH_CHANNEL_CLOSED		5	
#define SSH_CHANNEL_AUTH_SOCKET		6	
#define SSH_CHANNEL_X11_OPEN		7	
#define SSH_CHANNEL_LARVAL		10	
#define SSH_CHANNEL_RPORT_LISTENER	11	
#define SSH_CHANNEL_CONNECTING		12
#define SSH_CHANNEL_DYNAMIC		13
#define SSH_CHANNEL_ZOMBIE		14	
#define SSH_CHANNEL_MUX_LISTENER	15	
#define SSH_CHANNEL_MUX_CLIENT		16	
#define SSH_CHANNEL_ABANDONED		17	
#define SSH_CHANNEL_UNIX_LISTENER	18	
#define SSH_CHANNEL_RUNIX_LISTENER	19	
#define SSH_CHANNEL_MUX_PROXY		20	
#define SSH_CHANNEL_RDYNAMIC_OPEN	21	
#define SSH_CHANNEL_RDYNAMIC_FINISH	22	
#define SSH_CHANNEL_MAX_TYPE		23

#define CHANNEL_CANCEL_PORT_STATIC	-1


#define CHANNEL_NONBLOCK_LEAVE	0 
#define CHANNEL_NONBLOCK_SET	1 
#define CHANNEL_NONBLOCK_STDIO	2 


#define CHANNEL_RESTORE_RFD	0x01
#define CHANNEL_RESTORE_WFD	0x02
#define CHANNEL_RESTORE_EFD	0x04


#define FORWARD_DENY		0
#define FORWARD_REMOTE		(1)
#define FORWARD_LOCAL		(1<<1)
#define FORWARD_ALLOW		(FORWARD_REMOTE|FORWARD_LOCAL)

#define FORWARD_ADM		0x100
#define FORWARD_USER		0x101

struct ssh;
struct Channel;
typedef struct Channel Channel;
struct fwd_perm_list;

typedef void channel_open_fn(struct ssh *, int, int, void *);
typedef void channel_callback_fn(struct ssh *, int, int, void *);
typedef int channel_infilter_fn(struct ssh *, struct Channel *, char *, int);
typedef void channel_filter_cleanup_fn(struct ssh *, int, void *);
typedef u_char *channel_outfilter_fn(struct ssh *, struct Channel *,
    u_char **, size_t *);


typedef void channel_confirm_cb(struct ssh *, int, struct Channel *, void *);
typedef void channel_confirm_abandon_cb(struct ssh *, struct Channel *, void *);
struct channel_confirm {
	TAILQ_ENTRY(channel_confirm) entry;
	channel_confirm_cb *cb;
	channel_confirm_abandon_cb *abandon_cb;
	void *ctx;
};
TAILQ_HEAD(channel_confirms, channel_confirm);


struct channel_connect {
	char *host;
	int port;
	struct addrinfo *ai, *aitop;
};


typedef int mux_callback_fn(struct ssh *, struct Channel *);



struct Channel {
	int     type;		

	int     self;		
	uint32_t remote_id;	
	int	have_remote_id;	

	u_int   istate;		
	u_int   ostate;		
	int     flags;		
	int     rfd;		
	int     wfd;		
	int     efd;		
	int     sock;		
	u_int	io_want;	
	u_int	io_ready;	
	int	pfds[4];	
	int     ctl_chan;	
	int     isatty;		
#ifdef _AIX
	int     wfd_isatty;	
#endif
	int	client_tty;	
	int     force_drain;	
	time_t	notbefore;	
	int     delayed;	
	int	restore_block;	
	int	restore_flags[3];
	struct sshbuf *input;	
	struct sshbuf *output;	
	struct sshbuf *extended;

	char    *path;
		
	int     listening_port;	
	char   *listening_addr;	
	int     host_port;	
	char   *remote_name;	

	u_int	remote_window;
	u_int	remote_maxpacket;
	u_int	local_window;
	u_int	local_window_exceeded;
	u_int	local_window_max;
	u_int	local_consumed;
	u_int	local_maxpacket;
	int     extended_usage;
	int	single_connection;

	char   *ctype;		
	char   *xctype;		

	
	channel_open_fn		*open_confirm;
	void			*open_confirm_ctx;
	channel_callback_fn	*detach_user;
	int			detach_close;
	struct channel_confirms	status_confirms;

	
	channel_infilter_fn	*input_filter;
	channel_outfilter_fn	*output_filter;
	void			*filter_ctx;
	channel_filter_cleanup_fn *filter_cleanup;

	
	int			datagram;

	
	
	struct channel_connect	connect_ctx;

	
	mux_callback_fn		*mux_rcb;
	void			*mux_ctx;
	int			mux_pause;
	int			mux_downstream_id;

	

	
	time_t			lastused;
	
	int			inactive_deadline;
};

#define CHAN_EXTENDED_IGNORE		0
#define CHAN_EXTENDED_READ		1
#define CHAN_EXTENDED_WRITE		2


#define CHAN_SES_PACKET_DEFAULT	(32*1024)
#define CHAN_SES_WINDOW_DEFAULT	(64*CHAN_SES_PACKET_DEFAULT)
#define CHAN_TCP_PACKET_DEFAULT	(32*1024)
#define CHAN_TCP_WINDOW_DEFAULT	(64*CHAN_TCP_PACKET_DEFAULT)
#define CHAN_X11_PACKET_DEFAULT	(16*1024)
#define CHAN_X11_WINDOW_DEFAULT	(4*CHAN_X11_PACKET_DEFAULT)


#define CHAN_INPUT_OPEN			0
#define CHAN_INPUT_WAIT_DRAIN		1
#define CHAN_INPUT_WAIT_OCLOSE		2
#define CHAN_INPUT_CLOSED		3


#define CHAN_OUTPUT_OPEN		0
#define CHAN_OUTPUT_WAIT_DRAIN		1
#define CHAN_OUTPUT_WAIT_IEOF		2
#define CHAN_OUTPUT_CLOSED		3

#define CHAN_CLOSE_SENT			0x01
#define CHAN_CLOSE_RCVD			0x02
#define CHAN_EOF_SENT			0x04
#define CHAN_EOF_RCVD			0x08
#define CHAN_LOCAL			0x10


#define SSH_CHAN_IO_RFD			0x01
#define SSH_CHAN_IO_WFD			0x02
#define SSH_CHAN_IO_EFD_R		0x04
#define SSH_CHAN_IO_EFD_W		0x08
#define SSH_CHAN_IO_EFD			(SSH_CHAN_IO_EFD_R|SSH_CHAN_IO_EFD_W)
#define SSH_CHAN_IO_SOCK_R		0x10
#define SSH_CHAN_IO_SOCK_W		0x20
#define SSH_CHAN_IO_SOCK		(SSH_CHAN_IO_SOCK_R|SSH_CHAN_IO_SOCK_W)


#define CHAN_RBUF	(16*1024)


#define CHANNEL_MAX_READ	CHAN_SES_PACKET_DEFAULT


#define CHAN_INPUT_MAX	(16*1024*1024)


#define CHANNELS_MAX_CHANNELS	(16*1024)


#define CHANNEL_EFD_INPUT_ACTIVE(c) \
	(c->extended_usage == CHAN_EXTENDED_READ && \
	(c->efd != -1 || \
	sshbuf_len(c->extended) > 0))
#define CHANNEL_EFD_OUTPUT_ACTIVE(c) \
	(c->extended_usage == CHAN_EXTENDED_WRITE && \
	c->efd != -1 && (!(c->flags & (CHAN_EOF_RCVD|CHAN_CLOSE_RCVD)) || \
	sshbuf_len(c->extended) > 0))


void channel_init_channels(struct ssh *ssh);



Channel	*channel_by_id(struct ssh *, int);
Channel	*channel_by_remote_id(struct ssh *, u_int);
Channel	*channel_lookup(struct ssh *, int);
Channel *channel_new(struct ssh *, char *, int, int, int, int,
	    u_int, u_int, int, const char *, int);
void	 channel_set_fds(struct ssh *, int, int, int, int, int,
	    int, int, u_int);
void	 channel_free(struct ssh *, Channel *);
void	 channel_free_all(struct ssh *);
void	 channel_stop_listening(struct ssh *);
void	 channel_force_close(struct ssh *, Channel *, int);
void	 channel_set_xtype(struct ssh *, int, const char *);

void	 channel_send_open(struct ssh *, int);
void	 channel_request_start(struct ssh *, int, char *, int);
void	 channel_register_cleanup(struct ssh *, int,
	    channel_callback_fn *, int);
void	 channel_register_open_confirm(struct ssh *, int,
	    channel_open_fn *, void *);
void	 channel_register_filter(struct ssh *, int, channel_infilter_fn *,
	    channel_outfilter_fn *, channel_filter_cleanup_fn *, void *);
void	 channel_register_status_confirm(struct ssh *, int,
	    channel_confirm_cb *, channel_confirm_abandon_cb *, void *);
void	 channel_cancel_cleanup(struct ssh *, int);
int	 channel_close_fd(struct ssh *, Channel *, int *);
void	 channel_send_window_changes(struct ssh *);


void channel_add_timeout(struct ssh *, const char *, int);
void channel_clear_timeouts(struct ssh *);



int	 channel_proxy_downstream(struct ssh *, Channel *mc);
int	 channel_proxy_upstream(Channel *, int, u_int32_t, struct ssh *);



int	 channel_input_data(int, u_int32_t, struct ssh *);
int	 channel_input_extended_data(int, u_int32_t, struct ssh *);
int	 channel_input_ieof(int, u_int32_t, struct ssh *);
int	 channel_input_oclose(int, u_int32_t, struct ssh *);
int	 channel_input_open_confirmation(int, u_int32_t, struct ssh *);
int	 channel_input_open_failure(int, u_int32_t, struct ssh *);
int	 channel_input_port_open(int, u_int32_t, struct ssh *);
int	 channel_input_window_adjust(int, u_int32_t, struct ssh *);
int	 channel_input_status_confirm(int, u_int32_t, struct ssh *);


struct pollfd;
struct timespec;

void	 channel_prepare_poll(struct ssh *, struct pollfd **,
	    u_int *, u_int *, u_int, struct timespec *);
void	 channel_after_poll(struct ssh *, struct pollfd *, u_int);
int	 channel_output_poll(struct ssh *);

int      channel_not_very_much_buffered_data(struct ssh *);
void     channel_close_all(struct ssh *);
int      channel_still_open(struct ssh *);
int	 channel_tty_open(struct ssh *);
const char *channel_format_extended_usage(const Channel *);
char	*channel_open_message(struct ssh *);
int	 channel_find_open(struct ssh *);


struct Forward;
struct ForwardOptions;
void	 channel_set_af(struct ssh *, int af);
void     channel_permit_all(struct ssh *, int);
void	 channel_add_permission(struct ssh *, int, int, char *, int);
void	 channel_clear_permission(struct ssh *, int, int);
void	 channel_disable_admin(struct ssh *, int);
void	 channel_update_permission(struct ssh *, int, int);
Channel	*channel_connect_to_port(struct ssh *, const char *, u_short,
	    char *, char *, int *, const char **);
Channel *channel_connect_to_path(struct ssh *, const char *, char *, char *);
Channel	*channel_connect_stdio_fwd(struct ssh *, const char*,
	    int, int, int, int);
Channel	*channel_connect_by_listen_address(struct ssh *, const char *,
	    u_short, char *, char *);
Channel	*channel_connect_by_listen_path(struct ssh *, const char *,
	    char *, char *);
int	 channel_request_remote_forwarding(struct ssh *, struct Forward *);
int	 channel_setup_local_fwd_listener(struct ssh *, struct Forward *,
	    struct ForwardOptions *);
int	 channel_request_rforward_cancel(struct ssh *, struct Forward *);
int	 channel_setup_remote_fwd_listener(struct ssh *, struct Forward *,
	    int *, struct ForwardOptions *);
int	 channel_cancel_rport_listener(struct ssh *, struct Forward *);
int	 channel_cancel_lport_listener(struct ssh *, struct Forward *,
	    int, struct ForwardOptions *);
int	 permitopen_port(const char *);



void	 channel_set_x11_refuse_time(struct ssh *, time_t);
int	 x11_connect_display(struct ssh *);
int	 x11_create_display_inet(struct ssh *, int, int, int, u_int *, int **);
void	 x11_request_forwarding_with_spoofing(struct ssh *, int,
	    const char *, const char *, const char *, int);



int	 chan_is_dead(struct ssh *, Channel *, int);
void	 chan_mark_dead(struct ssh *, Channel *);



void	 chan_rcvd_oclose(struct ssh *, Channel *);
void	 chan_rcvd_eow(struct ssh *, Channel *);
void	 chan_read_failed(struct ssh *, Channel *);
void	 chan_ibuf_empty(struct ssh *, Channel *);
void	 chan_rcvd_ieof(struct ssh *, Channel *);
void	 chan_write_failed(struct ssh *, Channel *);
void	 chan_obuf_empty(struct ssh *, Channel *);

#endif
