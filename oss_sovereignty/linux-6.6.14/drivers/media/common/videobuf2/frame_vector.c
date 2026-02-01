
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include <linux/sched.h>

#include <media/frame_vector.h>

 
int get_vaddr_frames(unsigned long start, unsigned int nr_frames, bool write,
		     struct frame_vector *vec)
{
	int ret;
	unsigned int gup_flags = FOLL_LONGTERM;

	if (nr_frames == 0)
		return 0;

	if (WARN_ON_ONCE(nr_frames > vec->nr_allocated))
		nr_frames = vec->nr_allocated;

	start = untagged_addr(start);

	if (write)
		gup_flags |= FOLL_WRITE;

	ret = pin_user_pages_fast(start, nr_frames, gup_flags,
				  (struct page **)(vec->ptrs));
	vec->got_ref = true;
	vec->is_pfns = false;
	vec->nr_frames = ret;

	if (likely(ret > 0))
		return ret;

	vec->nr_frames = 0;
	return ret ? ret : -EFAULT;
}
EXPORT_SYMBOL(get_vaddr_frames);

 
void put_vaddr_frames(struct frame_vector *vec)
{
	struct page **pages;

	if (!vec->got_ref)
		goto out;
	pages = frame_vector_pages(vec);
	 
	if (WARN_ON(IS_ERR(pages)))
		goto out;

	unpin_user_pages(pages, vec->nr_frames);
	vec->got_ref = false;
out:
	vec->nr_frames = 0;
}
EXPORT_SYMBOL(put_vaddr_frames);

 
int frame_vector_to_pages(struct frame_vector *vec)
{
	int i;
	unsigned long *nums;
	struct page **pages;

	if (!vec->is_pfns)
		return 0;
	nums = frame_vector_pfns(vec);
	for (i = 0; i < vec->nr_frames; i++)
		if (!pfn_valid(nums[i]))
			return -EINVAL;
	pages = (struct page **)nums;
	for (i = 0; i < vec->nr_frames; i++)
		pages[i] = pfn_to_page(nums[i]);
	vec->is_pfns = false;
	return 0;
}
EXPORT_SYMBOL(frame_vector_to_pages);

 
void frame_vector_to_pfns(struct frame_vector *vec)
{
	int i;
	unsigned long *nums;
	struct page **pages;

	if (vec->is_pfns)
		return;
	pages = (struct page **)(vec->ptrs);
	nums = (unsigned long *)pages;
	for (i = 0; i < vec->nr_frames; i++)
		nums[i] = page_to_pfn(pages[i]);
	vec->is_pfns = true;
}
EXPORT_SYMBOL(frame_vector_to_pfns);

 
struct frame_vector *frame_vector_create(unsigned int nr_frames)
{
	struct frame_vector *vec;
	int size = sizeof(struct frame_vector) + sizeof(void *) * nr_frames;

	if (WARN_ON_ONCE(nr_frames == 0))
		return NULL;
	 
	if (WARN_ON_ONCE(nr_frames > INT_MAX / sizeof(void *) / 2))
		return NULL;
	 
	vec = kvmalloc(size, GFP_KERNEL);
	if (!vec)
		return NULL;
	vec->nr_allocated = nr_frames;
	vec->nr_frames = 0;
	return vec;
}
EXPORT_SYMBOL(frame_vector_create);

 
void frame_vector_destroy(struct frame_vector *vec)
{
	 
	VM_BUG_ON(vec->nr_frames > 0);
	kvfree(vec);
}
EXPORT_SYMBOL(frame_vector_destroy);
