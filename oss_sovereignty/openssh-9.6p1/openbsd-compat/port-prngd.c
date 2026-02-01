 

#include "includes.h"

#include <sys/types.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_UN_H
# include <sys/un.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>  

#include "atomicio.h"
#include "misc.h"
#include "log.h"

#if defined(PRNGD_PORT) || defined(PRNGD_SOCKET)
 
static int
get_random_bytes_prngd(unsigned char *buf, int len,
    unsigned short tcp_port, char *socket_path)
{
	int fd, addr_len, rval, errors;
	u_char msg[2];
	struct sockaddr_storage addr;
	struct sockaddr_in *addr_in = (struct sockaddr_in *)&addr;
	struct sockaddr_un *addr_un = (struct sockaddr_un *)&addr;
	sshsig_t old_sigpipe;

	 
	if (socket_path == NULL && tcp_port == 0)
		fatal("You must specify a port or a socket");
	if (socket_path != NULL &&
	    strlen(socket_path) >= sizeof(addr_un->sun_path))
		fatal("Random pool path is too long");
	if (len <= 0 || len > 255)
		fatal("Too many bytes (%d) to read from PRNGD", len);

	memset(&addr, '\0', sizeof(addr));

	if (tcp_port != 0) {
		addr_in->sin_family = AF_INET;
		addr_in->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		addr_in->sin_port = htons(tcp_port);
		addr_len = sizeof(*addr_in);
	} else {
		addr_un->sun_family = AF_UNIX;
		strlcpy(addr_un->sun_path, socket_path,
		    sizeof(addr_un->sun_path));
		addr_len = offsetof(struct sockaddr_un, sun_path) +
		    strlen(socket_path) + 1;
	}

	old_sigpipe = ssh_signal(SIGPIPE, SIG_IGN);

	errors = 0;
	rval = -1;
reopen:
	fd = socket(addr.ss_family, SOCK_STREAM, 0);
	if (fd == -1) {
		error("Couldn't create socket: %s", strerror(errno));
		goto done;
	}

	if (connect(fd, (struct sockaddr*)&addr, addr_len) == -1) {
		if (tcp_port != 0) {
			error("Couldn't connect to PRNGD port %d: %s",
			    tcp_port, strerror(errno));
		} else {
			error("Couldn't connect to PRNGD socket \"%s\": %s",
			    addr_un->sun_path, strerror(errno));
		}
		goto done;
	}

	 
	msg[0] = 0x02;
	msg[1] = len;

	if (atomicio(vwrite, fd, msg, sizeof(msg)) != sizeof(msg)) {
		if (errno == EPIPE && errors < 10) {
			close(fd);
			errors++;
			goto reopen;
		}
		error("Couldn't write to PRNGD socket: %s",
		    strerror(errno));
		goto done;
	}

	if (atomicio(read, fd, buf, len) != (size_t)len) {
		if (errno == EPIPE && errors < 10) {
			close(fd);
			errors++;
			goto reopen;
		}
		error("Couldn't read from PRNGD socket: %s",
		    strerror(errno));
		goto done;
	}

	rval = 0;
done:
	ssh_signal(SIGPIPE, old_sigpipe);
	if (fd != -1)
		close(fd);
	return rval;
}
#endif  

int
seed_from_prngd(unsigned char *buf, size_t bytes)
{
#ifdef PRNGD_PORT
	debug("trying egd/prngd port %d", PRNGD_PORT);
	if (get_random_bytes_prngd(buf, bytes, PRNGD_PORT, NULL) == 0)
		return 0;
#endif
#ifdef PRNGD_SOCKET
	debug("trying egd/prngd socket %s", PRNGD_SOCKET);
	if (get_random_bytes_prngd(buf, bytes, 0, PRNGD_SOCKET) == 0)
		return 0;
#endif
	return -1;
}
