 
 

#ifndef _TRACEFS_H_
#define _TRACEFS_H_

#include <linux/fs.h>
#include <linux/seq_file.h>

#include <linux/types.h>

struct file_operations;

#ifdef CONFIG_TRACING

struct eventfs_file;

struct dentry *eventfs_create_events_dir(const char *name,
					 struct dentry *parent);

struct eventfs_file *eventfs_add_subsystem_dir(const char *name,
					       struct dentry *parent);

struct eventfs_file *eventfs_add_dir(const char *name,
				     struct eventfs_file *ef_parent);

int eventfs_add_file(const char *name, umode_t mode,
		     struct eventfs_file *ef_parent, void *data,
		     const struct file_operations *fops);

int eventfs_add_events_file(const char *name, umode_t mode,
			 struct dentry *parent, void *data,
			 const struct file_operations *fops);

void eventfs_remove(struct eventfs_file *ef);

void eventfs_remove_events_dir(struct dentry *dentry);

struct dentry *tracefs_create_file(const char *name, umode_t mode,
				   struct dentry *parent, void *data,
				   const struct file_operations *fops);

struct dentry *tracefs_create_dir(const char *name, struct dentry *parent);

void tracefs_remove(struct dentry *dentry);

struct dentry *tracefs_create_instance_dir(const char *name, struct dentry *parent,
					   int (*mkdir)(const char *name),
					   int (*rmdir)(const char *name));

bool tracefs_initialized(void);

#endif  

#endif
