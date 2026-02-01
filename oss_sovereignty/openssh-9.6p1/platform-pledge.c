 

#include "includes.h"

#include <sys/types.h>

#include <stdarg.h>
#include <unistd.h>

#include "platform.h"

#include "openbsd-compat/openbsd-compat.h"

 
void
platform_pledge_agent(void)
{
#ifdef USE_SOLARIS_PRIVS
	 
	solaris_drop_privs_root_pinfo_net();
#endif
}

 
void
platform_pledge_sftp_server(void)
{
#ifdef USE_SOLARIS_PRIVS
	solaris_drop_privs_pinfo_net_fork_exec();
#endif
}

 
void
platform_pledge_mux(void)
{
#ifdef USE_SOLARIS_PRIVS
	solaris_drop_privs_root_pinfo_net_exec();
#endif
}
