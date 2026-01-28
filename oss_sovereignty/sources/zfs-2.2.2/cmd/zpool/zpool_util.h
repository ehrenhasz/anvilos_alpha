


#ifndef	ZPOOL_UTIL_H
#define	ZPOOL_UTIL_H

#include <libnvpair.h>
#include <libzfs.h>
#include <libzutil.h>

#ifdef	__cplusplus
extern "C" {
#endif


#define	ZPOOL_SCRIPTS_DIR SYSCONFDIR"/zfs/zpool.d"


void *safe_malloc(size_t);
void *safe_realloc(void *, size_t);
void zpool_no_memory(void);
uint_t num_logs(nvlist_t *nv);
uint64_t array64_max(uint64_t array[], unsigned int len);
int highbit64(uint64_t i);
int lowbit64(uint64_t i);


char *zpool_get_cmd_search_path(void);



nvlist_t *make_root_vdev(zpool_handle_t *zhp, nvlist_t *props, int force,
    int check_rep, boolean_t replacing, boolean_t dryrun, int argc,
    char **argv);
nvlist_t *split_mirror_vdev(zpool_handle_t *zhp, char *newname,
    nvlist_t *props, splitflags_t flags, int argc, char **argv);


int for_each_pool(int, char **, boolean_t unavail, zprop_list_t **, zfs_type_t,
    boolean_t, zpool_iter_f, void *);


int for_each_vdev(zpool_handle_t *zhp, pool_vdev_iter_f func, void *data);

typedef struct zpool_list zpool_list_t;

zpool_list_t *pool_list_get(int, char **, zprop_list_t **, zfs_type_t,
    boolean_t, int *);
void pool_list_update(zpool_list_t *);
int pool_list_iter(zpool_list_t *, int unavail, zpool_iter_f, void *);
void pool_list_free(zpool_list_t *);
int pool_list_count(zpool_list_t *);
void pool_list_remove(zpool_list_t *, zpool_handle_t *);

extern libzfs_handle_t *g_zfs;


typedef	struct vdev_cmd_data
{
	char **lines;	
	int lines_cnt;	

	char **cols;	
	int cols_cnt;	


	char *path;	
	char *upath;	
	char *pool;	
	char *cmd;	
	char *vdev_enc_sysfs_path;	
} vdev_cmd_data_t;

typedef struct vdev_cmd_data_list
{
	char *cmd;		
	unsigned int count;	

	
	libzfs_handle_t *g_zfs;
	char **vdev_names;
	int vdev_names_count;
	int cb_name_flags;

	vdev_cmd_data_t *data;	

	
	char **uniq_cols;
	int uniq_cols_cnt;
	int *uniq_cols_width;

} vdev_cmd_data_list_t;

vdev_cmd_data_list_t *all_pools_for_each_vdev_run(int argc, char **argv,
    char *cmd, libzfs_handle_t *g_zfs, char **vdev_names, int vdev_names_count,
    int cb_name_flags);

void free_vdev_cmd_data_list(vdev_cmd_data_list_t *vcdl);

void free_vdev_cmd_data(vdev_cmd_data_t *data);

int vdev_run_cmd_simple(char *path, char *cmd);

int check_device(const char *path, boolean_t force,
    boolean_t isspare, boolean_t iswholedisk);
boolean_t check_sector_size_database(char *path, int *sector_size);
void vdev_error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int check_file(const char *file, boolean_t force, boolean_t isspare);
void after_zpool_upgrade(zpool_handle_t *zhp);
int check_file_generic(const char *file, boolean_t force, boolean_t isspare);

#ifdef	__cplusplus
}
#endif

#endif	
