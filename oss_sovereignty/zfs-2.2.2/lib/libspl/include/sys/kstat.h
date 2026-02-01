 
 

#ifndef	_SYS_KSTAT_H
#define	_SYS_KSTAT_H



 

#include <sys/types.h>
#include <sys/time.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef int	kid_t;		 

 

#define	KSTAT_IOC_BASE		('K' << 8)

#define	KSTAT_IOC_CHAIN_ID	KSTAT_IOC_BASE | 0x01
#define	KSTAT_IOC_READ		KSTAT_IOC_BASE | 0x02
#define	KSTAT_IOC_WRITE		KSTAT_IOC_BASE | 0x03

 

#define	KSTAT_STRLEN	255	 

 

typedef struct kstat {
	 
	hrtime_t	ks_crtime;	 
	struct kstat	*ks_next;	 
	kid_t		ks_kid;		 
	char		ks_module[KSTAT_STRLEN];  
	uchar_t		ks_resv;	 
	int		ks_instance;	 
	char		ks_name[KSTAT_STRLEN];  
	uchar_t		ks_type;	 
	char		ks_class[KSTAT_STRLEN];  
	uchar_t		ks_flags;	 
	void		*ks_data;	 
	uint_t		ks_ndata;	 
	size_t		ks_data_size;	 
	hrtime_t	ks_snaptime;	 
	 
	int		(*ks_update)(struct kstat *, int);  
	void		*ks_private;	 
	int		(*ks_snapshot)(struct kstat *, void *, int);
	void		*ks_lock;	 
} kstat_t;

 

#if	defined(_KERNEL)

#define	KSTAT_ENTER(k)	\
	{ kmutex_t *lp = (k)->ks_lock; if (lp) mutex_enter(lp); }

#define	KSTAT_EXIT(k)	\
	{ kmutex_t *lp = (k)->ks_lock; if (lp) mutex_exit(lp); }

#define	KSTAT_UPDATE(k, rw)		(*(k)->ks_update)((k), (rw))

#define	KSTAT_SNAPSHOT(k, buf, rw)	(*(k)->ks_snapshot)((k), (buf), (rw))

#endif	 

 

 

 

 

 

#define	KSTAT_TYPE_RAW		0	 
					 
#define	KSTAT_TYPE_NAMED	1	 
					 
#define	KSTAT_TYPE_INTR		2	 
					 
#define	KSTAT_TYPE_IO		3	 
					 
#define	KSTAT_TYPE_TIMER	4	 
					 

#define	KSTAT_NUM_TYPES		5

 

 

#define	KSTAT_FLAG_VIRTUAL		0x01
#define	KSTAT_FLAG_VAR_SIZE		0x02
#define	KSTAT_FLAG_WRITABLE		0x04
#define	KSTAT_FLAG_PERSISTENT		0x08
#define	KSTAT_FLAG_DORMANT		0x10
#define	KSTAT_FLAG_INVALID		0x20
#define	KSTAT_FLAG_LONGSTRINGS		0x40
#define	KSTAT_FLAG_NO_HEADERS		0x80

 

#define	KSTAT_READ	0
#define	KSTAT_WRITE	1

 

 

typedef struct kstat_named {
	char	name[KSTAT_STRLEN];	 
	uchar_t	data_type;		 
	union {
		char		c[16];	 
		int32_t		i32;
		uint32_t	ui32;
		struct {
			union {
				char 		*ptr;	 
#if defined(_KERNEL) && defined(_MULTI_DATAMODEL)
				caddr32_t	ptr32;
#endif
				char 		__pad[8];  
			} addr;
			uint32_t	len;	 
		} str;
 
#if defined(_INT64_TYPE)
		int64_t		i64;
		uint64_t	ui64;
#endif
		long		l;
		ulong_t		ul;

		 

		longlong_t	ll;
		u_longlong_t	ull;
		float		f;
		double		d;
	} value;			 
} kstat_named_t;

#define	KSTAT_DATA_CHAR		0
#define	KSTAT_DATA_INT32	1
#define	KSTAT_DATA_UINT32	2
#define	KSTAT_DATA_INT64	3
#define	KSTAT_DATA_UINT64	4

#if !defined(_LP64)
#define	KSTAT_DATA_LONG		KSTAT_DATA_INT32
#define	KSTAT_DATA_ULONG	KSTAT_DATA_UINT32
#else
#if !defined(_KERNEL)
#define	KSTAT_DATA_LONG		KSTAT_DATA_INT64
#define	KSTAT_DATA_ULONG	KSTAT_DATA_UINT64
#else
#define	KSTAT_DATA_LONG		7	 
#define	KSTAT_DATA_ULONG	8	 
#endif	 
#endif	 

 
#define	KSTAT_DATA_STRING	9

 

#define	KSTAT_DATA_LONGLONG	KSTAT_DATA_INT64
#define	KSTAT_DATA_ULONGLONG	KSTAT_DATA_UINT64
#define	KSTAT_DATA_FLOAT	5
#define	KSTAT_DATA_DOUBLE	6

#define	KSTAT_NAMED_PTR(kptr)	((kstat_named_t *)(kptr)->ks_data)

 
#define	KSTAT_NAMED_STR_PTR(knptr) ((knptr)->value.str.addr.ptr)

 
#define	KSTAT_NAMED_STR_BUFLEN(knptr) ((knptr)->value.str.len)

 

#define	KSTAT_INTR_HARD			0
#define	KSTAT_INTR_SOFT			1
#define	KSTAT_INTR_WATCHDOG		2
#define	KSTAT_INTR_SPURIOUS		3
#define	KSTAT_INTR_MULTSVC		4

#define	KSTAT_NUM_INTRS			5

typedef struct kstat_intr {
	uint_t	intrs[KSTAT_NUM_INTRS];	 
} kstat_intr_t;

#define	KSTAT_INTR_PTR(kptr)	((kstat_intr_t *)(kptr)->ks_data)

 

typedef struct kstat_io {

	 

	u_longlong_t	nread;		 
	u_longlong_t	nwritten;	 
	uint_t		reads;		 
	uint_t		writes;		 

	 

	hrtime_t wtime;		 
	hrtime_t wlentime;	 
	hrtime_t wlastupdate;	 
	hrtime_t rtime;		 
	hrtime_t rlentime;	 
	hrtime_t rlastupdate;	 

	uint_t	wcnt;		 
	uint_t	rcnt;		 

} kstat_io_t;

#define	KSTAT_IO_PTR(kptr)	((kstat_io_t *)(kptr)->ks_data)

 

typedef struct kstat_timer {
	char		name[KSTAT_STRLEN];	 
	uchar_t		resv;			 
	u_longlong_t	num_events;		 
	hrtime_t	elapsed_time;		 
	hrtime_t	min_time;		 
	hrtime_t	max_time;		 
	hrtime_t	start_time;		 
	hrtime_t	stop_time;		 
} kstat_timer_t;

#define	KSTAT_TIMER_PTR(kptr)	((kstat_timer_t *)(kptr)->ks_data)

#if	defined(_KERNEL)

#include <sys/t_lock.h>

extern kid_t	kstat_chain_id;		 
extern void	kstat_init(void);	 

 

extern kstat_t *kstat_create(const char *, int, const char *, const char *,
    uchar_t, uint_t, uchar_t);
extern kstat_t *kstat_create_zone(const char *, int, const char *,
    const char *, uchar_t, uint_t, uchar_t, zoneid_t);
extern void kstat_install(kstat_t *);
extern void kstat_delete(kstat_t *);
extern void kstat_named_setstr(kstat_named_t *knp, const char *src);
extern void kstat_set_string(char *, const char *);
extern void kstat_delete_byname(const char *, int, const char *);
extern void kstat_delete_byname_zone(const char *, int, const char *, zoneid_t);
extern void kstat_named_init(kstat_named_t *, const char *, uchar_t);
extern void kstat_timer_init(kstat_timer_t *, const char *);
extern void kstat_timer_start(kstat_timer_t *);
extern void kstat_timer_stop(kstat_timer_t *);

extern void kstat_zone_add(kstat_t *, zoneid_t);
extern void kstat_zone_remove(kstat_t *, zoneid_t);
extern int kstat_zone_find(kstat_t *, zoneid_t);

extern kstat_t *kstat_hold_bykid(kid_t kid, zoneid_t);
extern kstat_t *kstat_hold_byname(const char *, int, const char *, zoneid_t);
extern void kstat_rele(kstat_t *);

#endif	 

#ifdef	__cplusplus
}
#endif

#endif	 
