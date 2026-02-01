
 
#include <linux/iova_bitmap.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/highmem.h>

#define BITS_PER_PAGE (PAGE_SIZE * BITS_PER_BYTE)

 
struct iova_bitmap_map {
	 
	unsigned long iova;

	 
	unsigned long pgshift;

	 
	unsigned long pgoff;

	 
	unsigned long npages;

	 
	struct page **pages;
};

 
struct iova_bitmap {
	 
	struct iova_bitmap_map mapped;

	 
	u64 __user *bitmap;

	 
	unsigned long mapped_base_index;

	 
	unsigned long mapped_total_index;

	 
	unsigned long iova;

	 
	size_t length;
};

 
static unsigned long iova_bitmap_offset_to_index(struct iova_bitmap *bitmap,
						 unsigned long iova)
{
	unsigned long pgsize = 1 << bitmap->mapped.pgshift;

	return iova / (BITS_PER_TYPE(*bitmap->bitmap) * pgsize);
}

 
static unsigned long iova_bitmap_index_to_offset(struct iova_bitmap *bitmap,
						 unsigned long index)
{
	unsigned long pgshift = bitmap->mapped.pgshift;

	return (index * BITS_PER_TYPE(*bitmap->bitmap)) << pgshift;
}

 
static unsigned long iova_bitmap_mapped_iova(struct iova_bitmap *bitmap)
{
	unsigned long skip = bitmap->mapped_base_index;

	return bitmap->iova + iova_bitmap_index_to_offset(bitmap, skip);
}

 
static int iova_bitmap_get(struct iova_bitmap *bitmap)
{
	struct iova_bitmap_map *mapped = &bitmap->mapped;
	unsigned long npages;
	u64 __user *addr;
	long ret;

	 
	npages = DIV_ROUND_UP((bitmap->mapped_total_index -
			       bitmap->mapped_base_index) *
			       sizeof(*bitmap->bitmap), PAGE_SIZE);

	 
	npages = min(npages,  PAGE_SIZE / sizeof(struct page *));

	 
	addr = bitmap->bitmap + bitmap->mapped_base_index;

	ret = pin_user_pages_fast((unsigned long)addr, npages,
				  FOLL_WRITE, mapped->pages);
	if (ret <= 0)
		return -EFAULT;

	mapped->npages = (unsigned long)ret;
	 
	mapped->iova = iova_bitmap_mapped_iova(bitmap);

	 
	mapped->pgoff = offset_in_page(addr);
	return 0;
}

 
static void iova_bitmap_put(struct iova_bitmap *bitmap)
{
	struct iova_bitmap_map *mapped = &bitmap->mapped;

	if (mapped->npages) {
		unpin_user_pages(mapped->pages, mapped->npages);
		mapped->npages = 0;
	}
}

 
struct iova_bitmap *iova_bitmap_alloc(unsigned long iova, size_t length,
				      unsigned long page_size, u64 __user *data)
{
	struct iova_bitmap_map *mapped;
	struct iova_bitmap *bitmap;
	int rc;

	bitmap = kzalloc(sizeof(*bitmap), GFP_KERNEL);
	if (!bitmap)
		return ERR_PTR(-ENOMEM);

	mapped = &bitmap->mapped;
	mapped->pgshift = __ffs(page_size);
	bitmap->bitmap = data;
	bitmap->mapped_total_index =
		iova_bitmap_offset_to_index(bitmap, length - 1) + 1;
	bitmap->iova = iova;
	bitmap->length = length;
	mapped->iova = iova;
	mapped->pages = (struct page **)__get_free_page(GFP_KERNEL);
	if (!mapped->pages) {
		rc = -ENOMEM;
		goto err;
	}

	rc = iova_bitmap_get(bitmap);
	if (rc)
		goto err;
	return bitmap;

err:
	iova_bitmap_free(bitmap);
	return ERR_PTR(rc);
}

 
void iova_bitmap_free(struct iova_bitmap *bitmap)
{
	struct iova_bitmap_map *mapped = &bitmap->mapped;

	iova_bitmap_put(bitmap);

	if (mapped->pages) {
		free_page((unsigned long)mapped->pages);
		mapped->pages = NULL;
	}

	kfree(bitmap);
}

 
static unsigned long iova_bitmap_mapped_remaining(struct iova_bitmap *bitmap)
{
	unsigned long remaining, bytes;

	bytes = (bitmap->mapped.npages << PAGE_SHIFT) - bitmap->mapped.pgoff;

	remaining = bitmap->mapped_total_index - bitmap->mapped_base_index;
	remaining = min_t(unsigned long, remaining,
			  bytes / sizeof(*bitmap->bitmap));

	return remaining;
}

 
static unsigned long iova_bitmap_mapped_length(struct iova_bitmap *bitmap)
{
	unsigned long max_iova = bitmap->iova + bitmap->length - 1;
	unsigned long iova = iova_bitmap_mapped_iova(bitmap);
	unsigned long remaining;

	 
	remaining = iova_bitmap_index_to_offset(bitmap,
			iova_bitmap_mapped_remaining(bitmap));

	if (iova + remaining - 1 > max_iova)
		remaining -= ((iova + remaining - 1) - max_iova);

	return remaining;
}

 
static bool iova_bitmap_done(struct iova_bitmap *bitmap)
{
	return bitmap->mapped_base_index >= bitmap->mapped_total_index;
}

 
static int iova_bitmap_advance(struct iova_bitmap *bitmap)
{
	unsigned long iova = iova_bitmap_mapped_length(bitmap) - 1;
	unsigned long count = iova_bitmap_offset_to_index(bitmap, iova) + 1;

	bitmap->mapped_base_index += count;

	iova_bitmap_put(bitmap);
	if (iova_bitmap_done(bitmap))
		return 0;

	 
	return iova_bitmap_get(bitmap);
}

 
int iova_bitmap_for_each(struct iova_bitmap *bitmap, void *opaque,
			 iova_bitmap_fn_t fn)
{
	int ret = 0;

	for (; !iova_bitmap_done(bitmap) && !ret;
	     ret = iova_bitmap_advance(bitmap)) {
		ret = fn(bitmap, iova_bitmap_mapped_iova(bitmap),
			 iova_bitmap_mapped_length(bitmap), opaque);
		if (ret)
			break;
	}

	return ret;
}

 
void iova_bitmap_set(struct iova_bitmap *bitmap,
		     unsigned long iova, size_t length)
{
	struct iova_bitmap_map *mapped = &bitmap->mapped;
	unsigned long cur_bit = ((iova - mapped->iova) >>
			mapped->pgshift) + mapped->pgoff * BITS_PER_BYTE;
	unsigned long last_bit = (((iova + length - 1) - mapped->iova) >>
			mapped->pgshift) + mapped->pgoff * BITS_PER_BYTE;

	do {
		unsigned int page_idx = cur_bit / BITS_PER_PAGE;
		unsigned int offset = cur_bit % BITS_PER_PAGE;
		unsigned int nbits = min(BITS_PER_PAGE - offset,
					 last_bit - cur_bit + 1);
		void *kaddr;

		kaddr = kmap_local_page(mapped->pages[page_idx]);
		bitmap_set(kaddr, offset, nbits);
		kunmap_local(kaddr);
		cur_bit += nbits;
	} while (cur_bit <= last_bit);
}
EXPORT_SYMBOL_GPL(iova_bitmap_set);
