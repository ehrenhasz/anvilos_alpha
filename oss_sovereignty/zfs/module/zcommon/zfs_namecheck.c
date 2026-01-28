#if !defined(_KERNEL)
#include <string.h>
#endif
#include <sys/dsl_dir.h>
#include <sys/param.h>
#include <sys/nvpair.h>
#include "zfs_namecheck.h"
#include "zfs_deleg.h"
int zfs_max_dataset_nesting = 50;
static int
valid_char(char c)
{
	return ((c >= 'a' && c <= 'z') ||
	    (c >= 'A' && c <= 'Z') ||
	    (c >= '0' && c <= '9') ||
	    c == '-' || c == '_' || c == '.' || c == ':' || c == ' ');
}
int
get_dataset_depth(const char *path)
{
	const char *loc = path;
	int nesting = 0;
	for (int i = 0; loc[i] != '\0' &&
	    loc[i] != '@' &&
	    loc[i] != '#'; i++) {
		if (loc[i] == '/')
			nesting++;
	}
	return (nesting);
}
int
zfs_component_namecheck(const char *path, namecheck_err_t *why, char *what)
{
	const char *loc;
	if (strlen(path) >= ZFS_MAX_DATASET_NAME_LEN) {
		if (why)
			*why = NAME_ERR_TOOLONG;
		return (-1);
	}
	if (path[0] == '\0') {
		if (why)
			*why = NAME_ERR_EMPTY_COMPONENT;
		return (-1);
	}
	for (loc = path; *loc; loc++) {
		if (!valid_char(*loc)) {
			if (why) {
				*why = NAME_ERR_INVALCHAR;
				*what = *loc;
			}
			return (-1);
		}
	}
	return (0);
}
int
permset_namecheck(const char *path, namecheck_err_t *why, char *what)
{
	if (strlen(path) >= ZFS_PERMSET_MAXLEN) {
		if (why)
			*why = NAME_ERR_TOOLONG;
		return (-1);
	}
	if (path[0] != '@') {
		if (why) {
			*why = NAME_ERR_NO_AT;
			*what = path[0];
		}
		return (-1);
	}
	return (zfs_component_namecheck(&path[1], why, what));
}
int
dataset_nestcheck(const char *path)
{
	return ((get_dataset_depth(path) < zfs_max_dataset_nesting) ? 0 : -1);
}
int
entity_namecheck(const char *path, namecheck_err_t *why, char *what)
{
	const char *end;
	EQUIV(why == NULL, what == NULL);
	if (strlen(path) >= ZFS_MAX_DATASET_NAME_LEN) {
		if (why)
			*why = NAME_ERR_TOOLONG;
		return (-1);
	}
	if (path[0] == '/') {
		if (why)
			*why = NAME_ERR_LEADING_SLASH;
		return (-1);
	}
	if (path[0] == '\0') {
		if (why)
			*why = NAME_ERR_EMPTY_COMPONENT;
		return (-1);
	}
	const char *start = path;
	boolean_t found_delim = B_FALSE;
	for (;;) {
		end = start;
		while (*end != '/' && *end != '@' && *end != '#' &&
		    *end != '\0')
			end++;
		if (*end == '\0' && end[-1] == '/') {
			if (why)
				*why = NAME_ERR_TRAILING_SLASH;
			return (-1);
		}
		for (const char *loc = start; loc != end; loc++) {
			if (!valid_char(*loc) && *loc != '%') {
				if (why) {
					*why = NAME_ERR_INVALCHAR;
					*what = *loc;
				}
				return (-1);
			}
		}
		if (*end == '\0' || *end == '/') {
			int component_length = end - start;
			if (component_length == 1) {
				if (start[0] == '.') {
					if (why)
						*why = NAME_ERR_SELF_REF;
					return (-1);
				}
			}
			if (component_length == 2) {
				if (start[0] == '.' && start[1] == '.') {
					if (why)
						*why = NAME_ERR_PARENT_REF;
					return (-1);
				}
			}
		}
		if (*end == '@' || *end == '#') {
			if (found_delim != 0) {
				if (why)
					*why = NAME_ERR_MULTIPLE_DELIMITERS;
				return (-1);
			}
			found_delim = B_TRUE;
		}
		if (start == end) {
			if (why)
				*why = NAME_ERR_EMPTY_COMPONENT;
			return (-1);
		}
		if (*end == '\0')
			return (0);
		if (*end == '/' && found_delim != 0) {
			if (why)
				*why = NAME_ERR_TRAILING_SLASH;
			return (-1);
		}
		start = end + 1;
	}
}
int
dataset_namecheck(const char *path, namecheck_err_t *why, char *what)
{
	int ret = entity_namecheck(path, why, what);
	if (ret == 0 && strchr(path, '#') != NULL) {
		if (why != NULL) {
			*why = NAME_ERR_INVALCHAR;
			*what = '#';
		}
		return (-1);
	}
	return (ret);
}
int
bookmark_namecheck(const char *path, namecheck_err_t *why, char *what)
{
	int ret = entity_namecheck(path, why, what);
	if (ret == 0 && strchr(path, '#') == NULL) {
		if (why != NULL) {
			*why = NAME_ERR_NO_POUND;
			*what = '#';
		}
		return (-1);
	}
	return (ret);
}
int
snapshot_namecheck(const char *path, namecheck_err_t *why, char *what)
{
	int ret = entity_namecheck(path, why, what);
	if (ret == 0 && strchr(path, '@') == NULL) {
		if (why != NULL) {
			*why = NAME_ERR_NO_AT;
			*what = '@';
		}
		return (-1);
	}
	return (ret);
}
int
mountpoint_namecheck(const char *path, namecheck_err_t *why)
{
	const char *start, *end;
	if (path == NULL || *path != '/') {
		if (why)
			*why = NAME_ERR_LEADING_SLASH;
		return (-1);
	}
	start = &path[1];
	do {
		end = start;
		while (*end != '/' && *end != '\0')
			end++;
		if (end - start >= ZFS_MAX_DATASET_NAME_LEN) {
			if (why)
				*why = NAME_ERR_TOOLONG;
			return (-1);
		}
		start = end + 1;
	} while (*end != '\0');
	return (0);
}
int
pool_namecheck(const char *pool, namecheck_err_t *why, char *what)
{
	const char *c;
	if (strlen(pool) >= (ZFS_MAX_DATASET_NAME_LEN - 2 -
	    strlen(ORIGIN_DIR_NAME) * 2)) {
		if (why)
			*why = NAME_ERR_TOOLONG;
		return (-1);
	}
	c = pool;
	while (*c != '\0') {
		if (!valid_char(*c)) {
			if (why) {
				*why = NAME_ERR_INVALCHAR;
				*what = *c;
			}
			return (-1);
		}
		c++;
	}
	if (!(*pool >= 'a' && *pool <= 'z') &&
	    !(*pool >= 'A' && *pool <= 'Z')) {
		if (why)
			*why = NAME_ERR_NOLETTER;
		return (-1);
	}
	if (strcmp(pool, "mirror") == 0 ||
	    strcmp(pool, "raidz") == 0 ||
	    strcmp(pool, "draid") == 0) {
		if (why)
			*why = NAME_ERR_RESERVED;
		return (-1);
	}
	return (0);
}
EXPORT_SYMBOL(entity_namecheck);
EXPORT_SYMBOL(pool_namecheck);
EXPORT_SYMBOL(dataset_namecheck);
EXPORT_SYMBOL(bookmark_namecheck);
EXPORT_SYMBOL(snapshot_namecheck);
EXPORT_SYMBOL(zfs_component_namecheck);
EXPORT_SYMBOL(dataset_nestcheck);
EXPORT_SYMBOL(get_dataset_depth);
EXPORT_SYMBOL(zfs_max_dataset_nesting);
ZFS_MODULE_PARAM(zfs, zfs_, max_dataset_nesting, INT, ZMOD_RW,
	"Limit to the amount of nesting a path can have. Defaults to 50.");
