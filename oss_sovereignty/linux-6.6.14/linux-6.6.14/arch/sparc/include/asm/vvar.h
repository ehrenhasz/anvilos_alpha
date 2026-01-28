#ifndef _ASM_SPARC_VVAR_DATA_H
#define _ASM_SPARC_VVAR_DATA_H
#include <asm/clocksource.h>
#include <asm/processor.h>
#include <asm/barrier.h>
#include <linux/time.h>
#include <linux/types.h>
struct vvar_data {
	unsigned int seq;
	int vclock_mode;
	struct {  
		u64	cycle_last;
		u64	mask;
		int	mult;
		int	shift;
	} clock;
	u64		wall_time_sec;
	u64		wall_time_snsec;
	u64		monotonic_time_snsec;
	u64		monotonic_time_sec;
	u64		monotonic_time_coarse_sec;
	u64		monotonic_time_coarse_nsec;
	u64		wall_time_coarse_sec;
	u64		wall_time_coarse_nsec;
	int		tz_minuteswest;
	int		tz_dsttime;
};
extern struct vvar_data *vvar_data;
extern int vdso_fix_stick;
static inline unsigned int vvar_read_begin(const struct vvar_data *s)
{
	unsigned int ret;
repeat:
	ret = READ_ONCE(s->seq);
	if (unlikely(ret & 1)) {
		cpu_relax();
		goto repeat;
	}
	smp_rmb();  
	return ret;
}
static inline int vvar_read_retry(const struct vvar_data *s,
					unsigned int start)
{
	smp_rmb();  
	return unlikely(s->seq != start);
}
static inline void vvar_write_begin(struct vvar_data *s)
{
	++s->seq;
	smp_wmb();  
}
static inline void vvar_write_end(struct vvar_data *s)
{
	smp_wmb();  
	++s->seq;
}
#endif  
