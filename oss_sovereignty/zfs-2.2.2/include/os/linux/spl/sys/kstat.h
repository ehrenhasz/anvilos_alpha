 

#ifndef _SPL_KSTAT_H
#define	_SPL_KSTAT_H

#include <linux/module.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/proc.h>

#define	KSTAT_STRLEN		255
#define	KSTAT_RAW_MAX		(128*1024)

 

#define	KSTAT_TYPE_RAW		0  
#define	KSTAT_TYPE_NAMED	1  
#define	KSTAT_TYPE_INTR		2  
#define	KSTAT_TYPE_IO		3  
#define	KSTAT_TYPE_TIMER	4  
#define	KSTAT_NUM_TYPES		5

#define	KSTAT_DATA_CHAR		0
#define	KSTAT_DATA_INT32	1
#define	KSTAT_DATA_UINT32	2
#define	KSTAT_DATA_INT64	3
#define	KSTAT_DATA_UINT64	4
#define	KSTAT_DATA_LONG		5
#define	KSTAT_DATA_ULONG	6
#define	KSTAT_DATA_STRING	7
#define	KSTAT_NUM_DATAS		8

#define	KSTAT_INTR_HARD		0
#define	KSTAT_INTR_SOFT		1
#define	KSTAT_INTR_WATCHDOG	2
#define	KSTAT_INTR_SPURIOUS	3
#define	KSTAT_INTR_MULTSVC	4
#define	KSTAT_NUM_INTRS		5

#define	KSTAT_FLAG_VIRTUAL	0x01
#define	KSTAT_FLAG_VAR_SIZE	0x02
#define	KSTAT_FLAG_WRITABLE	0x04
#define	KSTAT_FLAG_PERSISTENT	0x08
#define	KSTAT_FLAG_DORMANT	0x10
#define	KSTAT_FLAG_INVALID	0x20
#define	KSTAT_FLAG_LONGSTRINGS	0x40
#define	KSTAT_FLAG_NO_HEADERS	0x80

#define	KS_MAGIC		0x9d9d9d9d

 
#define	KSTAT_READ		0
#define	KSTAT_WRITE		1

struct kstat_s;
typedef struct kstat_s kstat_t;

typedef int kid_t;				 
typedef int kstat_update_t(struct kstat_s *, int);  

typedef struct kstat_module {
	char ksm_name[KSTAT_STRLEN];		 
	struct list_head ksm_module_list;	 
	struct list_head ksm_kstat_list;	 
	struct proc_dir_entry *ksm_proc;	 
} kstat_module_t;

typedef struct kstat_raw_ops {
	int (*headers)(char *buf, size_t size);
	int (*data)(char *buf, size_t size, void *data);
	void *(*addr)(kstat_t *ksp, loff_t index);
} kstat_raw_ops_t;

typedef struct kstat_proc_entry {
	char	kpe_name[KSTAT_STRLEN];		 
	char	kpe_module[KSTAT_STRLEN];	 
	kstat_module_t		*kpe_owner;	 
	struct list_head	kpe_list;	 
	struct proc_dir_entry	*kpe_proc;	 
} kstat_proc_entry_t;

struct kstat_s {
	int		ks_magic;		 
	kid_t		ks_kid;			 
	hrtime_t	ks_crtime;		 
	hrtime_t	ks_snaptime;		 
	int		ks_instance;		 
	char		ks_class[KSTAT_STRLEN];  
	uchar_t		ks_type;		 
	uchar_t		ks_flags;		 
	void		*ks_data;		 
	uint_t		ks_ndata;		 
	size_t		ks_data_size;		 
	kstat_update_t	*ks_update;		 
	void		*ks_private;		 
	kmutex_t	ks_private_lock;	 
	kmutex_t	*ks_lock;		 
	kstat_raw_ops_t	ks_raw_ops;		 
	char		*ks_raw_buf;		 
	size_t		ks_raw_bufsize;		 
	kstat_proc_entry_t	ks_proc;	 
};

typedef struct kstat_named_s {
	char	name[KSTAT_STRLEN];	 
	uchar_t	data_type;		 
	union {
		char c[16];	 
		int32_t	i32;	 
		uint32_t ui32;	 
		int64_t i64;	 
		uint64_t ui64;	 
		long l;		 
		ulong_t ul;	 
		struct {
			union {
				char *ptr;	 
				char __pad[8];	 
			} addr;
			uint32_t len;		 
		} string;
	} value;
} kstat_named_t;

#define	KSTAT_NAMED_STR_PTR(knptr) ((knptr)->value.string.addr.ptr)
#define	KSTAT_NAMED_STR_BUFLEN(knptr) ((knptr)->value.string.len)

#ifdef HAVE_PROC_OPS_STRUCT
typedef struct proc_ops kstat_proc_op_t;
#else
typedef struct file_operations kstat_proc_op_t;
#endif

typedef struct kstat_intr {
	uint_t intrs[KSTAT_NUM_INTRS];
} kstat_intr_t;

typedef struct kstat_io {
	u_longlong_t	nread;		 
	u_longlong_t	nwritten;	 
	uint_t		reads;		 
	uint_t		writes;		 
	hrtime_t	wtime;		 
	hrtime_t	wlentime;	 
	hrtime_t	wlastupdate;	 
	hrtime_t	rtime;		 
	hrtime_t	rlentime;	 
	hrtime_t	rlastupdate;	 
	uint_t		wcnt;		 
	uint_t		rcnt;		 
} kstat_io_t;

typedef struct kstat_timer {
	char		name[KSTAT_STRLEN];  
	u_longlong_t	num_events;	  
	hrtime_t	elapsed_time;	  
	hrtime_t	min_time;	  
	hrtime_t	max_time;	  
	hrtime_t	start_time;	  
	hrtime_t	stop_time;	  
} kstat_timer_t;

int spl_kstat_init(void);
void spl_kstat_fini(void);

extern void __kstat_set_raw_ops(kstat_t *ksp,
    int (*headers)(char *buf, size_t size),
    int (*data)(char *buf, size_t size, void *data),
    void* (*addr)(kstat_t *ksp, loff_t index));

extern kstat_t *__kstat_create(const char *ks_module, int ks_instance,
    const char *ks_name, const char *ks_class, uchar_t ks_type,
    uint_t ks_ndata, uchar_t ks_flags);

extern void kstat_proc_entry_init(kstat_proc_entry_t *kpep,
    const char *module, const char *name);
extern void kstat_proc_entry_delete(kstat_proc_entry_t *kpep);
extern void kstat_proc_entry_install(kstat_proc_entry_t *kpep, mode_t mode,
    const kstat_proc_op_t *file_ops, void *data);

extern void __kstat_install(kstat_t *ksp);
extern void __kstat_delete(kstat_t *ksp);

#define	kstat_set_raw_ops(k, h, d, a) \
    __kstat_set_raw_ops(k, h, d, a)
#define	kstat_create(m, i, n, c, t, s, f) \
    __kstat_create(m, i, n, c, t, s, f)

#define	kstat_install(k)		__kstat_install(k)
#define	kstat_delete(k)			__kstat_delete(k)

#endif   
