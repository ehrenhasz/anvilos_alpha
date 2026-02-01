
 

#include <linux/export.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/uio.h>
#include <linux/scatterlist.h>
#include <linux/netfs.h>
#include "internal.h"

 
ssize_t netfs_extract_user_iter(struct iov_iter *orig, size_t orig_len,
				struct iov_iter *new,
				iov_iter_extraction_t extraction_flags)
{
	struct bio_vec *bv = NULL;
	struct page **pages;
	unsigned int cur_npages;
	unsigned int max_pages;
	unsigned int npages = 0;
	unsigned int i;
	ssize_t ret;
	size_t count = orig_len, offset, len;
	size_t bv_size, pg_size;

	if (WARN_ON_ONCE(!iter_is_ubuf(orig) && !iter_is_iovec(orig)))
		return -EIO;

	max_pages = iov_iter_npages(orig, INT_MAX);
	bv_size = array_size(max_pages, sizeof(*bv));
	bv = kvmalloc(bv_size, GFP_KERNEL);
	if (!bv)
		return -ENOMEM;

	 
	pg_size = array_size(max_pages, sizeof(*pages));
	pages = (void *)bv + bv_size - pg_size;

	while (count && npages < max_pages) {
		ret = iov_iter_extract_pages(orig, &pages, count,
					     max_pages - npages, extraction_flags,
					     &offset);
		if (ret < 0) {
			pr_err("Couldn't get user pages (rc=%zd)\n", ret);
			break;
		}

		if (ret > count) {
			pr_err("get_pages rc=%zd more than %zu\n", ret, count);
			break;
		}

		count -= ret;
		ret += offset;
		cur_npages = DIV_ROUND_UP(ret, PAGE_SIZE);

		if (npages + cur_npages > max_pages) {
			pr_err("Out of bvec array capacity (%u vs %u)\n",
			       npages + cur_npages, max_pages);
			break;
		}

		for (i = 0; i < cur_npages; i++) {
			len = ret > PAGE_SIZE ? PAGE_SIZE : ret;
			bvec_set_page(bv + npages + i, *pages++, len - offset, offset);
			ret -= len;
			offset = 0;
		}

		npages += cur_npages;
	}

	iov_iter_bvec(new, orig->data_source, bv, npages, orig_len - count);
	return npages;
}
EXPORT_SYMBOL_GPL(netfs_extract_user_iter);
