
 
#include <linux/fs.h>
#include <linux/iomap.h>
#include "trace.h"

 
static inline int iomap_iter_advance(struct iomap_iter *iter)
{
	bool stale = iter->iomap.flags & IOMAP_F_STALE;

	 
	if (iter->iomap.length) {
		if (iter->processed < 0)
			return iter->processed;
		if (!iter->processed && !stale)
			return 0;
		if (WARN_ON_ONCE(iter->processed > iomap_length(iter)))
			return -EIO;
		iter->pos += iter->processed;
		iter->len -= iter->processed;
		if (!iter->len)
			return 0;
	}

	 
	iter->processed = 0;
	memset(&iter->iomap, 0, sizeof(iter->iomap));
	memset(&iter->srcmap, 0, sizeof(iter->srcmap));
	return 1;
}

static inline void iomap_iter_done(struct iomap_iter *iter)
{
	WARN_ON_ONCE(iter->iomap.offset > iter->pos);
	WARN_ON_ONCE(iter->iomap.length == 0);
	WARN_ON_ONCE(iter->iomap.offset + iter->iomap.length <= iter->pos);
	WARN_ON_ONCE(iter->iomap.flags & IOMAP_F_STALE);

	trace_iomap_iter_dstmap(iter->inode, &iter->iomap);
	if (iter->srcmap.type != IOMAP_HOLE)
		trace_iomap_iter_srcmap(iter->inode, &iter->srcmap);
}

 
int iomap_iter(struct iomap_iter *iter, const struct iomap_ops *ops)
{
	int ret;

	if (iter->iomap.length && ops->iomap_end) {
		ret = ops->iomap_end(iter->inode, iter->pos, iomap_length(iter),
				iter->processed > 0 ? iter->processed : 0,
				iter->flags, &iter->iomap);
		if (ret < 0 && !iter->processed)
			return ret;
	}

	trace_iomap_iter(iter, ops, _RET_IP_);
	ret = iomap_iter_advance(iter);
	if (ret <= 0)
		return ret;

	ret = ops->iomap_begin(iter->inode, iter->pos, iter->len, iter->flags,
			       &iter->iomap, &iter->srcmap);
	if (ret < 0)
		return ret;
	iomap_iter_done(iter);
	return 1;
}
