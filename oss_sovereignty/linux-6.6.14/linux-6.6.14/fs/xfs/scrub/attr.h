#ifndef __XFS_SCRUB_ATTR_H__
#define __XFS_SCRUB_ATTR_H__
struct xchk_xattr_buf {
	unsigned long		*usedmap;
	unsigned long		*freemap;
	void			*value;
	size_t			value_sz;
};
#endif	 
