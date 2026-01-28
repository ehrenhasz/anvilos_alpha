#ifndef	DM_PATH_SELECTOR_H
#define	DM_PATH_SELECTOR_H
#include <linux/device-mapper.h>
#include "dm-mpath.h"
struct path_selector_type;
struct path_selector {
	struct path_selector_type *type;
	void *context;
};
#define DM_PS_USE_HR_TIMER		0x00000001
#define dm_ps_use_hr_timer(type)	((type)->features & DM_PS_USE_HR_TIMER)
struct path_selector_type {
	char *name;
	struct module *module;
	unsigned int features;
	unsigned int table_args;
	unsigned int info_args;
	int (*create)(struct path_selector *ps, unsigned int argc, char **argv);
	void (*destroy)(struct path_selector *ps);
	int (*add_path)(struct path_selector *ps, struct dm_path *path,
			int argc, char **argv, char **error);
	struct dm_path *(*select_path)(struct path_selector *ps, size_t nr_bytes);
	void (*fail_path)(struct path_selector *ps, struct dm_path *p);
	int (*reinstate_path)(struct path_selector *ps, struct dm_path *p);
	int (*status)(struct path_selector *ps, struct dm_path *path,
		      status_type_t type, char *result, unsigned int maxlen);
	int (*start_io)(struct path_selector *ps, struct dm_path *path,
			size_t nr_bytes);
	int (*end_io)(struct path_selector *ps, struct dm_path *path,
		      size_t nr_bytes, u64 start_time);
};
int dm_register_path_selector(struct path_selector_type *type);
int dm_unregister_path_selector(struct path_selector_type *type);
struct path_selector_type *dm_get_path_selector(const char *name);
void dm_put_path_selector(struct path_selector_type *pst);
#endif
