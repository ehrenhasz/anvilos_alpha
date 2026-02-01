 

#ifndef _SPL_VMSYSTM_H
#define	_SPL_VMSYSTM_H

#include <linux/mmzone.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/highmem.h>
#include <linux/vmalloc.h>
#include <sys/types.h>
#include <asm/uaccess.h>

#ifdef HAVE_TOTALRAM_PAGES_FUNC
#define	zfs_totalram_pages	totalram_pages()
#else
#define	zfs_totalram_pages	totalram_pages
#endif

#ifdef HAVE_TOTALHIGH_PAGES
#define	zfs_totalhigh_pages	totalhigh_pages()
#else
#define	zfs_totalhigh_pages	totalhigh_pages
#endif

#define	membar_consumer()		smp_rmb()
#define	membar_producer()		smp_wmb()
#define	membar_sync()			smp_mb()

#define	physmem				zfs_totalram_pages

#define	xcopyin(from, to, size)		copy_from_user(to, from, size)
#define	xcopyout(from, to, size)	copy_to_user(to, from, size)

static __inline__ int
copyin(const void *from, void *to, size_t len)
{
	 
	if (xcopyin(from, to, len))
		return (-1);

	return (0);
}

static __inline__ int
copyout(const void *from, void *to, size_t len)
{
	 
	if (xcopyout(from, to, len))
		return (-1);

	return (0);
}

static __inline__ int
copyinstr(const void *from, void *to, size_t len, size_t *done)
{
	size_t rc;

	if (len == 0)
		return (-ENAMETOOLONG);

	 

	memset(to, 0, len);
	rc = copyin(from, to, len - 1);
	if (done != NULL)
		*done = rc;

	return (0);
}

#endif  
