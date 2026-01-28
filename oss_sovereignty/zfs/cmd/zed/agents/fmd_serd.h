#ifndef	_FMD_SERD_H
#define	_FMD_SERD_H
#ifdef	__cplusplus
extern "C" {
#endif
#include <sys/list.h>
#include <sys/time.h>
typedef struct fmd_serd_elem {
	list_node_t	se_list;	 
	hrtime_t	se_hrt;		 
} fmd_serd_elem_t;
typedef struct fmd_serd_eng {
	char		*sg_name;	 
	struct fmd_serd_eng *sg_next;	 
	list_t		sg_list;	 
	uint_t		sg_count;	 
	uint_t		sg_flags;	 
	uint_t		sg_n;		 
	hrtime_t	sg_t;		 
} fmd_serd_eng_t;
#define	FMD_SERD_FIRED	0x1		 
#define	FMD_SERD_DIRTY	0x2		 
typedef void fmd_serd_eng_f(fmd_serd_eng_t *, void *);
typedef struct fmd_serd_hash {
	fmd_serd_eng_t	**sh_hash;	 
	uint_t		sh_hashlen;	 
	uint_t		sh_count;	 
} fmd_serd_hash_t;
extern void fmd_serd_hash_create(fmd_serd_hash_t *);
extern void fmd_serd_hash_destroy(fmd_serd_hash_t *);
extern void fmd_serd_hash_apply(fmd_serd_hash_t *, fmd_serd_eng_f *, void *);
extern fmd_serd_eng_t *fmd_serd_eng_insert(fmd_serd_hash_t *,
    const char *, uint32_t, hrtime_t);
extern fmd_serd_eng_t *fmd_serd_eng_lookup(fmd_serd_hash_t *, const char *);
extern void fmd_serd_eng_delete(fmd_serd_hash_t *, const char *);
extern int fmd_serd_eng_record(fmd_serd_eng_t *, hrtime_t);
extern int fmd_serd_eng_fired(fmd_serd_eng_t *);
extern int fmd_serd_eng_empty(fmd_serd_eng_t *);
extern void fmd_serd_eng_reset(fmd_serd_eng_t *);
extern void fmd_serd_eng_gc(fmd_serd_eng_t *);
#ifdef	__cplusplus
}
#endif
#endif	 
