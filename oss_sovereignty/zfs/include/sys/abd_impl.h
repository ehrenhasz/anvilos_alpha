#ifndef _ABD_IMPL_H
#define	_ABD_IMPL_H
#include <sys/abd.h>
#include <sys/wmsum.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum abd_stats_op {
	ABDSTAT_INCR,  
	ABDSTAT_DECR   
} abd_stats_op_t;
struct scatterlist;  
struct abd_iter {
	void		*iter_mapaddr;	 
	size_t		iter_mapsize;	 
	abd_t		*iter_abd;	 
	size_t		iter_pos;
	size_t		iter_offset;	 
	struct scatterlist *iter_sg;	 
};
extern abd_t *abd_zero_scatter;
abd_t *abd_gang_get_offset(abd_t *, size_t *);
abd_t *abd_alloc_struct(size_t);
void abd_free_struct(abd_t *);
abd_t *abd_alloc_struct_impl(size_t);
abd_t *abd_get_offset_scatter(abd_t *, abd_t *, size_t, size_t);
void abd_free_struct_impl(abd_t *);
void abd_alloc_chunks(abd_t *, size_t);
void abd_free_chunks(abd_t *);
void abd_update_scatter_stats(abd_t *, abd_stats_op_t);
void abd_update_linear_stats(abd_t *, abd_stats_op_t);
void abd_verify_scatter(abd_t *);
void abd_free_linear_page(abd_t *);
void abd_iter_init(struct abd_iter  *, abd_t *);
boolean_t abd_iter_at_end(struct abd_iter *);
void abd_iter_advance(struct abd_iter *, size_t);
void abd_iter_map(struct abd_iter *);
void abd_iter_unmap(struct abd_iter *);
#define	ABDSTAT_INCR(stat, val) \
	wmsum_add(&abd_sums.stat, (val))
#define	ABDSTAT_BUMP(stat)	ABDSTAT_INCR(stat, 1)
#define	ABDSTAT_BUMPDOWN(stat)	ABDSTAT_INCR(stat, -1)
#define	ABD_SCATTER(abd)	(abd->abd_u.abd_scatter)
#define	ABD_LINEAR_BUF(abd)	(abd->abd_u.abd_linear.abd_buf)
#define	ABD_GANG(abd)		(abd->abd_u.abd_gang)
#if defined(_KERNEL)
#if defined(__FreeBSD__)
#define	abd_enter_critical(flags)	critical_enter()
#define	abd_exit_critical(flags)	critical_exit()
#else
#define	abd_enter_critical(flags)	local_irq_save(flags)
#define	abd_exit_critical(flags)	local_irq_restore(flags)
#endif
#else  
#define	abd_enter_critical(flags)	((void)0)
#define	abd_exit_critical(flags)	((void)0)
#endif
#ifdef __cplusplus
}
#endif
#endif	 
