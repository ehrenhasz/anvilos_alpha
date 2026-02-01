
 

#include <linux/magic.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/nsproxy.h>
#include <linux/path.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fs_struct.h>

#include "include/apparmor.h"
#include "include/path.h"
#include "include/policy.h"

 
static int prepend(char **buffer, int buflen, const char *str, int namelen)
{
	buflen -= namelen;
	if (buflen < 0)
		return -ENAMETOOLONG;
	*buffer -= namelen;
	memcpy(*buffer, str, namelen);
	return 0;
}

#define CHROOT_NSCONNECT (PATH_CHROOT_REL | PATH_CHROOT_NSCONNECT)

 
static int disconnect(const struct path *path, char *buf, char **name,
		      int flags, const char *disconnected)
{
	int error = 0;

	if (!(flags & PATH_CONNECT_PATH) &&
	    !(((flags & CHROOT_NSCONNECT) == CHROOT_NSCONNECT) &&
	      our_mnt(path->mnt))) {
		 
		error = -EACCES;
		if (**name == '/')
			*name = *name + 1;
	} else {
		if (**name != '/')
			 
			error = prepend(name, *name - buf, "/", 1);
		if (!error && disconnected)
			error = prepend(name, *name - buf, disconnected,
					strlen(disconnected));
	}

	return error;
}

 
static int d_namespace_path(const struct path *path, char *buf, char **name,
			    int flags, const char *disconnected)
{
	char *res;
	int error = 0;
	int connected = 1;
	int isdir = (flags & PATH_IS_DIR) ? 1 : 0;
	int buflen = aa_g_path_max - isdir;

	if (path->mnt->mnt_flags & MNT_INTERNAL) {
		 
		res = dentry_path(path->dentry, buf, buflen);
		*name = res;
		if (IS_ERR(res)) {
			*name = buf;
			return PTR_ERR(res);
		}
		if (path->dentry->d_sb->s_magic == PROC_SUPER_MAGIC &&
		    strncmp(*name, "/sys/", 5) == 0) {
			 
			error = prepend(name, *name - buf, "/proc", 5);
			goto out;
		} else
			error = disconnect(path, buf, name, flags,
					   disconnected);
		goto out;
	}

	 
	if (flags & PATH_CHROOT_REL) {
		struct path root;
		get_fs_root(current->fs, &root);
		res = __d_path(path, &root, buf, buflen);
		path_put(&root);
	} else {
		res = d_absolute_path(path, buf, buflen);
		if (!our_mnt(path->mnt))
			connected = 0;
	}

	 
	if (!res || IS_ERR(res)) {
		if (PTR_ERR(res) == -ENAMETOOLONG) {
			error = -ENAMETOOLONG;
			*name = buf;
			goto out;
		}
		connected = 0;
		res = dentry_path_raw(path->dentry, buf, buflen);
		if (IS_ERR(res)) {
			error = PTR_ERR(res);
			*name = buf;
			goto out;
		}
	} else if (!our_mnt(path->mnt))
		connected = 0;

	*name = res;

	if (!connected)
		error = disconnect(path, buf, name, flags, disconnected);

	 
	if (d_unlinked(path->dentry) && d_is_positive(path->dentry) &&
	    !(flags & (PATH_MEDIATE_DELETED | PATH_DELEGATE_DELETED))) {
			error = -ENOENT;
			goto out;
	}

out:
	 
	if (!error && isdir && ((*name)[1] != '\0' || (*name)[0] != '/'))
		strcpy(&buf[aa_g_path_max - 2], "/");

	return error;
}

 
int aa_path_name(const struct path *path, int flags, char *buffer,
		 const char **name, const char **info, const char *disconnected)
{
	char *str = NULL;
	int error = d_namespace_path(path, buffer, &str, flags, disconnected);

	if (info && error) {
		if (error == -ENOENT)
			*info = "Failed name lookup - deleted entry";
		else if (error == -EACCES)
			*info = "Failed name lookup - disconnected path";
		else if (error == -ENAMETOOLONG)
			*info = "Failed name lookup - name too long";
		else
			*info = "Failed name lookup";
	}

	*name = str;

	return error;
}
