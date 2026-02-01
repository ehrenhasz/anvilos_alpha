 

#include "includes.h"

#if !defined(HAVE_GETPEEREID)

#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h>

#if defined(SO_PEERCRED)
int
getpeereid(int s, uid_t *euid, gid_t *gid)
{
	struct ucred cred;
	socklen_t len = sizeof(cred);

	if (getsockopt(s, SOL_SOCKET, SO_PEERCRED, &cred, &len) < 0)
		return (-1);
	*euid = cred.uid;
	*gid = cred.gid;

	return (0);
}
#elif defined(HAVE_GETPEERUCRED)

#ifdef HAVE_UCRED_H
# include <ucred.h>
#endif

int
getpeereid(int s, uid_t *euid, gid_t *gid)
{
	ucred_t *ucred = NULL;

	if (getpeerucred(s, &ucred) == -1)
		return (-1);
	if ((*euid = ucred_geteuid(ucred)) == -1)
		return (-1);
	if ((*gid = ucred_getrgid(ucred)) == -1)
		return (-1);

	ucred_free(ucred);

	return (0);
}
#else
int
getpeereid(int s, uid_t *euid, gid_t *gid)
{
	*euid = geteuid();
	*gid = getgid();

	return (0);
}
#endif  

#endif  
