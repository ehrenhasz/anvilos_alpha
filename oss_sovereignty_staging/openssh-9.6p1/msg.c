 
 

#include "includes.h"

#include <sys/types.h>
#include <sys/uio.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include "sshbuf.h"
#include "ssherr.h"
#include "log.h"
#include "atomicio.h"
#include "msg.h"
#include "misc.h"

int
ssh_msg_send(int fd, u_char type, struct sshbuf *m)
{
	u_char buf[5];
	u_int mlen = sshbuf_len(m);

	debug3_f("type %u", (unsigned int)type & 0xff);

	put_u32(buf, mlen + 1);
	buf[4] = type;		 
	if (atomicio(vwrite, fd, buf, sizeof(buf)) != sizeof(buf)) {
		error_f("write: %s", strerror(errno));
		return (-1);
	}
	if (atomicio(vwrite, fd, sshbuf_mutable_ptr(m), mlen) != mlen) {
		error_f("write: %s", strerror(errno));
		return (-1);
	}
	return (0);
}

int
ssh_msg_recv(int fd, struct sshbuf *m)
{
	u_char buf[4], *p;
	u_int msg_len;
	int r;

	debug3("ssh_msg_recv entering");

	if (atomicio(read, fd, buf, sizeof(buf)) != sizeof(buf)) {
		if (errno != EPIPE)
			error_f("read header: %s", strerror(errno));
		return (-1);
	}
	msg_len = get_u32(buf);
	if (msg_len > sshbuf_max_size(m)) {
		error_f("read: bad msg_len %u", msg_len);
		return (-1);
	}
	sshbuf_reset(m);
	if ((r = sshbuf_reserve(m, msg_len, &p)) != 0) {
		error_fr(r, "reserve");
		return -1;
	}
	if (atomicio(read, fd, p, msg_len) != msg_len) {
		error_f("read: %s", strerror(errno));
		return (-1);
	}
	return (0);
}
