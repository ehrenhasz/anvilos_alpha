
 
#include "compress.h"
#include <linux/psi.h>
#include <linux/cpuhotplug.h>
#include <trace/events/erofs.h>

#define Z_EROFS_PCLUSTER_MAX_PAGES	(Z_EROFS_PCLUSTER_MAX_SIZE / PAGE_SIZE)
#define Z_EROFS_INLINE_BVECS		2

 
typedef void *z_erofs_next_pcluster_t;

struct z_erofs_bvec {
	struct page *page;
	int offset;
	unsigned int end;
};

#define __Z_EROFS_BVSET(name, total) \
struct name { \
	  \
	struct page *nextpage; \
	struct z_erofs_bvec bvec[total]; \
}
__Z_EROFS_BVSET(z_erofs_bvset,);
__Z_EROFS_BVSET(z_erofs_bvset_inline, Z_EROFS_INLINE_BVECS);

 
struct z_erofs_pcluster {
	struct erofs_workgroup obj;
	struct mutex lock;

	 
	z_erofs_next_pcluster_t next;

	 
	unsigned int length;

	 
	unsigned int vcnt;

	 
	unsigned short pageofs_out;

	 
	unsigned short pageofs_in;

	union {
		 
		struct z_erofs_bvset_inline bvset;

		 
		struct rcu_head rcu;
	};

	union {
		 
		unsigned short pclusterpages;

		 
		unsigned short tailpacking_size;
	};

	 
	unsigned char algorithmformat;

	 
	bool partial;

	 
	bool multibases;

	 
	struct z_erofs_bvec compressed_bvecs[];
};

 
#define Z_EROFS_PCLUSTER_TAIL           ((void *) 0x700 + POISON_POINTER_DELTA)
#define Z_EROFS_PCLUSTER_NIL            (NULL)

struct z_erofs_decompressqueue {
	struct super_block *sb;
	atomic_t pending_bios;
	z_erofs_next_pcluster_t head;

	union {
		struct completion done;
		struct work_struct work;
		struct kthread_work kthread_work;
	} u;
	bool eio, sync;
};

static inline bool z_erofs_is_inline_pcluster(struct z_erofs_pcluster *pcl)
{
	return !pcl->obj.index;
}

static inline unsigned int z_erofs_pclusterpages(struct z_erofs_pcluster *pcl)
{
	if (z_erofs_is_inline_pcluster(pcl))
		return 1;
	return pcl->pclusterpages;
}

 
#define Z_EROFS_PAGE_EIO			(1 << 30)

static inline void z_erofs_onlinepage_init(struct page *page)
{
	union {
		atomic_t o;
		unsigned long v;
	} u = { .o = ATOMIC_INIT(1) };

	set_page_private(page, u.v);
	smp_wmb();
	SetPagePrivate(page);
}

static inline void z_erofs_onlinepage_split(struct page *page)
{
	atomic_inc((atomic_t *)&page->private);
}

static void z_erofs_onlinepage_endio(struct page *page, int err)
{
	int orig, v;

	DBG_BUGON(!PagePrivate(page));

	do {
		orig = atomic_read((atomic_t *)&page->private);
		v = (orig - 1) | (err ? Z_EROFS_PAGE_EIO : 0);
	} while (atomic_cmpxchg((atomic_t *)&page->private, orig, v) != orig);

	if (!(v & ~Z_EROFS_PAGE_EIO)) {
		set_page_private(page, 0);
		ClearPagePrivate(page);
		if (!(v & Z_EROFS_PAGE_EIO))
			SetPageUptodate(page);
		unlock_page(page);
	}
}

#define Z_EROFS_ONSTACK_PAGES		32

 
struct z_erofs_pcluster_slab {
	struct kmem_cache *slab;
	unsigned int maxpages;
	char name[48];
};

#define _PCLP(n) { .maxpages = n }

static struct z_erofs_pcluster_slab pcluster_pool[] __read_mostly = {
	_PCLP(1), _PCLP(4), _PCLP(16), _PCLP(64), _PCLP(128),
	_PCLP(Z_EROFS_PCLUSTER_MAX_PAGES)
};

struct z_erofs_bvec_iter {
	struct page *bvpage;
	struct z_erofs_bvset *bvset;
	unsigned int nr, cur;
};

static struct page *z_erofs_bvec_iter_end(struct z_erofs_bvec_iter *iter)
{
	if (iter->bvpage)
		kunmap_local(iter->bvset);
	return iter->bvpage;
}

static struct page *z_erofs_bvset_flip(struct z_erofs_bvec_iter *iter)
{
	unsigned long base = (unsigned long)((struct z_erofs_bvset *)0)->bvec;
	 
	struct page *nextpage = iter->bvset->nextpage;
	struct page *oldpage;

	DBG_BUGON(!nextpage);
	oldpage = z_erofs_bvec_iter_end(iter);
	iter->bvpage = nextpage;
	iter->bvset = kmap_local_page(nextpage);
	iter->nr = (PAGE_SIZE - base) / sizeof(struct z_erofs_bvec);
	iter->cur = 0;
	return oldpage;
}

static void z_erofs_bvec_iter_begin(struct z_erofs_bvec_iter *iter,
				    struct z_erofs_bvset_inline *bvset,
				    unsigned int bootstrap_nr,
				    unsigned int cur)
{
	*iter = (struct z_erofs_bvec_iter) {
		.nr = bootstrap_nr,
		.bvset = (struct z_erofs_bvset *)bvset,
	};

	while (cur > iter->nr) {
		cur -= iter->nr;
		z_erofs_bvset_flip(iter);
	}
	iter->cur = cur;
}

static int z_erofs_bvec_enqueue(struct z_erofs_bvec_iter *iter,
				struct z_erofs_bvec *bvec,
				struct page **candidate_bvpage,
				struct page **pagepool)
{
	if (iter->cur >= iter->nr) {
		struct page *nextpage = *candidate_bvpage;

		if (!nextpage) {
			nextpage = erofs_allocpage(pagepool, GFP_NOFS);
			if (!nextpage)
				return -ENOMEM;
			set_page_private(nextpage, Z_EROFS_SHORTLIVED_PAGE);
		}
		DBG_BUGON(iter->bvset->nextpage);
		iter->bvset->nextpage = nextpage;
		z_erofs_bvset_flip(iter);

		iter->bvset->nextpage = NULL;
		*candidate_bvpage = NULL;
	}
	iter->bvset->bvec[iter->cur++] = *bvec;
	return 0;
}

static void z_erofs_bvec_dequeue(struct z_erofs_bvec_iter *iter,
				 struct z_erofs_bvec *bvec,
				 struct page **old_bvpage)
{
	if (iter->cur == iter->nr)
		*old_bvpage = z_erofs_bvset_flip(iter);
	else
		*old_bvpage = NULL;
	*bvec = iter->bvset->bvec[iter->cur++];
}

static void z_erofs_destroy_pcluster_pool(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pcluster_pool); ++i) {
		if (!pcluster_pool[i].slab)
			continue;
		kmem_cache_destroy(pcluster_pool[i].slab);
		pcluster_pool[i].slab = NULL;
	}
}

static int z_erofs_create_pcluster_pool(void)
{
	struct z_erofs_pcluster_slab *pcs;
	struct z_erofs_pcluster *a;
	unsigned int size;

	for (pcs = pcluster_pool;
	     pcs < pcluster_pool + ARRAY_SIZE(pcluster_pool); ++pcs) {
		size = struct_size(a, compressed_bvecs, pcs->maxpages);

		sprintf(pcs->name, "erofs_pcluster-%u", pcs->maxpages);
		pcs->slab = kmem_cache_create(pcs->name, size, 0,
					      SLAB_RECLAIM_ACCOUNT, NULL);
		if (pcs->slab)
			continue;

		z_erofs_destroy_pcluster_pool();
		return -ENOMEM;
	}
	return 0;
}

static struct z_erofs_pcluster *z_erofs_alloc_pcluster(unsigned int nrpages)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pcluster_pool); ++i) {
		struct z_erofs_pcluster_slab *pcs = pcluster_pool + i;
		struct z_erofs_pcluster *pcl;

		if (nrpages > pcs->maxpages)
			continue;

		pcl = kmem_cache_zalloc(pcs->slab, GFP_NOFS);
		if (!pcl)
			return ERR_PTR(-ENOMEM);
		pcl->pclusterpages = nrpages;
		return pcl;
	}
	return ERR_PTR(-EINVAL);
}

static void z_erofs_free_pcluster(struct z_erofs_pcluster *pcl)
{
	unsigned int pclusterpages = z_erofs_pclusterpages(pcl);
	int i;

	for (i = 0; i < ARRAY_SIZE(pcluster_pool); ++i) {
		struct z_erofs_pcluster_slab *pcs = pcluster_pool + i;

		if (pclusterpages > pcs->maxpages)
			continue;

		kmem_cache_free(pcs->slab, pcl);
		return;
	}
	DBG_BUGON(1);
}

static struct workqueue_struct *z_erofs_workqueue __read_mostly;

#ifdef CONFIG_EROFS_FS_PCPU_KTHREAD
static struct kthread_worker __rcu **z_erofs_pcpu_workers;

static void erofs_destroy_percpu_workers(void)
{
	struct kthread_worker *worker;
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		worker = rcu_dereference_protected(
					z_erofs_pcpu_workers[cpu], 1);
		rcu_assign_pointer(z_erofs_pcpu_workers[cpu], NULL);
		if (worker)
			kthread_destroy_worker(worker);
	}
	kfree(z_erofs_pcpu_workers);
}

static struct kthread_worker *erofs_init_percpu_worker(int cpu)
{
	struct kthread_worker *worker =
		kthread_create_worker_on_cpu(cpu, 0, "erofs_worker/%u", cpu);

	if (IS_ERR(worker))
		return worker;
	if (IS_ENABLED(CONFIG_EROFS_FS_PCPU_KTHREAD_HIPRI))
		sched_set_fifo_low(worker->task);
	return worker;
}

static int erofs_init_percpu_workers(void)
{
	struct kthread_worker *worker;
	unsigned int cpu;

	z_erofs_pcpu_workers = kcalloc(num_possible_cpus(),
			sizeof(struct kthread_worker *), GFP_ATOMIC);
	if (!z_erofs_pcpu_workers)
		return -ENOMEM;

	for_each_online_cpu(cpu) {	 
		worker = erofs_init_percpu_worker(cpu);
		if (!IS_ERR(worker))
			rcu_assign_pointer(z_erofs_pcpu_workers[cpu], worker);
	}
	return 0;
}
#else
static inline void erofs_destroy_percpu_workers(void) {}
static inline int erofs_init_percpu_workers(void) { return 0; }
#endif

#if defined(CONFIG_HOTPLUG_CPU) && defined(CONFIG_EROFS_FS_PCPU_KTHREAD)
static DEFINE_SPINLOCK(z_erofs_pcpu_worker_lock);
static enum cpuhp_state erofs_cpuhp_state;

static int erofs_cpu_online(unsigned int cpu)
{
	struct kthread_worker *worker, *old;

	worker = erofs_init_percpu_worker(cpu);
	if (IS_ERR(worker))
		return PTR_ERR(worker);

	spin_lock(&z_erofs_pcpu_worker_lock);
	old = rcu_dereference_protected(z_erofs_pcpu_workers[cpu],
			lockdep_is_held(&z_erofs_pcpu_worker_lock));
	if (!old)
		rcu_assign_pointer(z_erofs_pcpu_workers[cpu], worker);
	spin_unlock(&z_erofs_pcpu_worker_lock);
	if (old)
		kthread_destroy_worker(worker);
	return 0;
}

static int erofs_cpu_offline(unsigned int cpu)
{
	struct kthread_worker *worker;

	spin_lock(&z_erofs_pcpu_worker_lock);
	worker = rcu_dereference_protected(z_erofs_pcpu_workers[cpu],
			lockdep_is_held(&z_erofs_pcpu_worker_lock));
	rcu_assign_pointer(z_erofs_pcpu_workers[cpu], NULL);
	spin_unlock(&z_erofs_pcpu_worker_lock);

	synchronize_rcu();
	if (worker)
		kthread_destroy_worker(worker);
	return 0;
}

static int erofs_cpu_hotplug_init(void)
{
	int state;

	state = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
			"fs/erofs:online", erofs_cpu_online, erofs_cpu_offline);
	if (state < 0)
		return state;

	erofs_cpuhp_state = state;
	return 0;
}

static void erofs_cpu_hotplug_destroy(void)
{
	if (erofs_cpuhp_state)
		cpuhp_remove_state_nocalls(erofs_cpuhp_state);
}
#else  
static inline int erofs_cpu_hotplug_init(void) { return 0; }
static inline void erofs_cpu_hotplug_destroy(void) {}
#endif

void z_erofs_exit_zip_subsystem(void)
{
	erofs_cpu_hotplug_destroy();
	erofs_destroy_percpu_workers();
	destroy_workqueue(z_erofs_workqueue);
	z_erofs_destroy_pcluster_pool();
}

int __init z_erofs_init_zip_subsystem(void)
{
	int err = z_erofs_create_pcluster_pool();

	if (err)
		goto out_error_pcluster_pool;

	z_erofs_workqueue = alloc_workqueue("erofs_worker",
			WQ_UNBOUND | WQ_HIGHPRI, num_possible_cpus());
	if (!z_erofs_workqueue) {
		err = -ENOMEM;
		goto out_error_workqueue_init;
	}

	err = erofs_init_percpu_workers();
	if (err)
		goto out_error_pcpu_worker;

	err = erofs_cpu_hotplug_init();
	if (err < 0)
		goto out_error_cpuhp_init;
	return err;

out_error_cpuhp_init:
	erofs_destroy_percpu_workers();
out_error_pcpu_worker:
	destroy_workqueue(z_erofs_workqueue);
out_error_workqueue_init:
	z_erofs_destroy_pcluster_pool();
out_error_pcluster_pool:
	return err;
}

enum z_erofs_pclustermode {
	Z_EROFS_PCLUSTER_INFLIGHT,
	 
	Z_EROFS_PCLUSTER_FOLLOWED_NOINPLACE,
	 
	Z_EROFS_PCLUSTER_FOLLOWED,
};

struct z_erofs_decompress_frontend {
	struct inode *const inode;
	struct erofs_map_blocks map;
	struct z_erofs_bvec_iter biter;

	struct page *pagepool;
	struct page *candidate_bvpage;
	struct z_erofs_pcluster *pcl;
	z_erofs_next_pcluster_t owned_head;
	enum z_erofs_pclustermode mode;

	erofs_off_t headoffset;

	 
	unsigned int icur;
};

#define DECOMPRESS_FRONTEND_INIT(__i) { \
	.inode = __i, .owned_head = Z_EROFS_PCLUSTER_TAIL, \
	.mode = Z_EROFS_PCLUSTER_FOLLOWED }

static bool z_erofs_should_alloc_cache(struct z_erofs_decompress_frontend *fe)
{
	unsigned int cachestrategy = EROFS_I_SB(fe->inode)->opt.cache_strategy;

	if (cachestrategy <= EROFS_ZIP_CACHE_DISABLED)
		return false;

	if (!(fe->map.m_flags & EROFS_MAP_FULL_MAPPED))
		return true;

	if (cachestrategy >= EROFS_ZIP_CACHE_READAROUND &&
	    fe->map.m_la < fe->headoffset)
		return true;

	return false;
}

static void z_erofs_bind_cache(struct z_erofs_decompress_frontend *fe)
{
	struct address_space *mc = MNGD_MAPPING(EROFS_I_SB(fe->inode));
	struct z_erofs_pcluster *pcl = fe->pcl;
	bool shouldalloc = z_erofs_should_alloc_cache(fe);
	bool standalone = true;
	 
	gfp_t gfp = (mapping_gfp_mask(mc) & ~__GFP_DIRECT_RECLAIM) |
			__GFP_NOMEMALLOC | __GFP_NORETRY | __GFP_NOWARN;
	unsigned int i;

	if (fe->mode < Z_EROFS_PCLUSTER_FOLLOWED)
		return;

	for (i = 0; i < pcl->pclusterpages; ++i) {
		struct page *page;
		void *t;	 
		struct page *newpage = NULL;

		 
		if (READ_ONCE(pcl->compressed_bvecs[i].page))
			continue;

		page = find_get_page(mc, pcl->obj.index + i);

		if (page) {
			t = (void *)((unsigned long)page | 1);
		} else {
			 
			standalone = false;
			if (!shouldalloc)
				continue;

			 
			newpage = erofs_allocpage(&fe->pagepool, gfp);
			if (!newpage)
				continue;
			set_page_private(newpage, Z_EROFS_PREALLOCATED_PAGE);
			t = (void *)((unsigned long)newpage | 1);
		}

		if (!cmpxchg_relaxed(&pcl->compressed_bvecs[i].page, NULL, t))
			continue;

		if (page)
			put_page(page);
		else if (newpage)
			erofs_pagepool_add(&fe->pagepool, newpage);
	}

	 
	if (standalone)
		fe->mode = Z_EROFS_PCLUSTER_FOLLOWED_NOINPLACE;
}

 
int erofs_try_to_free_all_cached_pages(struct erofs_sb_info *sbi,
				       struct erofs_workgroup *grp)
{
	struct z_erofs_pcluster *const pcl =
		container_of(grp, struct z_erofs_pcluster, obj);
	int i;

	DBG_BUGON(z_erofs_is_inline_pcluster(pcl));
	 
	for (i = 0; i < pcl->pclusterpages; ++i) {
		struct page *page = pcl->compressed_bvecs[i].page;

		if (!page)
			continue;

		 
		if (!trylock_page(page))
			return -EBUSY;

		if (!erofs_page_is_managed(sbi, page))
			continue;

		 
		WRITE_ONCE(pcl->compressed_bvecs[i].page, NULL);
		detach_page_private(page);
		unlock_page(page);
	}
	return 0;
}

static bool z_erofs_cache_release_folio(struct folio *folio, gfp_t gfp)
{
	struct z_erofs_pcluster *pcl = folio_get_private(folio);
	bool ret;
	int i;

	if (!folio_test_private(folio))
		return true;

	ret = false;
	spin_lock(&pcl->obj.lockref.lock);
	if (pcl->obj.lockref.count > 0)
		goto out;

	DBG_BUGON(z_erofs_is_inline_pcluster(pcl));
	for (i = 0; i < pcl->pclusterpages; ++i) {
		if (pcl->compressed_bvecs[i].page == &folio->page) {
			WRITE_ONCE(pcl->compressed_bvecs[i].page, NULL);
			ret = true;
			break;
		}
	}
	if (ret)
		folio_detach_private(folio);
out:
	spin_unlock(&pcl->obj.lockref.lock);
	return ret;
}

 
static void z_erofs_cache_invalidate_folio(struct folio *folio,
					   size_t offset, size_t length)
{
	const size_t stop = length + offset;

	 
	DBG_BUGON(stop > folio_size(folio) || stop < length);

	if (offset == 0 && stop == folio_size(folio))
		while (!z_erofs_cache_release_folio(folio, GFP_NOFS))
			cond_resched();
}

static const struct address_space_operations z_erofs_cache_aops = {
	.release_folio = z_erofs_cache_release_folio,
	.invalidate_folio = z_erofs_cache_invalidate_folio,
};

int erofs_init_managed_cache(struct super_block *sb)
{
	struct inode *const inode = new_inode(sb);

	if (!inode)
		return -ENOMEM;

	set_nlink(inode, 1);
	inode->i_size = OFFSET_MAX;
	inode->i_mapping->a_ops = &z_erofs_cache_aops;
	mapping_set_gfp_mask(inode->i_mapping, GFP_NOFS);
	EROFS_SB(sb)->managed_cache = inode;
	return 0;
}

static bool z_erofs_try_inplace_io(struct z_erofs_decompress_frontend *fe,
				   struct z_erofs_bvec *bvec)
{
	struct z_erofs_pcluster *const pcl = fe->pcl;

	while (fe->icur > 0) {
		if (!cmpxchg(&pcl->compressed_bvecs[--fe->icur].page,
			     NULL, bvec->page)) {
			pcl->compressed_bvecs[fe->icur] = *bvec;
			return true;
		}
	}
	return false;
}

 
static int z_erofs_attach_page(struct z_erofs_decompress_frontend *fe,
			       struct z_erofs_bvec *bvec, bool exclusive)
{
	int ret;

	if (exclusive) {
		 
		if (z_erofs_try_inplace_io(fe, bvec))
			return 0;
		 
		if (fe->mode >= Z_EROFS_PCLUSTER_FOLLOWED &&
		    !fe->candidate_bvpage)
			fe->candidate_bvpage = bvec->page;
	}
	ret = z_erofs_bvec_enqueue(&fe->biter, bvec, &fe->candidate_bvpage,
				   &fe->pagepool);
	fe->pcl->vcnt += (ret >= 0);
	return ret;
}

static void z_erofs_try_to_claim_pcluster(struct z_erofs_decompress_frontend *f)
{
	struct z_erofs_pcluster *pcl = f->pcl;
	z_erofs_next_pcluster_t *owned_head = &f->owned_head;

	 
	if (cmpxchg(&pcl->next, Z_EROFS_PCLUSTER_NIL,
		    *owned_head) == Z_EROFS_PCLUSTER_NIL) {
		*owned_head = &pcl->next;
		 
		f->mode = Z_EROFS_PCLUSTER_FOLLOWED;
		return;
	}

	 
	f->mode = Z_EROFS_PCLUSTER_INFLIGHT;
}

static int z_erofs_register_pcluster(struct z_erofs_decompress_frontend *fe)
{
	struct erofs_map_blocks *map = &fe->map;
	bool ztailpacking = map->m_flags & EROFS_MAP_META;
	struct z_erofs_pcluster *pcl;
	struct erofs_workgroup *grp;
	int err;

	if (!(map->m_flags & EROFS_MAP_ENCODED) ||
	    (!ztailpacking && !(map->m_pa >> PAGE_SHIFT))) {
		DBG_BUGON(1);
		return -EFSCORRUPTED;
	}

	 
	pcl = z_erofs_alloc_pcluster(ztailpacking ? 1 :
				     map->m_plen >> PAGE_SHIFT);
	if (IS_ERR(pcl))
		return PTR_ERR(pcl);

	spin_lock_init(&pcl->obj.lockref.lock);
	pcl->obj.lockref.count = 1;	 
	pcl->algorithmformat = map->m_algorithmformat;
	pcl->length = 0;
	pcl->partial = true;

	 
	pcl->next = fe->owned_head;
	pcl->pageofs_out = map->m_la & ~PAGE_MASK;
	fe->mode = Z_EROFS_PCLUSTER_FOLLOWED;

	 
	mutex_init(&pcl->lock);
	DBG_BUGON(!mutex_trylock(&pcl->lock));

	if (ztailpacking) {
		pcl->obj.index = 0;	 
		pcl->pageofs_in = erofs_blkoff(fe->inode->i_sb, map->m_pa);
		pcl->tailpacking_size = map->m_plen;
	} else {
		pcl->obj.index = map->m_pa >> PAGE_SHIFT;

		grp = erofs_insert_workgroup(fe->inode->i_sb, &pcl->obj);
		if (IS_ERR(grp)) {
			err = PTR_ERR(grp);
			goto err_out;
		}

		if (grp != &pcl->obj) {
			fe->pcl = container_of(grp,
					struct z_erofs_pcluster, obj);
			err = -EEXIST;
			goto err_out;
		}
	}
	fe->owned_head = &pcl->next;
	fe->pcl = pcl;
	return 0;

err_out:
	mutex_unlock(&pcl->lock);
	z_erofs_free_pcluster(pcl);
	return err;
}

static int z_erofs_pcluster_begin(struct z_erofs_decompress_frontend *fe)
{
	struct erofs_map_blocks *map = &fe->map;
	struct super_block *sb = fe->inode->i_sb;
	erofs_blk_t blknr = erofs_blknr(sb, map->m_pa);
	struct erofs_workgroup *grp = NULL;
	int ret;

	DBG_BUGON(fe->pcl);

	 
	DBG_BUGON(fe->owned_head == Z_EROFS_PCLUSTER_NIL);

	if (!(map->m_flags & EROFS_MAP_META)) {
		grp = erofs_find_workgroup(sb, blknr);
	} else if ((map->m_pa & ~PAGE_MASK) + map->m_plen > PAGE_SIZE) {
		DBG_BUGON(1);
		return -EFSCORRUPTED;
	}

	if (grp) {
		fe->pcl = container_of(grp, struct z_erofs_pcluster, obj);
		ret = -EEXIST;
	} else {
		ret = z_erofs_register_pcluster(fe);
	}

	if (ret == -EEXIST) {
		mutex_lock(&fe->pcl->lock);
		z_erofs_try_to_claim_pcluster(fe);
	} else if (ret) {
		return ret;
	}

	z_erofs_bvec_iter_begin(&fe->biter, &fe->pcl->bvset,
				Z_EROFS_INLINE_BVECS, fe->pcl->vcnt);
	if (!z_erofs_is_inline_pcluster(fe->pcl)) {
		 
		z_erofs_bind_cache(fe);
	} else {
		void *mptr;

		mptr = erofs_read_metabuf(&map->buf, sb, blknr, EROFS_NO_KMAP);
		if (IS_ERR(mptr)) {
			ret = PTR_ERR(mptr);
			erofs_err(sb, "failed to get inline data %d", ret);
			return ret;
		}
		get_page(map->buf.page);
		WRITE_ONCE(fe->pcl->compressed_bvecs[0].page, map->buf.page);
		fe->mode = Z_EROFS_PCLUSTER_FOLLOWED_NOINPLACE;
	}
	 
	fe->icur = z_erofs_pclusterpages(fe->pcl);
	return 0;
}

 
static void z_erofs_rcu_callback(struct rcu_head *head)
{
	z_erofs_free_pcluster(container_of(head,
			struct z_erofs_pcluster, rcu));
}

void erofs_workgroup_free_rcu(struct erofs_workgroup *grp)
{
	struct z_erofs_pcluster *const pcl =
		container_of(grp, struct z_erofs_pcluster, obj);

	call_rcu(&pcl->rcu, z_erofs_rcu_callback);
}

static void z_erofs_pcluster_end(struct z_erofs_decompress_frontend *fe)
{
	struct z_erofs_pcluster *pcl = fe->pcl;

	if (!pcl)
		return;

	z_erofs_bvec_iter_end(&fe->biter);
	mutex_unlock(&pcl->lock);

	if (fe->candidate_bvpage)
		fe->candidate_bvpage = NULL;

	 
	if (fe->mode < Z_EROFS_PCLUSTER_FOLLOWED_NOINPLACE)
		erofs_workgroup_put(&pcl->obj);

	fe->pcl = NULL;
}

static int z_erofs_read_fragment(struct super_block *sb, struct page *page,
			unsigned int cur, unsigned int end, erofs_off_t pos)
{
	struct inode *packed_inode = EROFS_SB(sb)->packed_inode;
	struct erofs_buf buf = __EROFS_BUF_INITIALIZER;
	unsigned int cnt;
	u8 *src;

	if (!packed_inode)
		return -EFSCORRUPTED;

	buf.inode = packed_inode;
	for (; cur < end; cur += cnt, pos += cnt) {
		cnt = min_t(unsigned int, end - cur,
			    sb->s_blocksize - erofs_blkoff(sb, pos));
		src = erofs_bread(&buf, erofs_blknr(sb, pos), EROFS_KMAP);
		if (IS_ERR(src)) {
			erofs_put_metabuf(&buf);
			return PTR_ERR(src);
		}
		memcpy_to_page(page, cur, src + erofs_blkoff(sb, pos), cnt);
	}
	erofs_put_metabuf(&buf);
	return 0;
}

static int z_erofs_do_read_page(struct z_erofs_decompress_frontend *fe,
				struct page *page)
{
	struct inode *const inode = fe->inode;
	struct erofs_map_blocks *const map = &fe->map;
	const loff_t offset = page_offset(page);
	bool tight = true, exclusive;
	unsigned int cur, end, len, split;
	int err = 0;

	z_erofs_onlinepage_init(page);

	split = 0;
	end = PAGE_SIZE;
repeat:
	if (offset + end - 1 < map->m_la ||
	    offset + end - 1 >= map->m_la + map->m_llen) {
		z_erofs_pcluster_end(fe);
		map->m_la = offset + end - 1;
		map->m_llen = 0;
		err = z_erofs_map_blocks_iter(inode, map, 0);
		if (err)
			goto out;
	}

	cur = offset > map->m_la ? 0 : map->m_la - offset;
	 
	++split;

	if (!(map->m_flags & EROFS_MAP_MAPPED)) {
		zero_user_segment(page, cur, end);
		tight = false;
		goto next_part;
	}

	if (map->m_flags & EROFS_MAP_FRAGMENT) {
		erofs_off_t fpos = offset + cur - map->m_la;

		len = min_t(unsigned int, map->m_llen - fpos, end - cur);
		err = z_erofs_read_fragment(inode->i_sb, page, cur, cur + len,
				EROFS_I(inode)->z_fragmentoff + fpos);
		if (err)
			goto out;
		tight = false;
		goto next_part;
	}

	if (!fe->pcl) {
		err = z_erofs_pcluster_begin(fe);
		if (err)
			goto out;
	}

	 
	tight &= (fe->mode > Z_EROFS_PCLUSTER_FOLLOWED_NOINPLACE);
	exclusive = (!cur && ((split <= 1) || tight));
	if (cur)
		tight &= (fe->mode >= Z_EROFS_PCLUSTER_FOLLOWED);

	err = z_erofs_attach_page(fe, &((struct z_erofs_bvec) {
					.page = page,
					.offset = offset - map->m_la,
					.end = end,
				  }), exclusive);
	if (err)
		goto out;

	z_erofs_onlinepage_split(page);
	if (fe->pcl->pageofs_out != (map->m_la & ~PAGE_MASK))
		fe->pcl->multibases = true;
	if (fe->pcl->length < offset + end - map->m_la) {
		fe->pcl->length = offset + end - map->m_la;
		fe->pcl->pageofs_out = map->m_la & ~PAGE_MASK;
	}
	if ((map->m_flags & EROFS_MAP_FULL_MAPPED) &&
	    !(map->m_flags & EROFS_MAP_PARTIAL_REF) &&
	    fe->pcl->length == map->m_llen)
		fe->pcl->partial = false;
next_part:
	 
	map->m_llen = offset + cur - map->m_la;
	map->m_flags &= ~EROFS_MAP_FULL_MAPPED;

	end = cur;
	if (end > 0)
		goto repeat;

out:
	z_erofs_onlinepage_endio(page, err);
	return err;
}

static bool z_erofs_is_sync_decompress(struct erofs_sb_info *sbi,
				       unsigned int readahead_pages)
{
	 
	if ((sbi->opt.sync_decompress == EROFS_SYNC_DECOMPRESS_AUTO) &&
	    !readahead_pages)
		return true;

	if ((sbi->opt.sync_decompress == EROFS_SYNC_DECOMPRESS_FORCE_ON) &&
	    (readahead_pages <= sbi->opt.max_sync_decompress_pages))
		return true;

	return false;
}

static bool z_erofs_page_is_invalidated(struct page *page)
{
	return !page->mapping && !z_erofs_is_shortlived_page(page);
}

struct z_erofs_decompress_backend {
	struct page *onstack_pages[Z_EROFS_ONSTACK_PAGES];
	struct super_block *sb;
	struct z_erofs_pcluster *pcl;

	 
	struct page **decompressed_pages;
	 
	struct page **compressed_pages;

	struct list_head decompressed_secondary_bvecs;
	struct page **pagepool;
	unsigned int onstack_used, nr_pages;
};

struct z_erofs_bvec_item {
	struct z_erofs_bvec bvec;
	struct list_head list;
};

static void z_erofs_do_decompressed_bvec(struct z_erofs_decompress_backend *be,
					 struct z_erofs_bvec *bvec)
{
	struct z_erofs_bvec_item *item;
	unsigned int pgnr;

	if (!((bvec->offset + be->pcl->pageofs_out) & ~PAGE_MASK) &&
	    (bvec->end == PAGE_SIZE ||
	     bvec->offset + bvec->end == be->pcl->length)) {
		pgnr = (bvec->offset + be->pcl->pageofs_out) >> PAGE_SHIFT;
		DBG_BUGON(pgnr >= be->nr_pages);
		if (!be->decompressed_pages[pgnr]) {
			be->decompressed_pages[pgnr] = bvec->page;
			return;
		}
	}

	 
	item = kmalloc(sizeof(*item), GFP_KERNEL | __GFP_NOFAIL);
	item->bvec = *bvec;
	list_add(&item->list, &be->decompressed_secondary_bvecs);
}

static void z_erofs_fill_other_copies(struct z_erofs_decompress_backend *be,
				      int err)
{
	unsigned int off0 = be->pcl->pageofs_out;
	struct list_head *p, *n;

	list_for_each_safe(p, n, &be->decompressed_secondary_bvecs) {
		struct z_erofs_bvec_item *bvi;
		unsigned int end, cur;
		void *dst, *src;

		bvi = container_of(p, struct z_erofs_bvec_item, list);
		cur = bvi->bvec.offset < 0 ? -bvi->bvec.offset : 0;
		end = min_t(unsigned int, be->pcl->length - bvi->bvec.offset,
			    bvi->bvec.end);
		dst = kmap_local_page(bvi->bvec.page);
		while (cur < end) {
			unsigned int pgnr, scur, len;

			pgnr = (bvi->bvec.offset + cur + off0) >> PAGE_SHIFT;
			DBG_BUGON(pgnr >= be->nr_pages);

			scur = bvi->bvec.offset + cur -
					((pgnr << PAGE_SHIFT) - off0);
			len = min_t(unsigned int, end - cur, PAGE_SIZE - scur);
			if (!be->decompressed_pages[pgnr]) {
				err = -EFSCORRUPTED;
				cur += len;
				continue;
			}
			src = kmap_local_page(be->decompressed_pages[pgnr]);
			memcpy(dst + cur, src + scur, len);
			kunmap_local(src);
			cur += len;
		}
		kunmap_local(dst);
		z_erofs_onlinepage_endio(bvi->bvec.page, err);
		list_del(p);
		kfree(bvi);
	}
}

static void z_erofs_parse_out_bvecs(struct z_erofs_decompress_backend *be)
{
	struct z_erofs_pcluster *pcl = be->pcl;
	struct z_erofs_bvec_iter biter;
	struct page *old_bvpage;
	int i;

	z_erofs_bvec_iter_begin(&biter, &pcl->bvset, Z_EROFS_INLINE_BVECS, 0);
	for (i = 0; i < pcl->vcnt; ++i) {
		struct z_erofs_bvec bvec;

		z_erofs_bvec_dequeue(&biter, &bvec, &old_bvpage);

		if (old_bvpage)
			z_erofs_put_shortlivedpage(be->pagepool, old_bvpage);

		DBG_BUGON(z_erofs_page_is_invalidated(bvec.page));
		z_erofs_do_decompressed_bvec(be, &bvec);
	}

	old_bvpage = z_erofs_bvec_iter_end(&biter);
	if (old_bvpage)
		z_erofs_put_shortlivedpage(be->pagepool, old_bvpage);
}

static int z_erofs_parse_in_bvecs(struct z_erofs_decompress_backend *be,
				  bool *overlapped)
{
	struct z_erofs_pcluster *pcl = be->pcl;
	unsigned int pclusterpages = z_erofs_pclusterpages(pcl);
	int i, err = 0;

	*overlapped = false;
	for (i = 0; i < pclusterpages; ++i) {
		struct z_erofs_bvec *bvec = &pcl->compressed_bvecs[i];
		struct page *page = bvec->page;

		 
		if (!page) {
			DBG_BUGON(1);
			continue;
		}
		be->compressed_pages[i] = page;

		if (z_erofs_is_inline_pcluster(pcl)) {
			if (!PageUptodate(page))
				err = -EIO;
			continue;
		}

		DBG_BUGON(z_erofs_page_is_invalidated(page));
		if (!z_erofs_is_shortlived_page(page)) {
			if (erofs_page_is_managed(EROFS_SB(be->sb), page)) {
				if (!PageUptodate(page))
					err = -EIO;
				continue;
			}
			z_erofs_do_decompressed_bvec(be, bvec);
			*overlapped = true;
		}
	}

	if (err)
		return err;
	return 0;
}

static int z_erofs_decompress_pcluster(struct z_erofs_decompress_backend *be,
				       int err)
{
	struct erofs_sb_info *const sbi = EROFS_SB(be->sb);
	struct z_erofs_pcluster *pcl = be->pcl;
	unsigned int pclusterpages = z_erofs_pclusterpages(pcl);
	const struct z_erofs_decompressor *decompressor =
				&erofs_decompressors[pcl->algorithmformat];
	unsigned int i, inputsize;
	int err2;
	struct page *page;
	bool overlapped;

	mutex_lock(&pcl->lock);
	be->nr_pages = PAGE_ALIGN(pcl->length + pcl->pageofs_out) >> PAGE_SHIFT;

	 
	be->decompressed_pages = NULL;
	be->compressed_pages = NULL;
	be->onstack_used = 0;
	if (be->nr_pages <= Z_EROFS_ONSTACK_PAGES) {
		be->decompressed_pages = be->onstack_pages;
		be->onstack_used = be->nr_pages;
		memset(be->decompressed_pages, 0,
		       sizeof(struct page *) * be->nr_pages);
	}

	if (pclusterpages + be->onstack_used <= Z_EROFS_ONSTACK_PAGES)
		be->compressed_pages = be->onstack_pages + be->onstack_used;

	if (!be->decompressed_pages)
		be->decompressed_pages =
			kvcalloc(be->nr_pages, sizeof(struct page *),
				 GFP_KERNEL | __GFP_NOFAIL);
	if (!be->compressed_pages)
		be->compressed_pages =
			kvcalloc(pclusterpages, sizeof(struct page *),
				 GFP_KERNEL | __GFP_NOFAIL);

	z_erofs_parse_out_bvecs(be);
	err2 = z_erofs_parse_in_bvecs(be, &overlapped);
	if (err2)
		err = err2;
	if (err)
		goto out;

	if (z_erofs_is_inline_pcluster(pcl))
		inputsize = pcl->tailpacking_size;
	else
		inputsize = pclusterpages * PAGE_SIZE;

	err = decompressor->decompress(&(struct z_erofs_decompress_req) {
					.sb = be->sb,
					.in = be->compressed_pages,
					.out = be->decompressed_pages,
					.pageofs_in = pcl->pageofs_in,
					.pageofs_out = pcl->pageofs_out,
					.inputsize = inputsize,
					.outputsize = pcl->length,
					.alg = pcl->algorithmformat,
					.inplace_io = overlapped,
					.partial_decoding = pcl->partial,
					.fillgaps = pcl->multibases,
				 }, be->pagepool);

out:
	 
	if (z_erofs_is_inline_pcluster(pcl)) {
		page = pcl->compressed_bvecs[0].page;
		WRITE_ONCE(pcl->compressed_bvecs[0].page, NULL);
		put_page(page);
	} else {
		for (i = 0; i < pclusterpages; ++i) {
			 
			page = be->compressed_pages[i];

			if (erofs_page_is_managed(sbi, page))
				continue;
			(void)z_erofs_put_shortlivedpage(be->pagepool, page);
			WRITE_ONCE(pcl->compressed_bvecs[i].page, NULL);
		}
	}
	if (be->compressed_pages < be->onstack_pages ||
	    be->compressed_pages >= be->onstack_pages + Z_EROFS_ONSTACK_PAGES)
		kvfree(be->compressed_pages);
	z_erofs_fill_other_copies(be, err);

	for (i = 0; i < be->nr_pages; ++i) {
		page = be->decompressed_pages[i];
		if (!page)
			continue;

		DBG_BUGON(z_erofs_page_is_invalidated(page));

		 
		if (z_erofs_put_shortlivedpage(be->pagepool, page))
			continue;
		z_erofs_onlinepage_endio(page, err);
	}

	if (be->decompressed_pages != be->onstack_pages)
		kvfree(be->decompressed_pages);

	pcl->length = 0;
	pcl->partial = true;
	pcl->multibases = false;
	pcl->bvset.nextpage = NULL;
	pcl->vcnt = 0;

	 
	WRITE_ONCE(pcl->next, Z_EROFS_PCLUSTER_NIL);
	mutex_unlock(&pcl->lock);
	return err;
}

static void z_erofs_decompress_queue(const struct z_erofs_decompressqueue *io,
				     struct page **pagepool)
{
	struct z_erofs_decompress_backend be = {
		.sb = io->sb,
		.pagepool = pagepool,
		.decompressed_secondary_bvecs =
			LIST_HEAD_INIT(be.decompressed_secondary_bvecs),
	};
	z_erofs_next_pcluster_t owned = io->head;

	while (owned != Z_EROFS_PCLUSTER_TAIL) {
		DBG_BUGON(owned == Z_EROFS_PCLUSTER_NIL);

		be.pcl = container_of(owned, struct z_erofs_pcluster, next);
		owned = READ_ONCE(be.pcl->next);

		z_erofs_decompress_pcluster(&be, io->eio ? -EIO : 0);
		if (z_erofs_is_inline_pcluster(be.pcl))
			z_erofs_free_pcluster(be.pcl);
		else
			erofs_workgroup_put(&be.pcl->obj);
	}
}

static void z_erofs_decompressqueue_work(struct work_struct *work)
{
	struct z_erofs_decompressqueue *bgq =
		container_of(work, struct z_erofs_decompressqueue, u.work);
	struct page *pagepool = NULL;

	DBG_BUGON(bgq->head == Z_EROFS_PCLUSTER_TAIL);
	z_erofs_decompress_queue(bgq, &pagepool);
	erofs_release_pages(&pagepool);
	kvfree(bgq);
}

#ifdef CONFIG_EROFS_FS_PCPU_KTHREAD
static void z_erofs_decompressqueue_kthread_work(struct kthread_work *work)
{
	z_erofs_decompressqueue_work((struct work_struct *)work);
}
#endif

static void z_erofs_decompress_kickoff(struct z_erofs_decompressqueue *io,
				       int bios)
{
	struct erofs_sb_info *const sbi = EROFS_SB(io->sb);

	 
	if (io->sync) {
		if (!atomic_add_return(bios, &io->pending_bios))
			complete(&io->u.done);
		return;
	}

	if (atomic_add_return(bios, &io->pending_bios))
		return;
	 
	if (!in_task() || irqs_disabled() || rcu_read_lock_any_held()) {
#ifdef CONFIG_EROFS_FS_PCPU_KTHREAD
		struct kthread_worker *worker;

		rcu_read_lock();
		worker = rcu_dereference(
				z_erofs_pcpu_workers[raw_smp_processor_id()]);
		if (!worker) {
			INIT_WORK(&io->u.work, z_erofs_decompressqueue_work);
			queue_work(z_erofs_workqueue, &io->u.work);
		} else {
			kthread_queue_work(worker, &io->u.kthread_work);
		}
		rcu_read_unlock();
#else
		queue_work(z_erofs_workqueue, &io->u.work);
#endif
		 
		if (sbi->opt.sync_decompress == EROFS_SYNC_DECOMPRESS_AUTO)
			sbi->opt.sync_decompress = EROFS_SYNC_DECOMPRESS_FORCE_ON;
		return;
	}
	z_erofs_decompressqueue_work(&io->u.work);
}

static struct page *pickup_page_for_submission(struct z_erofs_pcluster *pcl,
					       unsigned int nr,
					       struct page **pagepool,
					       struct address_space *mc)
{
	const pgoff_t index = pcl->obj.index;
	gfp_t gfp = mapping_gfp_mask(mc);
	bool tocache = false;

	struct address_space *mapping;
	struct page *oldpage, *page;
	int justfound;

repeat:
	page = READ_ONCE(pcl->compressed_bvecs[nr].page);
	oldpage = page;

	if (!page)
		goto out_allocpage;

	justfound = (unsigned long)page & 1UL;
	page = (struct page *)((unsigned long)page & ~1UL);

	 
	if (page->private == Z_EROFS_PREALLOCATED_PAGE) {
		WRITE_ONCE(pcl->compressed_bvecs[nr].page, page);
		set_page_private(page, 0);
		tocache = true;
		goto out_tocache;
	}
	mapping = READ_ONCE(page->mapping);

	 
	if (mapping && mapping != mc)
		 
		goto out;

	 
	if (z_erofs_is_shortlived_page(page))
		goto out;

	lock_page(page);

	 
	DBG_BUGON(justfound && PagePrivate(page));

	 
	if (page->mapping == mc) {
		WRITE_ONCE(pcl->compressed_bvecs[nr].page, page);

		if (!PagePrivate(page)) {
			 
			DBG_BUGON(!justfound);

			justfound = 0;
			set_page_private(page, (unsigned long)pcl);
			SetPagePrivate(page);
		}

		 
		if (PageUptodate(page)) {
			unlock_page(page);
			page = NULL;
		}
		goto out;
	}

	 
	DBG_BUGON(page->mapping);
	DBG_BUGON(!justfound);

	tocache = true;
	unlock_page(page);
	put_page(page);
out_allocpage:
	page = erofs_allocpage(pagepool, gfp | __GFP_NOFAIL);
	if (oldpage != cmpxchg(&pcl->compressed_bvecs[nr].page,
			       oldpage, page)) {
		erofs_pagepool_add(pagepool, page);
		cond_resched();
		goto repeat;
	}
out_tocache:
	if (!tocache || add_to_page_cache_lru(page, mc, index + nr, gfp)) {
		 
		set_page_private(page, Z_EROFS_SHORTLIVED_PAGE);
		goto out;
	}
	attach_page_private(page, pcl);
	 
	put_page(page);

out:	 
	return page;
}

static struct z_erofs_decompressqueue *jobqueue_init(struct super_block *sb,
			      struct z_erofs_decompressqueue *fgq, bool *fg)
{
	struct z_erofs_decompressqueue *q;

	if (fg && !*fg) {
		q = kvzalloc(sizeof(*q), GFP_KERNEL | __GFP_NOWARN);
		if (!q) {
			*fg = true;
			goto fg_out;
		}
#ifdef CONFIG_EROFS_FS_PCPU_KTHREAD
		kthread_init_work(&q->u.kthread_work,
				  z_erofs_decompressqueue_kthread_work);
#else
		INIT_WORK(&q->u.work, z_erofs_decompressqueue_work);
#endif
	} else {
fg_out:
		q = fgq;
		init_completion(&fgq->u.done);
		atomic_set(&fgq->pending_bios, 0);
		q->eio = false;
		q->sync = true;
	}
	q->sb = sb;
	q->head = Z_EROFS_PCLUSTER_TAIL;
	return q;
}

 
enum {
	JQ_BYPASS,
	JQ_SUBMIT,
	NR_JOBQUEUES,
};

static void move_to_bypass_jobqueue(struct z_erofs_pcluster *pcl,
				    z_erofs_next_pcluster_t qtail[],
				    z_erofs_next_pcluster_t owned_head)
{
	z_erofs_next_pcluster_t *const submit_qtail = qtail[JQ_SUBMIT];
	z_erofs_next_pcluster_t *const bypass_qtail = qtail[JQ_BYPASS];

	WRITE_ONCE(pcl->next, Z_EROFS_PCLUSTER_TAIL);

	WRITE_ONCE(*submit_qtail, owned_head);
	WRITE_ONCE(*bypass_qtail, &pcl->next);

	qtail[JQ_BYPASS] = &pcl->next;
}

static void z_erofs_decompressqueue_endio(struct bio *bio)
{
	struct z_erofs_decompressqueue *q = bio->bi_private;
	blk_status_t err = bio->bi_status;
	struct bio_vec *bvec;
	struct bvec_iter_all iter_all;

	bio_for_each_segment_all(bvec, bio, iter_all) {
		struct page *page = bvec->bv_page;

		DBG_BUGON(PageUptodate(page));
		DBG_BUGON(z_erofs_page_is_invalidated(page));

		if (erofs_page_is_managed(EROFS_SB(q->sb), page)) {
			if (!err)
				SetPageUptodate(page);
			unlock_page(page);
		}
	}
	if (err)
		q->eio = true;
	z_erofs_decompress_kickoff(q, -1);
	bio_put(bio);
}

static void z_erofs_submit_queue(struct z_erofs_decompress_frontend *f,
				 struct z_erofs_decompressqueue *fgq,
				 bool *force_fg, bool readahead)
{
	struct super_block *sb = f->inode->i_sb;
	struct address_space *mc = MNGD_MAPPING(EROFS_SB(sb));
	z_erofs_next_pcluster_t qtail[NR_JOBQUEUES];
	struct z_erofs_decompressqueue *q[NR_JOBQUEUES];
	z_erofs_next_pcluster_t owned_head = f->owned_head;
	 
	pgoff_t last_index;
	struct block_device *last_bdev;
	unsigned int nr_bios = 0;
	struct bio *bio = NULL;
	unsigned long pflags;
	int memstall = 0;

	 
	q[JQ_BYPASS] = jobqueue_init(sb, fgq + JQ_BYPASS, NULL);
	q[JQ_SUBMIT] = jobqueue_init(sb, fgq + JQ_SUBMIT, force_fg);

	qtail[JQ_BYPASS] = &q[JQ_BYPASS]->head;
	qtail[JQ_SUBMIT] = &q[JQ_SUBMIT]->head;

	 
	q[JQ_SUBMIT]->head = owned_head;

	do {
		struct erofs_map_dev mdev;
		struct z_erofs_pcluster *pcl;
		pgoff_t cur, end;
		unsigned int i = 0;
		bool bypass = true;

		DBG_BUGON(owned_head == Z_EROFS_PCLUSTER_NIL);
		pcl = container_of(owned_head, struct z_erofs_pcluster, next);
		owned_head = READ_ONCE(pcl->next);

		if (z_erofs_is_inline_pcluster(pcl)) {
			move_to_bypass_jobqueue(pcl, qtail, owned_head);
			continue;
		}

		 
		mdev = (struct erofs_map_dev) {
			.m_pa = erofs_pos(sb, pcl->obj.index),
		};
		(void)erofs_map_dev(sb, &mdev);

		cur = erofs_blknr(sb, mdev.m_pa);
		end = cur + pcl->pclusterpages;

		do {
			struct page *page;

			page = pickup_page_for_submission(pcl, i++,
					&f->pagepool, mc);
			if (!page)
				continue;

			if (bio && (cur != last_index + 1 ||
				    last_bdev != mdev.m_bdev)) {
submit_bio_retry:
				submit_bio(bio);
				if (memstall) {
					psi_memstall_leave(&pflags);
					memstall = 0;
				}
				bio = NULL;
			}

			if (unlikely(PageWorkingset(page)) && !memstall) {
				psi_memstall_enter(&pflags);
				memstall = 1;
			}

			if (!bio) {
				bio = bio_alloc(mdev.m_bdev, BIO_MAX_VECS,
						REQ_OP_READ, GFP_NOIO);
				bio->bi_end_io = z_erofs_decompressqueue_endio;

				last_bdev = mdev.m_bdev;
				bio->bi_iter.bi_sector = (sector_t)cur <<
					(sb->s_blocksize_bits - 9);
				bio->bi_private = q[JQ_SUBMIT];
				if (readahead)
					bio->bi_opf |= REQ_RAHEAD;
				++nr_bios;
			}

			if (bio_add_page(bio, page, PAGE_SIZE, 0) < PAGE_SIZE)
				goto submit_bio_retry;

			last_index = cur;
			bypass = false;
		} while (++cur < end);

		if (!bypass)
			qtail[JQ_SUBMIT] = &pcl->next;
		else
			move_to_bypass_jobqueue(pcl, qtail, owned_head);
	} while (owned_head != Z_EROFS_PCLUSTER_TAIL);

	if (bio) {
		submit_bio(bio);
		if (memstall)
			psi_memstall_leave(&pflags);
	}

	 
	if (!*force_fg && !nr_bios) {
		kvfree(q[JQ_SUBMIT]);
		return;
	}
	z_erofs_decompress_kickoff(q[JQ_SUBMIT], nr_bios);
}

static void z_erofs_runqueue(struct z_erofs_decompress_frontend *f,
			     bool force_fg, bool ra)
{
	struct z_erofs_decompressqueue io[NR_JOBQUEUES];

	if (f->owned_head == Z_EROFS_PCLUSTER_TAIL)
		return;
	z_erofs_submit_queue(f, io, &force_fg, ra);

	 
	z_erofs_decompress_queue(&io[JQ_BYPASS], &f->pagepool);

	if (!force_fg)
		return;

	 
	wait_for_completion_io(&io[JQ_SUBMIT].u.done);

	 
	z_erofs_decompress_queue(&io[JQ_SUBMIT], &f->pagepool);
}

 
static void z_erofs_pcluster_readmore(struct z_erofs_decompress_frontend *f,
		struct readahead_control *rac, bool backmost)
{
	struct inode *inode = f->inode;
	struct erofs_map_blocks *map = &f->map;
	erofs_off_t cur, end, headoffset = f->headoffset;
	int err;

	if (backmost) {
		if (rac)
			end = headoffset + readahead_length(rac) - 1;
		else
			end = headoffset + PAGE_SIZE - 1;
		map->m_la = end;
		err = z_erofs_map_blocks_iter(inode, map,
					      EROFS_GET_BLOCKS_READMORE);
		if (err)
			return;

		 
		if (rac) {
			cur = round_up(map->m_la + map->m_llen, PAGE_SIZE);
			readahead_expand(rac, headoffset, cur - headoffset);
			return;
		}
		end = round_up(end, PAGE_SIZE);
	} else {
		end = round_up(map->m_la, PAGE_SIZE);

		if (!map->m_llen)
			return;
	}

	cur = map->m_la + map->m_llen - 1;
	while ((cur >= end) && (cur < i_size_read(inode))) {
		pgoff_t index = cur >> PAGE_SHIFT;
		struct page *page;

		page = erofs_grab_cache_page_nowait(inode->i_mapping, index);
		if (page) {
			if (PageUptodate(page))
				unlock_page(page);
			else
				(void)z_erofs_do_read_page(f, page);
			put_page(page);
		}

		if (cur < PAGE_SIZE)
			break;
		cur = (index << PAGE_SHIFT) - 1;
	}
}

static int z_erofs_read_folio(struct file *file, struct folio *folio)
{
	struct inode *const inode = folio->mapping->host;
	struct erofs_sb_info *const sbi = EROFS_I_SB(inode);
	struct z_erofs_decompress_frontend f = DECOMPRESS_FRONTEND_INIT(inode);
	int err;

	trace_erofs_read_folio(folio, false);
	f.headoffset = (erofs_off_t)folio->index << PAGE_SHIFT;

	z_erofs_pcluster_readmore(&f, NULL, true);
	err = z_erofs_do_read_page(&f, &folio->page);
	z_erofs_pcluster_readmore(&f, NULL, false);
	z_erofs_pcluster_end(&f);

	 
	z_erofs_runqueue(&f, z_erofs_is_sync_decompress(sbi, 0), false);

	if (err && err != -EINTR)
		erofs_err(inode->i_sb, "read error %d @ %lu of nid %llu",
			  err, folio->index, EROFS_I(inode)->nid);

	erofs_put_metabuf(&f.map.buf);
	erofs_release_pages(&f.pagepool);
	return err;
}

static void z_erofs_readahead(struct readahead_control *rac)
{
	struct inode *const inode = rac->mapping->host;
	struct erofs_sb_info *const sbi = EROFS_I_SB(inode);
	struct z_erofs_decompress_frontend f = DECOMPRESS_FRONTEND_INIT(inode);
	struct folio *head = NULL, *folio;
	unsigned int nr_folios;
	int err;

	f.headoffset = readahead_pos(rac);

	z_erofs_pcluster_readmore(&f, rac, true);
	nr_folios = readahead_count(rac);
	trace_erofs_readpages(inode, readahead_index(rac), nr_folios, false);

	while ((folio = readahead_folio(rac))) {
		folio->private = head;
		head = folio;
	}

	 
	while (head) {
		folio = head;
		head = folio_get_private(folio);

		err = z_erofs_do_read_page(&f, &folio->page);
		if (err && err != -EINTR)
			erofs_err(inode->i_sb, "readahead error at folio %lu @ nid %llu",
				  folio->index, EROFS_I(inode)->nid);
	}
	z_erofs_pcluster_readmore(&f, rac, false);
	z_erofs_pcluster_end(&f);

	z_erofs_runqueue(&f, z_erofs_is_sync_decompress(sbi, nr_folios), true);
	erofs_put_metabuf(&f.map.buf);
	erofs_release_pages(&f.pagepool);
}

const struct address_space_operations z_erofs_aops = {
	.read_folio = z_erofs_read_folio,
	.readahead = z_erofs_readahead,
};
