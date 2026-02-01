
 

#include "fsverity_private.h"

#include <linux/uaccess.h>

 
int fsverity_ioctl_measure(struct file *filp, void __user *_uarg)
{
	const struct inode *inode = file_inode(filp);
	struct fsverity_digest __user *uarg = _uarg;
	const struct fsverity_info *vi;
	const struct fsverity_hash_alg *hash_alg;
	struct fsverity_digest arg;

	vi = fsverity_get_info(inode);
	if (!vi)
		return -ENODATA;  
	hash_alg = vi->tree_params.hash_alg;

	 

	if (get_user(arg.digest_size, &uarg->digest_size))
		return -EFAULT;
	if (arg.digest_size < hash_alg->digest_size)
		return -EOVERFLOW;

	memset(&arg, 0, sizeof(arg));
	arg.digest_algorithm = hash_alg - fsverity_hash_algs;
	arg.digest_size = hash_alg->digest_size;

	if (copy_to_user(uarg, &arg, sizeof(arg)))
		return -EFAULT;

	if (copy_to_user(uarg->digest, vi->file_digest, hash_alg->digest_size))
		return -EFAULT;

	return 0;
}
EXPORT_SYMBOL_GPL(fsverity_ioctl_measure);

 
int fsverity_get_digest(struct inode *inode,
			u8 raw_digest[FS_VERITY_MAX_DIGEST_SIZE],
			u8 *alg, enum hash_algo *halg)
{
	const struct fsverity_info *vi;
	const struct fsverity_hash_alg *hash_alg;

	vi = fsverity_get_info(inode);
	if (!vi)
		return 0;  

	hash_alg = vi->tree_params.hash_alg;
	memcpy(raw_digest, vi->file_digest, hash_alg->digest_size);
	if (alg)
		*alg = hash_alg - fsverity_hash_algs;
	if (halg)
		*halg = hash_alg->algo_id;
	return hash_alg->digest_size;
}
EXPORT_SYMBOL_GPL(fsverity_get_digest);
