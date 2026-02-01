
 

 
static void update_fastmap_work_fn(struct work_struct *wrk)
{
	struct ubi_device *ubi = container_of(wrk, struct ubi_device, fm_work);

	ubi_update_fastmap(ubi);
	spin_lock(&ubi->wl_lock);
	ubi->fm_work_scheduled = 0;
	spin_unlock(&ubi->wl_lock);
}

 
static struct ubi_wl_entry *find_anchor_wl_entry(struct rb_root *root)
{
	struct rb_node *p;
	struct ubi_wl_entry *e, *victim = NULL;
	int max_ec = UBI_MAX_ERASECOUNTER;

	ubi_rb_for_each_entry(p, e, root, u.rb) {
		if (e->pnum < UBI_FM_MAX_START && e->ec < max_ec) {
			victim = e;
			max_ec = e->ec;
		}
	}

	return victim;
}

static inline void return_unused_peb(struct ubi_device *ubi,
				     struct ubi_wl_entry *e)
{
	wl_tree_add(e, &ubi->free);
	ubi->free_count++;
}

 
static void return_unused_pool_pebs(struct ubi_device *ubi,
				    struct ubi_fm_pool *pool)
{
	int i;
	struct ubi_wl_entry *e;

	for (i = pool->used; i < pool->size; i++) {
		e = ubi->lookuptbl[pool->pebs[i]];
		return_unused_peb(ubi, e);
	}
}

 
struct ubi_wl_entry *ubi_wl_get_fm_peb(struct ubi_device *ubi, int anchor)
{
	struct ubi_wl_entry *e = NULL;

	if (!ubi->free.rb_node || (ubi->free_count - ubi->beb_rsvd_pebs < 1))
		goto out;

	if (anchor)
		e = find_anchor_wl_entry(&ubi->free);
	else
		e = find_mean_wl_entry(ubi, &ubi->free);

	if (!e)
		goto out;

	self_check_in_wl_tree(ubi, e, &ubi->free);

	 
	rb_erase(&e->u.rb, &ubi->free);
	ubi->free_count--;
out:
	return e;
}

 
static bool has_enough_free_count(struct ubi_device *ubi, bool is_wl_pool)
{
	int fm_used = 0;	
	int beb_rsvd_pebs;

	if (!ubi->free.rb_node)
		return false;

	beb_rsvd_pebs = is_wl_pool ? ubi->beb_rsvd_pebs : 0;
	if (ubi->fm_wl_pool.size > 0 && !(ubi->ro_mode || ubi->fm_disabled))
		fm_used = ubi->fm_size / ubi->leb_size - 1;

	return ubi->free_count - beb_rsvd_pebs > fm_used;
}

 
void ubi_refill_pools(struct ubi_device *ubi)
{
	struct ubi_fm_pool *wl_pool = &ubi->fm_wl_pool;
	struct ubi_fm_pool *pool = &ubi->fm_pool;
	struct ubi_wl_entry *e;
	int enough;

	spin_lock(&ubi->wl_lock);

	return_unused_pool_pebs(ubi, wl_pool);
	return_unused_pool_pebs(ubi, pool);

	wl_pool->size = 0;
	pool->size = 0;

	if (ubi->fm_anchor) {
		wl_tree_add(ubi->fm_anchor, &ubi->free);
		ubi->free_count++;
		ubi->fm_anchor = NULL;
	}

	if (!ubi->fm_disabled)
		 
		ubi->fm_anchor = ubi_wl_get_fm_peb(ubi, 1);

	for (;;) {
		enough = 0;
		if (pool->size < pool->max_size) {
			if (!has_enough_free_count(ubi, false))
				break;

			e = wl_get_wle(ubi);
			if (!e)
				break;

			pool->pebs[pool->size] = e->pnum;
			pool->size++;
		} else
			enough++;

		if (wl_pool->size < wl_pool->max_size) {
			if (!has_enough_free_count(ubi, true))
				break;

			e = find_wl_entry(ubi, &ubi->free, WL_FREE_MAX_DIFF);
			self_check_in_wl_tree(ubi, e, &ubi->free);
			rb_erase(&e->u.rb, &ubi->free);
			ubi->free_count--;

			wl_pool->pebs[wl_pool->size] = e->pnum;
			wl_pool->size++;
		} else
			enough++;

		if (enough == 2)
			break;
	}

	wl_pool->used = 0;
	pool->used = 0;

	spin_unlock(&ubi->wl_lock);
}

 
static int produce_free_peb(struct ubi_device *ubi)
{
	int err;

	while (!ubi->free.rb_node && ubi->works_count) {
		dbg_wl("do one work synchronously");
		err = do_work(ubi);

		if (err)
			return err;
	}

	return 0;
}

 
int ubi_wl_get_peb(struct ubi_device *ubi)
{
	int ret, attempts = 0;
	struct ubi_fm_pool *pool = &ubi->fm_pool;
	struct ubi_fm_pool *wl_pool = &ubi->fm_wl_pool;

again:
	down_read(&ubi->fm_eba_sem);
	spin_lock(&ubi->wl_lock);

	 
	if (pool->used == pool->size || wl_pool->used == wl_pool->size) {
		spin_unlock(&ubi->wl_lock);
		up_read(&ubi->fm_eba_sem);
		ret = ubi_update_fastmap(ubi);
		if (ret) {
			ubi_msg(ubi, "Unable to write a new fastmap: %i", ret);
			down_read(&ubi->fm_eba_sem);
			return -ENOSPC;
		}
		down_read(&ubi->fm_eba_sem);
		spin_lock(&ubi->wl_lock);
	}

	if (pool->used == pool->size) {
		spin_unlock(&ubi->wl_lock);
		attempts++;
		if (attempts == 10) {
			ubi_err(ubi, "Unable to get a free PEB from user WL pool");
			ret = -ENOSPC;
			goto out;
		}
		up_read(&ubi->fm_eba_sem);
		ret = produce_free_peb(ubi);
		if (ret < 0) {
			down_read(&ubi->fm_eba_sem);
			goto out;
		}
		goto again;
	}

	ubi_assert(pool->used < pool->size);
	ret = pool->pebs[pool->used++];
	prot_queue_add(ubi, ubi->lookuptbl[ret]);
	spin_unlock(&ubi->wl_lock);
out:
	return ret;
}

 
static struct ubi_wl_entry *next_peb_for_wl(struct ubi_device *ubi)
{
	struct ubi_fm_pool *pool = &ubi->fm_wl_pool;
	int pnum;

	if (pool->used == pool->size)
		return NULL;

	pnum = pool->pebs[pool->used];
	return ubi->lookuptbl[pnum];
}

 
static bool need_wear_leveling(struct ubi_device *ubi)
{
	int ec;
	struct ubi_wl_entry *e;

	if (!ubi->used.rb_node)
		return false;

	e = next_peb_for_wl(ubi);
	if (!e) {
		if (!ubi->free.rb_node)
			return false;
		e = find_wl_entry(ubi, &ubi->free, WL_FREE_MAX_DIFF);
		ec = e->ec;
	} else {
		ec = e->ec;
		if (ubi->free.rb_node) {
			e = find_wl_entry(ubi, &ubi->free, WL_FREE_MAX_DIFF);
			ec = max(ec, e->ec);
		}
	}
	e = rb_entry(rb_first(&ubi->used), struct ubi_wl_entry, u.rb);

	return ec - e->ec >= UBI_WL_THRESHOLD;
}

 
static struct ubi_wl_entry *get_peb_for_wl(struct ubi_device *ubi)
{
	struct ubi_fm_pool *pool = &ubi->fm_wl_pool;
	int pnum;

	ubi_assert(rwsem_is_locked(&ubi->fm_eba_sem));

	if (pool->used == pool->size) {
		 
		if (!ubi->fm_work_scheduled) {
			ubi->fm_work_scheduled = 1;
			schedule_work(&ubi->fm_work);
		}
		return NULL;
	}

	pnum = pool->pebs[pool->used++];
	return ubi->lookuptbl[pnum];
}

 
int ubi_ensure_anchor_pebs(struct ubi_device *ubi)
{
	struct ubi_work *wrk;
	struct ubi_wl_entry *anchor;

	spin_lock(&ubi->wl_lock);

	 
	if (ubi->fm_anchor) {
		spin_unlock(&ubi->wl_lock);
		return 0;
	}

	 
	anchor = ubi_wl_get_fm_peb(ubi, 1);
	if (anchor) {
		ubi->fm_anchor = anchor;
		spin_unlock(&ubi->wl_lock);
		return 0;
	}

	ubi->fm_do_produce_anchor = 1;
	 
	if (ubi->wl_scheduled) {
		spin_unlock(&ubi->wl_lock);
		return 0;
	}
	ubi->wl_scheduled = 1;
	spin_unlock(&ubi->wl_lock);

	wrk = kmalloc(sizeof(struct ubi_work), GFP_NOFS);
	if (!wrk) {
		spin_lock(&ubi->wl_lock);
		ubi->wl_scheduled = 0;
		spin_unlock(&ubi->wl_lock);
		return -ENOMEM;
	}

	wrk->func = &wear_leveling_worker;
	__schedule_ubi_work(ubi, wrk);
	return 0;
}

 
int ubi_wl_put_fm_peb(struct ubi_device *ubi, struct ubi_wl_entry *fm_e,
		      int lnum, int torture)
{
	struct ubi_wl_entry *e;
	int vol_id, pnum = fm_e->pnum;

	dbg_wl("PEB %d", pnum);

	ubi_assert(pnum >= 0);
	ubi_assert(pnum < ubi->peb_count);

	spin_lock(&ubi->wl_lock);
	e = ubi->lookuptbl[pnum];

	 
	if (!e) {
		e = fm_e;
		ubi_assert(e->ec >= 0);
		ubi->lookuptbl[pnum] = e;
	}

	spin_unlock(&ubi->wl_lock);

	vol_id = lnum ? UBI_FM_DATA_VOLUME_ID : UBI_FM_SB_VOLUME_ID;
	return schedule_erase(ubi, e, vol_id, lnum, torture, true);
}

 
int ubi_is_erase_work(struct ubi_work *wrk)
{
	return wrk->func == erase_worker;
}

static void ubi_fastmap_close(struct ubi_device *ubi)
{
	int i;

	return_unused_pool_pebs(ubi, &ubi->fm_pool);
	return_unused_pool_pebs(ubi, &ubi->fm_wl_pool);

	if (ubi->fm_anchor) {
		return_unused_peb(ubi, ubi->fm_anchor);
		ubi->fm_anchor = NULL;
	}

	if (ubi->fm) {
		for (i = 0; i < ubi->fm->used_blocks; i++)
			kfree(ubi->fm->e[i]);
	}
	kfree(ubi->fm);
}

 
static struct ubi_wl_entry *may_reserve_for_fm(struct ubi_device *ubi,
					   struct ubi_wl_entry *e,
					   struct rb_root *root) {
	if (e && !ubi->fm_disabled && !ubi->fm &&
	    e->pnum < UBI_FM_MAX_START)
		e = rb_entry(rb_next(root->rb_node),
			     struct ubi_wl_entry, u.rb);

	return e;
}
