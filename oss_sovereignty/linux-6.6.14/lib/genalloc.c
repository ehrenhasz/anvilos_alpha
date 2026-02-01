
 

#include <linux/slab.h>
#include <linux/export.h>
#include <linux/bitmap.h>
#include <linux/rculist.h>
#include <linux/interrupt.h>
#include <linux/genalloc.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>

static inline size_t chunk_size(const struct gen_pool_chunk *chunk)
{
	return chunk->end_addr - chunk->start_addr + 1;
}

static inline int
set_bits_ll(unsigned long *addr, unsigned long mask_to_set)
{
	unsigned long val = READ_ONCE(*addr);

	do {
		if (val & mask_to_set)
			return -EBUSY;
		cpu_relax();
	} while (!try_cmpxchg(addr, &val, val | mask_to_set));

	return 0;
}

static inline int
clear_bits_ll(unsigned long *addr, unsigned long mask_to_clear)
{
	unsigned long val = READ_ONCE(*addr);

	do {
		if ((val & mask_to_clear) != mask_to_clear)
			return -EBUSY;
		cpu_relax();
	} while (!try_cmpxchg(addr, &val, val & ~mask_to_clear));

	return 0;
}

 
static unsigned long
bitmap_set_ll(unsigned long *map, unsigned long start, unsigned long nr)
{
	unsigned long *p = map + BIT_WORD(start);
	const unsigned long size = start + nr;
	int bits_to_set = BITS_PER_LONG - (start % BITS_PER_LONG);
	unsigned long mask_to_set = BITMAP_FIRST_WORD_MASK(start);

	while (nr >= bits_to_set) {
		if (set_bits_ll(p, mask_to_set))
			return nr;
		nr -= bits_to_set;
		bits_to_set = BITS_PER_LONG;
		mask_to_set = ~0UL;
		p++;
	}
	if (nr) {
		mask_to_set &= BITMAP_LAST_WORD_MASK(size);
		if (set_bits_ll(p, mask_to_set))
			return nr;
	}

	return 0;
}

 
static unsigned long
bitmap_clear_ll(unsigned long *map, unsigned long start, unsigned long nr)
{
	unsigned long *p = map + BIT_WORD(start);
	const unsigned long size = start + nr;
	int bits_to_clear = BITS_PER_LONG - (start % BITS_PER_LONG);
	unsigned long mask_to_clear = BITMAP_FIRST_WORD_MASK(start);

	while (nr >= bits_to_clear) {
		if (clear_bits_ll(p, mask_to_clear))
			return nr;
		nr -= bits_to_clear;
		bits_to_clear = BITS_PER_LONG;
		mask_to_clear = ~0UL;
		p++;
	}
	if (nr) {
		mask_to_clear &= BITMAP_LAST_WORD_MASK(size);
		if (clear_bits_ll(p, mask_to_clear))
			return nr;
	}

	return 0;
}

 
struct gen_pool *gen_pool_create(int min_alloc_order, int nid)
{
	struct gen_pool *pool;

	pool = kmalloc_node(sizeof(struct gen_pool), GFP_KERNEL, nid);
	if (pool != NULL) {
		spin_lock_init(&pool->lock);
		INIT_LIST_HEAD(&pool->chunks);
		pool->min_alloc_order = min_alloc_order;
		pool->algo = gen_pool_first_fit;
		pool->data = NULL;
		pool->name = NULL;
	}
	return pool;
}
EXPORT_SYMBOL(gen_pool_create);

 
int gen_pool_add_owner(struct gen_pool *pool, unsigned long virt, phys_addr_t phys,
		 size_t size, int nid, void *owner)
{
	struct gen_pool_chunk *chunk;
	unsigned long nbits = size >> pool->min_alloc_order;
	unsigned long nbytes = sizeof(struct gen_pool_chunk) +
				BITS_TO_LONGS(nbits) * sizeof(long);

	chunk = vzalloc_node(nbytes, nid);
	if (unlikely(chunk == NULL))
		return -ENOMEM;

	chunk->phys_addr = phys;
	chunk->start_addr = virt;
	chunk->end_addr = virt + size - 1;
	chunk->owner = owner;
	atomic_long_set(&chunk->avail, size);

	spin_lock(&pool->lock);
	list_add_rcu(&chunk->next_chunk, &pool->chunks);
	spin_unlock(&pool->lock);

	return 0;
}
EXPORT_SYMBOL(gen_pool_add_owner);

 
phys_addr_t gen_pool_virt_to_phys(struct gen_pool *pool, unsigned long addr)
{
	struct gen_pool_chunk *chunk;
	phys_addr_t paddr = -1;

	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &pool->chunks, next_chunk) {
		if (addr >= chunk->start_addr && addr <= chunk->end_addr) {
			paddr = chunk->phys_addr + (addr - chunk->start_addr);
			break;
		}
	}
	rcu_read_unlock();

	return paddr;
}
EXPORT_SYMBOL(gen_pool_virt_to_phys);

 
void gen_pool_destroy(struct gen_pool *pool)
{
	struct list_head *_chunk, *_next_chunk;
	struct gen_pool_chunk *chunk;
	int order = pool->min_alloc_order;
	unsigned long bit, end_bit;

	list_for_each_safe(_chunk, _next_chunk, &pool->chunks) {
		chunk = list_entry(_chunk, struct gen_pool_chunk, next_chunk);
		list_del(&chunk->next_chunk);

		end_bit = chunk_size(chunk) >> order;
		bit = find_first_bit(chunk->bits, end_bit);
		BUG_ON(bit < end_bit);

		vfree(chunk);
	}
	kfree_const(pool->name);
	kfree(pool);
}
EXPORT_SYMBOL(gen_pool_destroy);

 
unsigned long gen_pool_alloc_algo_owner(struct gen_pool *pool, size_t size,
		genpool_algo_t algo, void *data, void **owner)
{
	struct gen_pool_chunk *chunk;
	unsigned long addr = 0;
	int order = pool->min_alloc_order;
	unsigned long nbits, start_bit, end_bit, remain;

#ifndef CONFIG_ARCH_HAVE_NMI_SAFE_CMPXCHG
	BUG_ON(in_nmi());
#endif

	if (owner)
		*owner = NULL;

	if (size == 0)
		return 0;

	nbits = (size + (1UL << order) - 1) >> order;
	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &pool->chunks, next_chunk) {
		if (size > atomic_long_read(&chunk->avail))
			continue;

		start_bit = 0;
		end_bit = chunk_size(chunk) >> order;
retry:
		start_bit = algo(chunk->bits, end_bit, start_bit,
				 nbits, data, pool, chunk->start_addr);
		if (start_bit >= end_bit)
			continue;
		remain = bitmap_set_ll(chunk->bits, start_bit, nbits);
		if (remain) {
			remain = bitmap_clear_ll(chunk->bits, start_bit,
						 nbits - remain);
			BUG_ON(remain);
			goto retry;
		}

		addr = chunk->start_addr + ((unsigned long)start_bit << order);
		size = nbits << order;
		atomic_long_sub(size, &chunk->avail);
		if (owner)
			*owner = chunk->owner;
		break;
	}
	rcu_read_unlock();
	return addr;
}
EXPORT_SYMBOL(gen_pool_alloc_algo_owner);

 
void *gen_pool_dma_alloc(struct gen_pool *pool, size_t size, dma_addr_t *dma)
{
	return gen_pool_dma_alloc_algo(pool, size, dma, pool->algo, pool->data);
}
EXPORT_SYMBOL(gen_pool_dma_alloc);

 
void *gen_pool_dma_alloc_algo(struct gen_pool *pool, size_t size,
		dma_addr_t *dma, genpool_algo_t algo, void *data)
{
	unsigned long vaddr;

	if (!pool)
		return NULL;

	vaddr = gen_pool_alloc_algo(pool, size, algo, data);
	if (!vaddr)
		return NULL;

	if (dma)
		*dma = gen_pool_virt_to_phys(pool, vaddr);

	return (void *)vaddr;
}
EXPORT_SYMBOL(gen_pool_dma_alloc_algo);

 
void *gen_pool_dma_alloc_align(struct gen_pool *pool, size_t size,
		dma_addr_t *dma, int align)
{
	struct genpool_data_align data = { .align = align };

	return gen_pool_dma_alloc_algo(pool, size, dma,
			gen_pool_first_fit_align, &data);
}
EXPORT_SYMBOL(gen_pool_dma_alloc_align);

 
void *gen_pool_dma_zalloc(struct gen_pool *pool, size_t size, dma_addr_t *dma)
{
	return gen_pool_dma_zalloc_algo(pool, size, dma, pool->algo, pool->data);
}
EXPORT_SYMBOL(gen_pool_dma_zalloc);

 
void *gen_pool_dma_zalloc_algo(struct gen_pool *pool, size_t size,
		dma_addr_t *dma, genpool_algo_t algo, void *data)
{
	void *vaddr = gen_pool_dma_alloc_algo(pool, size, dma, algo, data);

	if (vaddr)
		memset(vaddr, 0, size);

	return vaddr;
}
EXPORT_SYMBOL(gen_pool_dma_zalloc_algo);

 
void *gen_pool_dma_zalloc_align(struct gen_pool *pool, size_t size,
		dma_addr_t *dma, int align)
{
	struct genpool_data_align data = { .align = align };

	return gen_pool_dma_zalloc_algo(pool, size, dma,
			gen_pool_first_fit_align, &data);
}
EXPORT_SYMBOL(gen_pool_dma_zalloc_align);

 
void gen_pool_free_owner(struct gen_pool *pool, unsigned long addr, size_t size,
		void **owner)
{
	struct gen_pool_chunk *chunk;
	int order = pool->min_alloc_order;
	unsigned long start_bit, nbits, remain;

#ifndef CONFIG_ARCH_HAVE_NMI_SAFE_CMPXCHG
	BUG_ON(in_nmi());
#endif

	if (owner)
		*owner = NULL;

	nbits = (size + (1UL << order) - 1) >> order;
	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &pool->chunks, next_chunk) {
		if (addr >= chunk->start_addr && addr <= chunk->end_addr) {
			BUG_ON(addr + size - 1 > chunk->end_addr);
			start_bit = (addr - chunk->start_addr) >> order;
			remain = bitmap_clear_ll(chunk->bits, start_bit, nbits);
			BUG_ON(remain);
			size = nbits << order;
			atomic_long_add(size, &chunk->avail);
			if (owner)
				*owner = chunk->owner;
			rcu_read_unlock();
			return;
		}
	}
	rcu_read_unlock();
	BUG();
}
EXPORT_SYMBOL(gen_pool_free_owner);

 
void gen_pool_for_each_chunk(struct gen_pool *pool,
	void (*func)(struct gen_pool *pool, struct gen_pool_chunk *chunk, void *data),
	void *data)
{
	struct gen_pool_chunk *chunk;

	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &(pool)->chunks, next_chunk)
		func(pool, chunk, data);
	rcu_read_unlock();
}
EXPORT_SYMBOL(gen_pool_for_each_chunk);

 
bool gen_pool_has_addr(struct gen_pool *pool, unsigned long start,
			size_t size)
{
	bool found = false;
	unsigned long end = start + size - 1;
	struct gen_pool_chunk *chunk;

	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &(pool)->chunks, next_chunk) {
		if (start >= chunk->start_addr && start <= chunk->end_addr) {
			if (end <= chunk->end_addr) {
				found = true;
				break;
			}
		}
	}
	rcu_read_unlock();
	return found;
}
EXPORT_SYMBOL(gen_pool_has_addr);

 
size_t gen_pool_avail(struct gen_pool *pool)
{
	struct gen_pool_chunk *chunk;
	size_t avail = 0;

	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &pool->chunks, next_chunk)
		avail += atomic_long_read(&chunk->avail);
	rcu_read_unlock();
	return avail;
}
EXPORT_SYMBOL_GPL(gen_pool_avail);

 
size_t gen_pool_size(struct gen_pool *pool)
{
	struct gen_pool_chunk *chunk;
	size_t size = 0;

	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &pool->chunks, next_chunk)
		size += chunk_size(chunk);
	rcu_read_unlock();
	return size;
}
EXPORT_SYMBOL_GPL(gen_pool_size);

 
void gen_pool_set_algo(struct gen_pool *pool, genpool_algo_t algo, void *data)
{
	rcu_read_lock();

	pool->algo = algo;
	if (!pool->algo)
		pool->algo = gen_pool_first_fit;

	pool->data = data;

	rcu_read_unlock();
}
EXPORT_SYMBOL(gen_pool_set_algo);

 
unsigned long gen_pool_first_fit(unsigned long *map, unsigned long size,
		unsigned long start, unsigned int nr, void *data,
		struct gen_pool *pool, unsigned long start_addr)
{
	return bitmap_find_next_zero_area(map, size, start, nr, 0);
}
EXPORT_SYMBOL(gen_pool_first_fit);

 
unsigned long gen_pool_first_fit_align(unsigned long *map, unsigned long size,
		unsigned long start, unsigned int nr, void *data,
		struct gen_pool *pool, unsigned long start_addr)
{
	struct genpool_data_align *alignment;
	unsigned long align_mask, align_off;
	int order;

	alignment = data;
	order = pool->min_alloc_order;
	align_mask = ((alignment->align + (1UL << order) - 1) >> order) - 1;
	align_off = (start_addr & (alignment->align - 1)) >> order;

	return bitmap_find_next_zero_area_off(map, size, start, nr,
					      align_mask, align_off);
}
EXPORT_SYMBOL(gen_pool_first_fit_align);

 
unsigned long gen_pool_fixed_alloc(unsigned long *map, unsigned long size,
		unsigned long start, unsigned int nr, void *data,
		struct gen_pool *pool, unsigned long start_addr)
{
	struct genpool_data_fixed *fixed_data;
	int order;
	unsigned long offset_bit;
	unsigned long start_bit;

	fixed_data = data;
	order = pool->min_alloc_order;
	offset_bit = fixed_data->offset >> order;
	if (WARN_ON(fixed_data->offset & ((1UL << order) - 1)))
		return size;

	start_bit = bitmap_find_next_zero_area(map, size,
			start + offset_bit, nr, 0);
	if (start_bit != offset_bit)
		start_bit = size;
	return start_bit;
}
EXPORT_SYMBOL(gen_pool_fixed_alloc);

 
unsigned long gen_pool_first_fit_order_align(unsigned long *map,
		unsigned long size, unsigned long start,
		unsigned int nr, void *data, struct gen_pool *pool,
		unsigned long start_addr)
{
	unsigned long align_mask = roundup_pow_of_two(nr) - 1;

	return bitmap_find_next_zero_area(map, size, start, nr, align_mask);
}
EXPORT_SYMBOL(gen_pool_first_fit_order_align);

 
unsigned long gen_pool_best_fit(unsigned long *map, unsigned long size,
		unsigned long start, unsigned int nr, void *data,
		struct gen_pool *pool, unsigned long start_addr)
{
	unsigned long start_bit = size;
	unsigned long len = size + 1;
	unsigned long index;

	index = bitmap_find_next_zero_area(map, size, start, nr, 0);

	while (index < size) {
		unsigned long next_bit = find_next_bit(map, size, index + nr);
		if ((next_bit - index) < len) {
			len = next_bit - index;
			start_bit = index;
			if (len == nr)
				return start_bit;
		}
		index = bitmap_find_next_zero_area(map, size,
						   next_bit + 1, nr, 0);
	}

	return start_bit;
}
EXPORT_SYMBOL(gen_pool_best_fit);

static void devm_gen_pool_release(struct device *dev, void *res)
{
	gen_pool_destroy(*(struct gen_pool **)res);
}

static int devm_gen_pool_match(struct device *dev, void *res, void *data)
{
	struct gen_pool **p = res;

	 
	if (!data && !(*p)->name)
		return 1;

	if (!data || !(*p)->name)
		return 0;

	return !strcmp((*p)->name, data);
}

 
struct gen_pool *gen_pool_get(struct device *dev, const char *name)
{
	struct gen_pool **p;

	p = devres_find(dev, devm_gen_pool_release, devm_gen_pool_match,
			(void *)name);
	if (!p)
		return NULL;
	return *p;
}
EXPORT_SYMBOL_GPL(gen_pool_get);

 
struct gen_pool *devm_gen_pool_create(struct device *dev, int min_alloc_order,
				      int nid, const char *name)
{
	struct gen_pool **ptr, *pool;
	const char *pool_name = NULL;

	 
	if (gen_pool_get(dev, name))
		return ERR_PTR(-EINVAL);

	if (name) {
		pool_name = kstrdup_const(name, GFP_KERNEL);
		if (!pool_name)
			return ERR_PTR(-ENOMEM);
	}

	ptr = devres_alloc(devm_gen_pool_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		goto free_pool_name;

	pool = gen_pool_create(min_alloc_order, nid);
	if (!pool)
		goto free_devres;

	*ptr = pool;
	pool->name = pool_name;
	devres_add(dev, ptr);

	return pool;

free_devres:
	devres_free(ptr);
free_pool_name:
	kfree_const(pool_name);

	return ERR_PTR(-ENOMEM);
}
EXPORT_SYMBOL(devm_gen_pool_create);

#ifdef CONFIG_OF
 
struct gen_pool *of_gen_pool_get(struct device_node *np,
	const char *propname, int index)
{
	struct platform_device *pdev;
	struct device_node *np_pool, *parent;
	const char *name = NULL;
	struct gen_pool *pool = NULL;

	np_pool = of_parse_phandle(np, propname, index);
	if (!np_pool)
		return NULL;

	pdev = of_find_device_by_node(np_pool);
	if (!pdev) {
		 
		parent = of_get_parent(np_pool);
		pdev = of_find_device_by_node(parent);
		of_node_put(parent);

		of_property_read_string(np_pool, "label", &name);
		if (!name)
			name = of_node_full_name(np_pool);
	}
	if (pdev)
		pool = gen_pool_get(&pdev->dev, name);
	of_node_put(np_pool);

	return pool;
}
EXPORT_SYMBOL_GPL(of_gen_pool_get);
#endif  
