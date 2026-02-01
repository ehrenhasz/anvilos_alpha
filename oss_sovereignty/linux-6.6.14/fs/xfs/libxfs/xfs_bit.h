 
 
#ifndef __XFS_BIT_H__
#define	__XFS_BIT_H__

 

 
static inline uint64_t xfs_mask64hi(int n)
{
	return (uint64_t)-1 << (64 - (n));
}
static inline uint32_t xfs_mask32lo(int n)
{
	return ((uint32_t)1 << (n)) - 1;
}
static inline uint64_t xfs_mask64lo(int n)
{
	return ((uint64_t)1 << (n)) - 1;
}

 
static inline int xfs_highbit32(uint32_t v)
{
	return fls(v) - 1;
}

 
static inline int xfs_highbit64(uint64_t v)
{
	return fls64(v) - 1;
}

 
static inline int xfs_lowbit32(uint32_t v)
{
	return ffs(v) - 1;
}

 
static inline int xfs_lowbit64(uint64_t v)
{
	uint32_t	w = (uint32_t)v;
	int		n = 0;

	if (w) {	 
		n = ffs(w);
	} else {	 
		w = (uint32_t)(v >> 32);
		if (w) {
			n = ffs(w);
			if (n)
				n += 32;
		}
	}
	return n - 1;
}

 
extern int xfs_bitmap_empty(uint *map, uint size);

 
extern int xfs_contig_bits(uint *map, uint size, uint start_bit);

 
extern int xfs_next_bit(uint *map, uint size, uint start_bit);

#endif	 
