#ifndef	ZED_CONF_H
#define	ZED_CONF_H
#include <libzfs.h>
#include <stdint.h>
#include "zed_strings.h"
struct zed_conf {
	char		*pid_file;		 
	char		*zedlet_dir;		 
	char		*state_file;		 
	libzfs_handle_t	*zfs_hdl;		 
	zed_strings_t	*zedlets;		 
	char		*path;		 
	int		pid_fd;			 
	int		state_fd;		 
	int		zevent_fd;		 
	int16_t max_jobs;		 
	int32_t max_zevent_buf_len;	 
	boolean_t	do_force:1;		 
	boolean_t	do_foreground:1;	 
	boolean_t	do_memlock:1;		 
	boolean_t	do_verbose:1;		 
	boolean_t	do_zero:1;		 
	boolean_t	do_idle:1;		 
};
void zed_conf_init(struct zed_conf *zcp);
void zed_conf_destroy(struct zed_conf *zcp);
void zed_conf_parse_opts(struct zed_conf *zcp, int argc, char **argv);
int zed_conf_scan_dir(struct zed_conf *zcp);
int zed_conf_write_pid(struct zed_conf *zcp);
int zed_conf_open_state(struct zed_conf *zcp);
int zed_conf_read_state(struct zed_conf *zcp, uint64_t *eidp, int64_t etime[]);
int zed_conf_write_state(struct zed_conf *zcp, uint64_t eid, int64_t etime[]);
#endif	 
