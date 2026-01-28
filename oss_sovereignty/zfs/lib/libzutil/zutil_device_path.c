#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libzutil.h>
const char *
zfs_basename(const char *path)
{
	const char *bn = strrchr(path, '/');
	return (bn ? bn + 1 : path);
}
ssize_t
zfs_dirnamelen(const char *path)
{
	const char *end = strrchr(path, '/');
	return (end ? end - path : -1);
}
int
zfs_resolve_shortname(const char *name, char *path, size_t len)
{
	const char *env = getenv("ZPOOL_IMPORT_PATH");
	if (env) {
		for (;;) {
			env += strspn(env, ":");
			size_t dirlen = strcspn(env, ":");
			if (dirlen) {
				(void) snprintf(path, len, "%.*s/%s",
				    (int)dirlen, env, name);
				if (access(path, F_OK) == 0)
					return (0);
				env += dirlen;
			} else
				break;
		}
	} else {
		size_t count;
		const char *const *zpool_default_import_path =
		    zpool_default_search_paths(&count);
		for (size_t i = 0; i < count; ++i) {
			(void) snprintf(path, len, "%s/%s",
			    zpool_default_import_path[i], name);
			if (access(path, F_OK) == 0)
				return (0);
		}
	}
	return (errno = ENOENT);
}
static int
zfs_strcmp_shortname(const char *name, const char *cmp_name, int wholedisk)
{
	int path_len, cmp_len, i = 0, error = ENOENT;
	char *dir, *env, *envdup = NULL, *tmp = NULL;
	char path_name[MAXPATHLEN];
	const char *const *zpool_default_import_path = NULL;
	size_t count;
	cmp_len = strlen(cmp_name);
	env = getenv("ZPOOL_IMPORT_PATH");
	if (env) {
		envdup = strdup(env);
		dir = strtok_r(envdup, ":", &tmp);
	} else {
		zpool_default_import_path = zpool_default_search_paths(&count);
		dir = (char *)zpool_default_import_path[i];
	}
	while (dir) {
		if (env) {
			while (dir[strlen(dir)-1] == '/')
				dir[strlen(dir)-1] = '\0';
		}
		path_len = snprintf(path_name, MAXPATHLEN, "%s/%s", dir, name);
		if (wholedisk)
			path_len = zfs_append_partition(path_name, MAXPATHLEN);
		if ((path_len == cmp_len) && strcmp(path_name, cmp_name) == 0) {
			error = 0;
			break;
		}
		if (env) {
			dir = strtok_r(NULL, ":", &tmp);
		} else if (++i < count) {
			dir = (char *)zpool_default_import_path[i];
		} else {
			dir = NULL;
		}
	}
	if (env)
		free(envdup);
	return (error);
}
int
zfs_strcmp_pathname(const char *name, const char *cmp, int wholedisk)
{
	int path_len, cmp_len;
	char path_name[MAXPATHLEN];
	char cmp_name[MAXPATHLEN];
	char *dir, *tmp = NULL;
	cmp_name[0] = '\0';
	(void) strlcpy(path_name, cmp, sizeof (path_name));
	for (dir = strtok_r(path_name, "/", &tmp);
	    dir != NULL;
	    dir = strtok_r(NULL, "/", &tmp)) {
		strlcat(cmp_name, "/", sizeof (cmp_name));
		strlcat(cmp_name, dir, sizeof (cmp_name));
	}
	if (name[0] != '/')
		return (zfs_strcmp_shortname(name, cmp_name, wholedisk));
	(void) strlcpy(path_name, name, MAXPATHLEN);
	path_len = strlen(path_name);
	cmp_len = strlen(cmp_name);
	if (wholedisk) {
		path_len = zfs_append_partition(path_name, MAXPATHLEN);
		if (path_len == -1)
			return (ENOMEM);
	}
	if ((path_len != cmp_len) || strcmp(path_name, cmp_name))
		return (ENOENT);
	return (0);
}
