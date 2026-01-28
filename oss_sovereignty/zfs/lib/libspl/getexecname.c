#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libspl_impl.h"
const char *
getexecname(void)
{
	static char execname[PATH_MAX + 1] = "";
	static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
	char *ptr = execname;
	ssize_t rc;
	(void) pthread_mutex_lock(&mtx);
	if (strlen(execname) == 0) {
		rc = getexecname_impl(execname);
		if (rc == -1) {
			execname[0] = '\0';
			ptr = NULL;
		} else {
			execname[rc] = '\0';
		}
	}
	(void) pthread_mutex_unlock(&mtx);
	return (ptr);
}
