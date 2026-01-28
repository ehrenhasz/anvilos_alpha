#ifndef _OPENSOLARIS_SYS_FILE_H_
#define	_OPENSOLARIS_SYS_FILE_H_
#include <sys/refcount.h>
#include_next <sys/file.h>
#define	FKIOCTL	0x80000000	 
typedef	struct file	file_t;
#include <sys/capsicum.h>
static __inline file_t *
getf_caps(int fd, cap_rights_t *rightsp)
{
	struct file *fp;
	if (fget(curthread, fd, rightsp, &fp) == 0)
		return (fp);
	return (NULL);
}
#endif	 
