
#include <linux/stringify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs.h"

struct cgroupfs_cache_entry {
	char	subsys[32];
	char	mountpoint[PATH_MAX];
};

 
static struct cgroupfs_cache_entry *cached;

int cgroupfs_find_mountpoint(char *buf, size_t maxlen, const char *subsys)
{
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	char *p, *path;
	char mountpoint[PATH_MAX];

	if (cached && !strcmp(cached->subsys, subsys)) {
		if (strlen(cached->mountpoint) < maxlen) {
			strcpy(buf, cached->mountpoint);
			return 0;
		}
		return -1;
	}

	fp = fopen("/proc/mounts", "r");
	if (!fp)
		return -1;

	 
	mountpoint[0] = '\0';

	 
	while (getline(&line, &len, fp) != -1) {
		 
		p = strchr(line, ' ');
		if (p == NULL)
			continue;

		 
		path = ++p;
		p = strchr(p, ' ');
		if (p == NULL)
			continue;

		*p++ = '\0';

		 
		if (strncmp(p, "cgroup", 6))
			continue;

		if (p[6] == '2') {
			 
			strcpy(mountpoint, path);
			continue;
		}

		 
		p += 7;

		p = strstr(p, subsys);
		if (p == NULL)
			continue;

		 
		if (!strchr(" ,", p[-1]) || !strchr(" ,", p[strlen(subsys)]))
			continue;

		strcpy(mountpoint, path);
		break;
	}
	free(line);
	fclose(fp);

	if (!cached)
		cached = calloc(1, sizeof(*cached));

	if (cached) {
		strncpy(cached->subsys, subsys, sizeof(cached->subsys) - 1);
		strcpy(cached->mountpoint, mountpoint);
	}

	if (mountpoint[0] && strlen(mountpoint) < maxlen) {
		strcpy(buf, mountpoint);
		return 0;
	}
	return -1;
}
