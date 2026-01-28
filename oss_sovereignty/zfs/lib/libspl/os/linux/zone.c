#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <zone.h>
zoneid_t
getzoneid(void)
{
	char path[PATH_MAX];
	char buf[128] = { '\0' };
	char *cp;
	int c = snprintf(path, sizeof (path), "/proc/self/ns/user");
	if (c < 0 || c >= sizeof (path))
		return (0);
	ssize_t r = readlink(path, buf, sizeof (buf) - 1);
	if (r < 0)
		return (0);
	cp = strchr(buf, '[');
	if (cp == NULL)
		return (0);
	cp++;
	unsigned long n = strtoul(cp, NULL, 10);
	if (n == ULONG_MAX && errno == ERANGE)
		return (0);
	zoneid_t z = (zoneid_t)n;
	return (z);
}
