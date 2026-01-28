#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <zone.h>
zoneid_t
getzoneid(void)
{
	size_t size;
	int jailid;
	size = sizeof (jailid);
	if (sysctlbyname("security.jail.jailed", &jailid, &size, NULL, 0) == -1)
		assert(!"No security.jail.jailed sysctl!");
	return ((zoneid_t)jailid);
}
