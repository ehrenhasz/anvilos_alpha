 
 

#include "includes.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include "openbsd-compat/sys-tree.h"
#include "openbsd-compat/sys-queue.h"
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif
#include <grp.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#ifdef WITH_OPENSSL
#include <openssl/dh.h>
#include <openssl/bn.h>
#include <openssl/rand.h>
#include "openbsd-compat/openssl-compat.h"
#endif

#ifdef HAVE_SECUREWARE
#include <sys/security.h>
#include <prot.h>
#endif

#include "xmalloc.h"
#include "ssh.h"
#include "ssh2.h"
#include "sshpty.h"
#include "packet.h"
#include "log.h"
#include "sshbuf.h"
#include "misc.h"
#include "match.h"
#include "servconf.h"
#include "uidswap.h"
#include "compat.h"
#include "cipher.h"
#include "digest.h"
#include "sshkey.h"
#include "kex.h"
#include "authfile.h"
#include "pathnames.h"
#include "atomicio.h"
#include "canohost.h"
#include "hostfile.h"
#include "auth.h"
#include "authfd.h"
#include "msg.h"
#include "dispatch.h"
#include "channels.h"
#include "session.h"
#include "monitor.h"
#ifdef GSSAPI
#include "ssh-gss.h"
#endif
#include "monitor_wrap.h"
#include "ssh-sandbox.h"
#include "auth-options.h"
#include "version.h"
#include "ssherr.h"
#include "sk-api.h"
#include "srclimit.h"
#include "dh.h"

 
#define REEXEC_DEVCRYPTO_RESERVED_FD	(STDERR_FILENO + 1)
#define REEXEC_STARTUP_PIPE_FD		(STDERR_FILENO + 2)
#define REEXEC_CONFIG_PASS_FD		(STDERR_FILENO + 3)
#define REEXEC_MIN_FREE_FD		(STDERR_FILENO + 4)

extern char *__progname;

 
ServerOptions options;

 
char *config_file_name = _PATH_SERVER_CONFIG_FILE;

 
int debug_flag = 0;

 
static int test_flag = 0;

 
static int inetd_flag = 0;

 
static int no_daemon_flag = 0;

 
static int log_stderr = 0;

 
static char **saved_argv;
static int saved_argc;

 
static int rexeced_flag = 0;
static int rexec_flag = 1;
static int rexec_argc = 0;
static char **rexec_argv;

 
#define	MAX_LISTEN_SOCKS	16
static int listen_socks[MAX_LISTEN_SOCKS];
static int num_listen_socks = 0;

 
int auth_sock = -1;
static int have_agent = 0;

 
struct {
	struct sshkey	**host_keys;		 
	struct sshkey	**host_pubkeys;		 
	struct sshkey	**host_certificates;	 
	int		have_ssh2_key;
} sensitive_data;

 
static volatile sig_atomic_t received_sighup = 0;
static volatile sig_atomic_t received_sigterm = 0;

 
u_int utmp_len = HOST_NAME_MAX+1;

 
static int *startup_pipes = NULL;
static int *startup_flags = NULL;	 
static int startup_pipe = -1;		 

 
int use_privsep = -1;
struct monitor *pmonitor = NULL;
int privsep_is_preauth = 1;
static int privsep_chroot = 1;

 
Authctxt *the_authctxt = NULL;
struct ssh *the_active_state;

 
struct sshauthopt *auth_opts = NULL;

 
struct sshbuf *cfg;

 
struct include_list includes = TAILQ_HEAD_INITIALIZER(includes);

 
struct sshbuf *loginmsg;

 
struct passwd *privsep_pw = NULL;

 
void destroy_sensitive_data(void);
void demote_sensitive_data(void);
static void do_ssh2_kex(struct ssh *);

static char *listener_proctitle;

 
static void
close_listen_socks(void)
{
	int i;

	for (i = 0; i < num_listen_socks; i++)
		close(listen_socks[i]);
	num_listen_socks = 0;
}

static void
close_startup_pipes(void)
{
	int i;

	if (startup_pipes)
		for (i = 0; i < options.max_startups; i++)
			if (startup_pipes[i] != -1)
				close(startup_pipes[i]);
}

 

static void
sighup_handler(int sig)
{
	received_sighup = 1;
}

 
static void
sighup_restart(void)
{
	logit("Received SIGHUP; restarting.");
	if (options.pid_file != NULL)
		unlink(options.pid_file);
	platform_pre_restart();
	close_listen_socks();
	close_startup_pipes();
	ssh_signal(SIGHUP, SIG_IGN);  
	execv(saved_argv[0], saved_argv);
	logit("RESTART FAILED: av[0]='%.100s', error: %.100s.", saved_argv[0],
	    strerror(errno));
	exit(1);
}

 
static void
sigterm_handler(int sig)
{
	received_sigterm = sig;
}

 
static void
main_sigchld_handler(int sig)
{
	int save_errno = errno;
	pid_t pid;
	int status;

	while ((pid = waitpid(-1, &status, WNOHANG)) > 0 ||
	    (pid == -1 && errno == EINTR))
		;
	errno = save_errno;
}

 
static void
grace_alarm_handler(int sig)
{
	 
	if (getpgid(0) == getpid()) {
		ssh_signal(SIGTERM, SIG_IGN);
		kill(0, SIGTERM);
	}

	 
	sigdie("Timeout before authentication for %s port %d",
	    ssh_remote_ipaddr(the_active_state),
	    ssh_remote_port(the_active_state));
}

 
void
destroy_sensitive_data(void)
{
	u_int i;

	for (i = 0; i < options.num_host_key_files; i++) {
		if (sensitive_data.host_keys[i]) {
			sshkey_free(sensitive_data.host_keys[i]);
			sensitive_data.host_keys[i] = NULL;
		}
		if (sensitive_data.host_certificates[i]) {
			sshkey_free(sensitive_data.host_certificates[i]);
			sensitive_data.host_certificates[i] = NULL;
		}
	}
}

 
void
demote_sensitive_data(void)
{
	struct sshkey *tmp;
	u_int i;
	int r;

	for (i = 0; i < options.num_host_key_files; i++) {
		if (sensitive_data.host_keys[i]) {
			if ((r = sshkey_from_private(
			    sensitive_data.host_keys[i], &tmp)) != 0)
				fatal_r(r, "could not demote host %s key",
				    sshkey_type(sensitive_data.host_keys[i]));
			sshkey_free(sensitive_data.host_keys[i]);
			sensitive_data.host_keys[i] = tmp;
		}
		 
	}
}

static void
reseed_prngs(void)
{
	u_int32_t rnd[256];

#ifdef WITH_OPENSSL
	RAND_poll();
#endif
	arc4random_stir();  
	arc4random_buf(rnd, sizeof(rnd));  

#ifdef WITH_OPENSSL
	RAND_seed(rnd, sizeof(rnd));
	 
	if ((RAND_bytes((u_char *)rnd, 1)) != 1)
		fatal("%s: RAND_bytes failed", __func__);
#endif

	explicit_bzero(rnd, sizeof(rnd));
}

static void
privsep_preauth_child(void)
{
	gid_t gidset[1];

	 
	privsep_challenge_enable();

#ifdef GSSAPI
	 
	ssh_gssapi_prepare_supported_oids();
#endif

	reseed_prngs();

	 
	demote_sensitive_data();

	 
	if (privsep_chroot) {
		 
		if (chroot(_PATH_PRIVSEP_CHROOT_DIR) == -1)
			fatal("chroot(\"%s\"): %s", _PATH_PRIVSEP_CHROOT_DIR,
			    strerror(errno));
		if (chdir("/") == -1)
			fatal("chdir(\"/\"): %s", strerror(errno));

		 
		debug3("privsep user:group %u:%u", (u_int)privsep_pw->pw_uid,
		    (u_int)privsep_pw->pw_gid);
		gidset[0] = privsep_pw->pw_gid;
		if (setgroups(1, gidset) == -1)
			fatal("setgroups: %.100s", strerror(errno));
		permanently_set_uid(privsep_pw);
	}
}

static int
privsep_preauth(struct ssh *ssh)
{
	int status, r;
	pid_t pid;
	struct ssh_sandbox *box = NULL;

	 
	pmonitor = monitor_init();
	 
	pmonitor->m_pkex = &ssh->kex;

	if (use_privsep == PRIVSEP_ON)
		box = ssh_sandbox_init(pmonitor);
	pid = fork();
	if (pid == -1) {
		fatal("fork of unprivileged child failed");
	} else if (pid != 0) {
		debug2("Network child is on pid %ld", (long)pid);

		pmonitor->m_pid = pid;
		if (have_agent) {
			r = ssh_get_authentication_socket(&auth_sock);
			if (r != 0) {
				error_r(r, "Could not get agent socket");
				have_agent = 0;
			}
		}
		if (box != NULL)
			ssh_sandbox_parent_preauth(box, pid);
		monitor_child_preauth(ssh, pmonitor);

		 
		while (waitpid(pid, &status, 0) == -1) {
			if (errno == EINTR)
				continue;
			pmonitor->m_pid = -1;
			fatal_f("waitpid: %s", strerror(errno));
		}
		privsep_is_preauth = 0;
		pmonitor->m_pid = -1;
		if (WIFEXITED(status)) {
			if (WEXITSTATUS(status) != 0)
				fatal_f("preauth child exited with status %d",
				    WEXITSTATUS(status));
		} else if (WIFSIGNALED(status))
			fatal_f("preauth child terminated by signal %d",
			    WTERMSIG(status));
		if (box != NULL)
			ssh_sandbox_parent_finish(box);
		return 1;
	} else {
		 
		close(pmonitor->m_sendfd);
		close(pmonitor->m_log_recvfd);

		 
		set_log_handler(mm_log_handler, pmonitor);

		privsep_preauth_child();
		setproctitle("%s", "[net]");
		if (box != NULL)
			ssh_sandbox_child(box);

		return 0;
	}
}

static void
privsep_postauth(struct ssh *ssh, Authctxt *authctxt)
{
#ifdef DISABLE_FD_PASSING
	if (1) {
#else
	if (authctxt->pw->pw_uid == 0) {
#endif
		 
		use_privsep = 0;
		goto skip;
	}

	 
	monitor_reinit(pmonitor);

	pmonitor->m_pid = fork();
	if (pmonitor->m_pid == -1)
		fatal("fork of unprivileged child failed");
	else if (pmonitor->m_pid != 0) {
		verbose("User child is on pid %ld", (long)pmonitor->m_pid);
		sshbuf_reset(loginmsg);
		monitor_clear_keystate(ssh, pmonitor);
		monitor_child_postauth(ssh, pmonitor);

		 
		exit(0);
	}

	 

	close(pmonitor->m_sendfd);
	pmonitor->m_sendfd = -1;

	 
	demote_sensitive_data();

	reseed_prngs();

	 
	do_setusercontext(authctxt->pw);

 skip:
	 
	monitor_apply_keystate(ssh, pmonitor);

	 
	ssh_packet_set_authenticated(ssh);
}

static void
append_hostkey_type(struct sshbuf *b, const char *s)
{
	int r;

	if (match_pattern_list(s, options.hostkeyalgorithms, 0) != 1) {
		debug3_f("%s key not permitted by HostkeyAlgorithms", s);
		return;
	}
	if ((r = sshbuf_putf(b, "%s%s", sshbuf_len(b) > 0 ? "," : "", s)) != 0)
		fatal_fr(r, "sshbuf_putf");
}

static char *
list_hostkey_types(void)
{
	struct sshbuf *b;
	struct sshkey *key;
	char *ret;
	u_int i;

	if ((b = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	for (i = 0; i < options.num_host_key_files; i++) {
		key = sensitive_data.host_keys[i];
		if (key == NULL)
			key = sensitive_data.host_pubkeys[i];
		if (key == NULL)
			continue;
		switch (key->type) {
		case KEY_RSA:
			 
			append_hostkey_type(b, "rsa-sha2-512");
			append_hostkey_type(b, "rsa-sha2-256");
			 
		case KEY_DSA:
		case KEY_ECDSA:
		case KEY_ED25519:
		case KEY_ECDSA_SK:
		case KEY_ED25519_SK:
		case KEY_XMSS:
			append_hostkey_type(b, sshkey_ssh_name(key));
			break;
		}
		 
		key = sensitive_data.host_certificates[i];
		if (key == NULL)
			continue;
		switch (key->type) {
		case KEY_RSA_CERT:
			 
			append_hostkey_type(b,
			    "rsa-sha2-512-cert-v01@openssh.com");
			append_hostkey_type(b,
			    "rsa-sha2-256-cert-v01@openssh.com");
			 
		case KEY_DSA_CERT:
		case KEY_ECDSA_CERT:
		case KEY_ED25519_CERT:
		case KEY_ECDSA_SK_CERT:
		case KEY_ED25519_SK_CERT:
		case KEY_XMSS_CERT:
			append_hostkey_type(b, sshkey_ssh_name(key));
			break;
		}
	}
	if ((ret = sshbuf_dup_string(b)) == NULL)
		fatal_f("sshbuf_dup_string failed");
	sshbuf_free(b);
	debug_f("%s", ret);
	return ret;
}

static struct sshkey *
get_hostkey_by_type(int type, int nid, int need_private, struct ssh *ssh)
{
	u_int i;
	struct sshkey *key;

	for (i = 0; i < options.num_host_key_files; i++) {
		switch (type) {
		case KEY_RSA_CERT:
		case KEY_DSA_CERT:
		case KEY_ECDSA_CERT:
		case KEY_ED25519_CERT:
		case KEY_ECDSA_SK_CERT:
		case KEY_ED25519_SK_CERT:
		case KEY_XMSS_CERT:
			key = sensitive_data.host_certificates[i];
			break;
		default:
			key = sensitive_data.host_keys[i];
			if (key == NULL && !need_private)
				key = sensitive_data.host_pubkeys[i];
			break;
		}
		if (key == NULL || key->type != type)
			continue;
		switch (type) {
		case KEY_ECDSA:
		case KEY_ECDSA_SK:
		case KEY_ECDSA_CERT:
		case KEY_ECDSA_SK_CERT:
			if (key->ecdsa_nid != nid)
				continue;
			 
		default:
			return need_private ?
			    sensitive_data.host_keys[i] : key;
		}
	}
	return NULL;
}

struct sshkey *
get_hostkey_public_by_type(int type, int nid, struct ssh *ssh)
{
	return get_hostkey_by_type(type, nid, 0, ssh);
}

struct sshkey *
get_hostkey_private_by_type(int type, int nid, struct ssh *ssh)
{
	return get_hostkey_by_type(type, nid, 1, ssh);
}

struct sshkey *
get_hostkey_by_index(int ind)
{
	if (ind < 0 || (u_int)ind >= options.num_host_key_files)
		return (NULL);
	return (sensitive_data.host_keys[ind]);
}

struct sshkey *
get_hostkey_public_by_index(int ind, struct ssh *ssh)
{
	if (ind < 0 || (u_int)ind >= options.num_host_key_files)
		return (NULL);
	return (sensitive_data.host_pubkeys[ind]);
}

int
get_hostkey_index(struct sshkey *key, int compare, struct ssh *ssh)
{
	u_int i;

	for (i = 0; i < options.num_host_key_files; i++) {
		if (sshkey_is_cert(key)) {
			if (key == sensitive_data.host_certificates[i] ||
			    (compare && sensitive_data.host_certificates[i] &&
			    sshkey_equal(key,
			    sensitive_data.host_certificates[i])))
				return (i);
		} else {
			if (key == sensitive_data.host_keys[i] ||
			    (compare && sensitive_data.host_keys[i] &&
			    sshkey_equal(key, sensitive_data.host_keys[i])))
				return (i);
			if (key == sensitive_data.host_pubkeys[i] ||
			    (compare && sensitive_data.host_pubkeys[i] &&
			    sshkey_equal(key, sensitive_data.host_pubkeys[i])))
				return (i);
		}
	}
	return (-1);
}

 
static void
notify_hostkeys(struct ssh *ssh)
{
	struct sshbuf *buf;
	struct sshkey *key;
	u_int i, nkeys;
	int r;
	char *fp;

	 
	if (ssh->compat & SSH_BUG_HOSTKEYS)
		return;

	if ((buf = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new");
	for (i = nkeys = 0; i < options.num_host_key_files; i++) {
		key = get_hostkey_public_by_index(i, ssh);
		if (key == NULL || key->type == KEY_UNSPEC ||
		    sshkey_is_cert(key))
			continue;
		fp = sshkey_fingerprint(key, options.fingerprint_hash,
		    SSH_FP_DEFAULT);
		debug3_f("key %d: %s %s", i, sshkey_ssh_name(key), fp);
		free(fp);
		if (nkeys == 0) {
			 
			if ((r = sshpkt_start(ssh, SSH2_MSG_GLOBAL_REQUEST)) != 0 ||
			    (r = sshpkt_put_cstring(ssh, "hostkeys-00@openssh.com")) != 0 ||
			    (r = sshpkt_put_u8(ssh, 0)) != 0)  
				sshpkt_fatal(ssh, r, "%s: start request", __func__);
		}
		 
		sshbuf_reset(buf);
		if ((r = sshkey_putb(key, buf)) != 0)
			fatal_fr(r, "couldn't put hostkey %d", i);
		if ((r = sshpkt_put_stringb(ssh, buf)) != 0)
			sshpkt_fatal(ssh, r, "%s: append key", __func__);
		nkeys++;
	}
	debug3_f("sent %u hostkeys", nkeys);
	if (nkeys == 0)
		fatal_f("no hostkeys");
	if ((r = sshpkt_send(ssh)) != 0)
		sshpkt_fatal(ssh, r, "%s: send", __func__);
	sshbuf_free(buf);
}

 
static int
should_drop_connection(int startups)
{
	int p, r;

	if (startups < options.max_startups_begin)
		return 0;
	if (startups >= options.max_startups)
		return 1;
	if (options.max_startups_rate == 100)
		return 1;

	p  = 100 - options.max_startups_rate;
	p *= startups - options.max_startups_begin;
	p /= options.max_startups - options.max_startups_begin;
	p += options.max_startups_rate;
	r = arc4random_uniform(100);

	debug_f("p %d, r %d", p, r);
	return (r < p) ? 1 : 0;
}

 
static int
drop_connection(int sock, int startups, int notify_pipe)
{
	char *laddr, *raddr;
	const char msg[] = "Exceeded MaxStartups\r\n";
	static time_t last_drop, first_drop;
	static u_int ndropped;
	LogLevel drop_level = SYSLOG_LEVEL_VERBOSE;
	time_t now;

	now = monotime();
	if (!should_drop_connection(startups) &&
	    srclimit_check_allow(sock, notify_pipe) == 1) {
		if (last_drop != 0 &&
		    startups < options.max_startups_begin - 1) {
			 
			logit("exited MaxStartups throttling after %s, "
			    "%u connections dropped",
			    fmt_timeframe(now - first_drop), ndropped);
			last_drop = 0;
		}
		return 0;
	}

#define SSHD_MAXSTARTUPS_LOG_INTERVAL	(5 * 60)
	if (last_drop == 0) {
		error("beginning MaxStartups throttling");
		drop_level = SYSLOG_LEVEL_INFO;
		first_drop = now;
		ndropped = 0;
	} else if (last_drop + SSHD_MAXSTARTUPS_LOG_INTERVAL < now) {
		 
		error("in MaxStartups throttling for %s, "
		    "%u connections dropped",
		    fmt_timeframe(now - first_drop), ndropped + 1);
		drop_level = SYSLOG_LEVEL_INFO;
	}
	last_drop = now;
	ndropped++;

	laddr = get_local_ipaddr(sock);
	raddr = get_peer_ipaddr(sock);
	do_log2(drop_level, "drop connection #%d from [%s]:%d on [%s]:%d "
	    "past MaxStartups", startups, raddr, get_peer_port(sock),
	    laddr, get_local_port(sock));
	free(laddr);
	free(raddr);
	 
	(void)write(sock, msg, sizeof(msg) - 1);
	return 1;
}

static void
usage(void)
{
	fprintf(stderr, "%s, %s\n", SSH_RELEASE, SSH_OPENSSL_VERSION);
	fprintf(stderr,
"usage: sshd [-46DdeGiqTtV] [-C connection_spec] [-c host_cert_file]\n"
"            [-E log_file] [-f config_file] [-g login_grace_time]\n"
"            [-h host_key_file] [-o option] [-p port] [-u len]\n"
	);
	exit(1);
}

static void
send_rexec_state(int fd, struct sshbuf *conf)
{
	struct sshbuf *m = NULL, *inc = NULL;
	struct include_item *item = NULL;
	int r;

	debug3_f("entering fd = %d config len %zu", fd,
	    sshbuf_len(conf));

	if ((m = sshbuf_new()) == NULL || (inc = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");

	 
	TAILQ_FOREACH(item, &includes, entry) {
		if ((r = sshbuf_put_cstring(inc, item->selector)) != 0 ||
		    (r = sshbuf_put_cstring(inc, item->filename)) != 0 ||
		    (r = sshbuf_put_stringb(inc, item->contents)) != 0)
			fatal_fr(r, "compose includes");
	}

	 
	if ((r = sshbuf_put_stringb(m, conf)) != 0 ||
	    (r = sshbuf_put_stringb(m, inc)) != 0)
		fatal_fr(r, "compose config");
	if (ssh_msg_send(fd, 0, m) == -1)
		error_f("ssh_msg_send failed");

	sshbuf_free(m);
	sshbuf_free(inc);

	debug3_f("done");
}

static void
recv_rexec_state(int fd, struct sshbuf *conf)
{
	struct sshbuf *m, *inc;
	u_char *cp, ver;
	size_t len;
	int r;
	struct include_item *item;

	debug3_f("entering fd = %d", fd);

	if ((m = sshbuf_new()) == NULL || (inc = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if (ssh_msg_recv(fd, m) == -1)
		fatal_f("ssh_msg_recv failed");
	if ((r = sshbuf_get_u8(m, &ver)) != 0)
		fatal_fr(r, "parse version");
	if (ver != 0)
		fatal_f("rexec version mismatch");
	if ((r = sshbuf_get_string(m, &cp, &len)) != 0 ||
	    (r = sshbuf_get_stringb(m, inc)) != 0)
		fatal_fr(r, "parse config");

	if (conf != NULL && (r = sshbuf_put(conf, cp, len)))
		fatal_fr(r, "sshbuf_put");

	while (sshbuf_len(inc) != 0) {
		item = xcalloc(1, sizeof(*item));
		if ((item->contents = sshbuf_new()) == NULL)
			fatal_f("sshbuf_new failed");
		if ((r = sshbuf_get_cstring(inc, &item->selector, NULL)) != 0 ||
		    (r = sshbuf_get_cstring(inc, &item->filename, NULL)) != 0 ||
		    (r = sshbuf_get_stringb(inc, item->contents)) != 0)
			fatal_fr(r, "parse includes");
		TAILQ_INSERT_TAIL(&includes, item, entry);
	}

	free(cp);
	sshbuf_free(m);

	debug3_f("done");
}

 
static void
server_accept_inetd(int *sock_in, int *sock_out)
{
	if (rexeced_flag) {
		close(REEXEC_CONFIG_PASS_FD);
		*sock_in = *sock_out = dup(STDIN_FILENO);
	} else {
		*sock_in = dup(STDIN_FILENO);
		*sock_out = dup(STDOUT_FILENO);
	}
	 
	if (stdfd_devnull(1, 1, !log_stderr) == -1)
		error_f("stdfd_devnull failed");
	debug("inetd sockets after dupping: %d, %d", *sock_in, *sock_out);
}

 
static void
listen_on_addrs(struct listenaddr *la)
{
	int ret, listen_sock;
	struct addrinfo *ai;
	char ntop[NI_MAXHOST], strport[NI_MAXSERV];

	for (ai = la->addrs; ai; ai = ai->ai_next) {
		if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
			continue;
		if (num_listen_socks >= MAX_LISTEN_SOCKS)
			fatal("Too many listen sockets. "
			    "Enlarge MAX_LISTEN_SOCKS");
		if ((ret = getnameinfo(ai->ai_addr, ai->ai_addrlen,
		    ntop, sizeof(ntop), strport, sizeof(strport),
		    NI_NUMERICHOST|NI_NUMERICSERV)) != 0) {
			error("getnameinfo failed: %.100s",
			    ssh_gai_strerror(ret));
			continue;
		}
		 
		listen_sock = socket(ai->ai_family, ai->ai_socktype,
		    ai->ai_protocol);
		if (listen_sock == -1) {
			 
			verbose("socket: %.100s", strerror(errno));
			continue;
		}
		if (set_nonblock(listen_sock) == -1) {
			close(listen_sock);
			continue;
		}
		if (fcntl(listen_sock, F_SETFD, FD_CLOEXEC) == -1) {
			verbose("socket: CLOEXEC: %s", strerror(errno));
			close(listen_sock);
			continue;
		}
		 
		set_reuseaddr(listen_sock);
		if (la->rdomain != NULL &&
		    set_rdomain(listen_sock, la->rdomain) == -1) {
			close(listen_sock);
			continue;
		}

		 
		if (ai->ai_family == AF_INET6)
			sock_set_v6only(listen_sock);

		debug("Bind to port %s on %s.", strport, ntop);

		 
		if (bind(listen_sock, ai->ai_addr, ai->ai_addrlen) == -1) {
			error("Bind to port %s on %s failed: %.200s.",
			    strport, ntop, strerror(errno));
			close(listen_sock);
			continue;
		}
		listen_socks[num_listen_socks] = listen_sock;
		num_listen_socks++;

		 
		if (listen(listen_sock, SSH_LISTEN_BACKLOG) == -1)
			fatal("listen on [%s]:%s: %.100s",
			    ntop, strport, strerror(errno));
		logit("Server listening on %s port %s%s%s.",
		    ntop, strport,
		    la->rdomain == NULL ? "" : " rdomain ",
		    la->rdomain == NULL ? "" : la->rdomain);
	}
}

static void
server_listen(void)
{
	u_int i;

	 
	srclimit_init(options.max_startups, options.per_source_max_startups,
	    options.per_source_masklen_ipv4, options.per_source_masklen_ipv6);

	for (i = 0; i < options.num_listen_addrs; i++) {
		listen_on_addrs(&options.listen_addrs[i]);
		freeaddrinfo(options.listen_addrs[i].addrs);
		free(options.listen_addrs[i].rdomain);
		memset(&options.listen_addrs[i], 0,
		    sizeof(options.listen_addrs[i]));
	}
	free(options.listen_addrs);
	options.listen_addrs = NULL;
	options.num_listen_addrs = 0;

	if (!num_listen_socks)
		fatal("Cannot bind any address.");
}

 
static void
server_accept_loop(int *sock_in, int *sock_out, int *newsock, int *config_s)
{
	struct pollfd *pfd = NULL;
	int i, j, ret, npfd;
	int ostartups = -1, startups = 0, listening = 0, lameduck = 0;
	int startup_p[2] = { -1 , -1 }, *startup_pollfd;
	char c = 0;
	struct sockaddr_storage from;
	socklen_t fromlen;
	pid_t pid;
	u_char rnd[256];
	sigset_t nsigset, osigset;

	 
	startup_pipes = xcalloc(options.max_startups, sizeof(int));
	startup_flags = xcalloc(options.max_startups, sizeof(int));
	startup_pollfd = xcalloc(options.max_startups, sizeof(int));
	for (i = 0; i < options.max_startups; i++)
		startup_pipes[i] = -1;

	 
	sigemptyset(&nsigset);
	sigaddset(&nsigset, SIGHUP);
	sigaddset(&nsigset, SIGCHLD);
	sigaddset(&nsigset, SIGTERM);
	sigaddset(&nsigset, SIGQUIT);

	 
	pfd = xcalloc(num_listen_socks + options.max_startups,
	    sizeof(struct pollfd));

	 
	for (;;) {
		sigprocmask(SIG_BLOCK, &nsigset, &osigset);
		if (received_sigterm) {
			logit("Received signal %d; terminating.",
			    (int) received_sigterm);
			close_listen_socks();
			if (options.pid_file != NULL)
				unlink(options.pid_file);
			exit(received_sigterm == SIGTERM ? 0 : 255);
		}
		if (ostartups != startups) {
			setproctitle("%s [listener] %d of %d-%d startups",
			    listener_proctitle, startups,
			    options.max_startups_begin, options.max_startups);
			ostartups = startups;
		}
		if (received_sighup) {
			if (!lameduck) {
				debug("Received SIGHUP; waiting for children");
				close_listen_socks();
				lameduck = 1;
			}
			if (listening <= 0) {
				sigprocmask(SIG_SETMASK, &osigset, NULL);
				sighup_restart();
			}
		}

		for (i = 0; i < num_listen_socks; i++) {
			pfd[i].fd = listen_socks[i];
			pfd[i].events = POLLIN;
		}
		npfd = num_listen_socks;
		for (i = 0; i < options.max_startups; i++) {
			startup_pollfd[i] = -1;
			if (startup_pipes[i] != -1) {
				pfd[npfd].fd = startup_pipes[i];
				pfd[npfd].events = POLLIN;
				startup_pollfd[i] = npfd++;
			}
		}

		 
		ret = ppoll(pfd, npfd, NULL, &osigset);
		if (ret == -1 && errno != EINTR) {
			error("ppoll: %.100s", strerror(errno));
			if (errno == EINVAL)
				cleanup_exit(1);  
		}
		sigprocmask(SIG_SETMASK, &osigset, NULL);
		if (ret == -1)
			continue;

		for (i = 0; i < options.max_startups; i++) {
			if (startup_pipes[i] == -1 ||
			    startup_pollfd[i] == -1 ||
			    !(pfd[startup_pollfd[i]].revents & (POLLIN|POLLHUP)))
				continue;
			switch (read(startup_pipes[i], &c, sizeof(c))) {
			case -1:
				if (errno == EINTR || errno == EAGAIN)
					continue;
				if (errno != EPIPE) {
					error_f("startup pipe %d (fd=%d): "
					    "read %s", i, startup_pipes[i],
					    strerror(errno));
				}
				 
			case 0:
				 
				close(startup_pipes[i]);
				srclimit_done(startup_pipes[i]);
				startup_pipes[i] = -1;
				startups--;
				if (startup_flags[i])
					listening--;
				break;
			case 1:
				 
				if (startup_flags[i]) {
					listening--;
					startup_flags[i] = 0;
				}
				break;
			}
		}
		for (i = 0; i < num_listen_socks; i++) {
			if (!(pfd[i].revents & POLLIN))
				continue;
			fromlen = sizeof(from);
			*newsock = accept(listen_socks[i],
			    (struct sockaddr *)&from, &fromlen);
			if (*newsock == -1) {
				if (errno != EINTR && errno != EWOULDBLOCK &&
				    errno != ECONNABORTED && errno != EAGAIN)
					error("accept: %.100s",
					    strerror(errno));
				if (errno == EMFILE || errno == ENFILE)
					usleep(100 * 1000);
				continue;
			}
			if (unset_nonblock(*newsock) == -1) {
				close(*newsock);
				continue;
			}
			if (pipe(startup_p) == -1) {
				error_f("pipe(startup_p): %s", strerror(errno));
				close(*newsock);
				continue;
			}
			if (drop_connection(*newsock, startups, startup_p[0])) {
				close(*newsock);
				close(startup_p[0]);
				close(startup_p[1]);
				continue;
			}

			if (rexec_flag && socketpair(AF_UNIX,
			    SOCK_STREAM, 0, config_s) == -1) {
				error("reexec socketpair: %s",
				    strerror(errno));
				close(*newsock);
				close(startup_p[0]);
				close(startup_p[1]);
				continue;
			}

			for (j = 0; j < options.max_startups; j++)
				if (startup_pipes[j] == -1) {
					startup_pipes[j] = startup_p[0];
					startups++;
					startup_flags[j] = 1;
					break;
				}

			 
			if (debug_flag) {
				 
				debug("Server will not fork when running in debugging mode.");
				close_listen_socks();
				*sock_in = *newsock;
				*sock_out = *newsock;
				close(startup_p[0]);
				close(startup_p[1]);
				startup_pipe = -1;
				pid = getpid();
				if (rexec_flag) {
					send_rexec_state(config_s[0], cfg);
					close(config_s[0]);
				}
				free(pfd);
				return;
			}

			 
			platform_pre_fork();
			listening++;
			if ((pid = fork()) == 0) {
				 
				platform_post_fork_child();
				startup_pipe = startup_p[1];
				close_startup_pipes();
				close_listen_socks();
				*sock_in = *newsock;
				*sock_out = *newsock;
				log_init(__progname,
				    options.log_level,
				    options.log_facility,
				    log_stderr);
				if (rexec_flag)
					close(config_s[0]);
				else {
					 
					(void)atomicio(vwrite, startup_pipe,
					    "\0", 1);
				}
				free(pfd);
				return;
			}

			 
			platform_post_fork_parent(pid);
			if (pid == -1)
				error("fork: %.100s", strerror(errno));
			else
				debug("Forked child %ld.", (long)pid);

			close(startup_p[1]);

			if (rexec_flag) {
				close(config_s[1]);
				send_rexec_state(config_s[0], cfg);
				close(config_s[0]);
			}
			close(*newsock);

			 
			arc4random_stir();
			arc4random_buf(rnd, sizeof(rnd));
#ifdef WITH_OPENSSL
			RAND_seed(rnd, sizeof(rnd));
			if ((RAND_bytes((u_char *)rnd, 1)) != 1)
				fatal("%s: RAND_bytes failed", __func__);
#endif
			explicit_bzero(rnd, sizeof(rnd));
		}
	}
}

 
static void
check_ip_options(struct ssh *ssh)
{
#ifdef IP_OPTIONS
	int sock_in = ssh_packet_get_connection_in(ssh);
	struct sockaddr_storage from;
	u_char opts[200];
	socklen_t i, option_size = sizeof(opts), fromlen = sizeof(from);
	char text[sizeof(opts) * 3 + 1];

	memset(&from, 0, sizeof(from));
	if (getpeername(sock_in, (struct sockaddr *)&from,
	    &fromlen) == -1)
		return;
	if (from.ss_family != AF_INET)
		return;
	 

	if (getsockopt(sock_in, IPPROTO_IP, IP_OPTIONS, opts,
	    &option_size) >= 0 && option_size != 0) {
		text[0] = '\0';
		for (i = 0; i < option_size; i++)
			snprintf(text + i*3, sizeof(text) - i*3,
			    " %2.2x", opts[i]);
		fatal("Connection from %.100s port %d with IP opts: %.800s",
		    ssh_remote_ipaddr(ssh), ssh_remote_port(ssh), text);
	}
	return;
#endif  
}

 
static void
set_process_rdomain(struct ssh *ssh, const char *name)
{
#if defined(HAVE_SYS_SET_PROCESS_RDOMAIN)
	if (name == NULL)
		return;  

	if (strcmp(name, "%D") == 0) {
		 
		if ((name = ssh_packet_rdomain_in(ssh)) == NULL)
			return;
	}
	 
	return sys_set_process_rdomain(name);
#elif defined(__OpenBSD__)
	int rtable, ortable = getrtable();
	const char *errstr;

	if (name == NULL)
		return;  

	if (strcmp(name, "%D") == 0) {
		 
		if ((name = ssh_packet_rdomain_in(ssh)) == NULL)
			return;
	}

	rtable = (int)strtonum(name, 0, 255, &errstr);
	if (errstr != NULL)  
		fatal("Invalid routing domain \"%s\": %s", name, errstr);
	if (rtable != ortable && setrtable(rtable) != 0)
		fatal("Unable to set routing domain %d: %s",
		    rtable, strerror(errno));
	debug_f("set routing domain %d (was %d)", rtable, ortable);
#else  
	fatal("Unable to set routing domain: not supported in this platform");
#endif
}

static void
accumulate_host_timing_secret(struct sshbuf *server_cfg,
    struct sshkey *key)
{
	static struct ssh_digest_ctx *ctx;
	u_char *hash;
	size_t len;
	struct sshbuf *buf;
	int r;

	if (ctx == NULL && (ctx = ssh_digest_start(SSH_DIGEST_SHA512)) == NULL)
		fatal_f("ssh_digest_start");
	if (key == NULL) {  
		 
		if (ssh_digest_update(ctx, sshbuf_ptr(server_cfg),
		    sshbuf_len(server_cfg)) != 0)
			fatal_f("ssh_digest_update");
		len = ssh_digest_bytes(SSH_DIGEST_SHA512);
		hash = xmalloc(len);
		if (ssh_digest_final(ctx, hash, len) != 0)
			fatal_f("ssh_digest_final");
		options.timing_secret = PEEK_U64(hash);
		freezero(hash, len);
		ssh_digest_free(ctx);
		ctx = NULL;
		return;
	}
	if ((buf = sshbuf_new()) == NULL)
		fatal_f("could not allocate buffer");
	if ((r = sshkey_private_serialize(key, buf)) != 0)
		fatal_fr(r, "encode %s key", sshkey_ssh_name(key));
	if (ssh_digest_update(ctx, sshbuf_ptr(buf), sshbuf_len(buf)) != 0)
		fatal_f("ssh_digest_update");
	sshbuf_reset(buf);
	sshbuf_free(buf);
}

static char *
prepare_proctitle(int ac, char **av)
{
	char *ret = NULL;
	int i;

	for (i = 0; i < ac; i++)
		xextendf(&ret, " ", "%s", av[i]);
	return ret;
}

static void
print_config(struct ssh *ssh, struct connection_info *connection_info)
{
	 
	if (connection_info == NULL)
		connection_info = get_connection_info(ssh, 0, 0);
	connection_info->test = 1;
	parse_server_match_config(&options, &includes, connection_info);
	dump_config(&options);
	exit(0);
}

 
int
main(int ac, char **av)
{
	struct ssh *ssh = NULL;
	extern char *optarg;
	extern int optind;
	int r, opt, on = 1, do_dump_cfg = 0, already_daemon, remote_port;
	int sock_in = -1, sock_out = -1, newsock = -1;
	const char *remote_ip, *rdomain;
	char *fp, *line, *laddr, *logfile = NULL;
	int config_s[2] = { -1 , -1 };
	u_int i, j;
	u_int64_t ibytes, obytes;
	mode_t new_umask;
	struct sshkey *key;
	struct sshkey *pubkey;
	int keytype;
	Authctxt *authctxt;
	struct connection_info *connection_info = NULL;
	sigset_t sigmask;

#ifdef HAVE_SECUREWARE
	(void)set_auth_parameters(ac, av);
#endif
	__progname = ssh_get_progname(av[0]);

	sigemptyset(&sigmask);
	sigprocmask(SIG_SETMASK, &sigmask, NULL);

	 
	saved_argc = ac;
	rexec_argc = ac;
	saved_argv = xcalloc(ac + 1, sizeof(*saved_argv));
	for (i = 0; (int)i < ac; i++)
		saved_argv[i] = xstrdup(av[i]);
	saved_argv[i] = NULL;

#ifndef HAVE_SETPROCTITLE
	 
	compat_init_setproctitle(ac, av);
	av = saved_argv;
#endif

	if (geteuid() == 0 && setgroups(0, NULL) == -1)
		debug("setgroups(): %.200s", strerror(errno));

	 
	sanitise_stdfd();

	 
	initialize_server_options(&options);

	 
	while ((opt = getopt(ac, av,
	    "C:E:b:c:f:g:h:k:o:p:u:46DGQRTdeiqrtV")) != -1) {
		switch (opt) {
		case '4':
			options.address_family = AF_INET;
			break;
		case '6':
			options.address_family = AF_INET6;
			break;
		case 'f':
			config_file_name = optarg;
			break;
		case 'c':
			servconf_add_hostcert("[command-line]", 0,
			    &options, optarg);
			break;
		case 'd':
			if (debug_flag == 0) {
				debug_flag = 1;
				options.log_level = SYSLOG_LEVEL_DEBUG1;
			} else if (options.log_level < SYSLOG_LEVEL_DEBUG3)
				options.log_level++;
			break;
		case 'D':
			no_daemon_flag = 1;
			break;
		case 'G':
			do_dump_cfg = 1;
			break;
		case 'E':
			logfile = optarg;
			 
		case 'e':
			log_stderr = 1;
			break;
		case 'i':
			inetd_flag = 1;
			break;
		case 'r':
			rexec_flag = 0;
			break;
		case 'R':
			rexeced_flag = 1;
			inetd_flag = 1;
			break;
		case 'Q':
			 
			break;
		case 'q':
			options.log_level = SYSLOG_LEVEL_QUIET;
			break;
		case 'b':
			 
			break;
		case 'p':
			options.ports_from_cmdline = 1;
			if (options.num_ports >= MAX_PORTS) {
				fprintf(stderr, "too many ports.\n");
				exit(1);
			}
			options.ports[options.num_ports++] = a2port(optarg);
			if (options.ports[options.num_ports-1] <= 0) {
				fprintf(stderr, "Bad port number.\n");
				exit(1);
			}
			break;
		case 'g':
			if ((options.login_grace_time = convtime(optarg)) == -1) {
				fprintf(stderr, "Invalid login grace time.\n");
				exit(1);
			}
			break;
		case 'k':
			 
			break;
		case 'h':
			servconf_add_hostkey("[command-line]", 0,
			    &options, optarg, 1);
			break;
		case 't':
			test_flag = 1;
			break;
		case 'T':
			test_flag = 2;
			break;
		case 'C':
			connection_info = get_connection_info(ssh, 0, 0);
			if (parse_server_match_testspec(connection_info,
			    optarg) == -1)
				exit(1);
			break;
		case 'u':
			utmp_len = (u_int)strtonum(optarg, 0, HOST_NAME_MAX+1+1, NULL);
			if (utmp_len > HOST_NAME_MAX+1) {
				fprintf(stderr, "Invalid utmp length.\n");
				exit(1);
			}
			break;
		case 'o':
			line = xstrdup(optarg);
			if (process_server_config_line(&options, line,
			    "command-line", 0, NULL, NULL, &includes) != 0)
				exit(1);
			free(line);
			break;
		case 'V':
			fprintf(stderr, "%s, %s\n",
			    SSH_RELEASE, SSH_OPENSSL_VERSION);
			exit(0);
		default:
			usage();
			break;
		}
	}
	if (rexeced_flag || inetd_flag)
		rexec_flag = 0;
	if (!test_flag && !do_dump_cfg && rexec_flag && !path_absolute(av[0]))
		fatal("sshd re-exec requires execution with an absolute path");
	if (rexeced_flag)
		closefrom(REEXEC_MIN_FREE_FD);
	else
		closefrom(REEXEC_DEVCRYPTO_RESERVED_FD);

	seed_rng();

	 
	if (logfile != NULL)
		log_redirect_stderr_to(logfile);
	 
	log_init(__progname,
	    options.log_level == SYSLOG_LEVEL_NOT_SET ?
	    SYSLOG_LEVEL_INFO : options.log_level,
	    options.log_facility == SYSLOG_FACILITY_NOT_SET ?
	    SYSLOG_FACILITY_AUTH : options.log_facility,
	    log_stderr || !inetd_flag || debug_flag);

	 
	if (getenv("KRB5CCNAME") != NULL)
		(void) unsetenv("KRB5CCNAME");

	sensitive_data.have_ssh2_key = 0;

	 
	if (test_flag < 2 && connection_info != NULL)
		fatal("Config test connection parameter (-C) provided without "
		    "test mode (-T)");

	 
	if ((cfg = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if (rexeced_flag) {
		setproctitle("%s", "[rexeced]");
		recv_rexec_state(REEXEC_CONFIG_PASS_FD, cfg);
		if (!debug_flag) {
			startup_pipe = dup(REEXEC_STARTUP_PIPE_FD);
			close(REEXEC_STARTUP_PIPE_FD);
			 
			(void)atomicio(vwrite, startup_pipe, "\0", 1);
		}
	} else if (strcasecmp(config_file_name, "none") != 0)
		load_server_config(config_file_name, cfg);

	parse_server_config(&options, rexeced_flag ? "rexec" : config_file_name,
	    cfg, &includes, NULL, rexeced_flag);

#ifdef WITH_OPENSSL
	if (options.moduli_file != NULL)
		dh_set_moduli_file(options.moduli_file);
#endif

	 
	fill_default_server_options(&options);

	 
	if (options.authorized_keys_command_user == NULL &&
	    (options.authorized_keys_command != NULL &&
	    strcasecmp(options.authorized_keys_command, "none") != 0))
		fatal("AuthorizedKeysCommand set without "
		    "AuthorizedKeysCommandUser");
	if (options.authorized_principals_command_user == NULL &&
	    (options.authorized_principals_command != NULL &&
	    strcasecmp(options.authorized_principals_command, "none") != 0))
		fatal("AuthorizedPrincipalsCommand set without "
		    "AuthorizedPrincipalsCommandUser");

	 
	if (options.num_auth_methods != 0) {
		for (i = 0; i < options.num_auth_methods; i++) {
			if (auth2_methods_valid(options.auth_methods[i],
			    1) == 0)
				break;
		}
		if (i >= options.num_auth_methods)
			fatal("AuthenticationMethods cannot be satisfied by "
			    "enabled authentication methods");
	}

	 
	if (optind < ac) {
		fprintf(stderr, "Extra argument %s.\n", av[optind]);
		exit(1);
	}

	debug("sshd version %s, %s", SSH_VERSION, SSH_OPENSSL_VERSION);

	if (do_dump_cfg)
		print_config(ssh, connection_info);

	 
	privsep_chroot = use_privsep && (getuid() == 0 || geteuid() == 0);
	if ((privsep_pw = getpwnam(SSH_PRIVSEP_USER)) == NULL) {
		if (privsep_chroot || options.kerberos_authentication)
			fatal("Privilege separation user %s does not exist",
			    SSH_PRIVSEP_USER);
	} else {
		privsep_pw = pwcopy(privsep_pw);
		freezero(privsep_pw->pw_passwd, strlen(privsep_pw->pw_passwd));
		privsep_pw->pw_passwd = xstrdup("*");
	}
	endpwent();

	 
	sensitive_data.host_keys = xcalloc(options.num_host_key_files,
	    sizeof(struct sshkey *));
	sensitive_data.host_pubkeys = xcalloc(options.num_host_key_files,
	    sizeof(struct sshkey *));

	if (options.host_key_agent) {
		if (strcmp(options.host_key_agent, SSH_AUTHSOCKET_ENV_NAME))
			setenv(SSH_AUTHSOCKET_ENV_NAME,
			    options.host_key_agent, 1);
		if ((r = ssh_get_authentication_socket(NULL)) == 0)
			have_agent = 1;
		else
			error_r(r, "Could not connect to agent \"%s\"",
			    options.host_key_agent);
	}

	for (i = 0; i < options.num_host_key_files; i++) {
		int ll = options.host_key_file_userprovided[i] ?
		    SYSLOG_LEVEL_ERROR : SYSLOG_LEVEL_DEBUG1;

		if (options.host_key_files[i] == NULL)
			continue;
		if ((r = sshkey_load_private(options.host_key_files[i], "",
		    &key, NULL)) != 0 && r != SSH_ERR_SYSTEM_ERROR)
			do_log2_r(r, ll, "Unable to load host key \"%s\"",
			    options.host_key_files[i]);
		if (sshkey_is_sk(key) &&
		    key->sk_flags & SSH_SK_USER_PRESENCE_REQD) {
			debug("host key %s requires user presence, ignoring",
			    options.host_key_files[i]);
			key->sk_flags &= ~SSH_SK_USER_PRESENCE_REQD;
		}
		if (r == 0 && key != NULL &&
		    (r = sshkey_shield_private(key)) != 0) {
			do_log2_r(r, ll, "Unable to shield host key \"%s\"",
			    options.host_key_files[i]);
			sshkey_free(key);
			key = NULL;
		}
		if ((r = sshkey_load_public(options.host_key_files[i],
		    &pubkey, NULL)) != 0 && r != SSH_ERR_SYSTEM_ERROR)
			do_log2_r(r, ll, "Unable to load host key \"%s\"",
			    options.host_key_files[i]);
		if (pubkey != NULL && key != NULL) {
			if (!sshkey_equal(pubkey, key)) {
				error("Public key for %s does not match "
				    "private key", options.host_key_files[i]);
				sshkey_free(pubkey);
				pubkey = NULL;
			}
		}
		if (pubkey == NULL && key != NULL) {
			if ((r = sshkey_from_private(key, &pubkey)) != 0)
				fatal_r(r, "Could not demote key: \"%s\"",
				    options.host_key_files[i]);
		}
		if (pubkey != NULL && (r = sshkey_check_rsa_length(pubkey,
		    options.required_rsa_size)) != 0) {
			error_fr(r, "Host key %s", options.host_key_files[i]);
			sshkey_free(pubkey);
			sshkey_free(key);
			continue;
		}
		sensitive_data.host_keys[i] = key;
		sensitive_data.host_pubkeys[i] = pubkey;

		if (key == NULL && pubkey != NULL && have_agent) {
			debug("will rely on agent for hostkey %s",
			    options.host_key_files[i]);
			keytype = pubkey->type;
		} else if (key != NULL) {
			keytype = key->type;
			accumulate_host_timing_secret(cfg, key);
		} else {
			do_log2(ll, "Unable to load host key: %s",
			    options.host_key_files[i]);
			sensitive_data.host_keys[i] = NULL;
			sensitive_data.host_pubkeys[i] = NULL;
			continue;
		}

		switch (keytype) {
		case KEY_RSA:
		case KEY_DSA:
		case KEY_ECDSA:
		case KEY_ED25519:
		case KEY_ECDSA_SK:
		case KEY_ED25519_SK:
		case KEY_XMSS:
			if (have_agent || key != NULL)
				sensitive_data.have_ssh2_key = 1;
			break;
		}
		if ((fp = sshkey_fingerprint(pubkey, options.fingerprint_hash,
		    SSH_FP_DEFAULT)) == NULL)
			fatal("sshkey_fingerprint failed");
		debug("%s host key #%d: %s %s",
		    key ? "private" : "agent", i, sshkey_ssh_name(pubkey), fp);
		free(fp);
	}
	accumulate_host_timing_secret(cfg, NULL);
	if (!sensitive_data.have_ssh2_key) {
		logit("sshd: no hostkeys available -- exiting.");
		exit(1);
	}

	 
	sensitive_data.host_certificates = xcalloc(options.num_host_key_files,
	    sizeof(struct sshkey *));
	for (i = 0; i < options.num_host_key_files; i++)
		sensitive_data.host_certificates[i] = NULL;

	for (i = 0; i < options.num_host_cert_files; i++) {
		if (options.host_cert_files[i] == NULL)
			continue;
		if ((r = sshkey_load_public(options.host_cert_files[i],
		    &key, NULL)) != 0) {
			error_r(r, "Could not load host certificate \"%s\"",
			    options.host_cert_files[i]);
			continue;
		}
		if (!sshkey_is_cert(key)) {
			error("Certificate file is not a certificate: %s",
			    options.host_cert_files[i]);
			sshkey_free(key);
			continue;
		}
		 
		for (j = 0; j < options.num_host_key_files; j++) {
			if (sshkey_equal_public(key,
			    sensitive_data.host_pubkeys[j])) {
				sensitive_data.host_certificates[j] = key;
				break;
			}
		}
		if (j >= options.num_host_key_files) {
			error("No matching private key for certificate: %s",
			    options.host_cert_files[i]);
			sshkey_free(key);
			continue;
		}
		sensitive_data.host_certificates[j] = key;
		debug("host certificate: #%u type %d %s", j, key->type,
		    sshkey_type(key));
	}

	if (privsep_chroot) {
		struct stat st;

		if ((stat(_PATH_PRIVSEP_CHROOT_DIR, &st) == -1) ||
		    (S_ISDIR(st.st_mode) == 0))
			fatal("Missing privilege separation directory: %s",
			    _PATH_PRIVSEP_CHROOT_DIR);

#ifdef HAVE_CYGWIN
		if (check_ntsec(_PATH_PRIVSEP_CHROOT_DIR) &&
		    (st.st_uid != getuid () ||
		    (st.st_mode & (S_IWGRP|S_IWOTH)) != 0))
#else
		if (st.st_uid != 0 || (st.st_mode & (S_IWGRP|S_IWOTH)) != 0)
#endif
			fatal("%s must be owned by root and not group or "
			    "world-writable.", _PATH_PRIVSEP_CHROOT_DIR);
	}

	if (test_flag > 1)
		print_config(ssh, connection_info);

	 
	if (test_flag)
		exit(0);

	 
	if (setgroups(0, NULL) < 0)
		debug("setgroups() failed: %.200s", strerror(errno));

	if (rexec_flag) {
		if (rexec_argc < 0)
			fatal("rexec_argc %d < 0", rexec_argc);
		rexec_argv = xcalloc(rexec_argc + 2, sizeof(char *));
		for (i = 0; i < (u_int)rexec_argc; i++) {
			debug("rexec_argv[%d]='%s'", i, saved_argv[i]);
			rexec_argv[i] = saved_argv[i];
		}
		rexec_argv[rexec_argc] = "-R";
		rexec_argv[rexec_argc + 1] = NULL;
	}
	listener_proctitle = prepare_proctitle(ac, av);

	 
	new_umask = umask(0077) | 0022;
	(void) umask(new_umask);

	 
	if (debug_flag && (!inetd_flag || rexeced_flag))
		log_stderr = 1;
	log_init(__progname, options.log_level,
	    options.log_facility, log_stderr);
	for (i = 0; i < options.num_log_verbose; i++)
		log_verbose_add(options.log_verbose[i]);

	 
	already_daemon = daemonized();
	if (!(debug_flag || inetd_flag || no_daemon_flag || already_daemon)) {

		if (daemon(0, 0) == -1)
			fatal("daemon() failed: %.200s", strerror(errno));

		disconnect_controlling_tty();
	}
	 
	log_init(__progname, options.log_level, options.log_facility, log_stderr);

	 
	if (chdir("/") == -1)
		error("chdir(\"/\"): %s", strerror(errno));

	 
	ssh_signal(SIGPIPE, SIG_IGN);

	 
	if (inetd_flag) {
		server_accept_inetd(&sock_in, &sock_out);
	} else {
		platform_pre_listen();
		server_listen();

		ssh_signal(SIGHUP, sighup_handler);
		ssh_signal(SIGCHLD, main_sigchld_handler);
		ssh_signal(SIGTERM, sigterm_handler);
		ssh_signal(SIGQUIT, sigterm_handler);

		 
		if (options.pid_file != NULL && !debug_flag) {
			FILE *f = fopen(options.pid_file, "w");

			if (f == NULL) {
				error("Couldn't create pid file \"%s\": %s",
				    options.pid_file, strerror(errno));
			} else {
				fprintf(f, "%ld\n", (long) getpid());
				fclose(f);
			}
		}

		 
		server_accept_loop(&sock_in, &sock_out,
		    &newsock, config_s);
	}

	 
	setproctitle("%s", "[accepted]");

	 
	if (!debug_flag && !inetd_flag && setsid() == -1)
		error("setsid: %.100s", strerror(errno));

	if (rexec_flag) {
		debug("rexec start in %d out %d newsock %d pipe %d sock %d",
		    sock_in, sock_out, newsock, startup_pipe, config_s[0]);
		if (dup2(newsock, STDIN_FILENO) == -1)
			debug3_f("dup2 stdin: %s", strerror(errno));
		if (dup2(STDIN_FILENO, STDOUT_FILENO) == -1)
			debug3_f("dup2 stdout: %s", strerror(errno));
		if (startup_pipe == -1)
			close(REEXEC_STARTUP_PIPE_FD);
		else if (startup_pipe != REEXEC_STARTUP_PIPE_FD) {
			if (dup2(startup_pipe, REEXEC_STARTUP_PIPE_FD) == -1)
				debug3_f("dup2 startup_p: %s", strerror(errno));
			close(startup_pipe);
			startup_pipe = REEXEC_STARTUP_PIPE_FD;
		}

		if (dup2(config_s[1], REEXEC_CONFIG_PASS_FD) == -1)
			debug3_f("dup2 config_s: %s", strerror(errno));
		close(config_s[1]);

		ssh_signal(SIGHUP, SIG_IGN);  
		execv(rexec_argv[0], rexec_argv);

		 
		error("rexec of %s failed: %s", rexec_argv[0], strerror(errno));
		recv_rexec_state(REEXEC_CONFIG_PASS_FD, NULL);
		log_init(__progname, options.log_level,
		    options.log_facility, log_stderr);

		 
		close(REEXEC_CONFIG_PASS_FD);
		newsock = sock_out = sock_in = dup(STDIN_FILENO);
		if (stdfd_devnull(1, 1, 0) == -1)
			error_f("stdfd_devnull failed");
		debug("rexec cleanup in %d out %d newsock %d pipe %d sock %d",
		    sock_in, sock_out, newsock, startup_pipe, config_s[0]);
	}

	 
	fcntl(sock_out, F_SETFD, FD_CLOEXEC);
	fcntl(sock_in, F_SETFD, FD_CLOEXEC);

	 
	ssh_signal(SIGALRM, SIG_DFL);
	ssh_signal(SIGHUP, SIG_DFL);
	ssh_signal(SIGTERM, SIG_DFL);
	ssh_signal(SIGQUIT, SIG_DFL);
	ssh_signal(SIGCHLD, SIG_DFL);
	ssh_signal(SIGINT, SIG_DFL);

	 
	if ((ssh = ssh_packet_set_connection(NULL, sock_in, sock_out)) == NULL)
		fatal("Unable to create connection");
	the_active_state = ssh;
	ssh_packet_set_server(ssh);

	check_ip_options(ssh);

	 
	channel_init_channels(ssh);
	channel_set_af(ssh, options.address_family);
	process_channel_timeouts(ssh, &options);
	process_permitopen(ssh, &options);

	 
	if (options.tcp_keep_alive && ssh_packet_connection_is_on_socket(ssh) &&
	    setsockopt(sock_in, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) == -1)
		error("setsockopt SO_KEEPALIVE: %.100s", strerror(errno));

	if ((remote_port = ssh_remote_port(ssh)) < 0) {
		debug("ssh_remote_port failed");
		cleanup_exit(255);
	}

	if (options.routing_domain != NULL)
		set_process_rdomain(ssh, options.routing_domain);

	 
	remote_ip = ssh_remote_ipaddr(ssh);

#ifdef SSH_AUDIT_EVENTS
	audit_connection_from(remote_ip, remote_port);
#endif

	rdomain = ssh_packet_rdomain_in(ssh);

	 
	laddr = get_local_ipaddr(sock_in);
	verbose("Connection from %s port %d on %s port %d%s%s%s",
	    remote_ip, remote_port, laddr,  ssh_local_port(ssh),
	    rdomain == NULL ? "" : " rdomain \"",
	    rdomain == NULL ? "" : rdomain,
	    rdomain == NULL ? "" : "\"");
	free(laddr);

	 
	ssh_signal(SIGALRM, grace_alarm_handler);
	if (!debug_flag)
		alarm(options.login_grace_time);

	if ((r = kex_exchange_identification(ssh, -1,
	    options.version_addendum)) != 0)
		sshpkt_fatal(ssh, r, "banner exchange");

	ssh_packet_set_nonblocking(ssh);

	 
	authctxt = xcalloc(1, sizeof(*authctxt));
	ssh->authctxt = authctxt;

	authctxt->loginmsg = loginmsg;

	 
	the_authctxt = authctxt;

	 
	if ((auth_opts = sshauthopt_new_with_keys_defaults()) == NULL)
		fatal("allocation failed");

	 
	if ((loginmsg = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	auth_debug_reset();

	if (use_privsep) {
		if (privsep_preauth(ssh) == 1)
			goto authenticated;
	} else if (have_agent) {
		if ((r = ssh_get_authentication_socket(&auth_sock)) != 0) {
			error_r(r, "Unable to get agent socket");
			have_agent = 0;
		}
	}

	 
	 
	do_ssh2_kex(ssh);
	do_authentication2(ssh);

	 
	if (use_privsep) {
		mm_send_keystate(ssh, pmonitor);
		ssh_packet_clear_keys(ssh);
		exit(0);
	}

 authenticated:
	 
	alarm(0);
	ssh_signal(SIGALRM, SIG_DFL);
	authctxt->authenticated = 1;
	if (startup_pipe != -1) {
		close(startup_pipe);
		startup_pipe = -1;
	}

#ifdef SSH_AUDIT_EVENTS
	audit_event(ssh, SSH_AUTH_SUCCESS);
#endif

#ifdef GSSAPI
	if (options.gss_authentication) {
		temporarily_use_uid(authctxt->pw);
		ssh_gssapi_storecreds();
		restore_uid();
	}
#endif
#ifdef USE_PAM
	if (options.use_pam) {
		do_pam_setcred(1);
		do_pam_session(ssh);
	}
#endif

	 
	if (use_privsep) {
		privsep_postauth(ssh, authctxt);
		 
	}

	ssh_packet_set_timeout(ssh, options.client_alive_interval,
	    options.client_alive_count_max);

	 
	notify_hostkeys(ssh);

	 
	do_authenticated(ssh, authctxt);

	 
	ssh_packet_get_bytes(ssh, &ibytes, &obytes);
	verbose("Transferred: sent %llu, received %llu bytes",
	    (unsigned long long)obytes, (unsigned long long)ibytes);

	verbose("Closing connection to %.500s port %d", remote_ip, remote_port);

#ifdef USE_PAM
	if (options.use_pam)
		finish_pam();
#endif  

#ifdef SSH_AUDIT_EVENTS
	PRIVSEP(audit_event(ssh, SSH_CONNECTION_CLOSE));
#endif

	ssh_packet_close(ssh);

	if (use_privsep)
		mm_terminate();

	exit(0);
}

int
sshd_hostkey_sign(struct ssh *ssh, struct sshkey *privkey,
    struct sshkey *pubkey, u_char **signature, size_t *slenp,
    const u_char *data, size_t dlen, const char *alg)
{
	int r;

	if (use_privsep) {
		if (privkey) {
			if (mm_sshkey_sign(ssh, privkey, signature, slenp,
			    data, dlen, alg, options.sk_provider, NULL,
			    ssh->compat) < 0)
				fatal_f("privkey sign failed");
		} else {
			if (mm_sshkey_sign(ssh, pubkey, signature, slenp,
			    data, dlen, alg, options.sk_provider, NULL,
			    ssh->compat) < 0)
				fatal_f("pubkey sign failed");
		}
	} else {
		if (privkey) {
			if (sshkey_sign(privkey, signature, slenp, data, dlen,
			    alg, options.sk_provider, NULL, ssh->compat) < 0)
				fatal_f("privkey sign failed");
		} else {
			if ((r = ssh_agent_sign(auth_sock, pubkey,
			    signature, slenp, data, dlen, alg,
			    ssh->compat)) != 0) {
				fatal_fr(r, "agent sign failed");
			}
		}
	}
	return 0;
}

 
static void
do_ssh2_kex(struct ssh *ssh)
{
	char *hkalgs = NULL, *myproposal[PROPOSAL_MAX];
	const char *compression = NULL;
	struct kex *kex;
	int r;

	if (options.rekey_limit || options.rekey_interval)
		ssh_packet_set_rekey_limits(ssh, options.rekey_limit,
		    options.rekey_interval);

	if (options.compression == COMP_NONE)
		compression = "none";
	hkalgs = list_hostkey_types();

	kex_proposal_populate_entries(ssh, myproposal, options.kex_algorithms,
	    options.ciphers, options.macs, compression, hkalgs);

	free(hkalgs);

	 
	if ((r = kex_setup(ssh, myproposal)) != 0)
		fatal_r(r, "kex_setup");
	kex_set_server_sig_algs(ssh, options.pubkey_accepted_algos);
	kex = ssh->kex;

#ifdef WITH_OPENSSL
	kex->kex[KEX_DH_GRP1_SHA1] = kex_gen_server;
	kex->kex[KEX_DH_GRP14_SHA1] = kex_gen_server;
	kex->kex[KEX_DH_GRP14_SHA256] = kex_gen_server;
	kex->kex[KEX_DH_GRP16_SHA512] = kex_gen_server;
	kex->kex[KEX_DH_GRP18_SHA512] = kex_gen_server;
	kex->kex[KEX_DH_GEX_SHA1] = kexgex_server;
	kex->kex[KEX_DH_GEX_SHA256] = kexgex_server;
# ifdef OPENSSL_HAS_ECC
	kex->kex[KEX_ECDH_SHA2] = kex_gen_server;
# endif
#endif
	kex->kex[KEX_C25519_SHA256] = kex_gen_server;
	kex->kex[KEX_KEM_SNTRUP761X25519_SHA512] = kex_gen_server;
	kex->load_host_public_key=&get_hostkey_public_by_type;
	kex->load_host_private_key=&get_hostkey_private_by_type;
	kex->host_key_index=&get_hostkey_index;
	kex->sign = sshd_hostkey_sign;

	ssh_dispatch_run_fatal(ssh, DISPATCH_BLOCK, &kex->done);

#ifdef DEBUG_KEXDH
	 
	if ((r = sshpkt_start(ssh, SSH2_MSG_IGNORE)) != 0 ||
	    (r = sshpkt_put_cstring(ssh, "markus")) != 0 ||
	    (r = sshpkt_send(ssh)) != 0 ||
	    (r = ssh_packet_write_wait(ssh)) != 0)
		fatal_fr(r, "send test");
#endif
	kex_proposal_free_entries(myproposal);
	debug("KEX done");
}

 
void
cleanup_exit(int i)
{
	if (the_active_state != NULL && the_authctxt != NULL) {
		do_cleanup(the_active_state, the_authctxt);
		if (use_privsep && privsep_is_preauth &&
		    pmonitor != NULL && pmonitor->m_pid > 1) {
			debug("Killing privsep child %d", pmonitor->m_pid);
			if (kill(pmonitor->m_pid, SIGKILL) != 0 &&
			    errno != ESRCH) {
				error_f("kill(%d): %s", pmonitor->m_pid,
				    strerror(errno));
			}
		}
	}
#ifdef SSH_AUDIT_EVENTS
	 
	if (the_active_state != NULL && (!use_privsep || mm_is_monitor()))
		audit_event(the_active_state, SSH_CONNECTION_ABANDON);
#endif
	_exit(i);
}
