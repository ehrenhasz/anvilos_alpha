 
 

#include "includes.h"

#include <sys/types.h>

#include <signal.h>
#include <stdarg.h>

#include "ssh2.h"
#include "log.h"
#include "dispatch.h"
#include "packet.h"
#include "ssherr.h"

int
dispatch_protocol_error(int type, u_int32_t seq, struct ssh *ssh)
{
	int r;

	logit("dispatch_protocol_error: type %d seq %u", type, seq);
	if ((r = sshpkt_start(ssh, SSH2_MSG_UNIMPLEMENTED)) != 0 ||
	    (r = sshpkt_put_u32(ssh, seq)) != 0 ||
	    (r = sshpkt_send(ssh)) != 0 ||
	    (r = ssh_packet_write_wait(ssh)) != 0)
		sshpkt_fatal(ssh, r, "%s", __func__);
	return 0;
}

int
dispatch_protocol_ignore(int type, u_int32_t seq, struct ssh *ssh)
{
	logit("dispatch_protocol_ignore: type %d seq %u", type, seq);
	return 0;
}

void
ssh_dispatch_init(struct ssh *ssh, dispatch_fn *dflt)
{
	u_int i;
	for (i = 0; i < DISPATCH_MAX; i++)
		ssh->dispatch[i] = dflt;
}

void
ssh_dispatch_range(struct ssh *ssh, u_int from, u_int to, dispatch_fn *fn)
{
	u_int i;

	for (i = from; i <= to; i++) {
		if (i >= DISPATCH_MAX)
			break;
		ssh->dispatch[i] = fn;
	}
}

void
ssh_dispatch_set(struct ssh *ssh, int type, dispatch_fn *fn)
{
	ssh->dispatch[type] = fn;
}

int
ssh_dispatch_run(struct ssh *ssh, int mode, volatile sig_atomic_t *done)
{
	int r;
	u_char type;
	u_int32_t seqnr;

	for (;;) {
		if (mode == DISPATCH_BLOCK) {
			r = ssh_packet_read_seqnr(ssh, &type, &seqnr);
			if (r != 0)
				return r;
		} else {
			r = ssh_packet_read_poll_seqnr(ssh, &type, &seqnr);
			if (r != 0)
				return r;
			if (type == SSH_MSG_NONE)
				return 0;
		}
		if (type > 0 && type < DISPATCH_MAX &&
		    ssh->dispatch[type] != NULL) {
			if (ssh->dispatch_skip_packets) {
				debug2("skipped packet (type %u)", type);
				ssh->dispatch_skip_packets--;
				continue;
			}
			r = (*ssh->dispatch[type])(type, seqnr, ssh);
			if (r != 0)
				return r;
		} else {
			r = sshpkt_disconnect(ssh,
			    "protocol error: rcvd type %d", type);
			if (r != 0)
				return r;
			return SSH_ERR_DISCONNECTED;
		}
		if (done != NULL && *done)
			return 0;
	}
}

void
ssh_dispatch_run_fatal(struct ssh *ssh, int mode, volatile sig_atomic_t *done)
{
	int r;

	if ((r = ssh_dispatch_run(ssh, mode, done)) != 0)
		sshpkt_fatal(ssh, r, "%s", __func__);
}
