 
 

 

 

#include "includes.h"

#include <sys/types.h>

#include <errno.h>
#include <string.h>
#include <termios.h>
#include <stdarg.h>

#include "packet.h"
#include "log.h"
#include "compat.h"
#include "sshbuf.h"
#include "ssherr.h"

#define TTY_OP_END		0
 
#define TTY_OP_ISPEED	128
#define TTY_OP_OSPEED	129

 
static int
speed_to_baud(speed_t speed)
{
	switch (speed) {
	case B0:
		return 0;
	case B50:
		return 50;
	case B75:
		return 75;
	case B110:
		return 110;
	case B134:
		return 134;
	case B150:
		return 150;
	case B200:
		return 200;
	case B300:
		return 300;
	case B600:
		return 600;
	case B1200:
		return 1200;
	case B1800:
		return 1800;
	case B2400:
		return 2400;
	case B4800:
		return 4800;
	case B9600:
		return 9600;

#ifdef B19200
	case B19200:
		return 19200;
#else  
#ifdef EXTA
	case EXTA:
		return 19200;
#endif  
#endif  

#ifdef B38400
	case B38400:
		return 38400;
#else  
#ifdef EXTB
	case EXTB:
		return 38400;
#endif  
#endif  

#ifdef B7200
	case B7200:
		return 7200;
#endif  
#ifdef B14400
	case B14400:
		return 14400;
#endif  
#ifdef B28800
	case B28800:
		return 28800;
#endif  
#ifdef B57600
	case B57600:
		return 57600;
#endif  
#ifdef B76800
	case B76800:
		return 76800;
#endif  
#ifdef B115200
	case B115200:
		return 115200;
#endif  
#ifdef B230400
	case B230400:
		return 230400;
#endif  
	default:
		return 9600;
	}
}

 
static speed_t
baud_to_speed(int baud)
{
	switch (baud) {
	case 0:
		return B0;
	case 50:
		return B50;
	case 75:
		return B75;
	case 110:
		return B110;
	case 134:
		return B134;
	case 150:
		return B150;
	case 200:
		return B200;
	case 300:
		return B300;
	case 600:
		return B600;
	case 1200:
		return B1200;
	case 1800:
		return B1800;
	case 2400:
		return B2400;
	case 4800:
		return B4800;
	case 9600:
		return B9600;

#ifdef B19200
	case 19200:
		return B19200;
#else  
#ifdef EXTA
	case 19200:
		return EXTA;
#endif  
#endif  

#ifdef B38400
	case 38400:
		return B38400;
#else  
#ifdef EXTB
	case 38400:
		return EXTB;
#endif  
#endif  

#ifdef B7200
	case 7200:
		return B7200;
#endif  
#ifdef B14400
	case 14400:
		return B14400;
#endif  
#ifdef B28800
	case 28800:
		return B28800;
#endif  
#ifdef B57600
	case 57600:
		return B57600;
#endif  
#ifdef B76800
	case 76800:
		return B76800;
#endif  
#ifdef B115200
	case 115200:
		return B115200;
#endif  
#ifdef B230400
	case 230400:
		return B230400;
#endif  
	default:
		return B9600;
	}
}

 
static u_int
special_char_encode(cc_t c)
{
#ifdef _POSIX_VDISABLE
	if (c == _POSIX_VDISABLE)
		return 255;
#endif  
	return c;
}

 
static cc_t
special_char_decode(u_int c)
{
#ifdef _POSIX_VDISABLE
	if (c == 255)
		return _POSIX_VDISABLE;
#endif  
	return c;
}

 
void
ssh_tty_make_modes(struct ssh *ssh, int fd, struct termios *tiop)
{
	struct termios tio;
	struct sshbuf *buf;
	int r, ibaud, obaud;

	if ((buf = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");

	if (tiop == NULL) {
		if (fd == -1) {
			debug_f("no fd or tio");
			goto end;
		}
		if (tcgetattr(fd, &tio) == -1) {
			logit("tcgetattr: %.100s", strerror(errno));
			goto end;
		}
	} else
		tio = *tiop;

	 
	obaud = speed_to_baud(cfgetospeed(&tio));
	ibaud = speed_to_baud(cfgetispeed(&tio));
	if ((r = sshbuf_put_u8(buf, TTY_OP_OSPEED)) != 0 ||
	    (r = sshbuf_put_u32(buf, obaud)) != 0 ||
	    (r = sshbuf_put_u8(buf, TTY_OP_ISPEED)) != 0 ||
	    (r = sshbuf_put_u32(buf, ibaud)) != 0)
		fatal_fr(r, "compose");

	 
#define TTYCHAR(NAME, OP) \
	if ((r = sshbuf_put_u8(buf, OP)) != 0 || \
	    (r = sshbuf_put_u32(buf, \
	    special_char_encode(tio.c_cc[NAME]))) != 0) \
		fatal_fr(r, "compose %s", #NAME);

#define SSH_TTYMODE_IUTF8 42   

#define TTYMODE(NAME, FIELD, OP) \
	if (OP == SSH_TTYMODE_IUTF8 && (ssh->compat & SSH_BUG_UTF8TTYMODE)) { \
		debug3_f("SSH_BUG_UTF8TTYMODE"); \
	} else if ((r = sshbuf_put_u8(buf, OP)) != 0 || \
	    (r = sshbuf_put_u32(buf, ((tio.FIELD & NAME) != 0))) != 0) \
		fatal_fr(r, "compose %s", #NAME);

#include "ttymodes.h"

#undef TTYCHAR
#undef TTYMODE

end:
	 
	if ((r = sshbuf_put_u8(buf, TTY_OP_END)) != 0 ||
	    (r = sshpkt_put_stringb(ssh, buf)) != 0)
		fatal_fr(r, "compose end");
	sshbuf_free(buf);
}

 
void
ssh_tty_parse_modes(struct ssh *ssh, int fd)
{
	struct termios tio;
	struct sshbuf *buf;
	const u_char *data;
	u_char opcode;
	u_int baud, u;
	int r, failure = 0;
	size_t len;

	if ((r = sshpkt_get_string_direct(ssh, &data, &len)) != 0)
		fatal_fr(r, "parse");
	if (len == 0)
		return;
	if ((buf = sshbuf_from(data, len)) == NULL) {
		error_f("sshbuf_from failed");
		return;
	}

	 
	if (tcgetattr(fd, &tio) == -1) {
		logit("tcgetattr: %.100s", strerror(errno));
		failure = -1;
	}

	while (sshbuf_len(buf) > 0) {
		if ((r = sshbuf_get_u8(buf, &opcode)) != 0)
			fatal_fr(r, "parse opcode");
		switch (opcode) {
		case TTY_OP_END:
			goto set;

		case TTY_OP_ISPEED:
			if ((r = sshbuf_get_u32(buf, &baud)) != 0)
				fatal_fr(r, "parse ispeed");
			if (failure != -1 &&
			    cfsetispeed(&tio, baud_to_speed(baud)) == -1)
				error("cfsetispeed failed for %d", baud);
			break;

		case TTY_OP_OSPEED:
			if ((r = sshbuf_get_u32(buf, &baud)) != 0)
				fatal_fr(r, "parse ospeed");
			if (failure != -1 &&
			    cfsetospeed(&tio, baud_to_speed(baud)) == -1)
				error("cfsetospeed failed for %d", baud);
			break;

#define TTYCHAR(NAME, OP) \
		case OP: \
			if ((r = sshbuf_get_u32(buf, &u)) != 0) \
				fatal_fr(r, "parse %s", #NAME); \
			tio.c_cc[NAME] = special_char_decode(u); \
			break;
#define TTYMODE(NAME, FIELD, OP) \
		case OP: \
			if ((r = sshbuf_get_u32(buf, &u)) != 0) \
				fatal_fr(r, "parse %s", #NAME); \
			if (u) \
				tio.FIELD |= NAME; \
			else \
				tio.FIELD &= ~NAME; \
			break;

#include "ttymodes.h"

#undef TTYCHAR
#undef TTYMODE

		default:
			debug("Ignoring unsupported tty mode opcode %d (0x%x)",
			    opcode, opcode);
			 
			if (opcode > 0 && opcode < 160) {
				if ((r = sshbuf_get_u32(buf, NULL)) != 0)
					fatal_fr(r, "parse arg");
				break;
			} else {
				logit_f("unknown opcode %d", opcode);
				goto set;
			}
		}
	}

set:
	len = sshbuf_len(buf);
	sshbuf_free(buf);
	if (len > 0) {
		logit_f("%zu bytes left", len);
		return;		 
	}
	if (failure == -1)
		return;		 

	 
	if (tcsetattr(fd, TCSANOW, &tio) == -1)
		logit("Setting tty modes failed: %.100s", strerror(errno));
}
