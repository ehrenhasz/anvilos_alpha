

#ifndef __XFS_SCRUB_XFARRAY_H__
#define __XFS_SCRUB_XFARRAY_H__


typedef uint64_t		xfarray_idx_t;
#define XFARRAY_CURSOR_INIT	((__force xfarray_idx_t)0)


#define foreach_xfarray_idx(array, idx) \
	for ((idx) = XFARRAY_CURSOR_INIT; \
	     (idx) < xfarray_length(array); \
	     (idx)++)

struct xfarray {
	
	struct xfile	*xfile;

	
	xfarray_idx_t	nr;

	
	xfarray_idx_t	max_nr;

	
	uint64_t	unset_slots;

	
	size_t		obj_size;

	
	int		obj_size_log;
};

int xfarray_create(const char *descr, unsigned long long required_capacity,
		size_t obj_size, struct xfarray **arrayp);
void xfarray_destroy(struct xfarray *array);
int xfarray_load(struct xfarray *array, xfarray_idx_t idx, void *ptr);
int xfarray_unset(struct xfarray *array, xfarray_idx_t idx);
int xfarray_store(struct xfarray *array, xfarray_idx_t idx, const void *ptr);
int xfarray_store_anywhere(struct xfarray *array, const void *ptr);
bool xfarray_element_is_null(struct xfarray *array, const void *ptr);


static inline int xfarray_append(struct xfarray *array, const void *ptr)
{
	return xfarray_store(array, array->nr, ptr);
}

uint64_t xfarray_length(struct xfarray *array);
int xfarray_load_next(struct xfarray *array, xfarray_idx_t *idx, void *rec);



typedef cmp_func_t xfarray_cmp_fn;


#define XFARRAY_ISORT_SHIFT		(4)
#define XFARRAY_ISORT_NR		(1U << XFARRAY_ISORT_SHIFT)


#define XFARRAY_QSORT_PIVOT_NR		(9)

struct xfarray_sortinfo {
	struct xfarray		*array;

	
	xfarray_cmp_fn		cmp_fn;

	
	uint8_t			max_stack_depth;

	
	int8_t			stack_depth;

	
	uint8_t			max_stack_used;

	
	unsigned int		flags;

	
	struct xfile_page	xfpage;
	void			*page_kaddr;

#ifdef DEBUG
	
	uint64_t		loads;
	uint64_t		stores;
	uint64_t		compares;
	uint64_t		heapsorts;
#endif
	
};


#define XFARRAY_SORT_KILLABLE	(1U << 0)

int xfarray_sort(struct xfarray *array, xfarray_cmp_fn cmp_fn,
		unsigned int flags);

#endif 
