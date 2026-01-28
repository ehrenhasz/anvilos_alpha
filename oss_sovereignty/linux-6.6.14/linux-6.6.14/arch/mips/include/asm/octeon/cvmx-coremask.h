#ifndef __CVMX_COREMASK_H__
#define __CVMX_COREMASK_H__
#define CVMX_MIPS_MAX_CORES 1024
#define CVMX_COREMASK_ELTSZ 64
#define CVMX_COREMASK_BMPSZ (CVMX_MIPS_MAX_CORES / CVMX_COREMASK_ELTSZ)
struct cvmx_coremask {
	u64 coremask_bitmap[CVMX_COREMASK_BMPSZ];
};
static inline bool cvmx_coremask_is_core_set(const struct cvmx_coremask *pcm,
					    int core)
{
	int n, i;
	n = core % CVMX_COREMASK_ELTSZ;
	i = core / CVMX_COREMASK_ELTSZ;
	return (pcm->coremask_bitmap[i] & ((u64)1 << n)) != 0;
}
static inline void cvmx_coremask_copy(struct cvmx_coremask *dest,
				      const struct cvmx_coremask *src)
{
	memcpy(dest, src, sizeof(*dest));
}
static inline void cvmx_coremask_set64(struct cvmx_coremask *pcm,
				       uint64_t coremask_64)
{
	pcm->coremask_bitmap[0] = coremask_64;
}
static inline void cvmx_coremask_clear_core(struct cvmx_coremask *pcm, int core)
{
	int n, i;
	n = core % CVMX_COREMASK_ELTSZ;
	i = core / CVMX_COREMASK_ELTSZ;
	pcm->coremask_bitmap[i] &= ~(1ull << n);
}
#endif  
