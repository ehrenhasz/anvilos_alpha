
 

#include <linux/ctype.h>
#include <linux/efi.h>
#include <linux/fs.h>
#include <linux/fs_context.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/ucs2_string.h>
#include <linux/slab.h>
#include <linux/magic.h>
#include <linux/statfs.h>
#include <linux/printk.h>

#include "internal.h"

LIST_HEAD(efivarfs_list);

static void efivarfs_evict_inode(struct inode *inode)
{
	clear_inode(inode);
}

static int efivarfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	const u32 attr = EFI_VARIABLE_NON_VOLATILE |
			 EFI_VARIABLE_BOOTSERVICE_ACCESS |
			 EFI_VARIABLE_RUNTIME_ACCESS;
	u64 storage_space, remaining_space, max_variable_size;
	efi_status_t status;

	 
	storage_space = remaining_space = 0;
	if (efi_rt_services_supported(EFI_RT_SUPPORTED_QUERY_VARIABLE_INFO)) {
		status = efivar_query_variable_info(attr, &storage_space,
						    &remaining_space,
						    &max_variable_size);
		if (status != EFI_SUCCESS && status != EFI_UNSUPPORTED)
			pr_warn_ratelimited("query_variable_info() failed: 0x%lx\n",
					    status);
	}

	 
	buf->f_bsize	= 1;
	buf->f_namelen	= NAME_MAX;
	buf->f_blocks	= storage_space;
	buf->f_bfree	= remaining_space;
	buf->f_type	= dentry->d_sb->s_magic;

	 
	if (remaining_space > efivar_reserved_space())
		buf->f_bavail = remaining_space - efivar_reserved_space();
	else
		buf->f_bavail = 0;

	return 0;
}
static const struct super_operations efivarfs_ops = {
	.statfs = efivarfs_statfs,
	.drop_inode = generic_delete_inode,
	.evict_inode = efivarfs_evict_inode,
};

 
static int efivarfs_d_compare(const struct dentry *dentry,
			      unsigned int len, const char *str,
			      const struct qstr *name)
{
	int guid = len - EFI_VARIABLE_GUID_LEN;

	if (name->len != len)
		return 1;

	 
	if (memcmp(str, name->name, guid))
		return 1;

	 
	return strncasecmp(name->name + guid, str + guid, EFI_VARIABLE_GUID_LEN);
}

static int efivarfs_d_hash(const struct dentry *dentry, struct qstr *qstr)
{
	unsigned long hash = init_name_hash(dentry);
	const unsigned char *s = qstr->name;
	unsigned int len = qstr->len;

	if (!efivarfs_valid_name(s, len))
		return -EINVAL;

	while (len-- > EFI_VARIABLE_GUID_LEN)
		hash = partial_name_hash(*s++, hash);

	 
	while (len--)
		hash = partial_name_hash(tolower(*s++), hash);

	qstr->hash = end_name_hash(hash);
	return 0;
}

static const struct dentry_operations efivarfs_d_ops = {
	.d_compare = efivarfs_d_compare,
	.d_hash = efivarfs_d_hash,
	.d_delete = always_delete_dentry,
};

static struct dentry *efivarfs_alloc_dentry(struct dentry *parent, char *name)
{
	struct dentry *d;
	struct qstr q;
	int err;

	q.name = name;
	q.len = strlen(name);

	err = efivarfs_d_hash(parent, &q);
	if (err)
		return ERR_PTR(err);

	d = d_alloc(parent, &q);
	if (d)
		return d;

	return ERR_PTR(-ENOMEM);
}

static int efivarfs_callback(efi_char16_t *name16, efi_guid_t vendor,
			     unsigned long name_size, void *data)
{
	struct super_block *sb = (struct super_block *)data;
	struct efivar_entry *entry;
	struct inode *inode = NULL;
	struct dentry *dentry, *root = sb->s_root;
	unsigned long size = 0;
	char *name;
	int len;
	int err = -ENOMEM;
	bool is_removable = false;

	if (guid_equal(&vendor, &LINUX_EFI_RANDOM_SEED_TABLE_GUID))
		return 0;

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return err;

	memcpy(entry->var.VariableName, name16, name_size);
	memcpy(&(entry->var.VendorGuid), &vendor, sizeof(efi_guid_t));

	len = ucs2_utf8size(entry->var.VariableName);

	 
	name = kmalloc(len + 1 + EFI_VARIABLE_GUID_LEN + 1, GFP_KERNEL);
	if (!name)
		goto fail;

	ucs2_as_utf8(name, entry->var.VariableName, len);

	if (efivar_variable_is_removable(entry->var.VendorGuid, name, len))
		is_removable = true;

	name[len] = '-';

	efi_guid_to_str(&entry->var.VendorGuid, name + len + 1);

	name[len + EFI_VARIABLE_GUID_LEN+1] = '\0';

	 
	strreplace(name, '/', '!');

	inode = efivarfs_get_inode(sb, d_inode(root), S_IFREG | 0644, 0,
				   is_removable);
	if (!inode)
		goto fail_name;

	dentry = efivarfs_alloc_dentry(root, name);
	if (IS_ERR(dentry)) {
		err = PTR_ERR(dentry);
		goto fail_inode;
	}

	__efivar_entry_get(entry, NULL, &size, NULL);
	__efivar_entry_add(entry, &efivarfs_list);

	 
	kfree(name);

	inode_lock(inode);
	inode->i_private = entry;
	i_size_write(inode, size + sizeof(entry->var.Attributes));
	inode_unlock(inode);
	d_add(dentry, inode);

	return 0;

fail_inode:
	iput(inode);
fail_name:
	kfree(name);
fail:
	kfree(entry);
	return err;
}

static int efivarfs_destroy(struct efivar_entry *entry, void *data)
{
	efivar_entry_remove(entry);
	kfree(entry);
	return 0;
}

static int efivarfs_fill_super(struct super_block *sb, struct fs_context *fc)
{
	struct inode *inode = NULL;
	struct dentry *root;
	int err;

	if (!efivar_is_available())
		return -EOPNOTSUPP;

	sb->s_maxbytes          = MAX_LFS_FILESIZE;
	sb->s_blocksize         = PAGE_SIZE;
	sb->s_blocksize_bits    = PAGE_SHIFT;
	sb->s_magic             = EFIVARFS_MAGIC;
	sb->s_op                = &efivarfs_ops;
	sb->s_d_op		= &efivarfs_d_ops;
	sb->s_time_gran         = 1;

	if (!efivar_supports_writes())
		sb->s_flags |= SB_RDONLY;

	inode = efivarfs_get_inode(sb, NULL, S_IFDIR | 0755, 0, true);
	if (!inode)
		return -ENOMEM;
	inode->i_op = &efivarfs_dir_inode_operations;

	root = d_make_root(inode);
	sb->s_root = root;
	if (!root)
		return -ENOMEM;

	INIT_LIST_HEAD(&efivarfs_list);

	err = efivar_init(efivarfs_callback, (void *)sb, true, &efivarfs_list);
	if (err)
		efivar_entry_iter(efivarfs_destroy, &efivarfs_list, NULL);

	return err;
}

static int efivarfs_get_tree(struct fs_context *fc)
{
	return get_tree_single(fc, efivarfs_fill_super);
}

static int efivarfs_reconfigure(struct fs_context *fc)
{
	if (!efivar_supports_writes() && !(fc->sb_flags & SB_RDONLY)) {
		pr_err("Firmware does not support SetVariableRT. Can not remount with rw\n");
		return -EINVAL;
	}

	return 0;
}

static const struct fs_context_operations efivarfs_context_ops = {
	.get_tree	= efivarfs_get_tree,
	.reconfigure	= efivarfs_reconfigure,
};

static int efivarfs_init_fs_context(struct fs_context *fc)
{
	fc->ops = &efivarfs_context_ops;
	return 0;
}

static void efivarfs_kill_sb(struct super_block *sb)
{
	struct efivarfs_fs_info *sfi = sb->s_fs_info;

	kill_litter_super(sb);

	if (!efivar_is_available())
		return;

	 
	efivar_entry_iter(efivarfs_destroy, &efivarfs_list, NULL);
	kfree(sfi);
}

static struct file_system_type efivarfs_type = {
	.owner   = THIS_MODULE,
	.name    = "efivarfs",
	.init_fs_context = efivarfs_init_fs_context,
	.kill_sb = efivarfs_kill_sb,
};

static __init int efivarfs_init(void)
{
	return register_filesystem(&efivarfs_type);
}

static __exit void efivarfs_exit(void)
{
	unregister_filesystem(&efivarfs_type);
}

MODULE_AUTHOR("Matthew Garrett, Jeremy Kerr");
MODULE_DESCRIPTION("EFI Variable Filesystem");
MODULE_LICENSE("GPL");
MODULE_ALIAS_FS("efivarfs");

module_init(efivarfs_init);
module_exit(efivarfs_exit);
