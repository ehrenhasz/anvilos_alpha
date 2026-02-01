 
 

 

#ifndef	_NVPAIR_IMPL_H
#define	_NVPAIR_IMPL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/nvpair.h>

 

 
typedef struct i_nvp i_nvp_t;

struct i_nvp {
	union {
		 
		uint64_t	_nvi_align;

		struct {
			 
			i_nvp_t	*_nvi_next;

			 
			i_nvp_t	*_nvi_prev;

			 
			i_nvp_t	*_nvi_hashtable_next;
		} _nvi;
	} _nvi_un;

	 
	nvpair_t nvi_nvp;
};
#define	nvi_next	_nvi_un._nvi._nvi_next
#define	nvi_prev	_nvi_un._nvi._nvi_prev
#define	nvi_hashtable_next	_nvi_un._nvi._nvi_hashtable_next

typedef struct {
	i_nvp_t		*nvp_list;	 
	i_nvp_t		*nvp_last;	 
	const i_nvp_t	*nvp_curr;	 
	nv_alloc_t	*nvp_nva;	 
	uint32_t	nvp_stat;	 

	i_nvp_t		**nvp_hashtable;  
	uint32_t	nvp_nbuckets;	 
	uint32_t	nvp_nentries;	 
} nvpriv_t;

#ifdef	__cplusplus
}
#endif

#endif	 
