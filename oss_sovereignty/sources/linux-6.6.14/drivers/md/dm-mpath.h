


#ifndef	DM_MPATH_H
#define	DM_MPATH_H

struct dm_dev;

struct dm_path {
	struct dm_dev *dev;	
	void *pscontext;	
};


void dm_pg_init_complete(struct dm_path *path, unsigned int err_flags);

#endif
