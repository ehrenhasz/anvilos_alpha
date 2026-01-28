
#ifndef _BCACHE_JOURNAL_H
#define _BCACHE_JOURNAL_H




struct journal_replay {
	struct list_head	list;
	atomic_t		*pin;
	struct jset		j;
};


struct journal_write {
	struct jset		*data;
#define JSET_BITS		3

	struct cache_set	*c;
	struct closure_waitlist	wait;
	bool			dirty;
	bool			need_write;
};


struct journal {
	spinlock_t		lock;
	spinlock_t		flush_write_lock;
	bool			btree_flushing;
	bool			do_reserve;
	
	struct closure_waitlist	wait;
	struct closure		io;
	int			io_in_flight;
	struct delayed_work	work;

	
	unsigned int		blocks_free;
	uint64_t		seq;
	DECLARE_FIFO(atomic_t, pin);

	BKEY_PADDED(key);

	struct journal_write	w[2], *cur;
};


struct journal_device {
	
	uint64_t		seq[SB_JOURNAL_BUCKETS];

	
	unsigned int		cur_idx;

	
	unsigned int		last_idx;

	
	unsigned int		discard_idx;

#define DISCARD_READY		0
#define DISCARD_IN_FLIGHT	1
#define DISCARD_DONE		2
	
	atomic_t		discard_in_flight;

	struct work_struct	discard_work;
	struct bio		discard_bio;
	struct bio_vec		discard_bv;

	
	struct bio		bio;
	struct bio_vec		bv[8];
};

#define BTREE_FLUSH_NR	8

#define journal_pin_cmp(c, l, r)				\
	(fifo_idx(&(c)->journal.pin, (l)) > fifo_idx(&(c)->journal.pin, (r)))

#define JOURNAL_PIN	20000

#define journal_full(j)						\
	(!(j)->blocks_free || fifo_free(&(j)->pin) <= 1)

struct closure;
struct cache_set;
struct btree_op;
struct keylist;

atomic_t *bch_journal(struct cache_set *c,
		      struct keylist *keys,
		      struct closure *parent);
void bch_journal_next(struct journal *j);
void bch_journal_mark(struct cache_set *c, struct list_head *list);
void bch_journal_meta(struct cache_set *c, struct closure *cl);
int bch_journal_read(struct cache_set *c, struct list_head *list);
int bch_journal_replay(struct cache_set *c, struct list_head *list);

void bch_journal_free(struct cache_set *c);
int bch_journal_alloc(struct cache_set *c);
void bch_journal_space_reserve(struct journal *j);

#endif 
