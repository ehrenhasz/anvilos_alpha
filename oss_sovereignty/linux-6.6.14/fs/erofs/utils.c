
 
#include "internal.h"

struct page *erofs_allocpage(struct page **pagepool, gfp_t gfp)
{
	struct page *page = *pagepool;

	if (page) {
		DBG_BUGON(page_ref_count(page) != 1);
		*pagepool = (struct page *)page_private(page);
	} else {
		page = alloc_page(gfp);
	}
	return page;
}

void erofs_release_pages(struct page **pagepool)
{
	while (*pagepool) {
		struct page *page = *pagepool;

		*pagepool = (struct page *)page_private(page);
		put_page(page);
	}
}

#ifdef CONFIG_EROFS_FS_ZIP
 
static atomic_long_t erofs_global_shrink_cnt;

static bool erofs_workgroup_get(struct erofs_workgroup *grp)
{
	if (lockref_get_not_zero(&grp->lockref))
		return true;

	spin_lock(&grp->lockref.lock);
	if (__lockref_is_dead(&grp->lockref)) {
		spin_unlock(&grp->lockref.lock);
		return false;
	}

	if (!grp->lockref.count++)
		atomic_long_dec(&erofs_global_shrink_cnt);
	spin_unlock(&grp->lockref.lock);
	return true;
}

struct erofs_workgroup *erofs_find_workgroup(struct super_block *sb,
					     pgoff_t index)
{
	struct erofs_sb_info *sbi = EROFS_SB(sb);
	struct erofs_workgroup *grp;

repeat:
	rcu_read_lock();
	grp = xa_load(&sbi->managed_pslots, index);
	if (grp) {
		if (!erofs_workgroup_get(grp)) {
			 
			rcu_read_unlock();
			goto repeat;
		}

		DBG_BUGON(index != grp->index);
	}
	rcu_read_unlock();
	return grp;
}

struct erofs_workgroup *erofs_insert_workgroup(struct super_block *sb,
					       struct erofs_workgroup *grp)
{
	struct erofs_sb_info *const sbi = EROFS_SB(sb);
	struct erofs_workgroup *pre;

	DBG_BUGON(grp->lockref.count < 1);
repeat:
	xa_lock(&sbi->managed_pslots);
	pre = __xa_cmpxchg(&sbi->managed_pslots, grp->index,
			   NULL, grp, GFP_NOFS);
	if (pre) {
		if (xa_is_err(pre)) {
			pre = ERR_PTR(xa_err(pre));
		} else if (!erofs_workgroup_get(pre)) {
			 
			xa_unlock(&sbi->managed_pslots);
			cond_resched();
			goto repeat;
		}
		grp = pre;
	}
	xa_unlock(&sbi->managed_pslots);
	return grp;
}

static void  __erofs_workgroup_free(struct erofs_workgroup *grp)
{
	atomic_long_dec(&erofs_global_shrink_cnt);
	erofs_workgroup_free_rcu(grp);
}

void erofs_workgroup_put(struct erofs_workgroup *grp)
{
	if (lockref_put_or_lock(&grp->lockref))
		return;

	DBG_BUGON(__lockref_is_dead(&grp->lockref));
	if (grp->lockref.count == 1)
		atomic_long_inc(&erofs_global_shrink_cnt);
	--grp->lockref.count;
	spin_unlock(&grp->lockref.lock);
}

static bool erofs_try_to_release_workgroup(struct erofs_sb_info *sbi,
					   struct erofs_workgroup *grp)
{
	int free = false;

	spin_lock(&grp->lockref.lock);
	if (grp->lockref.count)
		goto out;

	 
	if (erofs_try_to_free_all_cached_pages(sbi, grp))
		goto out;

	 
	DBG_BUGON(__xa_erase(&sbi->managed_pslots, grp->index) != grp);

	lockref_mark_dead(&grp->lockref);
	free = true;
out:
	spin_unlock(&grp->lockref.lock);
	if (free)
		__erofs_workgroup_free(grp);
	return free;
}

static unsigned long erofs_shrink_workstation(struct erofs_sb_info *sbi,
					      unsigned long nr_shrink)
{
	struct erofs_workgroup *grp;
	unsigned int freed = 0;
	unsigned long index;

	xa_lock(&sbi->managed_pslots);
	xa_for_each(&sbi->managed_pslots, index, grp) {
		 
		if (!erofs_try_to_release_workgroup(sbi, grp))
			continue;
		xa_unlock(&sbi->managed_pslots);

		++freed;
		if (!--nr_shrink)
			return freed;
		xa_lock(&sbi->managed_pslots);
	}
	xa_unlock(&sbi->managed_pslots);
	return freed;
}

 
static unsigned int shrinker_run_no;

 
static DEFINE_SPINLOCK(erofs_sb_list_lock);
static LIST_HEAD(erofs_sb_list);

void erofs_shrinker_register(struct super_block *sb)
{
	struct erofs_sb_info *sbi = EROFS_SB(sb);

	mutex_init(&sbi->umount_mutex);

	spin_lock(&erofs_sb_list_lock);
	list_add(&sbi->list, &erofs_sb_list);
	spin_unlock(&erofs_sb_list_lock);
}

void erofs_shrinker_unregister(struct super_block *sb)
{
	struct erofs_sb_info *const sbi = EROFS_SB(sb);

	mutex_lock(&sbi->umount_mutex);
	 
	erofs_shrink_workstation(sbi, ~0UL);

	spin_lock(&erofs_sb_list_lock);
	list_del(&sbi->list);
	spin_unlock(&erofs_sb_list_lock);
	mutex_unlock(&sbi->umount_mutex);
}

static unsigned long erofs_shrink_count(struct shrinker *shrink,
					struct shrink_control *sc)
{
	return atomic_long_read(&erofs_global_shrink_cnt);
}

static unsigned long erofs_shrink_scan(struct shrinker *shrink,
				       struct shrink_control *sc)
{
	struct erofs_sb_info *sbi;
	struct list_head *p;

	unsigned long nr = sc->nr_to_scan;
	unsigned int run_no;
	unsigned long freed = 0;

	spin_lock(&erofs_sb_list_lock);
	do {
		run_no = ++shrinker_run_no;
	} while (run_no == 0);

	 
	p = erofs_sb_list.next;
	while (p != &erofs_sb_list) {
		sbi = list_entry(p, struct erofs_sb_info, list);

		 
		if (sbi->shrinker_run_no == run_no)
			break;

		if (!mutex_trylock(&sbi->umount_mutex)) {
			p = p->next;
			continue;
		}

		spin_unlock(&erofs_sb_list_lock);
		sbi->shrinker_run_no = run_no;

		freed += erofs_shrink_workstation(sbi, nr - freed);

		spin_lock(&erofs_sb_list_lock);
		 
		p = p->next;

		 
		list_move_tail(&sbi->list, &erofs_sb_list);
		mutex_unlock(&sbi->umount_mutex);

		if (freed >= nr)
			break;
	}
	spin_unlock(&erofs_sb_list_lock);
	return freed;
}

static struct shrinker erofs_shrinker_info = {
	.scan_objects = erofs_shrink_scan,
	.count_objects = erofs_shrink_count,
	.seeks = DEFAULT_SEEKS,
};

int __init erofs_init_shrinker(void)
{
	return register_shrinker(&erofs_shrinker_info, "erofs-shrinker");
}

void erofs_exit_shrinker(void)
{
	unregister_shrinker(&erofs_shrinker_info);
}
#endif	 
