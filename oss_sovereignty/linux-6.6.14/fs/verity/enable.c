
 

#include "fsverity_private.h"

#include <crypto/hash.h>
#include <linux/mount.h>
#include <linux/sched/signal.h>
#include <linux/uaccess.h>

struct block_buffer {
	u32 filled;
	bool is_root_hash;
	u8 *data;
};

 
static int hash_one_block(struct inode *inode,
			  const struct merkle_tree_params *params,
			  struct block_buffer *cur)
{
	struct block_buffer *next = cur + 1;
	int err;

	 
	if (WARN_ON_ONCE(next->is_root_hash && next->filled != 0))
		return -EINVAL;

	 
	memset(&cur->data[cur->filled], 0, params->block_size - cur->filled);

	err = fsverity_hash_block(params, inode, cur->data,
				  &next->data[next->filled]);
	if (err)
		return err;
	next->filled += params->digest_size;
	cur->filled = 0;
	return 0;
}

static int write_merkle_tree_block(struct inode *inode, const u8 *buf,
				   unsigned long index,
				   const struct merkle_tree_params *params)
{
	u64 pos = (u64)index << params->log_blocksize;
	int err;

	err = inode->i_sb->s_vop->write_merkle_tree_block(inode, buf, pos,
							  params->block_size);
	if (err)
		fsverity_err(inode, "Error %d writing Merkle tree block %lu",
			     err, index);
	return err;
}

 
static int build_merkle_tree(struct file *filp,
			     const struct merkle_tree_params *params,
			     u8 *root_hash)
{
	struct inode *inode = file_inode(filp);
	const u64 data_size = inode->i_size;
	const int num_levels = params->num_levels;
	struct block_buffer _buffers[1 + FS_VERITY_MAX_LEVELS + 1] = {};
	struct block_buffer *buffers = &_buffers[1];
	unsigned long level_offset[FS_VERITY_MAX_LEVELS];
	int level;
	u64 offset;
	int err;

	if (data_size == 0) {
		 
		memset(root_hash, 0, params->digest_size);
		return 0;
	}

	 
	for (level = -1; level < num_levels; level++) {
		buffers[level].data = kzalloc(params->block_size, GFP_KERNEL);
		if (!buffers[level].data) {
			err = -ENOMEM;
			goto out;
		}
	}
	buffers[num_levels].data = root_hash;
	buffers[num_levels].is_root_hash = true;

	BUILD_BUG_ON(sizeof(level_offset) != sizeof(params->level_start));
	memcpy(level_offset, params->level_start, sizeof(level_offset));

	 
	for (offset = 0; offset < data_size; offset += params->block_size) {
		ssize_t bytes_read;
		loff_t pos = offset;

		buffers[-1].filled = min_t(u64, params->block_size,
					   data_size - offset);
		bytes_read = __kernel_read(filp, buffers[-1].data,
					   buffers[-1].filled, &pos);
		if (bytes_read < 0) {
			err = bytes_read;
			fsverity_err(inode, "Error %d reading file data", err);
			goto out;
		}
		if (bytes_read != buffers[-1].filled) {
			err = -EINVAL;
			fsverity_err(inode, "Short read of file data");
			goto out;
		}
		err = hash_one_block(inode, params, &buffers[-1]);
		if (err)
			goto out;
		for (level = 0; level < num_levels; level++) {
			if (buffers[level].filled + params->digest_size <=
			    params->block_size) {
				 
				break;
			}
			 

			err = hash_one_block(inode, params, &buffers[level]);
			if (err)
				goto out;
			err = write_merkle_tree_block(inode,
						      buffers[level].data,
						      level_offset[level],
						      params);
			if (err)
				goto out;
			level_offset[level]++;
		}
		if (fatal_signal_pending(current)) {
			err = -EINTR;
			goto out;
		}
		cond_resched();
	}
	 
	for (level = 0; level < num_levels; level++) {
		if (buffers[level].filled != 0) {
			err = hash_one_block(inode, params, &buffers[level]);
			if (err)
				goto out;
			err = write_merkle_tree_block(inode,
						      buffers[level].data,
						      level_offset[level],
						      params);
			if (err)
				goto out;
		}
	}
	 
	if (WARN_ON_ONCE(buffers[num_levels].filled != params->digest_size)) {
		err = -EINVAL;
		goto out;
	}
	err = 0;
out:
	for (level = -1; level < num_levels; level++)
		kfree(buffers[level].data);
	return err;
}

static int enable_verity(struct file *filp,
			 const struct fsverity_enable_arg *arg)
{
	struct inode *inode = file_inode(filp);
	const struct fsverity_operations *vops = inode->i_sb->s_vop;
	struct merkle_tree_params params = { };
	struct fsverity_descriptor *desc;
	size_t desc_size = struct_size(desc, signature, arg->sig_size);
	struct fsverity_info *vi;
	int err;

	 
	desc = kzalloc(desc_size, GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	desc->version = 1;
	desc->hash_algorithm = arg->hash_algorithm;
	desc->log_blocksize = ilog2(arg->block_size);

	 
	if (arg->salt_size &&
	    copy_from_user(desc->salt, u64_to_user_ptr(arg->salt_ptr),
			   arg->salt_size)) {
		err = -EFAULT;
		goto out;
	}
	desc->salt_size = arg->salt_size;

	 
	if (arg->sig_size &&
	    copy_from_user(desc->signature, u64_to_user_ptr(arg->sig_ptr),
			   arg->sig_size)) {
		err = -EFAULT;
		goto out;
	}
	desc->sig_size = cpu_to_le32(arg->sig_size);

	desc->data_size = cpu_to_le64(inode->i_size);

	 
	err = fsverity_init_merkle_tree_params(&params, inode,
					       arg->hash_algorithm,
					       desc->log_blocksize,
					       desc->salt, desc->salt_size);
	if (err)
		goto out;

	 
	inode_lock(inode);
	if (IS_VERITY(inode))
		err = -EEXIST;
	else
		err = vops->begin_enable_verity(filp);
	inode_unlock(inode);
	if (err)
		goto out;

	 
	BUILD_BUG_ON(sizeof(desc->root_hash) < FS_VERITY_MAX_DIGEST_SIZE);
	err = build_merkle_tree(filp, &params, desc->root_hash);
	if (err) {
		fsverity_err(inode, "Error %d building Merkle tree", err);
		goto rollback;
	}

	 
	vi = fsverity_create_info(inode, desc);
	if (IS_ERR(vi)) {
		err = PTR_ERR(vi);
		goto rollback;
	}

	 
	inode_lock(inode);
	err = vops->end_enable_verity(filp, desc, desc_size, params.tree_size);
	inode_unlock(inode);
	if (err) {
		fsverity_err(inode, "%ps() failed with err %d",
			     vops->end_enable_verity, err);
		fsverity_free_info(vi);
	} else if (WARN_ON_ONCE(!IS_VERITY(inode))) {
		err = -EINVAL;
		fsverity_free_info(vi);
	} else {
		 

		 
		fsverity_set_info(inode, vi);
	}
out:
	kfree(params.hashstate);
	kfree(desc);
	return err;

rollback:
	inode_lock(inode);
	(void)vops->end_enable_verity(filp, NULL, 0, params.tree_size);
	inode_unlock(inode);
	goto out;
}

 
int fsverity_ioctl_enable(struct file *filp, const void __user *uarg)
{
	struct inode *inode = file_inode(filp);
	struct fsverity_enable_arg arg;
	int err;

	if (copy_from_user(&arg, uarg, sizeof(arg)))
		return -EFAULT;

	if (arg.version != 1)
		return -EINVAL;

	if (arg.__reserved1 ||
	    memchr_inv(arg.__reserved2, 0, sizeof(arg.__reserved2)))
		return -EINVAL;

	if (!is_power_of_2(arg.block_size))
		return -EINVAL;

	if (arg.salt_size > sizeof_field(struct fsverity_descriptor, salt))
		return -EMSGSIZE;

	if (arg.sig_size > FS_VERITY_MAX_SIGNATURE_SIZE)
		return -EMSGSIZE;

	 

	err = file_permission(filp, MAY_WRITE);
	if (err)
		return err;
	 
	if (!(filp->f_mode & FMODE_READ))
		return -EBADF;

	if (IS_APPEND(inode))
		return -EPERM;

	if (S_ISDIR(inode->i_mode))
		return -EISDIR;

	if (!S_ISREG(inode->i_mode))
		return -EINVAL;

	err = mnt_want_write_file(filp);
	if (err)  
		return err;

	err = deny_write_access(filp);
	if (err)  
		goto out_drop_write;

	err = enable_verity(filp, &arg);

	 

	 
	allow_write_access(filp);
out_drop_write:
	mnt_drop_write_file(filp);
	return err;
}
EXPORT_SYMBOL_GPL(fsverity_ioctl_enable);
