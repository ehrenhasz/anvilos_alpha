#include <sys/zfs_context.h>
#include <sys/range_tree.h>
#include <sys/space_reftree.h>
static int
space_reftree_compare(const void *x1, const void *x2)
{
	const space_ref_t *sr1 = (const space_ref_t *)x1;
	const space_ref_t *sr2 = (const space_ref_t *)x2;
	int cmp = TREE_CMP(sr1->sr_offset, sr2->sr_offset);
	if (likely(cmp))
		return (cmp);
	return (TREE_PCMP(sr1, sr2));
}
void
space_reftree_create(avl_tree_t *t)
{
	avl_create(t, space_reftree_compare,
	    sizeof (space_ref_t), offsetof(space_ref_t, sr_node));
}
void
space_reftree_destroy(avl_tree_t *t)
{
	space_ref_t *sr;
	void *cookie = NULL;
	while ((sr = avl_destroy_nodes(t, &cookie)) != NULL)
		kmem_free(sr, sizeof (*sr));
	avl_destroy(t);
}
static void
space_reftree_add_node(avl_tree_t *t, uint64_t offset, int64_t refcnt)
{
	space_ref_t *sr;
	sr = kmem_alloc(sizeof (*sr), KM_SLEEP);
	sr->sr_offset = offset;
	sr->sr_refcnt = refcnt;
	avl_add(t, sr);
}
void
space_reftree_add_seg(avl_tree_t *t, uint64_t start, uint64_t end,
    int64_t refcnt)
{
	space_reftree_add_node(t, start, refcnt);
	space_reftree_add_node(t, end, -refcnt);
}
void
space_reftree_add_map(avl_tree_t *t, range_tree_t *rt, int64_t refcnt)
{
	zfs_btree_index_t where;
	for (range_seg_t *rs = zfs_btree_first(&rt->rt_root, &where); rs; rs =
	    zfs_btree_next(&rt->rt_root, &where, &where)) {
		space_reftree_add_seg(t, rs_get_start(rs, rt), rs_get_end(rs,
		    rt),  refcnt);
	}
}
void
space_reftree_generate_map(avl_tree_t *t, range_tree_t *rt, int64_t minref)
{
	uint64_t start = -1ULL;
	int64_t refcnt = 0;
	space_ref_t *sr;
	range_tree_vacate(rt, NULL, NULL);
	for (sr = avl_first(t); sr != NULL; sr = AVL_NEXT(t, sr)) {
		refcnt += sr->sr_refcnt;
		if (refcnt >= minref) {
			if (start == -1ULL) {
				start = sr->sr_offset;
			}
		} else {
			if (start != -1ULL) {
				uint64_t end = sr->sr_offset;
				ASSERT(start <= end);
				if (end > start)
					range_tree_add(rt, start, end - start);
				start = -1ULL;
			}
		}
	}
	ASSERT(refcnt == 0);
	ASSERT(start == -1ULL);
}
