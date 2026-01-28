#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/sunddi.h>
#include <sys/zfs_ioctl.h>
#include <sys/zfs_onexit.h>
#include <sys/zvol.h>
void
zfs_onexit_init(zfs_onexit_t **zop)
{
	zfs_onexit_t *zo;
	zo = *zop = kmem_zalloc(sizeof (zfs_onexit_t), KM_SLEEP);
	mutex_init(&zo->zo_lock, NULL, MUTEX_DEFAULT, NULL);
	list_create(&zo->zo_actions, sizeof (zfs_onexit_action_node_t),
	    offsetof(zfs_onexit_action_node_t, za_link));
}
void
zfs_onexit_destroy(zfs_onexit_t *zo)
{
	zfs_onexit_action_node_t *ap;
	mutex_enter(&zo->zo_lock);
	while ((ap = list_remove_head(&zo->zo_actions)) != NULL) {
		mutex_exit(&zo->zo_lock);
		ap->za_func(ap->za_data);
		kmem_free(ap, sizeof (zfs_onexit_action_node_t));
		mutex_enter(&zo->zo_lock);
	}
	mutex_exit(&zo->zo_lock);
	list_destroy(&zo->zo_actions);
	mutex_destroy(&zo->zo_lock);
	kmem_free(zo, sizeof (zfs_onexit_t));
}
zfs_file_t *
zfs_onexit_fd_hold(int fd, minor_t *minorp)
{
	zfs_onexit_t *zo = NULL;
	zfs_file_t *fp = zfs_file_get(fd);
	if (fp == NULL)
		return (NULL);
	int error = zfsdev_getminor(fp, minorp);
	if (error) {
		zfs_onexit_fd_rele(fp);
		return (NULL);
	}
	zo = zfsdev_get_state(*minorp, ZST_ONEXIT);
	if (zo == NULL) {
		zfs_onexit_fd_rele(fp);
		return (NULL);
	}
	return (fp);
}
void
zfs_onexit_fd_rele(zfs_file_t *fp)
{
	zfs_file_put(fp);
}
static int
zfs_onexit_minor_to_state(minor_t minor, zfs_onexit_t **zo)
{
	*zo = zfsdev_get_state(minor, ZST_ONEXIT);
	if (*zo == NULL)
		return (SET_ERROR(EBADF));
	return (0);
}
int
zfs_onexit_add_cb(minor_t minor, void (*func)(void *), void *data,
    uintptr_t *action_handle)
{
	zfs_onexit_t *zo;
	zfs_onexit_action_node_t *ap;
	int error;
	error = zfs_onexit_minor_to_state(minor, &zo);
	if (error)
		return (error);
	ap = kmem_alloc(sizeof (zfs_onexit_action_node_t), KM_SLEEP);
	list_link_init(&ap->za_link);
	ap->za_func = func;
	ap->za_data = data;
	mutex_enter(&zo->zo_lock);
	list_insert_tail(&zo->zo_actions, ap);
	mutex_exit(&zo->zo_lock);
	if (action_handle)
		*action_handle = (uintptr_t)ap;
	return (0);
}
