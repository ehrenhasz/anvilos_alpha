
#ifndef _MM_PERCPU_INTERNAL_H
#define _MM_PERCPU_INTERNAL_H

#include <linux/types.h>
#include <linux/percpu.h>
#include <linux/memcontrol.h>


struct pcpu_block_md {
	int			scan_hint;	
	int			scan_hint_start; 
	int                     contig_hint;    
	int                     contig_hint_start; 
	int                     left_free;      
	int                     right_free;     
	int                     first_free;     
	int			nr_bits;	
};

struct pcpu_chunk {
#ifdef CONFIG_PERCPU_STATS
	int			nr_alloc;	
	size_t			max_alloc_size; 
#endif

	struct list_head	list;		
	int			free_bytes;	
	struct pcpu_block_md	chunk_md;
	unsigned long		*bound_map;	

	
	void			*base_addr ____cacheline_aligned_in_smp;

	unsigned long		*alloc_map;	
	struct pcpu_block_md	*md_blocks;	

	void			*data;		
	bool			immutable;	
	bool			isolated;	
	int			start_offset;	
	int			end_offset;	
#ifdef CONFIG_MEMCG_KMEM
	struct obj_cgroup	**obj_cgroups;	
#endif

	int			nr_pages;	
	int			nr_populated;	
	int                     nr_empty_pop_pages; 
	unsigned long		populated[];	
};

extern spinlock_t pcpu_lock;

extern struct list_head *pcpu_chunk_lists;
extern int pcpu_nr_slots;
extern int pcpu_sidelined_slot;
extern int pcpu_to_depopulate_slot;
extern int pcpu_nr_empty_pop_pages;

extern struct pcpu_chunk *pcpu_first_chunk;
extern struct pcpu_chunk *pcpu_reserved_chunk;


static inline int pcpu_chunk_nr_blocks(struct pcpu_chunk *chunk)
{
	return chunk->nr_pages * PAGE_SIZE / PCPU_BITMAP_BLOCK_SIZE;
}


static inline int pcpu_nr_pages_to_map_bits(int pages)
{
	return pages * PAGE_SIZE / PCPU_MIN_ALLOC_SIZE;
}


static inline int pcpu_chunk_map_bits(struct pcpu_chunk *chunk)
{
	return pcpu_nr_pages_to_map_bits(chunk->nr_pages);
}


static inline size_t pcpu_obj_full_size(size_t size)
{
	size_t extra_size = 0;

#ifdef CONFIG_MEMCG_KMEM
	if (!mem_cgroup_kmem_disabled())
		extra_size += size / PCPU_MIN_ALLOC_SIZE * sizeof(struct obj_cgroup *);
#endif

	return size * num_possible_cpus() + extra_size;
}

#ifdef CONFIG_PERCPU_STATS

#include <linux/spinlock.h>

struct percpu_stats {
	u64 nr_alloc;		
	u64 nr_dealloc;		
	u64 nr_cur_alloc;	
	u64 nr_max_alloc;	
	u32 nr_chunks;		
	u32 nr_max_chunks;	
	size_t min_alloc_size;	
	size_t max_alloc_size;	
};

extern struct percpu_stats pcpu_stats;
extern struct pcpu_alloc_info pcpu_stats_ai;


static inline void pcpu_stats_save_ai(const struct pcpu_alloc_info *ai)
{
	memcpy(&pcpu_stats_ai, ai, sizeof(struct pcpu_alloc_info));

	
	pcpu_stats.min_alloc_size = pcpu_stats_ai.unit_size;
}


static inline void pcpu_stats_area_alloc(struct pcpu_chunk *chunk, size_t size)
{
	lockdep_assert_held(&pcpu_lock);

	pcpu_stats.nr_alloc++;
	pcpu_stats.nr_cur_alloc++;
	pcpu_stats.nr_max_alloc =
		max(pcpu_stats.nr_max_alloc, pcpu_stats.nr_cur_alloc);
	pcpu_stats.min_alloc_size =
		min(pcpu_stats.min_alloc_size, size);
	pcpu_stats.max_alloc_size =
		max(pcpu_stats.max_alloc_size, size);

	chunk->nr_alloc++;
	chunk->max_alloc_size = max(chunk->max_alloc_size, size);
}


static inline void pcpu_stats_area_dealloc(struct pcpu_chunk *chunk)
{
	lockdep_assert_held(&pcpu_lock);

	pcpu_stats.nr_dealloc++;
	pcpu_stats.nr_cur_alloc--;

	chunk->nr_alloc--;
}


static inline void pcpu_stats_chunk_alloc(void)
{
	unsigned long flags;
	spin_lock_irqsave(&pcpu_lock, flags);

	pcpu_stats.nr_chunks++;
	pcpu_stats.nr_max_chunks =
		max(pcpu_stats.nr_max_chunks, pcpu_stats.nr_chunks);

	spin_unlock_irqrestore(&pcpu_lock, flags);
}


static inline void pcpu_stats_chunk_dealloc(void)
{
	unsigned long flags;
	spin_lock_irqsave(&pcpu_lock, flags);

	pcpu_stats.nr_chunks--;

	spin_unlock_irqrestore(&pcpu_lock, flags);
}

#else

static inline void pcpu_stats_save_ai(const struct pcpu_alloc_info *ai)
{
}

static inline void pcpu_stats_area_alloc(struct pcpu_chunk *chunk, size_t size)
{
}

static inline void pcpu_stats_area_dealloc(struct pcpu_chunk *chunk)
{
}

static inline void pcpu_stats_chunk_alloc(void)
{
}

static inline void pcpu_stats_chunk_dealloc(void)
{
}

#endif 

#endif
