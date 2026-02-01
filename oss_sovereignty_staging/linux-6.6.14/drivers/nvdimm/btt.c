
 
#include <linux/highmem.h>
#include <linux/debugfs.h>
#include <linux/blkdev.h>
#include <linux/pagemap.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/hdreg.h>
#include <linux/sizes.h>
#include <linux/ndctl.h>
#include <linux/fs.h>
#include <linux/nd.h>
#include <linux/backing-dev.h>
#include "btt.h"
#include "nd.h"

enum log_ent_request {
	LOG_NEW_ENT = 0,
	LOG_OLD_ENT
};

static struct device *to_dev(struct arena_info *arena)
{
	return &arena->nd_btt->dev;
}

static u64 adjust_initial_offset(struct nd_btt *nd_btt, u64 offset)
{
	return offset + nd_btt->initial_offset;
}

static int arena_read_bytes(struct arena_info *arena, resource_size_t offset,
		void *buf, size_t n, unsigned long flags)
{
	struct nd_btt *nd_btt = arena->nd_btt;
	struct nd_namespace_common *ndns = nd_btt->ndns;

	 
	offset = adjust_initial_offset(nd_btt, offset);
	return nvdimm_read_bytes(ndns, offset, buf, n, flags);
}

static int arena_write_bytes(struct arena_info *arena, resource_size_t offset,
		void *buf, size_t n, unsigned long flags)
{
	struct nd_btt *nd_btt = arena->nd_btt;
	struct nd_namespace_common *ndns = nd_btt->ndns;

	 
	offset = adjust_initial_offset(nd_btt, offset);
	return nvdimm_write_bytes(ndns, offset, buf, n, flags);
}

static int btt_info_write(struct arena_info *arena, struct btt_sb *super)
{
	int ret;

	 
	dev_WARN_ONCE(to_dev(arena), !IS_ALIGNED(arena->infooff, 512),
		"arena->infooff: %#llx is unaligned\n", arena->infooff);
	dev_WARN_ONCE(to_dev(arena), !IS_ALIGNED(arena->info2off, 512),
		"arena->info2off: %#llx is unaligned\n", arena->info2off);

	ret = arena_write_bytes(arena, arena->info2off, super,
			sizeof(struct btt_sb), 0);
	if (ret)
		return ret;

	return arena_write_bytes(arena, arena->infooff, super,
			sizeof(struct btt_sb), 0);
}

static int btt_info_read(struct arena_info *arena, struct btt_sb *super)
{
	return arena_read_bytes(arena, arena->infooff, super,
			sizeof(struct btt_sb), 0);
}

 
static int __btt_map_write(struct arena_info *arena, u32 lba, __le32 mapping,
		unsigned long flags)
{
	u64 ns_off = arena->mapoff + (lba * MAP_ENT_SIZE);

	if (unlikely(lba >= arena->external_nlba))
		dev_err_ratelimited(to_dev(arena),
			"%s: lba %#x out of range (max: %#x)\n",
			__func__, lba, arena->external_nlba);
	return arena_write_bytes(arena, ns_off, &mapping, MAP_ENT_SIZE, flags);
}

static int btt_map_write(struct arena_info *arena, u32 lba, u32 mapping,
			u32 z_flag, u32 e_flag, unsigned long rwb_flags)
{
	u32 ze;
	__le32 mapping_le;

	 
	mapping = ent_lba(mapping);

	ze = (z_flag << 1) + e_flag;
	switch (ze) {
	case 0:
		 
		mapping |= MAP_ENT_NORMAL;
		break;
	case 1:
		mapping |= (1 << MAP_ERR_SHIFT);
		break;
	case 2:
		mapping |= (1 << MAP_TRIM_SHIFT);
		break;
	default:
		 
		dev_err_ratelimited(to_dev(arena),
			"Invalid use of Z and E flags\n");
		return -EIO;
	}

	mapping_le = cpu_to_le32(mapping);
	return __btt_map_write(arena, lba, mapping_le, rwb_flags);
}

static int btt_map_read(struct arena_info *arena, u32 lba, u32 *mapping,
			int *trim, int *error, unsigned long rwb_flags)
{
	int ret;
	__le32 in;
	u32 raw_mapping, postmap, ze, z_flag, e_flag;
	u64 ns_off = arena->mapoff + (lba * MAP_ENT_SIZE);

	if (unlikely(lba >= arena->external_nlba))
		dev_err_ratelimited(to_dev(arena),
			"%s: lba %#x out of range (max: %#x)\n",
			__func__, lba, arena->external_nlba);

	ret = arena_read_bytes(arena, ns_off, &in, MAP_ENT_SIZE, rwb_flags);
	if (ret)
		return ret;

	raw_mapping = le32_to_cpu(in);

	z_flag = ent_z_flag(raw_mapping);
	e_flag = ent_e_flag(raw_mapping);
	ze = (z_flag << 1) + e_flag;
	postmap = ent_lba(raw_mapping);

	 
	z_flag = 0;
	e_flag = 0;

	switch (ze) {
	case 0:
		 
		*mapping = lba;
		break;
	case 1:
		*mapping = postmap;
		e_flag = 1;
		break;
	case 2:
		*mapping = postmap;
		z_flag = 1;
		break;
	case 3:
		*mapping = postmap;
		break;
	default:
		return -EIO;
	}

	if (trim)
		*trim = z_flag;
	if (error)
		*error = e_flag;

	return ret;
}

static int btt_log_group_read(struct arena_info *arena, u32 lane,
			struct log_group *log)
{
	return arena_read_bytes(arena,
			arena->logoff + (lane * LOG_GRP_SIZE), log,
			LOG_GRP_SIZE, 0);
}

static struct dentry *debugfs_root;

static void arena_debugfs_init(struct arena_info *a, struct dentry *parent,
				int idx)
{
	char dirname[32];
	struct dentry *d;

	 
	if (!parent)
		return;

	snprintf(dirname, 32, "arena%d", idx);
	d = debugfs_create_dir(dirname, parent);
	if (IS_ERR_OR_NULL(d))
		return;
	a->debugfs_dir = d;

	debugfs_create_x64("size", S_IRUGO, d, &a->size);
	debugfs_create_x64("external_lba_start", S_IRUGO, d,
				&a->external_lba_start);
	debugfs_create_x32("internal_nlba", S_IRUGO, d, &a->internal_nlba);
	debugfs_create_u32("internal_lbasize", S_IRUGO, d,
				&a->internal_lbasize);
	debugfs_create_x32("external_nlba", S_IRUGO, d, &a->external_nlba);
	debugfs_create_u32("external_lbasize", S_IRUGO, d,
				&a->external_lbasize);
	debugfs_create_u32("nfree", S_IRUGO, d, &a->nfree);
	debugfs_create_u16("version_major", S_IRUGO, d, &a->version_major);
	debugfs_create_u16("version_minor", S_IRUGO, d, &a->version_minor);
	debugfs_create_x64("nextoff", S_IRUGO, d, &a->nextoff);
	debugfs_create_x64("infooff", S_IRUGO, d, &a->infooff);
	debugfs_create_x64("dataoff", S_IRUGO, d, &a->dataoff);
	debugfs_create_x64("mapoff", S_IRUGO, d, &a->mapoff);
	debugfs_create_x64("logoff", S_IRUGO, d, &a->logoff);
	debugfs_create_x64("info2off", S_IRUGO, d, &a->info2off);
	debugfs_create_x32("flags", S_IRUGO, d, &a->flags);
	debugfs_create_u32("log_index_0", S_IRUGO, d, &a->log_index[0]);
	debugfs_create_u32("log_index_1", S_IRUGO, d, &a->log_index[1]);
}

static void btt_debugfs_init(struct btt *btt)
{
	int i = 0;
	struct arena_info *arena;

	btt->debugfs_dir = debugfs_create_dir(dev_name(&btt->nd_btt->dev),
						debugfs_root);
	if (IS_ERR_OR_NULL(btt->debugfs_dir))
		return;

	list_for_each_entry(arena, &btt->arena_list, list) {
		arena_debugfs_init(arena, btt->debugfs_dir, i);
		i++;
	}
}

static u32 log_seq(struct log_group *log, int log_idx)
{
	return le32_to_cpu(log->ent[log_idx].seq);
}

 
static int btt_log_get_old(struct arena_info *a, struct log_group *log)
{
	int idx0 = a->log_index[0];
	int idx1 = a->log_index[1];
	int old;

	 
	if (log_seq(log, idx0) == 0) {
		log->ent[idx0].seq = cpu_to_le32(1);
		return 0;
	}

	if (log_seq(log, idx0) == log_seq(log, idx1))
		return -EINVAL;
	if (log_seq(log, idx0) + log_seq(log, idx1) > 5)
		return -EINVAL;

	if (log_seq(log, idx0) < log_seq(log, idx1)) {
		if ((log_seq(log, idx1) - log_seq(log, idx0)) == 1)
			old = 0;
		else
			old = 1;
	} else {
		if ((log_seq(log, idx0) - log_seq(log, idx1)) == 1)
			old = 1;
		else
			old = 0;
	}

	return old;
}

 
static int btt_log_read(struct arena_info *arena, u32 lane,
			struct log_entry *ent, int old_flag)
{
	int ret;
	int old_ent, ret_ent;
	struct log_group log;

	ret = btt_log_group_read(arena, lane, &log);
	if (ret)
		return -EIO;

	old_ent = btt_log_get_old(arena, &log);
	if (old_ent < 0 || old_ent > 1) {
		dev_err(to_dev(arena),
				"log corruption (%d): lane %d seq [%d, %d]\n",
				old_ent, lane, log.ent[arena->log_index[0]].seq,
				log.ent[arena->log_index[1]].seq);
		 
		return -EIO;
	}

	ret_ent = (old_flag ? old_ent : (1 - old_ent));

	if (ent != NULL)
		memcpy(ent, &log.ent[arena->log_index[ret_ent]], LOG_ENT_SIZE);

	return ret_ent;
}

 
static int __btt_log_write(struct arena_info *arena, u32 lane,
			u32 sub, struct log_entry *ent, unsigned long flags)
{
	int ret;
	u32 group_slot = arena->log_index[sub];
	unsigned int log_half = LOG_ENT_SIZE / 2;
	void *src = ent;
	u64 ns_off;

	ns_off = arena->logoff + (lane * LOG_GRP_SIZE) +
		(group_slot * LOG_ENT_SIZE);
	 
	ret = arena_write_bytes(arena, ns_off, src, log_half, flags);
	if (ret)
		return ret;

	ns_off += log_half;
	src += log_half;
	return arena_write_bytes(arena, ns_off, src, log_half, flags);
}

static int btt_flog_write(struct arena_info *arena, u32 lane, u32 sub,
			struct log_entry *ent)
{
	int ret;

	ret = __btt_log_write(arena, lane, sub, ent, NVDIMM_IO_ATOMIC);
	if (ret)
		return ret;

	 
	arena->freelist[lane].sub = 1 - arena->freelist[lane].sub;
	if (++(arena->freelist[lane].seq) == 4)
		arena->freelist[lane].seq = 1;
	if (ent_e_flag(le32_to_cpu(ent->old_map)))
		arena->freelist[lane].has_err = 1;
	arena->freelist[lane].block = ent_lba(le32_to_cpu(ent->old_map));

	return ret;
}

 
static int btt_map_init(struct arena_info *arena)
{
	int ret = -EINVAL;
	void *zerobuf;
	size_t offset = 0;
	size_t chunk_size = SZ_2M;
	size_t mapsize = arena->logoff - arena->mapoff;

	zerobuf = kzalloc(chunk_size, GFP_KERNEL);
	if (!zerobuf)
		return -ENOMEM;

	 
	dev_WARN_ONCE(to_dev(arena), !IS_ALIGNED(arena->mapoff, 512),
		"arena->mapoff: %#llx is unaligned\n", arena->mapoff);

	while (mapsize) {
		size_t size = min(mapsize, chunk_size);

		dev_WARN_ONCE(to_dev(arena), size < 512,
			"chunk size: %#zx is unaligned\n", size);
		ret = arena_write_bytes(arena, arena->mapoff + offset, zerobuf,
				size, 0);
		if (ret)
			goto free;

		offset += size;
		mapsize -= size;
		cond_resched();
	}

 free:
	kfree(zerobuf);
	return ret;
}

 
static int btt_log_init(struct arena_info *arena)
{
	size_t logsize = arena->info2off - arena->logoff;
	size_t chunk_size = SZ_4K, offset = 0;
	struct log_entry ent;
	void *zerobuf;
	int ret;
	u32 i;

	zerobuf = kzalloc(chunk_size, GFP_KERNEL);
	if (!zerobuf)
		return -ENOMEM;
	 
	dev_WARN_ONCE(to_dev(arena), !IS_ALIGNED(arena->logoff, 512),
		"arena->logoff: %#llx is unaligned\n", arena->logoff);

	while (logsize) {
		size_t size = min(logsize, chunk_size);

		dev_WARN_ONCE(to_dev(arena), size < 512,
			"chunk size: %#zx is unaligned\n", size);
		ret = arena_write_bytes(arena, arena->logoff + offset, zerobuf,
				size, 0);
		if (ret)
			goto free;

		offset += size;
		logsize -= size;
		cond_resched();
	}

	for (i = 0; i < arena->nfree; i++) {
		ent.lba = cpu_to_le32(i);
		ent.old_map = cpu_to_le32(arena->external_nlba + i);
		ent.new_map = cpu_to_le32(arena->external_nlba + i);
		ent.seq = cpu_to_le32(LOG_SEQ_INIT);
		ret = __btt_log_write(arena, i, 0, &ent, 0);
		if (ret)
			goto free;
	}

 free:
	kfree(zerobuf);
	return ret;
}

static u64 to_namespace_offset(struct arena_info *arena, u64 lba)
{
	return arena->dataoff + ((u64)lba * arena->internal_lbasize);
}

static int arena_clear_freelist_error(struct arena_info *arena, u32 lane)
{
	int ret = 0;

	if (arena->freelist[lane].has_err) {
		void *zero_page = page_address(ZERO_PAGE(0));
		u32 lba = arena->freelist[lane].block;
		u64 nsoff = to_namespace_offset(arena, lba);
		unsigned long len = arena->sector_size;

		mutex_lock(&arena->err_lock);

		while (len) {
			unsigned long chunk = min(len, PAGE_SIZE);

			ret = arena_write_bytes(arena, nsoff, zero_page,
				chunk, 0);
			if (ret)
				break;
			len -= chunk;
			nsoff += chunk;
			if (len == 0)
				arena->freelist[lane].has_err = 0;
		}
		mutex_unlock(&arena->err_lock);
	}
	return ret;
}

static int btt_freelist_init(struct arena_info *arena)
{
	int new, ret;
	struct log_entry log_new;
	u32 i, map_entry, log_oldmap, log_newmap;

	arena->freelist = kcalloc(arena->nfree, sizeof(struct free_entry),
					GFP_KERNEL);
	if (!arena->freelist)
		return -ENOMEM;

	for (i = 0; i < arena->nfree; i++) {
		new = btt_log_read(arena, i, &log_new, LOG_NEW_ENT);
		if (new < 0)
			return new;

		 
		log_oldmap = ent_lba(le32_to_cpu(log_new.old_map));
		log_newmap = ent_lba(le32_to_cpu(log_new.new_map));

		 
		arena->freelist[i].sub = 1 - new;
		arena->freelist[i].seq = nd_inc_seq(le32_to_cpu(log_new.seq));
		arena->freelist[i].block = log_oldmap;

		 
		if (ent_e_flag(le32_to_cpu(log_new.old_map)) &&
		    !ent_normal(le32_to_cpu(log_new.old_map))) {
			arena->freelist[i].has_err = 1;
			ret = arena_clear_freelist_error(arena, i);
			if (ret)
				dev_err_ratelimited(to_dev(arena),
					"Unable to clear known errors\n");
		}

		 
		if (log_oldmap == log_newmap)
			continue;

		 
		ret = btt_map_read(arena, le32_to_cpu(log_new.lba), &map_entry,
				NULL, NULL, 0);
		if (ret)
			return ret;

		 
		if ((log_newmap != map_entry) && (log_oldmap == map_entry)) {
			 
			ret = btt_map_write(arena, le32_to_cpu(log_new.lba),
					le32_to_cpu(log_new.new_map), 0, 0, 0);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static bool ent_is_padding(struct log_entry *ent)
{
	return (ent->lba == 0) && (ent->old_map == 0) && (ent->new_map == 0)
		&& (ent->seq == 0);
}

 
static int log_set_indices(struct arena_info *arena)
{
	bool idx_set = false, initial_state = true;
	int ret, log_index[2] = {-1, -1};
	u32 i, j, next_idx = 0;
	struct log_group log;
	u32 pad_count = 0;

	for (i = 0; i < arena->nfree; i++) {
		ret = btt_log_group_read(arena, i, &log);
		if (ret < 0)
			return ret;

		for (j = 0; j < 4; j++) {
			if (!idx_set) {
				if (ent_is_padding(&log.ent[j])) {
					pad_count++;
					continue;
				} else {
					 
					if ((next_idx == 1) &&
						(j == log_index[0]))
						continue;
					 
					log_index[next_idx] = j;
					next_idx++;
				}
				if (next_idx == 2) {
					 
					idx_set = true;
				} else if (next_idx > 2) {
					 
					return -ENXIO;
				}
			} else {
				 
				if (j == log_index[0]) {
					 
					if (ent_is_padding(&log.ent[j]))
						return -ENXIO;
				} else if (j == log_index[1]) {
					;
					 
				} else {
					 
					if (!ent_is_padding(&log.ent[j]))
						return -ENXIO;
				}
			}
		}
		 
		if (pad_count < 3)
			initial_state = false;
		pad_count = 0;
	}

	if (!initial_state && !idx_set)
		return -ENXIO;

	 
	if (initial_state)
		log_index[1] = 1;

	 
	if ((log_index[0] == 0) && ((log_index[1] == 1) || (log_index[1] == 2)))
		;  
	else {
		dev_err(to_dev(arena), "Found an unknown padding scheme\n");
		return -ENXIO;
	}

	arena->log_index[0] = log_index[0];
	arena->log_index[1] = log_index[1];
	dev_dbg(to_dev(arena), "log_index_0 = %d\n", log_index[0]);
	dev_dbg(to_dev(arena), "log_index_1 = %d\n", log_index[1]);
	return 0;
}

static int btt_rtt_init(struct arena_info *arena)
{
	arena->rtt = kcalloc(arena->nfree, sizeof(u32), GFP_KERNEL);
	if (arena->rtt == NULL)
		return -ENOMEM;

	return 0;
}

static int btt_maplocks_init(struct arena_info *arena)
{
	u32 i;

	arena->map_locks = kcalloc(arena->nfree, sizeof(struct aligned_lock),
				GFP_KERNEL);
	if (!arena->map_locks)
		return -ENOMEM;

	for (i = 0; i < arena->nfree; i++)
		spin_lock_init(&arena->map_locks[i].lock);

	return 0;
}

static struct arena_info *alloc_arena(struct btt *btt, size_t size,
				size_t start, size_t arena_off)
{
	struct arena_info *arena;
	u64 logsize, mapsize, datasize;
	u64 available = size;

	arena = kzalloc(sizeof(struct arena_info), GFP_KERNEL);
	if (!arena)
		return NULL;
	arena->nd_btt = btt->nd_btt;
	arena->sector_size = btt->sector_size;
	mutex_init(&arena->err_lock);

	if (!size)
		return arena;

	arena->size = size;
	arena->external_lba_start = start;
	arena->external_lbasize = btt->lbasize;
	arena->internal_lbasize = roundup(arena->external_lbasize,
					INT_LBASIZE_ALIGNMENT);
	arena->nfree = BTT_DEFAULT_NFREE;
	arena->version_major = btt->nd_btt->version_major;
	arena->version_minor = btt->nd_btt->version_minor;

	if (available % BTT_PG_SIZE)
		available -= (available % BTT_PG_SIZE);

	 
	available -= 2 * BTT_PG_SIZE;

	 
	logsize = roundup(arena->nfree * LOG_GRP_SIZE, BTT_PG_SIZE);
	available -= logsize;

	 
	arena->internal_nlba = div_u64(available - BTT_PG_SIZE,
			arena->internal_lbasize + MAP_ENT_SIZE);
	arena->external_nlba = arena->internal_nlba - arena->nfree;

	mapsize = roundup((arena->external_nlba * MAP_ENT_SIZE), BTT_PG_SIZE);
	datasize = available - mapsize;

	 
	arena->infooff = arena_off;
	arena->dataoff = arena->infooff + BTT_PG_SIZE;
	arena->mapoff = arena->dataoff + datasize;
	arena->logoff = arena->mapoff + mapsize;
	arena->info2off = arena->logoff + logsize;

	 
	arena->log_index[0] = 0;
	arena->log_index[1] = 1;
	return arena;
}

static void free_arenas(struct btt *btt)
{
	struct arena_info *arena, *next;

	list_for_each_entry_safe(arena, next, &btt->arena_list, list) {
		list_del(&arena->list);
		kfree(arena->rtt);
		kfree(arena->map_locks);
		kfree(arena->freelist);
		debugfs_remove_recursive(arena->debugfs_dir);
		kfree(arena);
	}
}

 
static void parse_arena_meta(struct arena_info *arena, struct btt_sb *super,
				u64 arena_off)
{
	arena->internal_nlba = le32_to_cpu(super->internal_nlba);
	arena->internal_lbasize = le32_to_cpu(super->internal_lbasize);
	arena->external_nlba = le32_to_cpu(super->external_nlba);
	arena->external_lbasize = le32_to_cpu(super->external_lbasize);
	arena->nfree = le32_to_cpu(super->nfree);
	arena->version_major = le16_to_cpu(super->version_major);
	arena->version_minor = le16_to_cpu(super->version_minor);

	arena->nextoff = (super->nextoff == 0) ? 0 : (arena_off +
			le64_to_cpu(super->nextoff));
	arena->infooff = arena_off;
	arena->dataoff = arena_off + le64_to_cpu(super->dataoff);
	arena->mapoff = arena_off + le64_to_cpu(super->mapoff);
	arena->logoff = arena_off + le64_to_cpu(super->logoff);
	arena->info2off = arena_off + le64_to_cpu(super->info2off);

	arena->size = (le64_to_cpu(super->nextoff) > 0)
		? (le64_to_cpu(super->nextoff))
		: (arena->info2off - arena->infooff + BTT_PG_SIZE);

	arena->flags = le32_to_cpu(super->flags);
}

static int discover_arenas(struct btt *btt)
{
	int ret = 0;
	struct arena_info *arena;
	struct btt_sb *super;
	size_t remaining = btt->rawsize;
	u64 cur_nlba = 0;
	size_t cur_off = 0;
	int num_arenas = 0;

	super = kzalloc(sizeof(*super), GFP_KERNEL);
	if (!super)
		return -ENOMEM;

	while (remaining) {
		 
		arena = alloc_arena(btt, 0, 0, 0);
		if (!arena) {
			ret = -ENOMEM;
			goto out_super;
		}

		arena->infooff = cur_off;
		ret = btt_info_read(arena, super);
		if (ret)
			goto out;

		if (!nd_btt_arena_is_valid(btt->nd_btt, super)) {
			if (remaining == btt->rawsize) {
				btt->init_state = INIT_NOTFOUND;
				dev_info(to_dev(arena), "No existing arenas\n");
				goto out;
			} else {
				dev_err(to_dev(arena),
						"Found corrupted metadata!\n");
				ret = -ENODEV;
				goto out;
			}
		}

		arena->external_lba_start = cur_nlba;
		parse_arena_meta(arena, super, cur_off);

		ret = log_set_indices(arena);
		if (ret) {
			dev_err(to_dev(arena),
				"Unable to deduce log/padding indices\n");
			goto out;
		}

		ret = btt_freelist_init(arena);
		if (ret)
			goto out;

		ret = btt_rtt_init(arena);
		if (ret)
			goto out;

		ret = btt_maplocks_init(arena);
		if (ret)
			goto out;

		list_add_tail(&arena->list, &btt->arena_list);

		remaining -= arena->size;
		cur_off += arena->size;
		cur_nlba += arena->external_nlba;
		num_arenas++;

		if (arena->nextoff == 0)
			break;
	}
	btt->num_arenas = num_arenas;
	btt->nlba = cur_nlba;
	btt->init_state = INIT_READY;

	kfree(super);
	return ret;

 out:
	kfree(arena);
	free_arenas(btt);
 out_super:
	kfree(super);
	return ret;
}

static int create_arenas(struct btt *btt)
{
	size_t remaining = btt->rawsize;
	size_t cur_off = 0;

	while (remaining) {
		struct arena_info *arena;
		size_t arena_size = min_t(u64, ARENA_MAX_SIZE, remaining);

		remaining -= arena_size;
		if (arena_size < ARENA_MIN_SIZE)
			break;

		arena = alloc_arena(btt, arena_size, btt->nlba, cur_off);
		if (!arena) {
			free_arenas(btt);
			return -ENOMEM;
		}
		btt->nlba += arena->external_nlba;
		if (remaining >= ARENA_MIN_SIZE)
			arena->nextoff = arena->size;
		else
			arena->nextoff = 0;
		cur_off += arena_size;
		list_add_tail(&arena->list, &btt->arena_list);
	}

	return 0;
}

 
static int btt_arena_write_layout(struct arena_info *arena)
{
	int ret;
	u64 sum;
	struct btt_sb *super;
	struct nd_btt *nd_btt = arena->nd_btt;
	const uuid_t *parent_uuid = nd_dev_to_uuid(&nd_btt->ndns->dev);

	ret = btt_map_init(arena);
	if (ret)
		return ret;

	ret = btt_log_init(arena);
	if (ret)
		return ret;

	super = kzalloc(sizeof(struct btt_sb), GFP_NOIO);
	if (!super)
		return -ENOMEM;

	strncpy(super->signature, BTT_SIG, BTT_SIG_LEN);
	export_uuid(super->uuid, nd_btt->uuid);
	export_uuid(super->parent_uuid, parent_uuid);
	super->flags = cpu_to_le32(arena->flags);
	super->version_major = cpu_to_le16(arena->version_major);
	super->version_minor = cpu_to_le16(arena->version_minor);
	super->external_lbasize = cpu_to_le32(arena->external_lbasize);
	super->external_nlba = cpu_to_le32(arena->external_nlba);
	super->internal_lbasize = cpu_to_le32(arena->internal_lbasize);
	super->internal_nlba = cpu_to_le32(arena->internal_nlba);
	super->nfree = cpu_to_le32(arena->nfree);
	super->infosize = cpu_to_le32(sizeof(struct btt_sb));
	super->nextoff = cpu_to_le64(arena->nextoff);
	 
	super->dataoff = cpu_to_le64(arena->dataoff - arena->infooff);
	super->mapoff = cpu_to_le64(arena->mapoff - arena->infooff);
	super->logoff = cpu_to_le64(arena->logoff - arena->infooff);
	super->info2off = cpu_to_le64(arena->info2off - arena->infooff);

	super->flags = 0;
	sum = nd_sb_checksum((struct nd_gen_sb *) super);
	super->checksum = cpu_to_le64(sum);

	ret = btt_info_write(arena, super);

	kfree(super);
	return ret;
}

 
static int btt_meta_init(struct btt *btt)
{
	int ret = 0;
	struct arena_info *arena;

	mutex_lock(&btt->init_lock);
	list_for_each_entry(arena, &btt->arena_list, list) {
		ret = btt_arena_write_layout(arena);
		if (ret)
			goto unlock;

		ret = btt_freelist_init(arena);
		if (ret)
			goto unlock;

		ret = btt_rtt_init(arena);
		if (ret)
			goto unlock;

		ret = btt_maplocks_init(arena);
		if (ret)
			goto unlock;
	}

	btt->init_state = INIT_READY;

 unlock:
	mutex_unlock(&btt->init_lock);
	return ret;
}

static u32 btt_meta_size(struct btt *btt)
{
	return btt->lbasize - btt->sector_size;
}

 
static int lba_to_arena(struct btt *btt, sector_t sector, __u32 *premap,
				struct arena_info **arena)
{
	struct arena_info *arena_list;
	__u64 lba = div_u64(sector << SECTOR_SHIFT, btt->sector_size);

	list_for_each_entry(arena_list, &btt->arena_list, list) {
		if (lba < arena_list->external_nlba) {
			*arena = arena_list;
			*premap = lba;
			return 0;
		}
		lba -= arena_list->external_nlba;
	}

	return -EIO;
}

 
static void lock_map(struct arena_info *arena, u32 premap)
		__acquires(&arena->map_locks[idx].lock)
{
	u32 idx = (premap * MAP_ENT_SIZE / L1_CACHE_BYTES) % arena->nfree;

	spin_lock(&arena->map_locks[idx].lock);
}

static void unlock_map(struct arena_info *arena, u32 premap)
		__releases(&arena->map_locks[idx].lock)
{
	u32 idx = (premap * MAP_ENT_SIZE / L1_CACHE_BYTES) % arena->nfree;

	spin_unlock(&arena->map_locks[idx].lock);
}

static int btt_data_read(struct arena_info *arena, struct page *page,
			unsigned int off, u32 lba, u32 len)
{
	int ret;
	u64 nsoff = to_namespace_offset(arena, lba);
	void *mem = kmap_atomic(page);

	ret = arena_read_bytes(arena, nsoff, mem + off, len, NVDIMM_IO_ATOMIC);
	kunmap_atomic(mem);

	return ret;
}

static int btt_data_write(struct arena_info *arena, u32 lba,
			struct page *page, unsigned int off, u32 len)
{
	int ret;
	u64 nsoff = to_namespace_offset(arena, lba);
	void *mem = kmap_atomic(page);

	ret = arena_write_bytes(arena, nsoff, mem + off, len, NVDIMM_IO_ATOMIC);
	kunmap_atomic(mem);

	return ret;
}

static void zero_fill_data(struct page *page, unsigned int off, u32 len)
{
	void *mem = kmap_atomic(page);

	memset(mem + off, 0, len);
	kunmap_atomic(mem);
}

#ifdef CONFIG_BLK_DEV_INTEGRITY
static int btt_rw_integrity(struct btt *btt, struct bio_integrity_payload *bip,
			struct arena_info *arena, u32 postmap, int rw)
{
	unsigned int len = btt_meta_size(btt);
	u64 meta_nsoff;
	int ret = 0;

	if (bip == NULL)
		return 0;

	meta_nsoff = to_namespace_offset(arena, postmap) + btt->sector_size;

	while (len) {
		unsigned int cur_len;
		struct bio_vec bv;
		void *mem;

		bv = bvec_iter_bvec(bip->bip_vec, bip->bip_iter);
		 

		cur_len = min(len, bv.bv_len);
		mem = bvec_kmap_local(&bv);
		if (rw)
			ret = arena_write_bytes(arena, meta_nsoff, mem, cur_len,
					NVDIMM_IO_ATOMIC);
		else
			ret = arena_read_bytes(arena, meta_nsoff, mem, cur_len,
					NVDIMM_IO_ATOMIC);

		kunmap_local(mem);
		if (ret)
			return ret;

		len -= cur_len;
		meta_nsoff += cur_len;
		if (!bvec_iter_advance(bip->bip_vec, &bip->bip_iter, cur_len))
			return -EIO;
	}

	return ret;
}

#else  
static int btt_rw_integrity(struct btt *btt, struct bio_integrity_payload *bip,
			struct arena_info *arena, u32 postmap, int rw)
{
	return 0;
}
#endif

static int btt_read_pg(struct btt *btt, struct bio_integrity_payload *bip,
			struct page *page, unsigned int off, sector_t sector,
			unsigned int len)
{
	int ret = 0;
	int t_flag, e_flag;
	struct arena_info *arena = NULL;
	u32 lane = 0, premap, postmap;

	while (len) {
		u32 cur_len;

		lane = nd_region_acquire_lane(btt->nd_region);

		ret = lba_to_arena(btt, sector, &premap, &arena);
		if (ret)
			goto out_lane;

		cur_len = min(btt->sector_size, len);

		ret = btt_map_read(arena, premap, &postmap, &t_flag, &e_flag,
				NVDIMM_IO_ATOMIC);
		if (ret)
			goto out_lane;

		 
		while (1) {
			u32 new_map;
			int new_t, new_e;

			if (t_flag) {
				zero_fill_data(page, off, cur_len);
				goto out_lane;
			}

			if (e_flag) {
				ret = -EIO;
				goto out_lane;
			}

			arena->rtt[lane] = RTT_VALID | postmap;
			 
			barrier();

			ret = btt_map_read(arena, premap, &new_map, &new_t,
						&new_e, NVDIMM_IO_ATOMIC);
			if (ret)
				goto out_rtt;

			if ((postmap == new_map) && (t_flag == new_t) &&
					(e_flag == new_e))
				break;

			postmap = new_map;
			t_flag = new_t;
			e_flag = new_e;
		}

		ret = btt_data_read(arena, page, off, postmap, cur_len);
		if (ret) {
			 
			if (btt_map_write(arena, premap, postmap, 0, 1, NVDIMM_IO_ATOMIC))
				dev_warn_ratelimited(to_dev(arena),
					"Error persistently tracking bad blocks at %#x\n",
					premap);
			goto out_rtt;
		}

		if (bip) {
			ret = btt_rw_integrity(btt, bip, arena, postmap, READ);
			if (ret)
				goto out_rtt;
		}

		arena->rtt[lane] = RTT_INVALID;
		nd_region_release_lane(btt->nd_region, lane);

		len -= cur_len;
		off += cur_len;
		sector += btt->sector_size >> SECTOR_SHIFT;
	}

	return 0;

 out_rtt:
	arena->rtt[lane] = RTT_INVALID;
 out_lane:
	nd_region_release_lane(btt->nd_region, lane);
	return ret;
}

 
static bool btt_is_badblock(struct btt *btt, struct arena_info *arena,
		u32 postmap)
{
	u64 nsoff = adjust_initial_offset(arena->nd_btt,
			to_namespace_offset(arena, postmap));
	sector_t phys_sector = nsoff >> 9;

	return is_bad_pmem(btt->phys_bb, phys_sector, arena->internal_lbasize);
}

static int btt_write_pg(struct btt *btt, struct bio_integrity_payload *bip,
			sector_t sector, struct page *page, unsigned int off,
			unsigned int len)
{
	int ret = 0;
	struct arena_info *arena = NULL;
	u32 premap = 0, old_postmap, new_postmap, lane = 0, i;
	struct log_entry log;
	int sub;

	while (len) {
		u32 cur_len;
		int e_flag;

 retry:
		lane = nd_region_acquire_lane(btt->nd_region);

		ret = lba_to_arena(btt, sector, &premap, &arena);
		if (ret)
			goto out_lane;
		cur_len = min(btt->sector_size, len);

		if ((arena->flags & IB_FLAG_ERROR_MASK) != 0) {
			ret = -EIO;
			goto out_lane;
		}

		if (btt_is_badblock(btt, arena, arena->freelist[lane].block))
			arena->freelist[lane].has_err = 1;

		if (mutex_is_locked(&arena->err_lock)
				|| arena->freelist[lane].has_err) {
			nd_region_release_lane(btt->nd_region, lane);

			ret = arena_clear_freelist_error(arena, lane);
			if (ret)
				return ret;

			 
			goto retry;
		}

		new_postmap = arena->freelist[lane].block;

		 
		for (i = 0; i < arena->nfree; i++)
			while (arena->rtt[i] == (RTT_VALID | new_postmap))
				cpu_relax();


		if (new_postmap >= arena->internal_nlba) {
			ret = -EIO;
			goto out_lane;
		}

		ret = btt_data_write(arena, new_postmap, page, off, cur_len);
		if (ret)
			goto out_lane;

		if (bip) {
			ret = btt_rw_integrity(btt, bip, arena, new_postmap,
						WRITE);
			if (ret)
				goto out_lane;
		}

		lock_map(arena, premap);
		ret = btt_map_read(arena, premap, &old_postmap, NULL, &e_flag,
				NVDIMM_IO_ATOMIC);
		if (ret)
			goto out_map;
		if (old_postmap >= arena->internal_nlba) {
			ret = -EIO;
			goto out_map;
		}
		if (e_flag)
			set_e_flag(old_postmap);

		log.lba = cpu_to_le32(premap);
		log.old_map = cpu_to_le32(old_postmap);
		log.new_map = cpu_to_le32(new_postmap);
		log.seq = cpu_to_le32(arena->freelist[lane].seq);
		sub = arena->freelist[lane].sub;
		ret = btt_flog_write(arena, lane, sub, &log);
		if (ret)
			goto out_map;

		ret = btt_map_write(arena, premap, new_postmap, 0, 0,
			NVDIMM_IO_ATOMIC);
		if (ret)
			goto out_map;

		unlock_map(arena, premap);
		nd_region_release_lane(btt->nd_region, lane);

		if (e_flag) {
			ret = arena_clear_freelist_error(arena, lane);
			if (ret)
				return ret;
		}

		len -= cur_len;
		off += cur_len;
		sector += btt->sector_size >> SECTOR_SHIFT;
	}

	return 0;

 out_map:
	unlock_map(arena, premap);
 out_lane:
	nd_region_release_lane(btt->nd_region, lane);
	return ret;
}

static int btt_do_bvec(struct btt *btt, struct bio_integrity_payload *bip,
			struct page *page, unsigned int len, unsigned int off,
			enum req_op op, sector_t sector)
{
	int ret;

	if (!op_is_write(op)) {
		ret = btt_read_pg(btt, bip, page, off, sector, len);
		flush_dcache_page(page);
	} else {
		flush_dcache_page(page);
		ret = btt_write_pg(btt, bip, sector, page, off, len);
	}

	return ret;
}

static void btt_submit_bio(struct bio *bio)
{
	struct bio_integrity_payload *bip = bio_integrity(bio);
	struct btt *btt = bio->bi_bdev->bd_disk->private_data;
	struct bvec_iter iter;
	unsigned long start;
	struct bio_vec bvec;
	int err = 0;
	bool do_acct;

	if (!bio_integrity_prep(bio))
		return;

	do_acct = blk_queue_io_stat(bio->bi_bdev->bd_disk->queue);
	if (do_acct)
		start = bio_start_io_acct(bio);
	bio_for_each_segment(bvec, bio, iter) {
		unsigned int len = bvec.bv_len;

		if (len > PAGE_SIZE || len < btt->sector_size ||
				len % btt->sector_size) {
			dev_err_ratelimited(&btt->nd_btt->dev,
				"unaligned bio segment (len: %d)\n", len);
			bio->bi_status = BLK_STS_IOERR;
			break;
		}

		err = btt_do_bvec(btt, bip, bvec.bv_page, len, bvec.bv_offset,
				  bio_op(bio), iter.bi_sector);
		if (err) {
			dev_err(&btt->nd_btt->dev,
					"io error in %s sector %lld, len %d,\n",
					(op_is_write(bio_op(bio))) ? "WRITE" :
					"READ",
					(unsigned long long) iter.bi_sector, len);
			bio->bi_status = errno_to_blk_status(err);
			break;
		}
	}
	if (do_acct)
		bio_end_io_acct(bio, start);

	bio_endio(bio);
}

static int btt_getgeo(struct block_device *bd, struct hd_geometry *geo)
{
	 
	geo->heads = 1 << 6;
	geo->sectors = 1 << 5;
	geo->cylinders = get_capacity(bd->bd_disk) >> 11;
	return 0;
}

static const struct block_device_operations btt_fops = {
	.owner =		THIS_MODULE,
	.submit_bio =		btt_submit_bio,
	.getgeo =		btt_getgeo,
};

static int btt_blk_init(struct btt *btt)
{
	struct nd_btt *nd_btt = btt->nd_btt;
	struct nd_namespace_common *ndns = nd_btt->ndns;
	int rc = -ENOMEM;

	btt->btt_disk = blk_alloc_disk(NUMA_NO_NODE);
	if (!btt->btt_disk)
		return -ENOMEM;

	nvdimm_namespace_disk_name(ndns, btt->btt_disk->disk_name);
	btt->btt_disk->first_minor = 0;
	btt->btt_disk->fops = &btt_fops;
	btt->btt_disk->private_data = btt;

	blk_queue_logical_block_size(btt->btt_disk->queue, btt->sector_size);
	blk_queue_max_hw_sectors(btt->btt_disk->queue, UINT_MAX);
	blk_queue_flag_set(QUEUE_FLAG_NONROT, btt->btt_disk->queue);
	blk_queue_flag_set(QUEUE_FLAG_SYNCHRONOUS, btt->btt_disk->queue);

	if (btt_meta_size(btt)) {
		rc = nd_integrity_init(btt->btt_disk, btt_meta_size(btt));
		if (rc)
			goto out_cleanup_disk;
	}

	set_capacity(btt->btt_disk, btt->nlba * btt->sector_size >> 9);
	rc = device_add_disk(&btt->nd_btt->dev, btt->btt_disk, NULL);
	if (rc)
		goto out_cleanup_disk;

	btt->nd_btt->size = btt->nlba * (u64)btt->sector_size;
	nvdimm_check_and_set_ro(btt->btt_disk);

	return 0;

out_cleanup_disk:
	put_disk(btt->btt_disk);
	return rc;
}

static void btt_blk_cleanup(struct btt *btt)
{
	del_gendisk(btt->btt_disk);
	put_disk(btt->btt_disk);
}

 
static struct btt *btt_init(struct nd_btt *nd_btt, unsigned long long rawsize,
			    u32 lbasize, uuid_t *uuid,
			    struct nd_region *nd_region)
{
	int ret;
	struct btt *btt;
	struct nd_namespace_io *nsio;
	struct device *dev = &nd_btt->dev;

	btt = devm_kzalloc(dev, sizeof(struct btt), GFP_KERNEL);
	if (!btt)
		return NULL;

	btt->nd_btt = nd_btt;
	btt->rawsize = rawsize;
	btt->lbasize = lbasize;
	btt->sector_size = ((lbasize >= 4096) ? 4096 : 512);
	INIT_LIST_HEAD(&btt->arena_list);
	mutex_init(&btt->init_lock);
	btt->nd_region = nd_region;
	nsio = to_nd_namespace_io(&nd_btt->ndns->dev);
	btt->phys_bb = &nsio->bb;

	ret = discover_arenas(btt);
	if (ret) {
		dev_err(dev, "init: error in arena_discover: %d\n", ret);
		return NULL;
	}

	if (btt->init_state != INIT_READY && nd_region->ro) {
		dev_warn(dev, "%s is read-only, unable to init btt metadata\n",
				dev_name(&nd_region->dev));
		return NULL;
	} else if (btt->init_state != INIT_READY) {
		btt->num_arenas = (rawsize / ARENA_MAX_SIZE) +
			((rawsize % ARENA_MAX_SIZE) ? 1 : 0);
		dev_dbg(dev, "init: %d arenas for %llu rawsize\n",
				btt->num_arenas, rawsize);

		ret = create_arenas(btt);
		if (ret) {
			dev_info(dev, "init: create_arenas: %d\n", ret);
			return NULL;
		}

		ret = btt_meta_init(btt);
		if (ret) {
			dev_err(dev, "init: error in meta_init: %d\n", ret);
			return NULL;
		}
	}

	ret = btt_blk_init(btt);
	if (ret) {
		dev_err(dev, "init: error in blk_init: %d\n", ret);
		return NULL;
	}

	btt_debugfs_init(btt);

	return btt;
}

 
static void btt_fini(struct btt *btt)
{
	if (btt) {
		btt_blk_cleanup(btt);
		free_arenas(btt);
		debugfs_remove_recursive(btt->debugfs_dir);
	}
}

int nvdimm_namespace_attach_btt(struct nd_namespace_common *ndns)
{
	struct nd_btt *nd_btt = to_nd_btt(ndns->claim);
	struct nd_region *nd_region;
	struct btt_sb *btt_sb;
	struct btt *btt;
	size_t size, rawsize;
	int rc;

	if (!nd_btt->uuid || !nd_btt->ndns || !nd_btt->lbasize) {
		dev_dbg(&nd_btt->dev, "incomplete btt configuration\n");
		return -ENODEV;
	}

	btt_sb = devm_kzalloc(&nd_btt->dev, sizeof(*btt_sb), GFP_KERNEL);
	if (!btt_sb)
		return -ENOMEM;

	size = nvdimm_namespace_capacity(ndns);
	rc = devm_namespace_enable(&nd_btt->dev, ndns, size);
	if (rc)
		return rc;

	 
	nd_btt_version(nd_btt, ndns, btt_sb);

	rawsize = size - nd_btt->initial_offset;
	if (rawsize < ARENA_MIN_SIZE) {
		dev_dbg(&nd_btt->dev, "%s must be at least %ld bytes\n",
				dev_name(&ndns->dev),
				ARENA_MIN_SIZE + nd_btt->initial_offset);
		return -ENXIO;
	}
	nd_region = to_nd_region(nd_btt->dev.parent);
	btt = btt_init(nd_btt, rawsize, nd_btt->lbasize, nd_btt->uuid,
		       nd_region);
	if (!btt)
		return -ENOMEM;
	nd_btt->btt = btt;

	return 0;
}
EXPORT_SYMBOL(nvdimm_namespace_attach_btt);

int nvdimm_namespace_detach_btt(struct nd_btt *nd_btt)
{
	struct btt *btt = nd_btt->btt;

	btt_fini(btt);
	nd_btt->btt = NULL;

	return 0;
}
EXPORT_SYMBOL(nvdimm_namespace_detach_btt);

static int __init nd_btt_init(void)
{
	int rc = 0;

	debugfs_root = debugfs_create_dir("btt", NULL);
	if (IS_ERR_OR_NULL(debugfs_root))
		rc = -ENXIO;

	return rc;
}

static void __exit nd_btt_exit(void)
{
	debugfs_remove_recursive(debugfs_root);
}

MODULE_ALIAS_ND_DEVICE(ND_DEVICE_BTT);
MODULE_AUTHOR("Vishal Verma <vishal.l.verma@linux.intel.com>");
MODULE_LICENSE("GPL v2");
module_init(nd_btt_init);
module_exit(nd_btt_exit);
