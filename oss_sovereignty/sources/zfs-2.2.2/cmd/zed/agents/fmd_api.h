



#ifndef	_FMD_API_H
#define	_FMD_API_H

#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <libnvpair.h>
#include <stdarg.h>
#include <umem.h>

#ifdef	__cplusplus
extern "C" {
#endif



#define	FMD_API_VERSION		5

typedef struct fmd_hdl fmd_hdl_t;

typedef struct fmd_timer {
	timer_t		ft_tid;
	void		*ft_arg;
	fmd_hdl_t	*ft_hdl;
} fmd_timer_t;

#define	id_t	fmd_timer_t *


typedef struct fmd_event {
	hrtime_t	ev_hrt;		
} fmd_event_t;

typedef struct fmd_case {
	char		ci_uuid[48];	
	fmd_hdl_t	*ci_mod;	
	void		*ci_data;	
	ushort_t	ci_state;	
	ushort_t	ci_flags;	
	struct timeval	ci_tv;		
	void		*ci_bufptr;	
	size_t		ci_bufsiz;
} fmd_case_t;


#define	FMD_CASE_UNSOLVED	0	
#define	FMD_CASE_SOLVED		1	
#define	FMD_CASE_CLOSE_WAIT	2	
#define	FMD_CASE_CLOSED		3	
#define	FMD_CASE_REPAIRED	4	
#define	FMD_CASE_RESOLVED	5	

#define	FMD_CF_DIRTY		0x01	
#define	FMD_CF_SOLVED		0x02	
#define	FMD_CF_ISOLATED		0x04	
#define	FMD_CF_REPAIRED		0x08	
#define	FMD_CF_RESOLVED		0x10	


#define	FMD_TYPE_BOOL	0		
#define	FMD_TYPE_INT32	1		
#define	FMD_TYPE_UINT32	2		
#define	FMD_TYPE_INT64	3		
#define	FMD_TYPE_UINT64	4		
#define	FMD_TYPE_TIME	5		
#define	FMD_TYPE_SIZE	6		

typedef struct fmd_prop {
	const char *fmdp_name;		
	uint_t fmdp_type;		
	const char *fmdp_defv;		
} fmd_prop_t;

typedef struct fmd_stat {
	char fmds_name[32];		
	uint_t fmds_type;		
	char fmds_desc[64];		
	union {
		int bool;		
		int32_t i32;		
		uint32_t ui32;		
		int64_t i64;		
		uint64_t ui64;		
	} fmds_value;
} fmd_stat_t;

typedef struct fmd_hdl_ops {
	void (*fmdo_recv)(fmd_hdl_t *, fmd_event_t *, nvlist_t *, const char *);
	void (*fmdo_timeout)(fmd_hdl_t *, id_t, void *);
	void (*fmdo_close)(fmd_hdl_t *, fmd_case_t *);
	void (*fmdo_stats)(fmd_hdl_t *);
	void (*fmdo_gc)(fmd_hdl_t *);
} fmd_hdl_ops_t;

#define	FMD_SEND_SUCCESS	0	
#define	FMD_SEND_FAILED		1	
#define	FMD_SEND_RETRY		2	

typedef struct fmd_hdl_info {
	const char *fmdi_desc;		
	const char *fmdi_vers;		
	const fmd_hdl_ops_t *fmdi_ops;	
	const fmd_prop_t *fmdi_props;	
} fmd_hdl_info_t;

extern int fmd_hdl_register(fmd_hdl_t *, int, const fmd_hdl_info_t *);
extern void fmd_hdl_unregister(fmd_hdl_t *);

extern void fmd_hdl_setspecific(fmd_hdl_t *, void *);
extern void *fmd_hdl_getspecific(fmd_hdl_t *);

#define	FMD_SLEEP	UMEM_NOFAIL

extern void *fmd_hdl_alloc(fmd_hdl_t *, size_t, int);
extern void *fmd_hdl_zalloc(fmd_hdl_t *, size_t, int);
extern void fmd_hdl_free(fmd_hdl_t *, void *, size_t);

extern char *fmd_hdl_strdup(fmd_hdl_t *, const char *, int);
extern void fmd_hdl_strfree(fmd_hdl_t *, char *);

extern void fmd_hdl_vdebug(fmd_hdl_t *, const char *, va_list);
extern void fmd_hdl_debug(fmd_hdl_t *, const char *, ...);

extern int32_t fmd_prop_get_int32(fmd_hdl_t *, const char *);
extern int64_t fmd_prop_get_int64(fmd_hdl_t *, const char *);

#define	FMD_STAT_NOALLOC	0x0	
#define	FMD_STAT_ALLOC		0x1	

extern fmd_stat_t *fmd_stat_create(fmd_hdl_t *, uint_t, uint_t, fmd_stat_t *);
extern void fmd_stat_destroy(fmd_hdl_t *, uint_t, fmd_stat_t *);
extern void fmd_stat_setstr(fmd_hdl_t *, fmd_stat_t *, const char *);

extern fmd_case_t *fmd_case_open(fmd_hdl_t *, void *);
extern void fmd_case_reset(fmd_hdl_t *, fmd_case_t *);
extern void fmd_case_solve(fmd_hdl_t *, fmd_case_t *);
extern void fmd_case_close(fmd_hdl_t *, fmd_case_t *);

extern const char *fmd_case_uuid(fmd_hdl_t *, fmd_case_t *);
extern fmd_case_t *fmd_case_uulookup(fmd_hdl_t *, const char *);
extern void fmd_case_uuclose(fmd_hdl_t *, const char *);
extern int fmd_case_uuclosed(fmd_hdl_t *, const char *);
extern int fmd_case_uuisresolved(fmd_hdl_t *, const char *);
extern void fmd_case_uuresolved(fmd_hdl_t *, const char *);

extern boolean_t fmd_case_solved(fmd_hdl_t *, fmd_case_t *);

extern void fmd_case_add_ereport(fmd_hdl_t *, fmd_case_t *, fmd_event_t *);
extern void fmd_case_add_serd(fmd_hdl_t *, fmd_case_t *, const char *);
extern void fmd_case_add_suspect(fmd_hdl_t *, fmd_case_t *, nvlist_t *);

extern void fmd_case_setspecific(fmd_hdl_t *, fmd_case_t *, void *);
extern void *fmd_case_getspecific(fmd_hdl_t *, fmd_case_t *);

extern fmd_case_t *fmd_case_next(fmd_hdl_t *, fmd_case_t *);
extern fmd_case_t *fmd_case_prev(fmd_hdl_t *, fmd_case_t *);

extern void fmd_buf_create(fmd_hdl_t *, fmd_case_t *, const char *, size_t);
extern void fmd_buf_destroy(fmd_hdl_t *, fmd_case_t *, const char *);
extern void fmd_buf_read(fmd_hdl_t *, fmd_case_t *,
    const char *, void *, size_t);
extern void fmd_buf_write(fmd_hdl_t *, fmd_case_t *,
    const char *, const void *, size_t);
extern size_t fmd_buf_size(fmd_hdl_t *, fmd_case_t *, const char *);

extern void fmd_serd_create(fmd_hdl_t *, const char *, uint_t, hrtime_t);
extern void fmd_serd_destroy(fmd_hdl_t *, const char *);
extern int fmd_serd_exists(fmd_hdl_t *, const char *);
extern void fmd_serd_reset(fmd_hdl_t *, const char *);
extern int fmd_serd_record(fmd_hdl_t *, const char *, fmd_event_t *);
extern int fmd_serd_fired(fmd_hdl_t *, const char *);
extern int fmd_serd_empty(fmd_hdl_t *, const char *);

extern id_t fmd_timer_install(fmd_hdl_t *, void *, fmd_event_t *, hrtime_t);
extern void fmd_timer_remove(fmd_hdl_t *, id_t);

extern nvlist_t *fmd_nvl_create_fault(fmd_hdl_t *,
    const char *, uint8_t, nvlist_t *, nvlist_t *, nvlist_t *);

extern int fmd_nvl_class_match(fmd_hdl_t *, nvlist_t *, const char *);

#define	FMD_HAS_FAULT_FRU	0
#define	FMD_HAS_FAULT_ASRU	1
#define	FMD_HAS_FAULT_RESOURCE	2

extern void fmd_repair_fru(fmd_hdl_t *, const char *);
extern int fmd_repair_asru(fmd_hdl_t *, const char *);

extern nvlist_t *fmd_nvl_alloc(fmd_hdl_t *, int);
extern nvlist_t *fmd_nvl_dup(fmd_hdl_t *, nvlist_t *, int);



extern fmd_hdl_t *fmd_module_hdl(const char *);
extern boolean_t fmd_module_initialized(fmd_hdl_t *);
extern void fmd_module_recv(fmd_hdl_t *, nvlist_t *, const char *);


extern void _zfs_retire_init(fmd_hdl_t *);
extern void _zfs_retire_fini(fmd_hdl_t *);


extern void _zfs_diagnosis_init(fmd_hdl_t *);
extern void _zfs_diagnosis_fini(fmd_hdl_t *);

#ifdef	__cplusplus
}
#endif

#endif	
