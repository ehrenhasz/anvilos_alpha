
#include <linux/blkdev.h>
#include <linux/slab.h>

struct bd_holder_disk {
	struct list_head	list;
	struct kobject		*holder_dir;
	int			refcnt;
};

static struct bd_holder_disk *bd_find_holder_disk(struct block_device *bdev,
						  struct gendisk *disk)
{
	struct bd_holder_disk *holder;

	list_for_each_entry(holder, &disk->slave_bdevs, list)
		if (holder->holder_dir == bdev->bd_holder_dir)
			return holder;
	return NULL;
}

static int add_symlink(struct kobject *from, struct kobject *to)
{
	return sysfs_create_link(from, to, kobject_name(to));
}

static void del_symlink(struct kobject *from, struct kobject *to)
{
	sysfs_remove_link(from, kobject_name(to));
}

 
int bd_link_disk_holder(struct block_device *bdev, struct gendisk *disk)
{
	struct bd_holder_disk *holder;
	int ret = 0;

	if (WARN_ON_ONCE(!disk->slave_dir))
		return -EINVAL;

	if (bdev->bd_disk == disk)
		return -EINVAL;

	 
	mutex_lock(&bdev->bd_disk->open_mutex);
	if (!disk_live(bdev->bd_disk)) {
		mutex_unlock(&bdev->bd_disk->open_mutex);
		return -ENODEV;
	}
	kobject_get(bdev->bd_holder_dir);
	mutex_unlock(&bdev->bd_disk->open_mutex);

	mutex_lock(&disk->open_mutex);
	WARN_ON_ONCE(!bdev->bd_holder);

	holder = bd_find_holder_disk(bdev, disk);
	if (holder) {
		kobject_put(bdev->bd_holder_dir);
		holder->refcnt++;
		goto out_unlock;
	}

	holder = kzalloc(sizeof(*holder), GFP_KERNEL);
	if (!holder) {
		ret = -ENOMEM;
		goto out_unlock;
	}

	INIT_LIST_HEAD(&holder->list);
	holder->refcnt = 1;
	holder->holder_dir = bdev->bd_holder_dir;

	ret = add_symlink(disk->slave_dir, bdev_kobj(bdev));
	if (ret)
		goto out_free_holder;
	ret = add_symlink(bdev->bd_holder_dir, &disk_to_dev(disk)->kobj);
	if (ret)
		goto out_del_symlink;
	list_add(&holder->list, &disk->slave_bdevs);

	mutex_unlock(&disk->open_mutex);
	return 0;

out_del_symlink:
	del_symlink(disk->slave_dir, bdev_kobj(bdev));
out_free_holder:
	kfree(holder);
out_unlock:
	mutex_unlock(&disk->open_mutex);
	if (ret)
		kobject_put(bdev->bd_holder_dir);
	return ret;
}
EXPORT_SYMBOL_GPL(bd_link_disk_holder);

 
void bd_unlink_disk_holder(struct block_device *bdev, struct gendisk *disk)
{
	struct bd_holder_disk *holder;

	if (WARN_ON_ONCE(!disk->slave_dir))
		return;

	mutex_lock(&disk->open_mutex);
	holder = bd_find_holder_disk(bdev, disk);
	if (!WARN_ON_ONCE(holder == NULL) && !--holder->refcnt) {
		del_symlink(disk->slave_dir, bdev_kobj(bdev));
		del_symlink(holder->holder_dir, &disk_to_dev(disk)->kobj);
		kobject_put(holder->holder_dir);
		list_del_init(&holder->list);
		kfree(holder);
	}
	mutex_unlock(&disk->open_mutex);
}
EXPORT_SYMBOL_GPL(bd_unlink_disk_holder);
