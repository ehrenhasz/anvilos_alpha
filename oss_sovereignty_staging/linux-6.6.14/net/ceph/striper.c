 

#include <linux/ceph/ceph_debug.h>

#include <linux/math64.h>
#include <linux/slab.h>

#include <linux/ceph/striper.h>
#include <linux/ceph/types.h>

 
void ceph_calc_file_object_mapping(struct ceph_file_layout *l,
				   u64 off, u64 len,
				   u64 *objno, u64 *objoff, u32 *xlen)
{
	u32 stripes_per_object = l->object_size / l->stripe_unit;
	u64 blockno;	 
	u32 blockoff;	 
	u64 stripeno;	 
	u32 stripepos;	 
	u64 objsetno;	 
	u32 objsetpos;	 

	blockno = div_u64_rem(off, l->stripe_unit, &blockoff);
	stripeno = div_u64_rem(blockno, l->stripe_count, &stripepos);
	objsetno = div_u64_rem(stripeno, stripes_per_object, &objsetpos);

	*objno = objsetno * l->stripe_count + stripepos;
	*objoff = objsetpos * l->stripe_unit + blockoff;
	*xlen = min_t(u64, len, l->stripe_unit - blockoff);
}
EXPORT_SYMBOL(ceph_calc_file_object_mapping);

 
static struct ceph_object_extent *
lookup_last(struct list_head *object_extents, u64 objno,
	    struct list_head **add_pos)
{
	struct list_head *pos;

	list_for_each_prev(pos, object_extents) {
		struct ceph_object_extent *ex =
		    list_entry(pos, typeof(*ex), oe_item);

		if (ex->oe_objno == objno)
			return ex;

		if (ex->oe_objno < objno)
			break;
	}

	*add_pos = pos;
	return NULL;
}

static struct ceph_object_extent *
lookup_containing(struct list_head *object_extents, u64 objno,
		  u64 objoff, u32 xlen)
{
	struct ceph_object_extent *ex;

	list_for_each_entry(ex, object_extents, oe_item) {
		if (ex->oe_objno == objno &&
		    ex->oe_off <= objoff &&
		    ex->oe_off + ex->oe_len >= objoff + xlen)  
			return ex;

		if (ex->oe_objno > objno)
			break;
	}

	return NULL;
}

 
int ceph_file_to_extents(struct ceph_file_layout *l, u64 off, u64 len,
			 struct list_head *object_extents,
			 struct ceph_object_extent *alloc_fn(void *arg),
			 void *alloc_arg,
			 ceph_object_extent_fn_t action_fn,
			 void *action_arg)
{
	struct ceph_object_extent *last_ex, *ex;

	while (len) {
		struct list_head *add_pos = NULL;
		u64 objno, objoff;
		u32 xlen;

		ceph_calc_file_object_mapping(l, off, len, &objno, &objoff,
					      &xlen);

		last_ex = lookup_last(object_extents, objno, &add_pos);
		if (!last_ex || last_ex->oe_off + last_ex->oe_len != objoff) {
			ex = alloc_fn(alloc_arg);
			if (!ex)
				return -ENOMEM;

			ex->oe_objno = objno;
			ex->oe_off = objoff;
			ex->oe_len = xlen;
			if (action_fn)
				action_fn(ex, xlen, action_arg);

			if (!last_ex)
				list_add(&ex->oe_item, add_pos);
			else
				list_add(&ex->oe_item, &last_ex->oe_item);
		} else {
			last_ex->oe_len += xlen;
			if (action_fn)
				action_fn(last_ex, xlen, action_arg);
		}

		off += xlen;
		len -= xlen;
	}

	for (last_ex = list_first_entry(object_extents, typeof(*ex), oe_item),
	     ex = list_next_entry(last_ex, oe_item);
	     &ex->oe_item != object_extents;
	     last_ex = ex, ex = list_next_entry(ex, oe_item)) {
		if (last_ex->oe_objno > ex->oe_objno ||
		    (last_ex->oe_objno == ex->oe_objno &&
		     last_ex->oe_off + last_ex->oe_len >= ex->oe_off)) {
			WARN(1, "%s: object_extents list not sorted!\n",
			     __func__);
			return -EINVAL;
		}
	}

	return 0;
}
EXPORT_SYMBOL(ceph_file_to_extents);

 
int ceph_iterate_extents(struct ceph_file_layout *l, u64 off, u64 len,
			 struct list_head *object_extents,
			 ceph_object_extent_fn_t action_fn,
			 void *action_arg)
{
	while (len) {
		struct ceph_object_extent *ex;
		u64 objno, objoff;
		u32 xlen;

		ceph_calc_file_object_mapping(l, off, len, &objno, &objoff,
					      &xlen);

		ex = lookup_containing(object_extents, objno, objoff, xlen);
		if (!ex) {
			WARN(1, "%s: objno %llu %llu~%u not found!\n",
			     __func__, objno, objoff, xlen);
			return -EINVAL;
		}

		action_fn(ex, xlen, action_arg);

		off += xlen;
		len -= xlen;
	}

	return 0;
}
EXPORT_SYMBOL(ceph_iterate_extents);

 
int ceph_extent_to_file(struct ceph_file_layout *l,
			u64 objno, u64 objoff, u64 objlen,
			struct ceph_file_extent **file_extents,
			u32 *num_file_extents)
{
	u32 stripes_per_object = l->object_size / l->stripe_unit;
	u64 blockno;	 
	u32 blockoff;	 
	u64 stripeno;	 
	u32 stripepos;	 
	u64 objsetno;	 
	u32 i = 0;

	if (!objlen) {
		*file_extents = NULL;
		*num_file_extents = 0;
		return 0;
	}

	*num_file_extents = DIV_ROUND_UP_ULL(objoff + objlen, l->stripe_unit) -
				     DIV_ROUND_DOWN_ULL(objoff, l->stripe_unit);
	*file_extents = kmalloc_array(*num_file_extents, sizeof(**file_extents),
				      GFP_NOIO);
	if (!*file_extents)
		return -ENOMEM;

	div_u64_rem(objoff, l->stripe_unit, &blockoff);
	while (objlen) {
		u64 off, len;

		objsetno = div_u64_rem(objno, l->stripe_count, &stripepos);
		stripeno = div_u64(objoff, l->stripe_unit) +
						objsetno * stripes_per_object;
		blockno = stripeno * l->stripe_count + stripepos;
		off = blockno * l->stripe_unit + blockoff;
		len = min_t(u64, objlen, l->stripe_unit - blockoff);

		(*file_extents)[i].fe_off = off;
		(*file_extents)[i].fe_len = len;

		blockoff = 0;
		objoff += len;
		objlen -= len;
		i++;
	}

	BUG_ON(i != *num_file_extents);
	return 0;
}
EXPORT_SYMBOL(ceph_extent_to_file);

u64 ceph_get_num_objects(struct ceph_file_layout *l, u64 size)
{
	u64 period = (u64)l->stripe_count * l->object_size;
	u64 num_periods = DIV64_U64_ROUND_UP(size, period);
	u64 remainder_bytes;
	u64 remainder_objs = 0;

	div64_u64_rem(size, period, &remainder_bytes);
	if (remainder_bytes > 0 &&
	    remainder_bytes < (u64)l->stripe_count * l->stripe_unit)
		remainder_objs = l->stripe_count -
			    DIV_ROUND_UP_ULL(remainder_bytes, l->stripe_unit);

	return num_periods * l->stripe_count - remainder_objs;
}
EXPORT_SYMBOL(ceph_get_num_objects);
