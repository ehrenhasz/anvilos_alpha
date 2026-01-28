#include "test_util.h"
#include "sparsebit.h"
#include <limits.h>
#include <assert.h>
#define DUMP_LINE_MAX 100  
typedef uint32_t mask_t;
#define MASK_BITS (sizeof(mask_t) * CHAR_BIT)
struct node {
	struct node *parent;
	struct node *left;
	struct node *right;
	sparsebit_idx_t idx;  
	sparsebit_num_t num_after;  
	mask_t mask;
};
struct sparsebit {
	struct node *root;
	sparsebit_num_t num_set;
};
static sparsebit_num_t node_num_set(struct node *nodep)
{
	return nodep->num_after + __builtin_popcount(nodep->mask);
}
static struct node *node_first(struct sparsebit *s)
{
	struct node *nodep;
	for (nodep = s->root; nodep && nodep->left; nodep = nodep->left)
		;
	return nodep;
}
static struct node *node_next(struct sparsebit *s, struct node *np)
{
	struct node *nodep = np;
	if (nodep->right) {
		for (nodep = nodep->right; nodep->left; nodep = nodep->left)
			;
		return nodep;
	}
	while (nodep->parent && nodep == nodep->parent->right)
		nodep = nodep->parent;
	return nodep->parent;
}
static struct node *node_prev(struct sparsebit *s, struct node *np)
{
	struct node *nodep = np;
	if (nodep->left) {
		for (nodep = nodep->left; nodep->right; nodep = nodep->right)
			;
		return (struct node *) nodep;
	}
	while (nodep->parent && nodep == nodep->parent->left)
		nodep = nodep->parent;
	return (struct node *) nodep->parent;
}
static struct node *node_copy_subtree(struct node *subtree)
{
	struct node *root;
	root = calloc(1, sizeof(*root));
	if (!root) {
		perror("calloc");
		abort();
	}
	root->idx = subtree->idx;
	root->mask = subtree->mask;
	root->num_after = subtree->num_after;
	if (subtree->left) {
		root->left = node_copy_subtree(subtree->left);
		root->left->parent = root;
	}
	if (subtree->right) {
		root->right = node_copy_subtree(subtree->right);
		root->right->parent = root;
	}
	return root;
}
static struct node *node_find(struct sparsebit *s, sparsebit_idx_t idx)
{
	struct node *nodep;
	for (nodep = s->root; nodep;
	     nodep = nodep->idx > idx ? nodep->left : nodep->right) {
		if (idx >= nodep->idx &&
		    idx <= nodep->idx + MASK_BITS + nodep->num_after - 1)
			break;
	}
	return nodep;
}
static struct node *node_add(struct sparsebit *s, sparsebit_idx_t idx)
{
	struct node *nodep, *parentp, *prev;
	nodep = calloc(1, sizeof(*nodep));
	if (!nodep) {
		perror("calloc");
		abort();
	}
	nodep->idx = idx & -MASK_BITS;
	if (!s->root) {
		s->root = nodep;
		return nodep;
	}
	parentp = s->root;
	while (true) {
		if (idx < parentp->idx) {
			if (!parentp->left) {
				parentp->left = nodep;
				nodep->parent = parentp;
				break;
			}
			parentp = parentp->left;
		} else {
			assert(idx > parentp->idx + MASK_BITS + parentp->num_after - 1);
			if (!parentp->right) {
				parentp->right = nodep;
				nodep->parent = parentp;
				break;
			}
			parentp = parentp->right;
		}
	}
	prev = node_prev(s, nodep);
	while (prev && prev->idx + MASK_BITS + prev->num_after - 1 >= nodep->idx) {
		unsigned int n1 = (prev->idx + MASK_BITS + prev->num_after - 1)
			- nodep->idx;
		assert(prev->num_after > 0);
		assert(n1 < MASK_BITS);
		assert(!(nodep->mask & (1 << n1)));
		nodep->mask |= (1 << n1);
		prev->num_after--;
	}
	return nodep;
}
bool sparsebit_all_set(struct sparsebit *s)
{
	return s->root && s->num_set == 0;
}
static void node_rm(struct sparsebit *s, struct node *nodep)
{
	struct node *tmp;
	sparsebit_num_t num_set;
	num_set = node_num_set(nodep);
	assert(s->num_set >= num_set || sparsebit_all_set(s));
	s->num_set -= node_num_set(nodep);
	if (nodep->left && nodep->right) {
		for (tmp = nodep->right; tmp->left; tmp = tmp->left)
			;
		tmp->left = nodep->left;
		nodep->left = NULL;
		tmp->left->parent = tmp;
	}
	if (nodep->left) {
		if (!nodep->parent) {
			s->root = nodep->left;
			nodep->left->parent = NULL;
		} else {
			nodep->left->parent = nodep->parent;
			if (nodep == nodep->parent->left)
				nodep->parent->left = nodep->left;
			else {
				assert(nodep == nodep->parent->right);
				nodep->parent->right = nodep->left;
			}
		}
		nodep->parent = nodep->left = nodep->right = NULL;
		free(nodep);
		return;
	}
	if (nodep->right) {
		if (!nodep->parent) {
			s->root = nodep->right;
			nodep->right->parent = NULL;
		} else {
			nodep->right->parent = nodep->parent;
			if (nodep == nodep->parent->left)
				nodep->parent->left = nodep->right;
			else {
				assert(nodep == nodep->parent->right);
				nodep->parent->right = nodep->right;
			}
		}
		nodep->parent = nodep->left = nodep->right = NULL;
		free(nodep);
		return;
	}
	if (!nodep->parent) {
		s->root = NULL;
	} else {
		if (nodep->parent->left == nodep)
			nodep->parent->left = NULL;
		else {
			assert(nodep == nodep->parent->right);
			nodep->parent->right = NULL;
		}
	}
	nodep->parent = nodep->left = nodep->right = NULL;
	free(nodep);
	return;
}
static struct node *node_split(struct sparsebit *s, sparsebit_idx_t idx)
{
	struct node *nodep1, *nodep2;
	sparsebit_idx_t offset;
	sparsebit_num_t orig_num_after;
	assert(!(idx % MASK_BITS));
	nodep1 = node_find(s, idx);
	if (!nodep1)
		return node_add(s, idx);
	if (nodep1->idx == idx)
		return nodep1;
	offset = idx - (nodep1->idx + MASK_BITS);
	orig_num_after = nodep1->num_after;
	nodep1->num_after = offset;
	nodep2 = node_add(s, idx);
	nodep2->num_after = orig_num_after - offset;
	if (nodep2->num_after >= MASK_BITS) {
		nodep2->mask = ~(mask_t) 0;
		nodep2->num_after -= MASK_BITS;
	} else {
		nodep2->mask = (1 << nodep2->num_after) - 1;
		nodep2->num_after = 0;
	}
	return nodep2;
}
static void node_reduce(struct sparsebit *s, struct node *nodep)
{
	bool reduction_performed;
	do {
		reduction_performed = false;
		struct node *prev, *next, *tmp;
		if (nodep->mask == 0 && nodep->num_after == 0) {
			tmp = node_next(s, nodep);
			if (!tmp)
				tmp = node_prev(s, nodep);
			node_rm(s, nodep);
			nodep = tmp;
			reduction_performed = true;
			continue;
		}
		if (nodep->mask == 0) {
			assert(nodep->num_after != 0);
			assert(nodep->idx + MASK_BITS > nodep->idx);
			nodep->idx += MASK_BITS;
			if (nodep->num_after >= MASK_BITS) {
				nodep->mask = ~0;
				nodep->num_after -= MASK_BITS;
			} else {
				nodep->mask = (1u << nodep->num_after) - 1;
				nodep->num_after = 0;
			}
			reduction_performed = true;
			continue;
		}
		prev = node_prev(s, nodep);
		if (prev) {
			sparsebit_idx_t prev_highest_bit;
			if (prev->mask == 0 && prev->num_after == 0) {
				node_rm(s, prev);
				reduction_performed = true;
				continue;
			}
			if (nodep->mask + 1 == 0 &&
			    prev->idx + MASK_BITS == nodep->idx) {
				prev->num_after += MASK_BITS + nodep->num_after;
				nodep->mask = 0;
				nodep->num_after = 0;
				reduction_performed = true;
				continue;
			}
			prev_highest_bit = prev->idx + MASK_BITS - 1 + prev->num_after;
			if (prev_highest_bit + 1 == nodep->idx &&
			    (nodep->mask | (nodep->mask >> 1)) == nodep->mask) {
				unsigned int num_contiguous
					= __builtin_popcount(nodep->mask);
				assert((num_contiguous > 0) &&
				       ((1ULL << num_contiguous) - 1) == nodep->mask);
				prev->num_after += num_contiguous;
				nodep->mask = 0;
				if (num_contiguous == MASK_BITS) {
					prev->num_after += nodep->num_after;
					nodep->num_after = 0;
				}
				reduction_performed = true;
				continue;
			}
		}
		next = node_next(s, nodep);
		if (next) {
			if (next->mask == 0 && next->num_after == 0) {
				node_rm(s, next);
				reduction_performed = true;
				continue;
			}
			if (next->idx == nodep->idx + MASK_BITS + nodep->num_after &&
			    next->mask == ~(mask_t) 0) {
				nodep->num_after += MASK_BITS;
				next->mask = 0;
				nodep->num_after += next->num_after;
				next->num_after = 0;
				node_rm(s, next);
				next = NULL;
				reduction_performed = true;
				continue;
			}
		}
	} while (nodep && reduction_performed);
}
bool sparsebit_is_set(struct sparsebit *s, sparsebit_idx_t idx)
{
	struct node *nodep;
	for (nodep = s->root; nodep;
	     nodep = nodep->idx > idx ? nodep->left : nodep->right)
		if (idx >= nodep->idx &&
		    idx <= nodep->idx + MASK_BITS + nodep->num_after - 1)
			goto have_node;
	return false;
have_node:
	if (nodep->num_after && idx >= nodep->idx + MASK_BITS)
		return true;
	assert(idx >= nodep->idx && idx - nodep->idx < MASK_BITS);
	return !!(nodep->mask & (1 << (idx - nodep->idx)));
}
static void bit_set(struct sparsebit *s, sparsebit_idx_t idx)
{
	struct node *nodep;
	if (sparsebit_is_set(s, idx))
		return;
	nodep = node_split(s, idx & -MASK_BITS);
	assert(idx >= nodep->idx && idx <= nodep->idx + MASK_BITS - 1);
	assert(!(nodep->mask & (1 << (idx - nodep->idx))));
	nodep->mask |= 1 << (idx - nodep->idx);
	s->num_set++;
	node_reduce(s, nodep);
}
static void bit_clear(struct sparsebit *s, sparsebit_idx_t idx)
{
	struct node *nodep;
	if (!sparsebit_is_set(s, idx))
		return;
	nodep = node_find(s, idx);
	if (!nodep)
		return;
	if (idx >= nodep->idx + MASK_BITS)
		nodep = node_split(s, idx & -MASK_BITS);
	assert(idx >= nodep->idx && idx <= nodep->idx + MASK_BITS - 1);
	assert(nodep->mask & (1 << (idx - nodep->idx)));
	nodep->mask &= ~(1 << (idx - nodep->idx));
	assert(s->num_set > 0 || sparsebit_all_set(s));
	s->num_set--;
	node_reduce(s, nodep);
}
static void dump_nodes(FILE *stream, struct node *nodep,
	unsigned int indent)
{
	char *node_type;
	if (!nodep->parent)
		node_type = "root";
	else if (nodep == nodep->parent->left)
		node_type = "left";
	else {
		assert(nodep == nodep->parent->right);
		node_type = "right";
	}
	fprintf(stream, "%*s---- %s nodep: %p\n", indent, "", node_type, nodep);
	fprintf(stream, "%*s  parent: %p left: %p right: %p\n", indent, "",
		nodep->parent, nodep->left, nodep->right);
	fprintf(stream, "%*s  idx: 0x%lx mask: 0x%x num_after: 0x%lx\n",
		indent, "", nodep->idx, nodep->mask, nodep->num_after);
	if (nodep->left)
		dump_nodes(stream, nodep->left, indent + 2);
	if (nodep->right)
		dump_nodes(stream, nodep->right, indent + 2);
}
static inline sparsebit_idx_t node_first_set(struct node *nodep, int start)
{
	mask_t leading = (mask_t)1 << start;
	int n1 = __builtin_ctz(nodep->mask & -leading);
	return nodep->idx + n1;
}
static inline sparsebit_idx_t node_first_clear(struct node *nodep, int start)
{
	mask_t leading = (mask_t)1 << start;
	int n1 = __builtin_ctz(~nodep->mask & -leading);
	return nodep->idx + n1;
}
static void sparsebit_dump_internal(FILE *stream, struct sparsebit *s,
	unsigned int indent)
{
	fprintf(stream, "%*sroot: %p\n", indent, "", s->root);
	fprintf(stream, "%*snum_set: 0x%lx\n", indent, "", s->num_set);
	if (s->root)
		dump_nodes(stream, s->root, indent);
}
struct sparsebit *sparsebit_alloc(void)
{
	struct sparsebit *s;
	s = calloc(1, sizeof(*s));
	if (!s) {
		perror("calloc");
		abort();
	}
	return s;
}
void sparsebit_free(struct sparsebit **sbitp)
{
	struct sparsebit *s = *sbitp;
	if (!s)
		return;
	sparsebit_clear_all(s);
	free(s);
	*sbitp = NULL;
}
void sparsebit_copy(struct sparsebit *d, struct sparsebit *s)
{
	sparsebit_clear_all(d);
	if (s->root) {
		d->root = node_copy_subtree(s->root);
		d->num_set = s->num_set;
	}
}
bool sparsebit_is_set_num(struct sparsebit *s,
	sparsebit_idx_t idx, sparsebit_num_t num)
{
	sparsebit_idx_t next_cleared;
	assert(num > 0);
	assert(idx + num - 1 >= idx);
	if (!sparsebit_is_set(s, idx))
		return false;
	next_cleared = sparsebit_next_clear(s, idx);
	return next_cleared == 0 || next_cleared - idx >= num;
}
bool sparsebit_is_clear(struct sparsebit *s,
	sparsebit_idx_t idx)
{
	return !sparsebit_is_set(s, idx);
}
bool sparsebit_is_clear_num(struct sparsebit *s,
	sparsebit_idx_t idx, sparsebit_num_t num)
{
	sparsebit_idx_t next_set;
	assert(num > 0);
	assert(idx + num - 1 >= idx);
	if (!sparsebit_is_clear(s, idx))
		return false;
	next_set = sparsebit_next_set(s, idx);
	return next_set == 0 || next_set - idx >= num;
}
sparsebit_num_t sparsebit_num_set(struct sparsebit *s)
{
	return s->num_set;
}
bool sparsebit_any_set(struct sparsebit *s)
{
	if (!s->root)
		return false;
	assert(s->root->mask != 0);
	assert(s->num_set > 0 ||
	       (s->root->num_after == ((sparsebit_num_t) 0) - MASK_BITS &&
		s->root->mask == ~(mask_t) 0));
	return true;
}
bool sparsebit_all_clear(struct sparsebit *s)
{
	return !sparsebit_any_set(s);
}
bool sparsebit_any_clear(struct sparsebit *s)
{
	return !sparsebit_all_set(s);
}
sparsebit_idx_t sparsebit_first_set(struct sparsebit *s)
{
	struct node *nodep;
	assert(sparsebit_any_set(s));
	nodep = node_first(s);
	return node_first_set(nodep, 0);
}
sparsebit_idx_t sparsebit_first_clear(struct sparsebit *s)
{
	struct node *nodep1, *nodep2;
	assert(sparsebit_any_clear(s));
	nodep1 = node_first(s);
	if (!nodep1 || nodep1->idx > 0)
		return 0;
	if (nodep1->mask != ~(mask_t) 0)
		return node_first_clear(nodep1, 0);
	nodep2 = node_next(s, nodep1);
	if (!nodep2) {
		assert(nodep1->mask == ~(mask_t) 0);
		assert(nodep1->idx + MASK_BITS + nodep1->num_after != (sparsebit_idx_t) 0);
		return nodep1->idx + MASK_BITS + nodep1->num_after;
	}
	if (nodep1->idx + MASK_BITS + nodep1->num_after != nodep2->idx)
		return nodep1->idx + MASK_BITS + nodep1->num_after;
	return node_first_clear(nodep2, 0);
}
sparsebit_idx_t sparsebit_next_set(struct sparsebit *s,
	sparsebit_idx_t prev)
{
	sparsebit_idx_t lowest_possible = prev + 1;
	sparsebit_idx_t start;
	struct node *nodep;
	if (lowest_possible == 0)
		return 0;
	struct node *candidate = NULL;
	bool contains = false;
	for (nodep = s->root; nodep;) {
		if ((nodep->idx + MASK_BITS + nodep->num_after - 1)
			>= lowest_possible) {
			candidate = nodep;
			if (candidate->idx <= lowest_possible) {
				contains = true;
				break;
			}
			nodep = nodep->left;
		} else {
			nodep = nodep->right;
		}
	}
	if (!candidate)
		return 0;
	assert(candidate->mask != 0);
	if (!contains) {
		assert(candidate->idx > lowest_possible);
		return node_first_set(candidate, 0);
	}
	start = lowest_possible - candidate->idx;
	if (start < MASK_BITS && candidate->mask >= (1 << start))
		return node_first_set(candidate, start);
	if (candidate->num_after) {
		sparsebit_idx_t first_num_after_idx = candidate->idx + MASK_BITS;
		return lowest_possible < first_num_after_idx
			? first_num_after_idx : lowest_possible;
	}
	candidate = node_next(s, candidate);
	if (!candidate)
		return 0;
	return node_first_set(candidate, 0);
}
sparsebit_idx_t sparsebit_next_clear(struct sparsebit *s,
	sparsebit_idx_t prev)
{
	sparsebit_idx_t lowest_possible = prev + 1;
	sparsebit_idx_t idx;
	struct node *nodep1, *nodep2;
	if (lowest_possible == 0)
		return 0;
	nodep1 = node_find(s, lowest_possible);
	if (!nodep1)
		return lowest_possible;
	for (idx = lowest_possible - nodep1->idx; idx < MASK_BITS; idx++)
		if (!(nodep1->mask & (1 << idx)))
			return nodep1->idx + idx;
	nodep2 = node_next(s, nodep1);
	if (!nodep2)
		return nodep1->idx + MASK_BITS + nodep1->num_after;
	if (nodep1->idx + MASK_BITS + nodep1->num_after != nodep2->idx)
		return nodep1->idx + MASK_BITS + nodep1->num_after;
	return node_first_clear(nodep2, 0);
}
sparsebit_idx_t sparsebit_next_set_num(struct sparsebit *s,
	sparsebit_idx_t start, sparsebit_num_t num)
{
	sparsebit_idx_t idx;
	assert(num >= 1);
	for (idx = sparsebit_next_set(s, start);
		idx != 0 && idx + num - 1 >= idx;
		idx = sparsebit_next_set(s, idx)) {
		assert(sparsebit_is_set(s, idx));
		if (sparsebit_is_set_num(s, idx, num))
			return idx;
		idx = sparsebit_next_clear(s, idx);
		if (idx == 0)
			return 0;
	}
	return 0;
}
sparsebit_idx_t sparsebit_next_clear_num(struct sparsebit *s,
	sparsebit_idx_t start, sparsebit_num_t num)
{
	sparsebit_idx_t idx;
	assert(num >= 1);
	for (idx = sparsebit_next_clear(s, start);
		idx != 0 && idx + num - 1 >= idx;
		idx = sparsebit_next_clear(s, idx)) {
		assert(sparsebit_is_clear(s, idx));
		if (sparsebit_is_clear_num(s, idx, num))
			return idx;
		idx = sparsebit_next_set(s, idx);
		if (idx == 0)
			return 0;
	}
	return 0;
}
void sparsebit_set_num(struct sparsebit *s,
	sparsebit_idx_t start, sparsebit_num_t num)
{
	struct node *nodep, *next;
	unsigned int n1;
	sparsebit_idx_t idx;
	sparsebit_num_t n;
	sparsebit_idx_t middle_start, middle_end;
	assert(num > 0);
	assert(start + num - 1 >= start);
	for (idx = start, n = num; n > 0 && idx % MASK_BITS != 0; idx++, n--)
		bit_set(s, idx);
	middle_start = idx;
	middle_end = middle_start + (n & -MASK_BITS) - 1;
	if (n >= MASK_BITS) {
		nodep = node_split(s, middle_start);
		if (middle_end + 1 > middle_end)
			(void) node_split(s, middle_end + 1);
		for (next = node_next(s, nodep);
			next && (next->idx < middle_end);
			next = node_next(s, nodep)) {
			assert(next->idx + MASK_BITS + next->num_after - 1 <= middle_end);
			node_rm(s, next);
			next = NULL;
		}
		for (n1 = 0; n1 < MASK_BITS; n1++) {
			if (!(nodep->mask & (1 << n1))) {
				nodep->mask |= 1 << n1;
				s->num_set++;
			}
		}
		s->num_set -= nodep->num_after;
		nodep->num_after = middle_end - middle_start + 1 - MASK_BITS;
		s->num_set += nodep->num_after;
		node_reduce(s, nodep);
	}
	idx = middle_end + 1;
	n -= middle_end - middle_start + 1;
	assert(n < MASK_BITS);
	for (; n > 0; idx++, n--)
		bit_set(s, idx);
}
void sparsebit_clear_num(struct sparsebit *s,
	sparsebit_idx_t start, sparsebit_num_t num)
{
	struct node *nodep, *next;
	unsigned int n1;
	sparsebit_idx_t idx;
	sparsebit_num_t n;
	sparsebit_idx_t middle_start, middle_end;
	assert(num > 0);
	assert(start + num - 1 >= start);
	for (idx = start, n = num; n > 0 && idx % MASK_BITS != 0; idx++, n--)
		bit_clear(s, idx);
	middle_start = idx;
	middle_end = middle_start + (n & -MASK_BITS) - 1;
	if (n >= MASK_BITS) {
		nodep = node_split(s, middle_start);
		if (middle_end + 1 > middle_end)
			(void) node_split(s, middle_end + 1);
		for (next = node_next(s, nodep);
			next && (next->idx < middle_end);
			next = node_next(s, nodep)) {
			assert(next->idx + MASK_BITS + next->num_after - 1 <= middle_end);
			node_rm(s, next);
			next = NULL;
		}
		for (n1 = 0; n1 < MASK_BITS; n1++) {
			if (nodep->mask & (1 << n1)) {
				nodep->mask &= ~(1 << n1);
				s->num_set--;
			}
		}
		s->num_set -= nodep->num_after;
		nodep->num_after = 0;
		node_reduce(s, nodep);
		nodep = NULL;
	}
	idx = middle_end + 1;
	n -= middle_end - middle_start + 1;
	assert(n < MASK_BITS);
	for (; n > 0; idx++, n--)
		bit_clear(s, idx);
}
void sparsebit_set(struct sparsebit *s, sparsebit_idx_t idx)
{
	sparsebit_set_num(s, idx, 1);
}
void sparsebit_clear(struct sparsebit *s, sparsebit_idx_t idx)
{
	sparsebit_clear_num(s, idx, 1);
}
void sparsebit_set_all(struct sparsebit *s)
{
	sparsebit_set(s, 0);
	sparsebit_set_num(s, 1, ~(sparsebit_idx_t) 0);
	assert(sparsebit_all_set(s));
}
void sparsebit_clear_all(struct sparsebit *s)
{
	sparsebit_clear(s, 0);
	sparsebit_clear_num(s, 1, ~(sparsebit_idx_t) 0);
	assert(!sparsebit_any_set(s));
}
static size_t display_range(FILE *stream, sparsebit_idx_t low,
	sparsebit_idx_t high, bool prepend_comma_space)
{
	char *fmt_str;
	size_t sz;
	if (low == high)
		fmt_str = prepend_comma_space ? ", 0x%lx" : "0x%lx";
	else
		fmt_str = prepend_comma_space ? ", 0x%lx:0x%lx" : "0x%lx:0x%lx";
	if (!stream)
		sz = snprintf(NULL, 0, fmt_str, low, high);
	else
		sz = fprintf(stream, fmt_str, low, high);
	return sz;
}
void sparsebit_dump(FILE *stream, struct sparsebit *s,
	unsigned int indent)
{
	size_t current_line_len = 0;
	size_t sz;
	struct node *nodep;
	if (!sparsebit_any_set(s))
		return;
	fprintf(stream, "%*s", indent, "");
	for (nodep = node_first(s); nodep; nodep = node_next(s, nodep)) {
		unsigned int n1;
		sparsebit_idx_t low, high;
		for (n1 = 0; n1 < MASK_BITS; n1++) {
			if (nodep->mask & (1 << n1)) {
				low = high = nodep->idx + n1;
				for (; n1 < MASK_BITS; n1++) {
					if (nodep->mask & (1 << n1))
						high = nodep->idx + n1;
					else
						break;
				}
				if ((n1 == MASK_BITS) && nodep->num_after)
					high += nodep->num_after;
				sz = display_range(NULL, low, high,
					current_line_len != 0);
				if (current_line_len + sz > DUMP_LINE_MAX) {
					fputs("\n", stream);
					fprintf(stream, "%*s", indent, "");
					current_line_len = 0;
				}
				sz = display_range(stream, low, high,
					current_line_len != 0);
				current_line_len += sz;
			}
		}
		if (!(nodep->mask & (1 << (MASK_BITS - 1))) && nodep->num_after) {
			low = nodep->idx + MASK_BITS;
			high = nodep->idx + MASK_BITS + nodep->num_after - 1;
			sz = display_range(NULL, low, high,
				current_line_len != 0);
			if (current_line_len + sz > DUMP_LINE_MAX) {
				fputs("\n", stream);
				fprintf(stream, "%*s", indent, "");
				current_line_len = 0;
			}
			sz = display_range(stream, low, high,
				current_line_len != 0);
			current_line_len += sz;
		}
	}
	fputs("\n", stream);
}
void sparsebit_validate_internal(struct sparsebit *s)
{
	bool error_detected = false;
	struct node *nodep, *prev = NULL;
	sparsebit_num_t total_bits_set = 0;
	unsigned int n1;
	for (nodep = node_first(s); nodep;
		prev = nodep, nodep = node_next(s, nodep)) {
		for (n1 = 0; n1 < MASK_BITS; n1++)
			if (nodep->mask & (1 << n1))
				total_bits_set++;
		total_bits_set += nodep->num_after;
		if (nodep->mask == 0) {
			fprintf(stderr, "Node mask of zero, "
				"nodep: %p nodep->mask: 0x%x",
				nodep, nodep->mask);
			error_detected = true;
			break;
		}
		if (nodep->num_after
			> (~(sparsebit_num_t) 0) - MASK_BITS + 1) {
			fprintf(stderr, "num_after too large, "
				"nodep: %p nodep->num_after: 0x%lx",
				nodep, nodep->num_after);
			error_detected = true;
			break;
		}
		if (nodep->idx % MASK_BITS) {
			fprintf(stderr, "Node index not divisible by "
				"mask size,\n"
				"  nodep: %p nodep->idx: 0x%lx "
				"MASK_BITS: %lu\n",
				nodep, nodep->idx, MASK_BITS);
			error_detected = true;
			break;
		}
		if ((nodep->idx + MASK_BITS + nodep->num_after - 1) < nodep->idx) {
			fprintf(stderr, "Bits described by node wrap "
				"beyond highest supported index,\n"
				"  nodep: %p nodep->idx: 0x%lx\n"
				"  MASK_BITS: %lu nodep->num_after: 0x%lx",
				nodep, nodep->idx, MASK_BITS, nodep->num_after);
			error_detected = true;
			break;
		}
		if (nodep->left) {
			if (nodep->left->parent != nodep) {
				fprintf(stderr, "Left child parent pointer "
					"doesn't point to this node,\n"
					"  nodep: %p nodep->left: %p "
					"nodep->left->parent: %p",
					nodep, nodep->left,
					nodep->left->parent);
				error_detected = true;
				break;
			}
		}
		if (nodep->right) {
			if (nodep->right->parent != nodep) {
				fprintf(stderr, "Right child parent pointer "
					"doesn't point to this node,\n"
					"  nodep: %p nodep->right: %p "
					"nodep->right->parent: %p",
					nodep, nodep->right,
					nodep->right->parent);
				error_detected = true;
				break;
			}
		}
		if (!nodep->parent) {
			if (s->root != nodep) {
				fprintf(stderr, "Unexpected root node, "
					"s->root: %p nodep: %p",
					s->root, nodep);
				error_detected = true;
				break;
			}
		}
		if (prev) {
			if (prev->idx >= nodep->idx) {
				fprintf(stderr, "Previous node index "
					">= current node index,\n"
					"  prev: %p prev->idx: 0x%lx\n"
					"  nodep: %p nodep->idx: 0x%lx",
					prev, prev->idx, nodep, nodep->idx);
				error_detected = true;
				break;
			}
			if ((prev->idx + MASK_BITS + prev->num_after - 1)
				>= nodep->idx) {
				fprintf(stderr, "Previous node bit range "
					"overlap with current node bit range,\n"
					"  prev: %p prev->idx: 0x%lx "
					"prev->num_after: 0x%lx\n"
					"  nodep: %p nodep->idx: 0x%lx "
					"nodep->num_after: 0x%lx\n"
					"  MASK_BITS: %lu",
					prev, prev->idx, prev->num_after,
					nodep, nodep->idx, nodep->num_after,
					MASK_BITS);
				error_detected = true;
				break;
			}
			if (nodep->mask == ~(mask_t) 0 &&
			    prev->idx + MASK_BITS + prev->num_after == nodep->idx) {
				fprintf(stderr, "Current node has mask with "
					"all bits set and is adjacent to the "
					"previous node,\n"
					"  prev: %p prev->idx: 0x%lx "
					"prev->num_after: 0x%lx\n"
					"  nodep: %p nodep->idx: 0x%lx "
					"nodep->num_after: 0x%lx\n"
					"  MASK_BITS: %lu",
					prev, prev->idx, prev->num_after,
					nodep, nodep->idx, nodep->num_after,
					MASK_BITS);
				error_detected = true;
				break;
			}
		}
	}
	if (!error_detected) {
		if (s->num_set != total_bits_set) {
			fprintf(stderr, "Number of bits set mismatch,\n"
				"  s->num_set: 0x%lx total_bits_set: 0x%lx",
				s->num_set, total_bits_set);
			error_detected = true;
		}
	}
	if (error_detected) {
		fputs("  dump_internal:\n", stderr);
		sparsebit_dump_internal(stderr, s, 4);
		abort();
	}
}
#ifdef FUZZ
#include <stdlib.h>
struct range {
	sparsebit_idx_t first, last;
	bool set;
};
struct sparsebit *s;
struct range ranges[1000];
int num_ranges;
static bool get_value(sparsebit_idx_t idx)
{
	int i;
	for (i = num_ranges; --i >= 0; )
		if (ranges[i].first <= idx && idx <= ranges[i].last)
			return ranges[i].set;
	return false;
}
static void operate(int code, sparsebit_idx_t first, sparsebit_idx_t last)
{
	sparsebit_num_t num;
	sparsebit_idx_t next;
	if (first < last) {
		num = last - first + 1;
	} else {
		num = first - last + 1;
		first = last;
		last = first + num - 1;
	}
	switch (code) {
	case 0:
		sparsebit_set(s, first);
		assert(sparsebit_is_set(s, first));
		assert(!sparsebit_is_clear(s, first));
		assert(sparsebit_any_set(s));
		assert(!sparsebit_all_clear(s));
		if (get_value(first))
			return;
		if (num_ranges == 1000)
			exit(0);
		ranges[num_ranges++] = (struct range)
			{ .first = first, .last = first, .set = true };
		break;
	case 1:
		sparsebit_clear(s, first);
		assert(!sparsebit_is_set(s, first));
		assert(sparsebit_is_clear(s, first));
		assert(sparsebit_any_clear(s));
		assert(!sparsebit_all_set(s));
		if (!get_value(first))
			return;
		if (num_ranges == 1000)
			exit(0);
		ranges[num_ranges++] = (struct range)
			{ .first = first, .last = first, .set = false };
		break;
	case 2:
		assert(sparsebit_is_set(s, first) == get_value(first));
		assert(sparsebit_is_clear(s, first) == !get_value(first));
		break;
	case 3:
		if (sparsebit_any_set(s))
			assert(get_value(sparsebit_first_set(s)));
		if (sparsebit_any_clear(s))
			assert(!get_value(sparsebit_first_clear(s)));
		sparsebit_set_all(s);
		assert(!sparsebit_any_clear(s));
		assert(sparsebit_all_set(s));
		num_ranges = 0;
		ranges[num_ranges++] = (struct range)
			{ .first = 0, .last = ~(sparsebit_idx_t)0, .set = true };
		break;
	case 4:
		if (sparsebit_any_set(s))
			assert(get_value(sparsebit_first_set(s)));
		if (sparsebit_any_clear(s))
			assert(!get_value(sparsebit_first_clear(s)));
		sparsebit_clear_all(s);
		assert(!sparsebit_any_set(s));
		assert(sparsebit_all_clear(s));
		num_ranges = 0;
		break;
	case 5:
		next = sparsebit_next_set(s, first);
		assert(next == 0 || next > first);
		assert(next == 0 || get_value(next));
		break;
	case 6:
		next = sparsebit_next_clear(s, first);
		assert(next == 0 || next > first);
		assert(next == 0 || !get_value(next));
		break;
	case 7:
		next = sparsebit_next_clear(s, first);
		if (sparsebit_is_set_num(s, first, num)) {
			assert(next == 0 || next > last);
			if (first)
				next = sparsebit_next_set(s, first - 1);
			else if (sparsebit_any_set(s))
				next = sparsebit_first_set(s);
			else
				return;
			assert(next == first);
		} else {
			assert(sparsebit_is_clear(s, first) || next <= last);
		}
		break;
	case 8:
		next = sparsebit_next_set(s, first);
		if (sparsebit_is_clear_num(s, first, num)) {
			assert(next == 0 || next > last);
			if (first)
				next = sparsebit_next_clear(s, first - 1);
			else if (sparsebit_any_clear(s))
				next = sparsebit_first_clear(s);
			else
				return;
			assert(next == first);
		} else {
			assert(sparsebit_is_set(s, first) || next <= last);
		}
		break;
	case 9:
		sparsebit_set_num(s, first, num);
		assert(sparsebit_is_set_num(s, first, num));
		assert(!sparsebit_is_clear_num(s, first, num));
		assert(sparsebit_any_set(s));
		assert(!sparsebit_all_clear(s));
		if (num_ranges == 1000)
			exit(0);
		ranges[num_ranges++] = (struct range)
			{ .first = first, .last = last, .set = true };
		break;
	case 10:
		sparsebit_clear_num(s, first, num);
		assert(!sparsebit_is_set_num(s, first, num));
		assert(sparsebit_is_clear_num(s, first, num));
		assert(sparsebit_any_clear(s));
		assert(!sparsebit_all_set(s));
		if (num_ranges == 1000)
			exit(0);
		ranges[num_ranges++] = (struct range)
			{ .first = first, .last = last, .set = false };
		break;
	case 11:
		sparsebit_validate_internal(s);
		break;
	default:
		break;
	}
}
unsigned char get8(void)
{
	int ch;
	ch = getchar();
	if (ch == EOF)
		exit(0);
	return ch;
}
uint64_t get64(void)
{
	uint64_t x;
	x = get8();
	x = (x << 8) | get8();
	x = (x << 8) | get8();
	x = (x << 8) | get8();
	x = (x << 8) | get8();
	x = (x << 8) | get8();
	x = (x << 8) | get8();
	return (x << 8) | get8();
}
int main(void)
{
	s = sparsebit_alloc();
	for (;;) {
		uint8_t op = get8() & 0xf;
		uint64_t first = get64();
		uint64_t last = get64();
		operate(op, first, last);
	}
}
#endif
