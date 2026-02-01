 

#include "includes.h"

#if defined(WITH_IRIX_PROJECT) || \
    defined(WITH_IRIX_JOBS) || \
    defined(WITH_IRIX_ARRAY)

#include <errno.h>
#include <string.h>
#include <unistd.h>

#ifdef WITH_IRIX_PROJECT
# include <proj.h>
#endif  
#ifdef WITH_IRIX_JOBS
# include <sys/resource.h>
#endif
#ifdef WITH_IRIX_AUDIT
# include <sat.h>
#endif  

#include "log.h"

void
irix_setusercontext(struct passwd *pw)
{
#ifdef WITH_IRIX_PROJECT
	prid_t projid;
#endif
#ifdef WITH_IRIX_JOBS
	jid_t jid = 0;
#elif defined(WITH_IRIX_ARRAY)
	int jid = 0;
#endif

#ifdef WITH_IRIX_JOBS
	jid = jlimit_startjob(pw->pw_name, pw->pw_uid, "interactive");
	if (jid == -1)
		fatal("Failed to create job container: %.100s",
		    strerror(errno));
#endif  
#ifdef WITH_IRIX_ARRAY
	 
	if (jid == 0  && newarraysess() != 0)
		fatal("Failed to set up new array session: %.100s",
		    strerror(errno));
#endif  
#ifdef WITH_IRIX_PROJECT
	 
	if ((projid = getdfltprojuser(pw->pw_name)) == -1) {
		debug("Failed to get project id, using projid 0");
		projid = 0;
	}
	if (setprid(projid))
		fatal("Failed to initialize project %d for %s: %.100s",
		    (int)projid, pw->pw_name, strerror(errno));
#endif  
#ifdef WITH_IRIX_AUDIT
	if (sysconf(_SC_AUDIT)) {
		debug("Setting sat id to %d", (int) pw->pw_uid);
		if (satsetid(pw->pw_uid))
			debug("error setting satid: %.100s", strerror(errno));
	}
#endif  
}


#endif  
