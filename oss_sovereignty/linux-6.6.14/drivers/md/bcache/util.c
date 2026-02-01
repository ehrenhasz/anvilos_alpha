
 

#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <linux/sched/clock.h>

#include "util.h"

#define simple_strtoint(c, end, base)	simple_strtol(c, end, base)
#define simple_strtouint(c, end, base)	simple_strtoul(c, end, base)

#define STRTO_H(name, type)					\
int bch_ ## name ## _h(const char *cp, type *res)		\
{								\
	int u = 0;						\
	char *e;						\
	type i = simple_ ## name(cp, &e, 10);			\
								\
	switch (tolower(*e)) {					\
	default:						\
		return -EINVAL;					\
	case 'y':						\
	case 'z':						\
		u++;						\
		fallthrough;					\
	case 'e':						\
		u++;						\
		fallthrough;					\
	case 'p':						\
		u++;						\
		fallthrough;					\
	case 't':						\
		u++;						\
		fallthrough;					\
	case 'g':						\
		u++;						\
		fallthrough;					\
	case 'm':						\
		u++;						\
		fallthrough;					\
	case 'k':						\
		u++;						\
		if (e++ == cp)					\
			return -EINVAL;				\
		fallthrough;					\
	case '\n':						\
	case '\0':						\
		if (*e == '\n')					\
			e++;					\
	}							\
								\
	if (*e)							\
		return -EINVAL;					\
								\
	while (u--) {						\
		if ((type) ~0 > 0 &&				\
		    (type) ~0 / 1024 <= i)			\
			return -EINVAL;				\
		if ((i > 0 && ANYSINT_MAX(type) / 1024 < i) ||	\
		    (i < 0 && -ANYSINT_MAX(type) / 1024 > i))	\
			return -EINVAL;				\
		i *= 1024;					\
	}							\
								\
	*res = i;						\
	return 0;						\
}								\

STRTO_H(strtoint, int)
STRTO_H(strtouint, unsigned int)
STRTO_H(strtoll, long long)
STRTO_H(strtoull, unsigned long long)

 
ssize_t bch_hprint(char *buf, int64_t v)
{
	static const char units[] = "?kMGTPEZY";
	int u = 0, t;

	uint64_t q;

	if (v < 0)
		q = -v;
	else
		q = v;

	 
	do {
		u++;

		t = q & ~(~0 << 10);
		q >>= 10;
	} while (q >= 1000);

	if (v < 0)
		 
		return sprintf(buf, "-%llu.%i%c", q, t * 10 / 1024, units[u]);
	else
		return sprintf(buf, "%llu.%i%c", q, t * 10 / 1024, units[u]);
}

bool bch_is_zero(const char *p, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		if (p[i])
			return false;
	return true;
}

int bch_parse_uuid(const char *s, char *uuid)
{
	size_t i, j, x;

	memset(uuid, 0, 16);

	for (i = 0, j = 0;
	     i < strspn(s, "-0123456789:ABCDEFabcdef") && j < 32;
	     i++) {
		x = s[i] | 32;

		switch (x) {
		case '0'...'9':
			x -= '0';
			break;
		case 'a'...'f':
			x -= 'a' - 10;
			break;
		default:
			continue;
		}

		if (!(j & 1))
			x <<= 4;
		uuid[j++ >> 1] |= x;
	}
	return i;
}

void bch_time_stats_update(struct time_stats *stats, uint64_t start_time)
{
	uint64_t now, duration, last;

	spin_lock(&stats->lock);

	now		= local_clock();
	duration	= time_after64(now, start_time)
		? now - start_time : 0;
	last		= time_after64(now, stats->last)
		? now - stats->last : 0;

	stats->max_duration = max(stats->max_duration, duration);

	if (stats->last) {
		ewma_add(stats->average_duration, duration, 8, 8);

		if (stats->average_frequency)
			ewma_add(stats->average_frequency, last, 8, 8);
		else
			stats->average_frequency  = last << 8;
	} else {
		stats->average_duration  = duration << 8;
	}

	stats->last = now ?: 1;

	spin_unlock(&stats->lock);
}

 
uint64_t bch_next_delay(struct bch_ratelimit *d, uint64_t done)
{
	uint64_t now = local_clock();

	d->next += div_u64(done * NSEC_PER_SEC, atomic_long_read(&d->rate));

	 
	if (time_before64(now + NSEC_PER_SEC * 5LLU / 2LLU, d->next))
		d->next = now + NSEC_PER_SEC * 5LLU / 2LLU;

	if (time_after64(now - NSEC_PER_SEC * 2, d->next))
		d->next = now - NSEC_PER_SEC * 2;

	return time_after64(d->next, now)
		? div_u64(d->next - now, NSEC_PER_SEC / HZ)
		: 0;
}

 
void bch_bio_map(struct bio *bio, void *base)
{
	size_t size = bio->bi_iter.bi_size;
	struct bio_vec *bv = bio->bi_io_vec;

	BUG_ON(!bio->bi_iter.bi_size);
	BUG_ON(bio->bi_vcnt);

	bv->bv_offset = base ? offset_in_page(base) : 0;
	goto start;

	for (; size; bio->bi_vcnt++, bv++) {
		bv->bv_offset	= 0;
start:		bv->bv_len	= min_t(size_t, PAGE_SIZE - bv->bv_offset,
					size);
		if (base) {
			bv->bv_page = is_vmalloc_addr(base)
				? vmalloc_to_page(base)
				: virt_to_page(base);

			base += bv->bv_len;
		}

		size -= bv->bv_len;
	}
}

 
int bch_bio_alloc_pages(struct bio *bio, gfp_t gfp_mask)
{
	int i;
	struct bio_vec *bv;

	 
	for (i = 0, bv = bio->bi_io_vec; i < bio->bi_vcnt; bv++, i++) {
		bv->bv_page = alloc_page(gfp_mask);
		if (!bv->bv_page) {
			while (--bv >= bio->bi_io_vec)
				__free_page(bv->bv_page);
			return -ENOMEM;
		}
	}

	return 0;
}
