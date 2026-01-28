#ifndef __API_IO__
#define __API_IO__
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/types.h>
struct io {
	int fd;
	unsigned int buf_len;
	char *buf;
	char *end;
	char *data;
	int timeout_ms;
	bool eof;
};
static inline void io__init(struct io *io, int fd,
			    char *buf, unsigned int buf_len)
{
	io->fd = fd;
	io->buf_len = buf_len;
	io->buf = buf;
	io->end = buf;
	io->data = buf;
	io->timeout_ms = 0;
	io->eof = false;
}
static inline int io__get_char(struct io *io)
{
	char *ptr = io->data;
	if (io->eof)
		return -1;
	if (ptr == io->end) {
		ssize_t n;
		if (io->timeout_ms != 0) {
			struct pollfd pfds[] = {
				{
					.fd = io->fd,
					.events = POLLIN,
				},
			};
			n = poll(pfds, 1, io->timeout_ms);
			if (n == 0)
				errno = ETIMEDOUT;
			if (n > 0 && !(pfds[0].revents & POLLIN)) {
				errno = EIO;
				n = -1;
			}
			if (n <= 0) {
				io->eof = true;
				return -1;
			}
		}
		n = read(io->fd, io->buf, io->buf_len);
		if (n <= 0) {
			io->eof = true;
			return -1;
		}
		ptr = &io->buf[0];
		io->end = &io->buf[n];
	}
	io->data = ptr + 1;
	return *ptr;
}
static inline int io__get_hex(struct io *io, __u64 *hex)
{
	bool first_read = true;
	*hex = 0;
	while (true) {
		int ch = io__get_char(io);
		if (ch < 0)
			return ch;
		if (ch >= '0' && ch <= '9')
			*hex = (*hex << 4) | (ch - '0');
		else if (ch >= 'a' && ch <= 'f')
			*hex = (*hex << 4) | (ch - 'a' + 10);
		else if (ch >= 'A' && ch <= 'F')
			*hex = (*hex << 4) | (ch - 'A' + 10);
		else if (first_read)
			return -2;
		else
			return ch;
		first_read = false;
	}
}
static inline int io__get_dec(struct io *io, __u64 *dec)
{
	bool first_read = true;
	*dec = 0;
	while (true) {
		int ch = io__get_char(io);
		if (ch < 0)
			return ch;
		if (ch >= '0' && ch <= '9')
			*dec = (*dec * 10) + ch - '0';
		else if (first_read)
			return -2;
		else
			return ch;
		first_read = false;
	}
}
static inline ssize_t io__getline(struct io *io, char **line_out, size_t *line_len_out)
{
	char buf[128];
	int buf_pos = 0;
	char *line = NULL, *temp;
	size_t line_len = 0;
	int ch = 0;
	free(*line_out);
	while (ch != '\n') {
		ch = io__get_char(io);
		if (ch < 0)
			break;
		if (buf_pos == sizeof(buf)) {
			temp = realloc(line, line_len + sizeof(buf));
			if (!temp)
				goto err_out;
			line = temp;
			memcpy(&line[line_len], buf, sizeof(buf));
			line_len += sizeof(buf);
			buf_pos = 0;
		}
		buf[buf_pos++] = (char)ch;
	}
	temp = realloc(line, line_len + buf_pos + 1);
	if (!temp)
		goto err_out;
	line = temp;
	memcpy(&line[line_len], buf, buf_pos);
	line[line_len + buf_pos] = '\0';
	line_len += buf_pos;
	*line_out = line;
	*line_len_out = line_len;
	return line_len;
err_out:
	free(line);
	return -ENOMEM;
}
#endif  
