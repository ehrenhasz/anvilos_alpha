#ifndef _RAID1_H
#define _RAID1_H
#define BARRIER_UNIT_SECTOR_BITS	17
#define BARRIER_UNIT_SECTOR_SIZE	(1<<17)
#define BARRIER_BUCKETS_NR_BITS		(PAGE_SHIFT - ilog2(sizeof(atomic_t)))
#define BARRIER_BUCKETS_NR		(1<<BARRIER_BUCKETS_NR_BITS)
struct raid1_info {
	struct md_rdev	*rdev;
	sector_t	head_position;
	sector_t	next_seq_sect;
	sector_t	seq_start;
};
struct pool_info {
	struct mddev *mddev;
	int	raid_disks;
};
struct r1conf {
	struct mddev		*mddev;
	struct raid1_info	*mirrors;	 
	int			raid_disks;
	spinlock_t		device_lock;
	struct list_head	retry_list;
	struct list_head	bio_end_io_list;
	struct bio_list		pending_bio_list;
	wait_queue_head_t	wait_barrier;
	spinlock_t		resync_lock;
	atomic_t		nr_sync_pending;
	atomic_t		*nr_pending;
	atomic_t		*nr_waiting;
	atomic_t		*nr_queued;
	atomic_t		*barrier;
	int			array_frozen;
	int			fullsync;
	int			recovery_disabled;
	struct pool_info	*poolinfo;
	mempool_t		r1bio_pool;
	mempool_t		r1buf_pool;
	struct bio_set		bio_split;
	struct page		*tmppage;
	struct md_thread __rcu	*thread;
	sector_t		cluster_sync_low;
	sector_t		cluster_sync_high;
};
struct r1bio {
	atomic_t		remaining;  
	atomic_t		behind_remaining;  
	sector_t		sector;
	int			sectors;
	unsigned long		state;
	struct mddev		*mddev;
	struct bio		*master_bio;
	int			read_disk;
	struct list_head	retry_list;
	struct bio		*behind_master_bio;
	struct bio		*bios[];
};
enum r1bio_state {
	R1BIO_Uptodate,
	R1BIO_IsSync,
	R1BIO_Degraded,
	R1BIO_BehindIO,
	R1BIO_ReadError,
	R1BIO_Returned,
	R1BIO_MadeGood,
	R1BIO_WriteError,
	R1BIO_FailFast,
};
static inline int sector_to_idx(sector_t sector)
{
	return hash_long(sector >> BARRIER_UNIT_SECTOR_BITS,
			 BARRIER_BUCKETS_NR_BITS);
}
#endif
