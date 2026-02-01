 
 

#include "includes.h"

#include <sys/types.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include "log.h"
#include "sftp.h"
#include "misc.h"
#include "xmalloc.h"

void
cleanup_exit(int i)
{
	sftp_server_cleanup_exit(i);
}

int
main(int argc, char **argv)
{
	struct passwd *user_pw;

	 
	sanitise_stdfd();

	if ((user_pw = getpwuid(getuid())) == NULL) {
		fprintf(stderr, "No user found for uid %lu\n",
		    (u_long)getuid());
		return 1;
	}

	return (sftp_server_main(argc, argv, user_pw));
}
