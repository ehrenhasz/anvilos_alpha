 
 

#include "includes.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "ssherr.h"
#include "sshbuf.h"
#include "atomicio.h"

 
int
sshbuf_load_fd(int fd, struct sshbuf **blobp)
{
	u_char buf[4096];
	size_t len;
	struct stat st;
	int r;
	struct sshbuf *blob;

	*blobp = NULL;

	if (fstat(fd, &st) == -1)
		return SSH_ERR_SYSTEM_ERROR;
	if ((st.st_mode & (S_IFSOCK|S_IFCHR|S_IFIFO)) == 0 &&
	    st.st_size > SSHBUF_SIZE_MAX)
		return SSH_ERR_INVALID_FORMAT;
	if ((blob = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	for (;;) {
		if ((len = atomicio(read, fd, buf, sizeof(buf))) == 0) {
			if (errno == EPIPE)
				break;
			r = SSH_ERR_SYSTEM_ERROR;
			goto out;
		}
		if ((r = sshbuf_put(blob, buf, len)) != 0)
			goto out;
		if (sshbuf_len(blob) > SSHBUF_SIZE_MAX) {
			r = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
	}
	if ((st.st_mode & (S_IFSOCK|S_IFCHR|S_IFIFO)) == 0 &&
	    st.st_size != (off_t)sshbuf_len(blob)) {
		r = SSH_ERR_FILE_CHANGED;
		goto out;
	}
	 
	*blobp = blob;
	blob = NULL;  
	r = 0;
 out:
	explicit_bzero(buf, sizeof(buf));
	sshbuf_free(blob);
	return r;
}

int
sshbuf_load_file(const char *path, struct sshbuf **bufp)
{
	int r, fd, oerrno;

	*bufp = NULL;
	if ((fd = open(path, O_RDONLY)) == -1)
		return SSH_ERR_SYSTEM_ERROR;
	if ((r = sshbuf_load_fd(fd, bufp)) != 0)
		goto out;
	 
	r = 0;
 out:
	oerrno = errno;
	close(fd);
	if (r != 0)
		errno = oerrno;
	return r;
}

int
sshbuf_write_file(const char *path, struct sshbuf *buf)
{
	int fd, oerrno;

	if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1)
		return SSH_ERR_SYSTEM_ERROR;
	if (atomicio(vwrite, fd, sshbuf_mutable_ptr(buf),
	    sshbuf_len(buf)) != sshbuf_len(buf) || close(fd) != 0) {
		oerrno = errno;
		close(fd);
		unlink(path);
		errno = oerrno;
		return SSH_ERR_SYSTEM_ERROR;
	}
	return 0;
}

