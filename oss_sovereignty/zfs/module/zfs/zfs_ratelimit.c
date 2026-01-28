#include <sys/zfs_ratelimit.h>
void
zfs_ratelimit_init(zfs_ratelimit_t *rl, unsigned int *burst,
    unsigned int interval)
{
	rl->count = 0;
	rl->start = 0;
	rl->interval = interval;
	rl->burst = burst;
	mutex_init(&rl->lock, NULL, MUTEX_DEFAULT, NULL);
}
void
zfs_ratelimit_fini(zfs_ratelimit_t *rl)
{
	mutex_destroy(&rl->lock);
}
int
zfs_ratelimit(zfs_ratelimit_t *rl)
{
	hrtime_t now;
	hrtime_t elapsed;
	int error = 1;
	mutex_enter(&rl->lock);
	now = gethrtime();
	elapsed = now - rl->start;
	rl->count++;
	if (NSEC2SEC(elapsed) >= rl->interval) {
		rl->start = now;
		rl->count = 0;
	} else {
		if (rl->count >= *rl->burst) {
			error = 0;  
		}
	}
	mutex_exit(&rl->lock);
	return (error);
}
