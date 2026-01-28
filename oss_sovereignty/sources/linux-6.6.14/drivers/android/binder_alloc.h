


#ifndef _LINUX_BINDER_ALLOC_H
#define _LINUX_BINDER_ALLOC_H

#include <linux/rbtree.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/rtmutex.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/list_lru.h>
#include <uapi/linux/android/binder.h>

extern struct list_lru binder_alloc_lru;
struct binder_transaction;


struct binder_buffer {
	struct list_head entry; 
	struct rb_node rb_node; 
				
	unsigned free:1;
	unsigned clear_on_free:1;
	unsigned allow_user_free:1;
	unsigned async_transaction:1;
	unsigned oneway_spam_suspect:1;
	unsigned debug_id:27;

	struct binder_transaction *transaction;

	struct binder_node *target_node;
	size_t data_size;
	size_t offsets_size;
	size_t extra_buffers_size;
	void __user *user_data;
	int    pid;
};


struct binder_lru_page {
	struct list_head lru;
	struct page *page_ptr;
	struct binder_alloc *alloc;
};


struct binder_alloc {
	struct mutex mutex;
	struct vm_area_struct *vma;
	struct mm_struct *mm;
	void __user *buffer;
	struct list_head buffers;
	struct rb_root free_buffers;
	struct rb_root allocated_buffers;
	size_t free_async_space;
	struct binder_lru_page *pages;
	size_t buffer_size;
	int pid;
	size_t pages_high;
	bool oneway_spam_detected;
};

#ifdef CONFIG_ANDROID_BINDER_IPC_SELFTEST
void binder_selftest_alloc(struct binder_alloc *alloc);
#else
static inline void binder_selftest_alloc(struct binder_alloc *alloc) {}
#endif
enum lru_status binder_alloc_free_page(struct list_head *item,
				       struct list_lru_one *lru,
				       spinlock_t *lock, void *cb_arg);
extern struct binder_buffer *binder_alloc_new_buf(struct binder_alloc *alloc,
						  size_t data_size,
						  size_t offsets_size,
						  size_t extra_buffers_size,
						  int is_async,
						  int pid);
extern void binder_alloc_init(struct binder_alloc *alloc);
extern int binder_alloc_shrinker_init(void);
extern void binder_alloc_shrinker_exit(void);
extern void binder_alloc_vma_close(struct binder_alloc *alloc);
extern struct binder_buffer *
binder_alloc_prepare_to_free(struct binder_alloc *alloc,
			     uintptr_t user_ptr);
extern void binder_alloc_free_buf(struct binder_alloc *alloc,
				  struct binder_buffer *buffer);
extern int binder_alloc_mmap_handler(struct binder_alloc *alloc,
				     struct vm_area_struct *vma);
extern void binder_alloc_deferred_release(struct binder_alloc *alloc);
extern int binder_alloc_get_allocated_count(struct binder_alloc *alloc);
extern void binder_alloc_print_allocated(struct seq_file *m,
					 struct binder_alloc *alloc);
void binder_alloc_print_pages(struct seq_file *m,
			      struct binder_alloc *alloc);


static inline size_t
binder_alloc_get_free_async_space(struct binder_alloc *alloc)
{
	size_t free_async_space;

	mutex_lock(&alloc->mutex);
	free_async_space = alloc->free_async_space;
	mutex_unlock(&alloc->mutex);
	return free_async_space;
}

unsigned long
binder_alloc_copy_user_to_buffer(struct binder_alloc *alloc,
				 struct binder_buffer *buffer,
				 binder_size_t buffer_offset,
				 const void __user *from,
				 size_t bytes);

int binder_alloc_copy_to_buffer(struct binder_alloc *alloc,
				struct binder_buffer *buffer,
				binder_size_t buffer_offset,
				void *src,
				size_t bytes);

int binder_alloc_copy_from_buffer(struct binder_alloc *alloc,
				  void *dest,
				  struct binder_buffer *buffer,
				  binder_size_t buffer_offset,
				  size_t bytes);

#endif 

