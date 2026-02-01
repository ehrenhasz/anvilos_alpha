 

 

#ifndef _ZFS_DCACHE_H
#define	_ZFS_DCACHE_H

#include <linux/dcache.h>

#define	dname(dentry)	((char *)((dentry)->d_name.name))
#define	dlen(dentry)	((int)((dentry)->d_name.len))

#ifndef HAVE_D_MAKE_ROOT
#define	d_make_root(inode)	d_alloc_root(inode)
#endif  

#ifdef HAVE_DENTRY_D_U_ALIASES
#define	d_alias			d_u.d_alias
#endif

 
#if defined __powerpc__ && defined HAVE_FLUSH_DCACHE_PAGE_GPL_ONLY
#include <linux/simd_powerpc.h>
#define	flush_dcache_page(page)	do {					\
		if (!cpu_has_feature(CPU_FTR_COHERENT_ICACHE) &&	\
		    test_bit(PG_dcache_clean, &(page)->flags))		\
			clear_bit(PG_dcache_clean, &(page)->flags);	\
	} while (0)
#endif

 
typedef const struct dentry_operations	dentry_operations_t;

 
static inline void
d_clear_d_op(struct dentry *dentry)
{
	dentry->d_op = NULL;
	dentry->d_flags &= ~(
	    DCACHE_OP_HASH | DCACHE_OP_COMPARE |
	    DCACHE_OP_REVALIDATE | DCACHE_OP_DELETE);
}

 
static inline void
zpl_d_drop_aliases(struct inode *inode)
{
	struct dentry *dentry;
	spin_lock(&inode->i_lock);
	hlist_for_each_entry(dentry, &inode->i_dentry, d_alias) {
		if (!IS_ROOT(dentry) && !d_mountpoint(dentry) &&
		    (dentry->d_inode == inode)) {
			d_drop(dentry);
		}
	}
	spin_unlock(&inode->i_lock);
}
#endif  
