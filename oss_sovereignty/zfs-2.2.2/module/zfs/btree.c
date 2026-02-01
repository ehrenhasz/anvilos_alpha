 
 

#include	<sys/btree.h>
#include	<sys/bitops.h>
#include	<sys/zfs_context.h>

kmem_cache_t *zfs_btree_leaf_cache;

 
uint_t zfs_btree_verify_intensity = 0;

 
static void
bcpy(const void *src, void *dest, size_t size)
{
	(void) memcpy(dest, src, size);
}

static void
bmov(const void *src, void *dest, size_t size)
{
	(void) memmove(dest, src, size);
}

static boolean_t
zfs_btree_is_core(struct zfs_btree_hdr *hdr)
{
	return (hdr->bth_first == -1);
}

#ifdef _ILP32
#define	BTREE_POISON 0xabadb10c
#else
#define	BTREE_POISON 0xabadb10cdeadbeef
#endif

static void
zfs_btree_poison_node(zfs_btree_t *tree, zfs_btree_hdr_t *hdr)
{
#ifdef ZFS_DEBUG
	size_t size = tree->bt_elem_size;
	if (zfs_btree_is_core(hdr)) {
		zfs_btree_core_t *node = (zfs_btree_core_t *)hdr;
		for (uint32_t i = hdr->bth_count + 1; i <= BTREE_CORE_ELEMS;
		    i++) {
			node->btc_children[i] =
			    (zfs_btree_hdr_t *)BTREE_POISON;
		}
		(void) memset(node->btc_elems + hdr->bth_count * size, 0x0f,
		    (BTREE_CORE_ELEMS - hdr->bth_count) * size);
	} else {
		zfs_btree_leaf_t *leaf = (zfs_btree_leaf_t *)hdr;
		(void) memset(leaf->btl_elems, 0x0f, hdr->bth_first * size);
		(void) memset(leaf->btl_elems +
		    (hdr->bth_first + hdr->bth_count) * size, 0x0f,
		    tree->bt_leaf_size - offsetof(zfs_btree_leaf_t, btl_elems) -
		    (hdr->bth_first + hdr->bth_count) * size);
	}
#endif
}

static inline void
zfs_btree_poison_node_at(zfs_btree_t *tree, zfs_btree_hdr_t *hdr,
    uint32_t idx, uint32_t count)
{
#ifdef ZFS_DEBUG
	size_t size = tree->bt_elem_size;
	if (zfs_btree_is_core(hdr)) {
		ASSERT3U(idx, >=, hdr->bth_count);
		ASSERT3U(idx, <=, BTREE_CORE_ELEMS);
		ASSERT3U(idx + count, <=, BTREE_CORE_ELEMS);
		zfs_btree_core_t *node = (zfs_btree_core_t *)hdr;
		for (uint32_t i = 1; i <= count; i++) {
			node->btc_children[idx + i] =
			    (zfs_btree_hdr_t *)BTREE_POISON;
		}
		(void) memset(node->btc_elems + idx * size, 0x0f, count * size);
	} else {
		ASSERT3U(idx, <=, tree->bt_leaf_cap);
		ASSERT3U(idx + count, <=, tree->bt_leaf_cap);
		zfs_btree_leaf_t *leaf = (zfs_btree_leaf_t *)hdr;
		(void) memset(leaf->btl_elems +
		    (hdr->bth_first + idx) * size, 0x0f, count * size);
	}
#endif
}

static inline void
zfs_btree_verify_poison_at(zfs_btree_t *tree, zfs_btree_hdr_t *hdr,
    uint32_t idx)
{
#ifdef ZFS_DEBUG
	size_t size = tree->bt_elem_size;
	if (zfs_btree_is_core(hdr)) {
		ASSERT3U(idx, <, BTREE_CORE_ELEMS);
		zfs_btree_core_t *node = (zfs_btree_core_t *)hdr;
		zfs_btree_hdr_t *cval = (zfs_btree_hdr_t *)BTREE_POISON;
		VERIFY3P(node->btc_children[idx + 1], ==, cval);
		for (size_t i = 0; i < size; i++)
			VERIFY3U(node->btc_elems[idx * size + i], ==, 0x0f);
	} else  {
		ASSERT3U(idx, <, tree->bt_leaf_cap);
		zfs_btree_leaf_t *leaf = (zfs_btree_leaf_t *)hdr;
		if (idx >= tree->bt_leaf_cap - hdr->bth_first)
			return;
		for (size_t i = 0; i < size; i++) {
			VERIFY3U(leaf->btl_elems[(hdr->bth_first + idx)
			    * size + i], ==, 0x0f);
		}
	}
#endif
}

void
zfs_btree_init(void)
{
	zfs_btree_leaf_cache = kmem_cache_create("zfs_btree_leaf_cache",
	    BTREE_LEAF_SIZE, 0, NULL, NULL, NULL, NULL, NULL, 0);
}

void
zfs_btree_fini(void)
{
	kmem_cache_destroy(zfs_btree_leaf_cache);
}

static void *
zfs_btree_leaf_alloc(zfs_btree_t *tree)
{
	if (tree->bt_leaf_size == BTREE_LEAF_SIZE)
		return (kmem_cache_alloc(zfs_btree_leaf_cache, KM_SLEEP));
	else
		return (kmem_alloc(tree->bt_leaf_size, KM_SLEEP));
}

static void
zfs_btree_leaf_free(zfs_btree_t *tree, void *ptr)
{
	if (tree->bt_leaf_size == BTREE_LEAF_SIZE)
		return (kmem_cache_free(zfs_btree_leaf_cache, ptr));
	else
		return (kmem_free(ptr, tree->bt_leaf_size));
}

void
zfs_btree_create(zfs_btree_t *tree, int (*compar) (const void *, const void *),
    bt_find_in_buf_f bt_find_in_buf, size_t size)
{
	zfs_btree_create_custom(tree, compar, bt_find_in_buf, size,
	    BTREE_LEAF_SIZE);
}

static void *
zfs_btree_find_in_buf(zfs_btree_t *tree, uint8_t *buf, uint32_t nelems,
    const void *value, zfs_btree_index_t *where);

void
zfs_btree_create_custom(zfs_btree_t *tree,
    int (*compar) (const void *, const void *),
    bt_find_in_buf_f bt_find_in_buf,
    size_t size, size_t lsize)
{
	size_t esize = lsize - offsetof(zfs_btree_leaf_t, btl_elems);

	ASSERT3U(size, <=, esize / 2);
	memset(tree, 0, sizeof (*tree));
	tree->bt_compar = compar;
	tree->bt_find_in_buf = (bt_find_in_buf == NULL) ?
	    zfs_btree_find_in_buf : bt_find_in_buf;
	tree->bt_elem_size = size;
	tree->bt_leaf_size = lsize;
	tree->bt_leaf_cap = P2ALIGN(esize / size, 2);
	tree->bt_height = -1;
	tree->bt_bulk = NULL;
}

 
static void *
zfs_btree_find_in_buf(zfs_btree_t *tree, uint8_t *buf, uint32_t nelems,
    const void *value, zfs_btree_index_t *where)
{
	uint32_t max = nelems;
	uint32_t min = 0;
	while (max > min) {
		uint32_t idx = (min + max) / 2;
		uint8_t *cur = buf + idx * tree->bt_elem_size;
		int comp = tree->bt_compar(cur, value);
		if (comp < 0) {
			min = idx + 1;
		} else if (comp > 0) {
			max = idx;
		} else {
			where->bti_offset = idx;
			where->bti_before = B_FALSE;
			return (cur);
		}
	}

	where->bti_offset = max;
	where->bti_before = B_TRUE;
	return (NULL);
}

 
void *
zfs_btree_find(zfs_btree_t *tree, const void *value, zfs_btree_index_t *where)
{
	if (tree->bt_height == -1) {
		if (where != NULL) {
			where->bti_node = NULL;
			where->bti_offset = 0;
		}
		ASSERT0(tree->bt_num_elems);
		return (NULL);
	}

	 
	zfs_btree_index_t idx;
	size_t size = tree->bt_elem_size;
	if (tree->bt_bulk != NULL) {
		zfs_btree_leaf_t *last_leaf = tree->bt_bulk;
		int comp = tree->bt_compar(last_leaf->btl_elems +
		    (last_leaf->btl_hdr.bth_first +
		    last_leaf->btl_hdr.bth_count - 1) * size, value);
		if (comp < 0) {
			 
			if (where != NULL) {
				where->bti_node = (zfs_btree_hdr_t *)last_leaf;
				where->bti_offset =
				    last_leaf->btl_hdr.bth_count;
				where->bti_before = B_TRUE;
			}
			return (NULL);
		} else if (comp == 0) {
			if (where != NULL) {
				where->bti_node = (zfs_btree_hdr_t *)last_leaf;
				where->bti_offset =
				    last_leaf->btl_hdr.bth_count - 1;
				where->bti_before = B_FALSE;
			}
			return (last_leaf->btl_elems +
			    (last_leaf->btl_hdr.bth_first +
			    last_leaf->btl_hdr.bth_count - 1) * size);
		}
		if (tree->bt_compar(last_leaf->btl_elems +
		    last_leaf->btl_hdr.bth_first * size, value) <= 0) {
			 
			void *d = tree->bt_find_in_buf(tree,
			    last_leaf->btl_elems +
			    last_leaf->btl_hdr.bth_first * size,
			    last_leaf->btl_hdr.bth_count, value, &idx);

			if (where != NULL) {
				idx.bti_node = (zfs_btree_hdr_t *)last_leaf;
				*where = idx;
			}
			return (d);
		}
	}

	zfs_btree_core_t *node = NULL;
	uint32_t child = 0;
	uint32_t depth = 0;

	 
	for (node = (zfs_btree_core_t *)tree->bt_root; depth < tree->bt_height;
	    node = (zfs_btree_core_t *)node->btc_children[child], depth++) {
		ASSERT3P(node, !=, NULL);
		void *d = tree->bt_find_in_buf(tree, node->btc_elems,
		    node->btc_hdr.bth_count, value, &idx);
		EQUIV(d != NULL, !idx.bti_before);
		if (d != NULL) {
			if (where != NULL) {
				idx.bti_node = (zfs_btree_hdr_t *)node;
				*where = idx;
			}
			return (d);
		}
		ASSERT(idx.bti_before);
		child = idx.bti_offset;
	}

	 
	zfs_btree_leaf_t *leaf = (depth == 0 ?
	    (zfs_btree_leaf_t *)tree->bt_root : (zfs_btree_leaf_t *)node);
	void *d = tree->bt_find_in_buf(tree, leaf->btl_elems +
	    leaf->btl_hdr.bth_first * size,
	    leaf->btl_hdr.bth_count, value, &idx);

	if (where != NULL) {
		idx.bti_node = (zfs_btree_hdr_t *)leaf;
		*where = idx;
	}

	return (d);
}

 

enum bt_shift_shape {
	BSS_TRAPEZOID,
	BSS_PARALLELOGRAM
};

enum bt_shift_direction {
	BSD_LEFT,
	BSD_RIGHT
};

 
static inline void
bt_shift_core(zfs_btree_t *tree, zfs_btree_core_t *node, uint32_t idx,
    uint32_t count, uint32_t off, enum bt_shift_shape shape,
    enum bt_shift_direction dir)
{
	size_t size = tree->bt_elem_size;
	ASSERT(zfs_btree_is_core(&node->btc_hdr));

	uint8_t *e_start = node->btc_elems + idx * size;
	uint8_t *e_out = (dir == BSD_LEFT ? e_start - off * size :
	    e_start + off * size);
	bmov(e_start, e_out, count * size);

	zfs_btree_hdr_t **c_start = node->btc_children + idx +
	    (shape == BSS_TRAPEZOID ? 0 : 1);
	zfs_btree_hdr_t **c_out = (dir == BSD_LEFT ? c_start - off :
	    c_start + off);
	uint32_t c_count = count + (shape == BSS_TRAPEZOID ? 1 : 0);
	bmov(c_start, c_out, c_count * sizeof (*c_start));
}

 
static inline void
bt_shift_core_left(zfs_btree_t *tree, zfs_btree_core_t *node, uint32_t idx,
    uint32_t count, enum bt_shift_shape shape)
{
	bt_shift_core(tree, node, idx, count, 1, shape, BSD_LEFT);
}

 
static inline void
bt_shift_core_right(zfs_btree_t *tree, zfs_btree_core_t *node, uint32_t idx,
    uint32_t count, enum bt_shift_shape shape)
{
	bt_shift_core(tree, node, idx, count, 1, shape, BSD_RIGHT);
}

 
static inline void
bt_shift_leaf(zfs_btree_t *tree, zfs_btree_leaf_t *node, uint32_t idx,
    uint32_t count, uint32_t off, enum bt_shift_direction dir)
{
	size_t size = tree->bt_elem_size;
	zfs_btree_hdr_t *hdr = &node->btl_hdr;
	ASSERT(!zfs_btree_is_core(hdr));

	if (count == 0)
		return;
	uint8_t *start = node->btl_elems + (hdr->bth_first + idx) * size;
	uint8_t *out = (dir == BSD_LEFT ? start - off * size :
	    start + off * size);
	bmov(start, out, count * size);
}

 
static void
bt_grow_leaf(zfs_btree_t *tree, zfs_btree_leaf_t *leaf, uint32_t idx,
    uint32_t n)
{
	zfs_btree_hdr_t *hdr = &leaf->btl_hdr;
	ASSERT(!zfs_btree_is_core(hdr));
	ASSERT3U(idx, <=, hdr->bth_count);
	uint32_t capacity = tree->bt_leaf_cap;
	ASSERT3U(hdr->bth_count + n, <=, capacity);
	boolean_t cl = (hdr->bth_first >= n);
	boolean_t cr = (hdr->bth_first + hdr->bth_count + n <= capacity);

	if (cl && (!cr || idx <= hdr->bth_count / 2)) {
		 
		hdr->bth_first -= n;
		bt_shift_leaf(tree, leaf, n, idx, n, BSD_LEFT);
	} else if (cr) {
		 
		bt_shift_leaf(tree, leaf, idx, hdr->bth_count - idx, n,
		    BSD_RIGHT);
	} else {
		 
		uint32_t fn = hdr->bth_first -
		    (capacity - (hdr->bth_count + n)) / 2;
		hdr->bth_first -= fn;
		bt_shift_leaf(tree, leaf, fn, idx, fn, BSD_LEFT);
		bt_shift_leaf(tree, leaf, fn + idx, hdr->bth_count - idx,
		    n - fn, BSD_RIGHT);
	}
	hdr->bth_count += n;
}

 
static void
bt_shrink_leaf(zfs_btree_t *tree, zfs_btree_leaf_t *leaf, uint32_t idx,
    uint32_t n)
{
	zfs_btree_hdr_t *hdr = &leaf->btl_hdr;
	ASSERT(!zfs_btree_is_core(hdr));
	ASSERT3U(idx, <=, hdr->bth_count);
	ASSERT3U(idx + n, <=, hdr->bth_count);

	if (idx <= (hdr->bth_count - n) / 2) {
		bt_shift_leaf(tree, leaf, 0, idx, n, BSD_RIGHT);
		zfs_btree_poison_node_at(tree, hdr, 0, n);
		hdr->bth_first += n;
	} else {
		bt_shift_leaf(tree, leaf, idx + n, hdr->bth_count - idx - n, n,
		    BSD_LEFT);
		zfs_btree_poison_node_at(tree, hdr, hdr->bth_count - n, n);
	}
	hdr->bth_count -= n;
}

 
static inline void
bt_transfer_core(zfs_btree_t *tree, zfs_btree_core_t *source, uint32_t sidx,
    uint32_t count, zfs_btree_core_t *dest, uint32_t didx,
    enum bt_shift_shape shape)
{
	size_t size = tree->bt_elem_size;
	ASSERT(zfs_btree_is_core(&source->btc_hdr));
	ASSERT(zfs_btree_is_core(&dest->btc_hdr));

	bcpy(source->btc_elems + sidx * size, dest->btc_elems + didx * size,
	    count * size);

	uint32_t c_count = count + (shape == BSS_TRAPEZOID ? 1 : 0);
	bcpy(source->btc_children + sidx + (shape == BSS_TRAPEZOID ? 0 : 1),
	    dest->btc_children + didx + (shape == BSS_TRAPEZOID ? 0 : 1),
	    c_count * sizeof (*source->btc_children));
}

static inline void
bt_transfer_leaf(zfs_btree_t *tree, zfs_btree_leaf_t *source, uint32_t sidx,
    uint32_t count, zfs_btree_leaf_t *dest, uint32_t didx)
{
	size_t size = tree->bt_elem_size;
	ASSERT(!zfs_btree_is_core(&source->btl_hdr));
	ASSERT(!zfs_btree_is_core(&dest->btl_hdr));

	bcpy(source->btl_elems + (source->btl_hdr.bth_first + sidx) * size,
	    dest->btl_elems + (dest->btl_hdr.bth_first + didx) * size,
	    count * size);
}

 
static void *
zfs_btree_first_helper(zfs_btree_t *tree, zfs_btree_hdr_t *hdr,
    zfs_btree_index_t *where)
{
	zfs_btree_hdr_t *node;

	for (node = hdr; zfs_btree_is_core(node);
	    node = ((zfs_btree_core_t *)node)->btc_children[0])
		;

	ASSERT(!zfs_btree_is_core(node));
	zfs_btree_leaf_t *leaf = (zfs_btree_leaf_t *)node;
	if (where != NULL) {
		where->bti_node = node;
		where->bti_offset = 0;
		where->bti_before = B_FALSE;
	}
	return (&leaf->btl_elems[node->bth_first * tree->bt_elem_size]);
}

 
static void
zfs_btree_insert_core_impl(zfs_btree_t *tree, zfs_btree_core_t *parent,
    uint32_t offset, zfs_btree_hdr_t *new_node, void *buf)
{
	size_t size = tree->bt_elem_size;
	zfs_btree_hdr_t *par_hdr = &parent->btc_hdr;
	ASSERT3P(par_hdr, ==, new_node->bth_parent);
	ASSERT3U(par_hdr->bth_count, <, BTREE_CORE_ELEMS);

	if (zfs_btree_verify_intensity >= 5) {
		zfs_btree_verify_poison_at(tree, par_hdr,
		    par_hdr->bth_count);
	}
	 
	uint32_t count = par_hdr->bth_count - offset;
	bt_shift_core_right(tree, parent, offset, count,
	    BSS_PARALLELOGRAM);

	 
	parent->btc_children[offset + 1] = new_node;
	bcpy(buf, parent->btc_elems + offset * size, size);
	par_hdr->bth_count++;
}

 
static void
zfs_btree_insert_into_parent(zfs_btree_t *tree, zfs_btree_hdr_t *old_node,
    zfs_btree_hdr_t *new_node, void *buf)
{
	ASSERT3P(old_node->bth_parent, ==, new_node->bth_parent);
	size_t size = tree->bt_elem_size;
	zfs_btree_core_t *parent = old_node->bth_parent;

	 
	if (parent == NULL) {
		ASSERT3P(old_node, ==, tree->bt_root);
		tree->bt_num_nodes++;
		zfs_btree_core_t *new_root =
		    kmem_alloc(sizeof (zfs_btree_core_t) + BTREE_CORE_ELEMS *
		    size, KM_SLEEP);
		zfs_btree_hdr_t *new_root_hdr = &new_root->btc_hdr;
		new_root_hdr->bth_parent = NULL;
		new_root_hdr->bth_first = -1;
		new_root_hdr->bth_count = 1;

		old_node->bth_parent = new_node->bth_parent = new_root;
		new_root->btc_children[0] = old_node;
		new_root->btc_children[1] = new_node;
		bcpy(buf, new_root->btc_elems, size);

		tree->bt_height++;
		tree->bt_root = new_root_hdr;
		zfs_btree_poison_node(tree, new_root_hdr);
		return;
	}

	 
	zfs_btree_hdr_t *par_hdr = &parent->btc_hdr;
	zfs_btree_index_t idx;
	ASSERT(zfs_btree_is_core(par_hdr));
	VERIFY3P(tree->bt_find_in_buf(tree, parent->btc_elems,
	    par_hdr->bth_count, buf, &idx), ==, NULL);
	ASSERT(idx.bti_before);
	uint32_t offset = idx.bti_offset;
	ASSERT3U(offset, <=, par_hdr->bth_count);
	ASSERT3P(parent->btc_children[offset], ==, old_node);

	 
	if (par_hdr->bth_count != BTREE_CORE_ELEMS) {
		zfs_btree_insert_core_impl(tree, parent, offset, new_node, buf);
		return;
	}

	 
	uint32_t move_count = MAX((BTREE_CORE_ELEMS / (tree->bt_bulk == NULL ?
	    2 : 4)) - 1, 2);
	uint32_t keep_count = BTREE_CORE_ELEMS - move_count - 1;
	ASSERT3U(BTREE_CORE_ELEMS - move_count, >=, 2);
	tree->bt_num_nodes++;
	zfs_btree_core_t *new_parent = kmem_alloc(sizeof (zfs_btree_core_t) +
	    BTREE_CORE_ELEMS * size, KM_SLEEP);
	zfs_btree_hdr_t *new_par_hdr = &new_parent->btc_hdr;
	new_par_hdr->bth_parent = par_hdr->bth_parent;
	new_par_hdr->bth_first = -1;
	new_par_hdr->bth_count = move_count;
	zfs_btree_poison_node(tree, new_par_hdr);

	par_hdr->bth_count = keep_count;

	bt_transfer_core(tree, parent, keep_count + 1, move_count, new_parent,
	    0, BSS_TRAPEZOID);

	 
	uint8_t *tmp_buf = kmem_alloc(size, KM_SLEEP);
	bcpy(parent->btc_elems + keep_count * size, tmp_buf,
	    size);
	zfs_btree_poison_node(tree, par_hdr);

	if (offset < keep_count) {
		 
		zfs_btree_insert_core_impl(tree, parent, offset, new_node,
		    buf);

		 
		bcpy(tmp_buf, buf, size);
	} else if (offset > keep_count) {
		 
		new_node->bth_parent = new_parent;
		zfs_btree_insert_core_impl(tree, new_parent,
		    offset - keep_count - 1, new_node, buf);

		 
		bcpy(tmp_buf, buf, size);
	} else {
		 
		bt_shift_core_right(tree, new_parent, 0, move_count,
		    BSS_TRAPEZOID);
		new_parent->btc_children[0] = new_node;
		bcpy(tmp_buf, new_parent->btc_elems, size);
		new_par_hdr->bth_count++;
	}
	kmem_free(tmp_buf, size);
	zfs_btree_poison_node(tree, par_hdr);

	for (uint32_t i = 0; i <= new_parent->btc_hdr.bth_count; i++)
		new_parent->btc_children[i]->bth_parent = new_parent;

	for (uint32_t i = 0; i <= parent->btc_hdr.bth_count; i++)
		ASSERT3P(parent->btc_children[i]->bth_parent, ==, parent);

	 
	zfs_btree_insert_into_parent(tree, &parent->btc_hdr,
	    &new_parent->btc_hdr, buf);
}

 
static void
zfs_btree_insert_leaf_impl(zfs_btree_t *tree, zfs_btree_leaf_t *leaf,
    uint32_t idx, const void *value)
{
	size_t size = tree->bt_elem_size;
	zfs_btree_hdr_t *hdr = &leaf->btl_hdr;
	ASSERT3U(leaf->btl_hdr.bth_count, <, tree->bt_leaf_cap);

	if (zfs_btree_verify_intensity >= 5) {
		zfs_btree_verify_poison_at(tree, &leaf->btl_hdr,
		    leaf->btl_hdr.bth_count);
	}

	bt_grow_leaf(tree, leaf, idx, 1);
	uint8_t *start = leaf->btl_elems + (hdr->bth_first + idx) * size;
	bcpy(value, start, size);
}

static void
zfs_btree_verify_order_helper(zfs_btree_t *tree, zfs_btree_hdr_t *hdr);

 
static void
zfs_btree_insert_into_leaf(zfs_btree_t *tree, zfs_btree_leaf_t *leaf,
    const void *value, uint32_t idx)
{
	size_t size = tree->bt_elem_size;
	uint32_t capacity = tree->bt_leaf_cap;

	 
	if (leaf->btl_hdr.bth_count != capacity) {
		zfs_btree_insert_leaf_impl(tree, leaf, idx, value);
		return;
	}

	 
	uint32_t move_count = MAX(capacity / (tree->bt_bulk ? 4 : 2), 1) - 1;
	uint32_t keep_count = capacity - move_count - 1;
	ASSERT3U(keep_count, >=, 1);
	 
	if (idx < keep_count) {
		keep_count--;
		move_count++;
	}
	tree->bt_num_nodes++;
	zfs_btree_leaf_t *new_leaf = zfs_btree_leaf_alloc(tree);
	zfs_btree_hdr_t *new_hdr = &new_leaf->btl_hdr;
	new_hdr->bth_parent = leaf->btl_hdr.bth_parent;
	new_hdr->bth_first = (tree->bt_bulk ? 0 : capacity / 4) +
	    (idx >= keep_count && idx <= keep_count + move_count / 2);
	new_hdr->bth_count = move_count;
	zfs_btree_poison_node(tree, new_hdr);

	if (tree->bt_bulk != NULL && leaf == tree->bt_bulk)
		tree->bt_bulk = new_leaf;

	 
	bt_transfer_leaf(tree, leaf, keep_count + 1, move_count, new_leaf, 0);

	 
	uint8_t *buf = kmem_alloc(size, KM_SLEEP);
	bcpy(leaf->btl_elems + (leaf->btl_hdr.bth_first + keep_count) * size,
	    buf, size);

	bt_shrink_leaf(tree, leaf, keep_count, 1 + move_count);

	if (idx < keep_count) {
		 
		zfs_btree_insert_leaf_impl(tree, leaf, idx, value);
	} else if (idx > keep_count) {
		 
		zfs_btree_insert_leaf_impl(tree, new_leaf, idx - keep_count -
		    1, value);
	} else {
		 
		zfs_btree_insert_leaf_impl(tree, new_leaf, 0, buf);
		bcpy(value, buf, size);
	}

	 
	zfs_btree_insert_into_parent(tree, &leaf->btl_hdr, &new_leaf->btl_hdr,
	    buf);
	kmem_free(buf, size);
}

static uint32_t
zfs_btree_find_parent_idx(zfs_btree_t *tree, zfs_btree_hdr_t *hdr)
{
	void *buf;
	if (zfs_btree_is_core(hdr)) {
		buf = ((zfs_btree_core_t *)hdr)->btc_elems;
	} else {
		buf = ((zfs_btree_leaf_t *)hdr)->btl_elems +
		    hdr->bth_first * tree->bt_elem_size;
	}
	zfs_btree_index_t idx;
	zfs_btree_core_t *parent = hdr->bth_parent;
	VERIFY3P(tree->bt_find_in_buf(tree, parent->btc_elems,
	    parent->btc_hdr.bth_count, buf, &idx), ==, NULL);
	ASSERT(idx.bti_before);
	ASSERT3U(idx.bti_offset, <=, parent->btc_hdr.bth_count);
	ASSERT3P(parent->btc_children[idx.bti_offset], ==, hdr);
	return (idx.bti_offset);
}

 
static void
zfs_btree_bulk_finish(zfs_btree_t *tree)
{
	ASSERT3P(tree->bt_bulk, !=, NULL);
	ASSERT3P(tree->bt_root, !=, NULL);
	zfs_btree_leaf_t *leaf = tree->bt_bulk;
	zfs_btree_hdr_t *hdr = &leaf->btl_hdr;
	zfs_btree_core_t *parent = hdr->bth_parent;
	size_t size = tree->bt_elem_size;
	uint32_t capacity = tree->bt_leaf_cap;

	 
	if (parent == NULL) {
		tree->bt_bulk = NULL;
		return;
	}

	 
	if (hdr->bth_count < capacity / 2) {
		 
		zfs_btree_index_t idx = {
			.bti_node = hdr,
			.bti_offset = 0
		};
		VERIFY3P(zfs_btree_prev(tree, &idx, &idx), !=, NULL);
		ASSERT(zfs_btree_is_core(idx.bti_node));
		zfs_btree_core_t *common = (zfs_btree_core_t *)idx.bti_node;
		uint32_t common_idx = idx.bti_offset;

		VERIFY3P(zfs_btree_prev(tree, &idx, &idx), !=, NULL);
		ASSERT(!zfs_btree_is_core(idx.bti_node));
		zfs_btree_leaf_t *l_neighbor = (zfs_btree_leaf_t *)idx.bti_node;
		zfs_btree_hdr_t *l_hdr = idx.bti_node;
		uint32_t move_count = (capacity / 2) - hdr->bth_count;
		ASSERT3U(l_neighbor->btl_hdr.bth_count - move_count, >=,
		    capacity / 2);

		if (zfs_btree_verify_intensity >= 5) {
			for (uint32_t i = 0; i < move_count; i++) {
				zfs_btree_verify_poison_at(tree, hdr,
				    leaf->btl_hdr.bth_count + i);
			}
		}

		 
		bt_grow_leaf(tree, leaf, 0, move_count);

		 
		uint8_t *separator = common->btc_elems + common_idx * size;
		uint8_t *out = leaf->btl_elems +
		    (hdr->bth_first + move_count - 1) * size;
		bcpy(separator, out, size);

		 
		bt_transfer_leaf(tree, l_neighbor, l_hdr->bth_count -
		    (move_count - 1), move_count - 1, leaf, 0);

		 
		bcpy(l_neighbor->btl_elems + (l_hdr->bth_first +
		    l_hdr->bth_count - move_count) * size, separator, size);

		 
		bt_shrink_leaf(tree, l_neighbor, l_hdr->bth_count - move_count,
		    move_count);

		ASSERT3U(l_hdr->bth_count, >=, capacity / 2);
		ASSERT3U(hdr->bth_count, >=, capacity / 2);
	}

	 
	capacity = BTREE_CORE_ELEMS;
	while (parent->btc_hdr.bth_parent != NULL) {
		zfs_btree_core_t *cur = parent;
		zfs_btree_hdr_t *hdr = &cur->btc_hdr;
		parent = hdr->bth_parent;
		 
		if (hdr->bth_count >= capacity / 2)
			continue;

		 
		uint32_t parent_idx = zfs_btree_find_parent_idx(tree, hdr);
		ASSERT3U(parent_idx, >, 0);
		zfs_btree_core_t *l_neighbor =
		    (zfs_btree_core_t *)parent->btc_children[parent_idx - 1];
		uint32_t move_count = (capacity / 2) - hdr->bth_count;
		ASSERT3U(l_neighbor->btc_hdr.bth_count - move_count, >=,
		    capacity / 2);

		if (zfs_btree_verify_intensity >= 5) {
			for (uint32_t i = 0; i < move_count; i++) {
				zfs_btree_verify_poison_at(tree, hdr,
				    hdr->bth_count + i);
			}
		}
		 
		bt_shift_core(tree, cur, 0, hdr->bth_count, move_count,
		    BSS_TRAPEZOID, BSD_RIGHT);

		 
		uint8_t *separator = parent->btc_elems + ((parent_idx - 1) *
		    size);
		uint8_t *e_out = cur->btc_elems + ((move_count - 1) * size);
		bcpy(separator, e_out, size);

		 
		move_count--;
		uint32_t move_idx = l_neighbor->btc_hdr.bth_count - move_count;
		bt_transfer_core(tree, l_neighbor, move_idx, move_count, cur, 0,
		    BSS_TRAPEZOID);

		 
		move_idx--;
		bcpy(l_neighbor->btc_elems + move_idx * size, separator, size);

		l_neighbor->btc_hdr.bth_count -= move_count + 1;
		hdr->bth_count += move_count + 1;

		ASSERT3U(l_neighbor->btc_hdr.bth_count, >=, capacity / 2);
		ASSERT3U(hdr->bth_count, >=, capacity / 2);

		zfs_btree_poison_node(tree, &l_neighbor->btc_hdr);

		for (uint32_t i = 0; i <= hdr->bth_count; i++)
			cur->btc_children[i]->bth_parent = cur;
	}

	tree->bt_bulk = NULL;
	zfs_btree_verify(tree);
}

 
void
zfs_btree_add_idx(zfs_btree_t *tree, const void *value,
    const zfs_btree_index_t *where)
{
	zfs_btree_index_t idx = {0};

	 
	if (tree->bt_bulk != NULL) {
		if (where->bti_node != &tree->bt_bulk->btl_hdr) {
			zfs_btree_bulk_finish(tree);
			VERIFY3P(zfs_btree_find(tree, value, &idx), ==, NULL);
			where = &idx;
		}
	}

	tree->bt_num_elems++;
	 
	if (where->bti_node == NULL) {
		ASSERT3U(tree->bt_num_elems, ==, 1);
		ASSERT3S(tree->bt_height, ==, -1);
		ASSERT3P(tree->bt_root, ==, NULL);
		ASSERT0(where->bti_offset);

		tree->bt_num_nodes++;
		zfs_btree_leaf_t *leaf = zfs_btree_leaf_alloc(tree);
		tree->bt_root = &leaf->btl_hdr;
		tree->bt_height++;

		zfs_btree_hdr_t *hdr = &leaf->btl_hdr;
		hdr->bth_parent = NULL;
		hdr->bth_first = 0;
		hdr->bth_count = 0;
		zfs_btree_poison_node(tree, hdr);

		zfs_btree_insert_into_leaf(tree, leaf, value, 0);
		tree->bt_bulk = leaf;
	} else if (!zfs_btree_is_core(where->bti_node)) {
		 
		zfs_btree_insert_into_leaf(tree,
		    (zfs_btree_leaf_t *)where->bti_node, value,
		    where->bti_offset);
	} else {
		 
		zfs_btree_core_t *node = (zfs_btree_core_t *)where->bti_node;

		 
		uint32_t off = where->bti_offset;
		zfs_btree_hdr_t *subtree = node->btc_children[off + 1];
		size_t size = tree->bt_elem_size;
		uint8_t *buf = kmem_alloc(size, KM_SLEEP);
		bcpy(node->btc_elems + off * size, buf, size);
		bcpy(value, node->btc_elems + off * size, size);

		 
		zfs_btree_index_t new_idx;
		VERIFY3P(zfs_btree_first_helper(tree, subtree, &new_idx), !=,
		    NULL);
		ASSERT0(new_idx.bti_offset);
		ASSERT(!zfs_btree_is_core(new_idx.bti_node));
		zfs_btree_insert_into_leaf(tree,
		    (zfs_btree_leaf_t *)new_idx.bti_node, buf, 0);
		kmem_free(buf, size);
	}
	zfs_btree_verify(tree);
}

 
void *
zfs_btree_first(zfs_btree_t *tree, zfs_btree_index_t *where)
{
	if (tree->bt_height == -1) {
		ASSERT0(tree->bt_num_elems);
		return (NULL);
	}
	return (zfs_btree_first_helper(tree, tree->bt_root, where));
}

 
static void *
zfs_btree_last_helper(zfs_btree_t *btree, zfs_btree_hdr_t *hdr,
    zfs_btree_index_t *where)
{
	zfs_btree_hdr_t *node;

	for (node = hdr; zfs_btree_is_core(node); node =
	    ((zfs_btree_core_t *)node)->btc_children[node->bth_count])
		;

	zfs_btree_leaf_t *leaf = (zfs_btree_leaf_t *)node;
	if (where != NULL) {
		where->bti_node = node;
		where->bti_offset = node->bth_count - 1;
		where->bti_before = B_FALSE;
	}
	return (leaf->btl_elems + (node->bth_first + node->bth_count - 1) *
	    btree->bt_elem_size);
}

 
void *
zfs_btree_last(zfs_btree_t *tree, zfs_btree_index_t *where)
{
	if (tree->bt_height == -1) {
		ASSERT0(tree->bt_num_elems);
		return (NULL);
	}
	return (zfs_btree_last_helper(tree, tree->bt_root, where));
}

 
static void *
zfs_btree_next_helper(zfs_btree_t *tree, const zfs_btree_index_t *idx,
    zfs_btree_index_t *out_idx,
    void (*done_func)(zfs_btree_t *, zfs_btree_hdr_t *))
{
	if (idx->bti_node == NULL) {
		ASSERT3S(tree->bt_height, ==, -1);
		return (NULL);
	}

	uint32_t offset = idx->bti_offset;
	if (!zfs_btree_is_core(idx->bti_node)) {
		 
		zfs_btree_leaf_t *leaf = (zfs_btree_leaf_t *)idx->bti_node;
		uint32_t new_off = offset + (idx->bti_before ? 0 : 1);
		if (leaf->btl_hdr.bth_count > new_off) {
			out_idx->bti_node = &leaf->btl_hdr;
			out_idx->bti_offset = new_off;
			out_idx->bti_before = B_FALSE;
			return (leaf->btl_elems + (leaf->btl_hdr.bth_first +
			    new_off) * tree->bt_elem_size);
		}

		zfs_btree_hdr_t *prev = &leaf->btl_hdr;
		for (zfs_btree_core_t *node = leaf->btl_hdr.bth_parent;
		    node != NULL; node = node->btc_hdr.bth_parent) {
			zfs_btree_hdr_t *hdr = &node->btc_hdr;
			ASSERT(zfs_btree_is_core(hdr));
			uint32_t i = zfs_btree_find_parent_idx(tree, prev);
			if (done_func != NULL)
				done_func(tree, prev);
			if (i == hdr->bth_count) {
				prev = hdr;
				continue;
			}
			out_idx->bti_node = hdr;
			out_idx->bti_offset = i;
			out_idx->bti_before = B_FALSE;
			return (node->btc_elems + i * tree->bt_elem_size);
		}
		if (done_func != NULL)
			done_func(tree, prev);
		 
		return (NULL);
	}

	 
	ASSERT(zfs_btree_is_core(idx->bti_node));
	zfs_btree_core_t *node = (zfs_btree_core_t *)idx->bti_node;
	if (idx->bti_before) {
		out_idx->bti_before = B_FALSE;
		return (node->btc_elems + offset * tree->bt_elem_size);
	}

	 
	zfs_btree_hdr_t *child = node->btc_children[offset + 1];
	return (zfs_btree_first_helper(tree, child, out_idx));
}

 
void *
zfs_btree_next(zfs_btree_t *tree, const zfs_btree_index_t *idx,
    zfs_btree_index_t *out_idx)
{
	return (zfs_btree_next_helper(tree, idx, out_idx, NULL));
}

 
void *
zfs_btree_prev(zfs_btree_t *tree, const zfs_btree_index_t *idx,
    zfs_btree_index_t *out_idx)
{
	if (idx->bti_node == NULL) {
		ASSERT3S(tree->bt_height, ==, -1);
		return (NULL);
	}

	uint32_t offset = idx->bti_offset;
	if (!zfs_btree_is_core(idx->bti_node)) {
		 
		zfs_btree_leaf_t *leaf = (zfs_btree_leaf_t *)idx->bti_node;
		if (offset != 0) {
			out_idx->bti_node = &leaf->btl_hdr;
			out_idx->bti_offset = offset - 1;
			out_idx->bti_before = B_FALSE;
			return (leaf->btl_elems + (leaf->btl_hdr.bth_first +
			    offset - 1) * tree->bt_elem_size);
		}
		zfs_btree_hdr_t *prev = &leaf->btl_hdr;
		for (zfs_btree_core_t *node = leaf->btl_hdr.bth_parent;
		    node != NULL; node = node->btc_hdr.bth_parent) {
			zfs_btree_hdr_t *hdr = &node->btc_hdr;
			ASSERT(zfs_btree_is_core(hdr));
			uint32_t i = zfs_btree_find_parent_idx(tree, prev);
			if (i == 0) {
				prev = hdr;
				continue;
			}
			out_idx->bti_node = hdr;
			out_idx->bti_offset = i - 1;
			out_idx->bti_before = B_FALSE;
			return (node->btc_elems + (i - 1) * tree->bt_elem_size);
		}
		 
		return (NULL);
	}

	 
	ASSERT(zfs_btree_is_core(idx->bti_node));
	zfs_btree_core_t *node = (zfs_btree_core_t *)idx->bti_node;
	zfs_btree_hdr_t *child = node->btc_children[offset];
	return (zfs_btree_last_helper(tree, child, out_idx));
}

 
void *
zfs_btree_get(zfs_btree_t *tree, zfs_btree_index_t *idx)
{
	ASSERT(!idx->bti_before);
	size_t size = tree->bt_elem_size;
	if (!zfs_btree_is_core(idx->bti_node)) {
		zfs_btree_leaf_t *leaf = (zfs_btree_leaf_t *)idx->bti_node;
		return (leaf->btl_elems + (leaf->btl_hdr.bth_first +
		    idx->bti_offset) * size);
	}
	zfs_btree_core_t *node = (zfs_btree_core_t *)idx->bti_node;
	return (node->btc_elems + idx->bti_offset * size);
}

 
void
zfs_btree_add(zfs_btree_t *tree, const void *node)
{
	zfs_btree_index_t where = {0};
	VERIFY3P(zfs_btree_find(tree, node, &where), ==, NULL);
	zfs_btree_add_idx(tree, node, &where);
}

 
static void
zfs_btree_node_destroy(zfs_btree_t *tree, zfs_btree_hdr_t *node)
{
	tree->bt_num_nodes--;
	if (!zfs_btree_is_core(node)) {
		zfs_btree_leaf_free(tree, node);
	} else {
		kmem_free(node, sizeof (zfs_btree_core_t) +
		    BTREE_CORE_ELEMS * tree->bt_elem_size);
	}
}

 
static void
zfs_btree_remove_from_node(zfs_btree_t *tree, zfs_btree_core_t *node,
    zfs_btree_hdr_t *rm_hdr)
{
	size_t size = tree->bt_elem_size;
	uint32_t min_count = (BTREE_CORE_ELEMS / 2) - 1;
	zfs_btree_hdr_t *hdr = &node->btc_hdr;
	 
	if (hdr->bth_parent == NULL && hdr->bth_count <= 1) {
		ASSERT3U(hdr->bth_count, ==, 1);
		ASSERT3P(tree->bt_root, ==, node);
		ASSERT3P(node->btc_children[1], ==, rm_hdr);
		tree->bt_root = node->btc_children[0];
		node->btc_children[0]->bth_parent = NULL;
		zfs_btree_node_destroy(tree, hdr);
		tree->bt_height--;
		return;
	}

	uint32_t idx;
	for (idx = 0; idx <= hdr->bth_count; idx++) {
		if (node->btc_children[idx] == rm_hdr)
			break;
	}
	ASSERT3U(idx, <=, hdr->bth_count);

	 
	if (hdr->bth_parent == NULL ||
	    hdr->bth_count > min_count) {
		 
		bt_shift_core_left(tree, node, idx, hdr->bth_count - idx,
		    BSS_PARALLELOGRAM);
		hdr->bth_count--;
		zfs_btree_poison_node_at(tree, hdr, hdr->bth_count, 1);
		return;
	}

	ASSERT3U(hdr->bth_count, ==, min_count);

	 
	zfs_btree_core_t *parent = hdr->bth_parent;
	uint32_t parent_idx = zfs_btree_find_parent_idx(tree, hdr);

	zfs_btree_hdr_t *l_hdr = (parent_idx == 0 ? NULL :
	    parent->btc_children[parent_idx - 1]);
	if (l_hdr != NULL && l_hdr->bth_count > min_count) {
		 
		ASSERT(zfs_btree_is_core(l_hdr));
		zfs_btree_core_t *neighbor = (zfs_btree_core_t *)l_hdr;

		 
		bt_shift_core_right(tree, node, 0, idx - 1, BSS_TRAPEZOID);

		 
		uint8_t *separator = parent->btc_elems + (parent_idx - 1) *
		    size;
		bcpy(separator, node->btc_elems, size);

		 
		node->btc_children[0] =
		    neighbor->btc_children[l_hdr->bth_count];
		node->btc_children[0]->bth_parent = node;

		 
		uint8_t *take_elem = neighbor->btc_elems +
		    (l_hdr->bth_count - 1) * size;
		bcpy(take_elem, separator, size);
		l_hdr->bth_count--;
		zfs_btree_poison_node_at(tree, l_hdr, l_hdr->bth_count, 1);
		return;
	}

	zfs_btree_hdr_t *r_hdr = (parent_idx == parent->btc_hdr.bth_count ?
	    NULL : parent->btc_children[parent_idx + 1]);
	if (r_hdr != NULL && r_hdr->bth_count > min_count) {
		 
		ASSERT(zfs_btree_is_core(r_hdr));
		zfs_btree_core_t *neighbor = (zfs_btree_core_t *)r_hdr;

		 
		bt_shift_core_left(tree, node, idx, hdr->bth_count - idx,
		    BSS_PARALLELOGRAM);

		 
		uint8_t *separator = parent->btc_elems + parent_idx * size;
		bcpy(separator, node->btc_elems + (hdr->bth_count - 1) * size,
		    size);

		 
		node->btc_children[hdr->bth_count] = neighbor->btc_children[0];
		node->btc_children[hdr->bth_count]->bth_parent = node;

		 
		uint8_t *take_elem = neighbor->btc_elems;
		bcpy(take_elem, separator, size);
		r_hdr->bth_count--;

		 
		bt_shift_core_left(tree, neighbor, 1, r_hdr->bth_count,
		    BSS_TRAPEZOID);
		zfs_btree_poison_node_at(tree, r_hdr, r_hdr->bth_count, 1);
		return;
	}

	 
	zfs_btree_hdr_t *new_rm_hdr, *keep_hdr;
	uint32_t new_idx = idx;
	if (l_hdr != NULL) {
		keep_hdr = l_hdr;
		new_rm_hdr = hdr;
		new_idx += keep_hdr->bth_count + 1;
	} else {
		ASSERT3P(r_hdr, !=, NULL);
		keep_hdr = hdr;
		new_rm_hdr = r_hdr;
		parent_idx++;
	}

	ASSERT(zfs_btree_is_core(keep_hdr));
	ASSERT(zfs_btree_is_core(new_rm_hdr));

	zfs_btree_core_t *keep = (zfs_btree_core_t *)keep_hdr;
	zfs_btree_core_t *rm = (zfs_btree_core_t *)new_rm_hdr;

	if (zfs_btree_verify_intensity >= 5) {
		for (uint32_t i = 0; i < new_rm_hdr->bth_count + 1; i++) {
			zfs_btree_verify_poison_at(tree, keep_hdr,
			    keep_hdr->bth_count + i);
		}
	}

	 
	uint8_t *e_out = keep->btc_elems + keep_hdr->bth_count * size;
	uint8_t *separator = parent->btc_elems + (parent_idx - 1) *
	    size;
	bcpy(separator, e_out, size);
	keep_hdr->bth_count++;

	 
	bt_transfer_core(tree, rm, 0, new_rm_hdr->bth_count, keep,
	    keep_hdr->bth_count, BSS_TRAPEZOID);

	uint32_t old_count = keep_hdr->bth_count;

	 
	keep_hdr->bth_count += new_rm_hdr->bth_count;
	ASSERT3U(keep_hdr->bth_count, ==, (min_count * 2) + 1);

	 
	ASSERT3P(keep->btc_children[new_idx], ==, rm_hdr);
	bt_shift_core_left(tree, keep, new_idx, keep_hdr->bth_count - new_idx,
	    BSS_PARALLELOGRAM);
	keep_hdr->bth_count--;

	 
	zfs_btree_hdr_t **new_start = keep->btc_children +
	    old_count - 1;
	for (uint32_t i = 0; i < new_rm_hdr->bth_count + 1; i++)
		new_start[i]->bth_parent = keep;
	for (uint32_t i = 0; i <= keep_hdr->bth_count; i++) {
		ASSERT3P(keep->btc_children[i]->bth_parent, ==, keep);
		ASSERT3P(keep->btc_children[i], !=, rm_hdr);
	}
	zfs_btree_poison_node_at(tree, keep_hdr, keep_hdr->bth_count, 1);

	new_rm_hdr->bth_count = 0;
	zfs_btree_remove_from_node(tree, parent, new_rm_hdr);
	zfs_btree_node_destroy(tree, new_rm_hdr);
}

 
void
zfs_btree_remove_idx(zfs_btree_t *tree, zfs_btree_index_t *where)
{
	size_t size = tree->bt_elem_size;
	zfs_btree_hdr_t *hdr = where->bti_node;
	uint32_t idx = where->bti_offset;

	ASSERT(!where->bti_before);
	if (tree->bt_bulk != NULL) {
		 
		uint8_t *value = zfs_btree_get(tree, where);
		uint8_t *tmp = kmem_alloc(size, KM_SLEEP);
		bcpy(value, tmp, size);
		zfs_btree_bulk_finish(tree);
		VERIFY3P(zfs_btree_find(tree, tmp, where), !=, NULL);
		kmem_free(tmp, size);
		hdr = where->bti_node;
		idx = where->bti_offset;
	}

	tree->bt_num_elems--;
	 
	if (zfs_btree_is_core(hdr)) {
		zfs_btree_core_t *node = (zfs_btree_core_t *)hdr;
		zfs_btree_hdr_t *left_subtree = node->btc_children[idx];
		void *new_value = zfs_btree_last_helper(tree, left_subtree,
		    where);
		ASSERT3P(new_value, !=, NULL);

		bcpy(new_value, node->btc_elems + idx * size, size);

		hdr = where->bti_node;
		idx = where->bti_offset;
		ASSERT(!where->bti_before);
	}

	 
	ASSERT(!zfs_btree_is_core(hdr));
	zfs_btree_leaf_t *leaf = (zfs_btree_leaf_t *)hdr;
	ASSERT3U(hdr->bth_count, >, 0);

	uint32_t min_count = (tree->bt_leaf_cap / 2) - 1;

	 
	if (hdr->bth_count > min_count || hdr->bth_parent == NULL) {
		bt_shrink_leaf(tree, leaf, idx, 1);
		if (hdr->bth_parent == NULL) {
			ASSERT0(tree->bt_height);
			if (hdr->bth_count == 0) {
				tree->bt_root = NULL;
				tree->bt_height--;
				zfs_btree_node_destroy(tree, &leaf->btl_hdr);
			}
		}
		zfs_btree_verify(tree);
		return;
	}
	ASSERT3U(hdr->bth_count, ==, min_count);

	 
	zfs_btree_core_t *parent = hdr->bth_parent;
	uint32_t parent_idx = zfs_btree_find_parent_idx(tree, hdr);

	zfs_btree_hdr_t *l_hdr = (parent_idx == 0 ? NULL :
	    parent->btc_children[parent_idx - 1]);
	if (l_hdr != NULL && l_hdr->bth_count > min_count) {
		 
		ASSERT(!zfs_btree_is_core(l_hdr));
		zfs_btree_leaf_t *neighbor = (zfs_btree_leaf_t *)l_hdr;

		 
		bt_shift_leaf(tree, leaf, 0, idx, 1, BSD_RIGHT);

		 
		uint8_t *separator = parent->btc_elems + (parent_idx - 1) *
		    size;
		bcpy(separator, leaf->btl_elems + hdr->bth_first * size, size);

		 
		uint8_t *take_elem = neighbor->btl_elems +
		    (l_hdr->bth_first + l_hdr->bth_count - 1) * size;
		bcpy(take_elem, separator, size);

		 
		bt_shrink_leaf(tree, neighbor, l_hdr->bth_count - 1, 1);
		zfs_btree_verify(tree);
		return;
	}

	zfs_btree_hdr_t *r_hdr = (parent_idx == parent->btc_hdr.bth_count ?
	    NULL : parent->btc_children[parent_idx + 1]);
	if (r_hdr != NULL && r_hdr->bth_count > min_count) {
		 
		ASSERT(!zfs_btree_is_core(r_hdr));
		zfs_btree_leaf_t *neighbor = (zfs_btree_leaf_t *)r_hdr;

		 
		bt_shift_leaf(tree, leaf, idx + 1, hdr->bth_count - idx - 1,
		    1, BSD_LEFT);

		 
		uint8_t *separator = parent->btc_elems + parent_idx * size;
		bcpy(separator, leaf->btl_elems + (hdr->bth_first +
		    hdr->bth_count - 1) * size, size);

		 
		uint8_t *take_elem = neighbor->btl_elems +
		    r_hdr->bth_first * size;
		bcpy(take_elem, separator, size);

		 
		bt_shrink_leaf(tree, neighbor, 0, 1);
		zfs_btree_verify(tree);
		return;
	}

	 
	zfs_btree_hdr_t *rm_hdr, *k_hdr;
	if (l_hdr != NULL) {
		k_hdr = l_hdr;
		rm_hdr = hdr;
	} else {
		ASSERT3P(r_hdr, !=, NULL);
		k_hdr = hdr;
		rm_hdr = r_hdr;
		parent_idx++;
	}
	ASSERT(!zfs_btree_is_core(k_hdr));
	ASSERT(!zfs_btree_is_core(rm_hdr));
	ASSERT3U(k_hdr->bth_count, ==, min_count);
	ASSERT3U(rm_hdr->bth_count, ==, min_count);
	zfs_btree_leaf_t *keep = (zfs_btree_leaf_t *)k_hdr;
	zfs_btree_leaf_t *rm = (zfs_btree_leaf_t *)rm_hdr;

	if (zfs_btree_verify_intensity >= 5) {
		for (uint32_t i = 0; i < rm_hdr->bth_count + 1; i++) {
			zfs_btree_verify_poison_at(tree, k_hdr,
			    k_hdr->bth_count + i);
		}
	}

	 
	bt_shrink_leaf(tree, leaf, idx, 1);

	 
	uint32_t k_count = k_hdr->bth_count;
	bt_grow_leaf(tree, keep, k_count, 1 + rm_hdr->bth_count);
	ASSERT3U(k_hdr->bth_count, ==, min_count * 2);

	 
	uint8_t *out = keep->btl_elems + (k_hdr->bth_first + k_count) * size;
	uint8_t *separator = parent->btc_elems + (parent_idx - 1) * size;
	bcpy(separator, out, size);

	 
	bt_transfer_leaf(tree, rm, 0, rm_hdr->bth_count, keep, k_count + 1);

	 
	zfs_btree_remove_from_node(tree, parent, rm_hdr);
	zfs_btree_node_destroy(tree, rm_hdr);
	zfs_btree_verify(tree);
}

 
void
zfs_btree_remove(zfs_btree_t *tree, const void *value)
{
	zfs_btree_index_t where = {0};
	VERIFY3P(zfs_btree_find(tree, value, &where), !=, NULL);
	zfs_btree_remove_idx(tree, &where);
}

 
ulong_t
zfs_btree_numnodes(zfs_btree_t *tree)
{
	return (tree->bt_num_elems);
}

 
void *
zfs_btree_destroy_nodes(zfs_btree_t *tree, zfs_btree_index_t **cookie)
{
	if (*cookie == NULL) {
		if (tree->bt_height == -1)
			return (NULL);
		*cookie = kmem_alloc(sizeof (**cookie), KM_SLEEP);
		return (zfs_btree_first(tree, *cookie));
	}

	void *rval = zfs_btree_next_helper(tree, *cookie, *cookie,
	    zfs_btree_node_destroy);
	if (rval == NULL)   {
		tree->bt_root = NULL;
		tree->bt_height = -1;
		tree->bt_num_elems = 0;
		kmem_free(*cookie, sizeof (**cookie));
		tree->bt_bulk = NULL;
	}
	return (rval);
}

static void
zfs_btree_clear_helper(zfs_btree_t *tree, zfs_btree_hdr_t *hdr)
{
	if (zfs_btree_is_core(hdr)) {
		zfs_btree_core_t *btc = (zfs_btree_core_t *)hdr;
		for (uint32_t i = 0; i <= hdr->bth_count; i++)
			zfs_btree_clear_helper(tree, btc->btc_children[i]);
	}

	zfs_btree_node_destroy(tree, hdr);
}

void
zfs_btree_clear(zfs_btree_t *tree)
{
	if (tree->bt_root == NULL) {
		ASSERT0(tree->bt_num_elems);
		return;
	}

	zfs_btree_clear_helper(tree, tree->bt_root);
	tree->bt_num_elems = 0;
	tree->bt_root = NULL;
	tree->bt_num_nodes = 0;
	tree->bt_height = -1;
	tree->bt_bulk = NULL;
}

void
zfs_btree_destroy(zfs_btree_t *tree)
{
	ASSERT0(tree->bt_num_elems);
	ASSERT3P(tree->bt_root, ==, NULL);
}

 
static void
zfs_btree_verify_pointers_helper(zfs_btree_t *tree, zfs_btree_hdr_t *hdr)
{
	if (!zfs_btree_is_core(hdr))
		return;

	zfs_btree_core_t *node = (zfs_btree_core_t *)hdr;
	for (uint32_t i = 0; i <= hdr->bth_count; i++) {
		VERIFY3P(node->btc_children[i]->bth_parent, ==, hdr);
		zfs_btree_verify_pointers_helper(tree, node->btc_children[i]);
	}
}

 
static void
zfs_btree_verify_pointers(zfs_btree_t *tree)
{
	if (tree->bt_height == -1) {
		VERIFY3P(tree->bt_root, ==, NULL);
		return;
	}
	VERIFY3P(tree->bt_root->bth_parent, ==, NULL);
	zfs_btree_verify_pointers_helper(tree, tree->bt_root);
}

 
static uint64_t
zfs_btree_verify_counts_helper(zfs_btree_t *tree, zfs_btree_hdr_t *hdr)
{
	if (!zfs_btree_is_core(hdr)) {
		if (tree->bt_root != hdr && tree->bt_bulk &&
		    hdr != &tree->bt_bulk->btl_hdr) {
			VERIFY3U(hdr->bth_count, >=, tree->bt_leaf_cap / 2 - 1);
		}

		return (hdr->bth_count);
	} else {

		zfs_btree_core_t *node = (zfs_btree_core_t *)hdr;
		uint64_t ret = hdr->bth_count;
		if (tree->bt_root != hdr && tree->bt_bulk == NULL)
			VERIFY3P(hdr->bth_count, >=, BTREE_CORE_ELEMS / 2 - 1);
		for (uint32_t i = 0; i <= hdr->bth_count; i++) {
			ret += zfs_btree_verify_counts_helper(tree,
			    node->btc_children[i]);
		}

		return (ret);
	}
}

 
static void
zfs_btree_verify_counts(zfs_btree_t *tree)
{
	EQUIV(tree->bt_num_elems == 0, tree->bt_height == -1);
	if (tree->bt_height == -1) {
		return;
	}
	VERIFY3P(zfs_btree_verify_counts_helper(tree, tree->bt_root), ==,
	    tree->bt_num_elems);
}

 
static uint64_t
zfs_btree_verify_height_helper(zfs_btree_t *tree, zfs_btree_hdr_t *hdr,
    int32_t height)
{
	if (!zfs_btree_is_core(hdr)) {
		VERIFY0(height);
		return (1);
	}

	zfs_btree_core_t *node = (zfs_btree_core_t *)hdr;
	uint64_t ret = 1;
	for (uint32_t i = 0; i <= hdr->bth_count; i++) {
		ret += zfs_btree_verify_height_helper(tree,
		    node->btc_children[i], height - 1);
	}
	return (ret);
}

 
static void
zfs_btree_verify_height(zfs_btree_t *tree)
{
	EQUIV(tree->bt_height == -1, tree->bt_root == NULL);
	if (tree->bt_height == -1) {
		return;
	}

	VERIFY3U(zfs_btree_verify_height_helper(tree, tree->bt_root,
	    tree->bt_height), ==, tree->bt_num_nodes);
}

 
static void
zfs_btree_verify_order_helper(zfs_btree_t *tree, zfs_btree_hdr_t *hdr)
{
	size_t size = tree->bt_elem_size;
	if (!zfs_btree_is_core(hdr)) {
		zfs_btree_leaf_t *leaf = (zfs_btree_leaf_t *)hdr;
		for (uint32_t i = 1; i < hdr->bth_count; i++) {
			VERIFY3S(tree->bt_compar(leaf->btl_elems +
			    (hdr->bth_first + i - 1) * size,
			    leaf->btl_elems +
			    (hdr->bth_first + i) * size), ==, -1);
		}
		return;
	}

	zfs_btree_core_t *node = (zfs_btree_core_t *)hdr;
	for (uint32_t i = 1; i < hdr->bth_count; i++) {
		VERIFY3S(tree->bt_compar(node->btc_elems + (i - 1) * size,
		    node->btc_elems + i * size), ==, -1);
	}
	for (uint32_t i = 0; i < hdr->bth_count; i++) {
		uint8_t *left_child_last = NULL;
		zfs_btree_hdr_t *left_child_hdr = node->btc_children[i];
		if (zfs_btree_is_core(left_child_hdr)) {
			zfs_btree_core_t *left_child =
			    (zfs_btree_core_t *)left_child_hdr;
			left_child_last = left_child->btc_elems +
			    (left_child_hdr->bth_count - 1) * size;
		} else {
			zfs_btree_leaf_t *left_child =
			    (zfs_btree_leaf_t *)left_child_hdr;
			left_child_last = left_child->btl_elems +
			    (left_child_hdr->bth_first +
			    left_child_hdr->bth_count - 1) * size;
		}
		int comp = tree->bt_compar(node->btc_elems + i * size,
		    left_child_last);
		if (comp <= 0) {
			panic("btree: compar returned %d (expected 1) at "
			    "%px %d: compar(%px,  %px)", comp, node, i,
			    node->btc_elems + i * size, left_child_last);
		}

		uint8_t *right_child_first = NULL;
		zfs_btree_hdr_t *right_child_hdr = node->btc_children[i + 1];
		if (zfs_btree_is_core(right_child_hdr)) {
			zfs_btree_core_t *right_child =
			    (zfs_btree_core_t *)right_child_hdr;
			right_child_first = right_child->btc_elems;
		} else {
			zfs_btree_leaf_t *right_child =
			    (zfs_btree_leaf_t *)right_child_hdr;
			right_child_first = right_child->btl_elems +
			    right_child_hdr->bth_first * size;
		}
		comp = tree->bt_compar(node->btc_elems + i * size,
		    right_child_first);
		if (comp >= 0) {
			panic("btree: compar returned %d (expected -1) at "
			    "%px %d: compar(%px,  %px)", comp, node, i,
			    node->btc_elems + i * size, right_child_first);
		}
	}
	for (uint32_t i = 0; i <= hdr->bth_count; i++)
		zfs_btree_verify_order_helper(tree, node->btc_children[i]);
}

 
static void
zfs_btree_verify_order(zfs_btree_t *tree)
{
	EQUIV(tree->bt_height == -1, tree->bt_root == NULL);
	if (tree->bt_height == -1) {
		return;
	}

	zfs_btree_verify_order_helper(tree, tree->bt_root);
}

#ifdef ZFS_DEBUG
 
static void
zfs_btree_verify_poison_helper(zfs_btree_t *tree, zfs_btree_hdr_t *hdr)
{
	size_t size = tree->bt_elem_size;
	if (!zfs_btree_is_core(hdr)) {
		zfs_btree_leaf_t *leaf = (zfs_btree_leaf_t *)hdr;
		for (size_t i = 0; i < hdr->bth_first * size; i++)
			VERIFY3U(leaf->btl_elems[i], ==, 0x0f);
		size_t esize = tree->bt_leaf_size -
		    offsetof(zfs_btree_leaf_t, btl_elems);
		for (size_t i = (hdr->bth_first + hdr->bth_count) * size;
		    i < esize; i++)
			VERIFY3U(leaf->btl_elems[i], ==, 0x0f);
	} else {
		zfs_btree_core_t *node = (zfs_btree_core_t *)hdr;
		for (size_t i = hdr->bth_count * size;
		    i < BTREE_CORE_ELEMS * size; i++)
			VERIFY3U(node->btc_elems[i], ==, 0x0f);

		for (uint32_t i = hdr->bth_count + 1; i <= BTREE_CORE_ELEMS;
		    i++) {
			VERIFY3P(node->btc_children[i], ==,
			    (zfs_btree_hdr_t *)BTREE_POISON);
		}

		for (uint32_t i = 0; i <= hdr->bth_count; i++) {
			zfs_btree_verify_poison_helper(tree,
			    node->btc_children[i]);
		}
	}
}
#endif

 
static void
zfs_btree_verify_poison(zfs_btree_t *tree)
{
#ifdef ZFS_DEBUG
	if (tree->bt_height == -1)
		return;
	zfs_btree_verify_poison_helper(tree, tree->bt_root);
#endif
}

void
zfs_btree_verify(zfs_btree_t *tree)
{
	if (zfs_btree_verify_intensity == 0)
		return;
	zfs_btree_verify_height(tree);
	if (zfs_btree_verify_intensity == 1)
		return;
	zfs_btree_verify_pointers(tree);
	if (zfs_btree_verify_intensity == 2)
		return;
	zfs_btree_verify_counts(tree);
	if (zfs_btree_verify_intensity == 3)
		return;
	zfs_btree_verify_order(tree);

	if (zfs_btree_verify_intensity == 4)
		return;
	zfs_btree_verify_poison(tree);
}

 
ZFS_MODULE_PARAM(zfs, zfs_, btree_verify_intensity, UINT, ZMOD_RW,
	"Enable btree verification. Levels above 4 require ZFS be built "
	"with debugging");
 
