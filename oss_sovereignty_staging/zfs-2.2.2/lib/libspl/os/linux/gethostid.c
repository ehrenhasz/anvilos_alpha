 
 

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/systeminfo.h>

static unsigned long
get_spl_hostid(void)
{
	FILE *f;
	unsigned long hostid;
	char *env;

	 
	env = getenv("ZFS_HOSTID");
	if (env)
		return (strtoull(env, NULL, 0));

	f = fopen("/proc/sys/kernel/spl/hostid", "re");
	if (!f)
		return (0);

	if (fscanf(f, "%lx", &hostid) != 1)
		hostid = 0;

	fclose(f);

	return (hostid);
}

unsigned long
get_system_hostid(void)
{
	unsigned long hostid = get_spl_hostid();
	uint32_t system_hostid;

	 
	if (hostid == 0) {
		int fd = open("/etc/hostid", O_RDONLY | O_CLOEXEC);
		if (fd >= 0) {
			if (read(fd, &system_hostid, sizeof (system_hostid))
			    != sizeof (system_hostid))
				hostid = 0;
			else
				hostid = system_hostid;
			(void) close(fd);
		}
	}

	return (hostid & HOSTID_MASK);
}
