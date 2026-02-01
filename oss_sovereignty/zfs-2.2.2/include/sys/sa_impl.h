 
 

#ifndef	_SYS_SA_IMPL_H
#define	_SYS_SA_IMPL_H

#include <sys/dmu.h>
#include <sys/zfs_refcount.h>
#include <sys/list.h>

 
typedef struct sa_attr_table {
	sa_attr_type_t	sa_attr;
	uint8_t sa_registered;
	uint16_t sa_length;
	sa_bswap_type_t sa_byteswap;
	char *sa_name;
} sa_attr_table_t;

 

#define	ATTR_BSWAP(x)	BF32_GET(x, 16, 8)
#define	ATTR_LENGTH(x)	BF32_GET(x, 24, 16)
#define	ATTR_NUM(x)	BF32_GET(x, 0, 16)
#define	ATTR_ENCODE(x, attr, length, bswap) \
{ \
	BF64_SET(x, 24, 16, length); \
	BF64_SET(x, 16, 8, bswap); \
	BF64_SET(x, 0, 16, attr); \
}

#define	TOC_OFF(x)		BF32_GET(x, 0, 23)
#define	TOC_ATTR_PRESENT(x)	BF32_GET(x, 31, 1)
#define	TOC_LEN_IDX(x)		BF32_GET(x, 24, 4)
#define	TOC_ATTR_ENCODE(x, len_idx, offset) \
{ \
	BF32_SET(x, 31, 1, 1); \
	BF32_SET(x, 24, 7, len_idx); \
	BF32_SET(x, 0, 24, offset); \
}

#define	SA_LAYOUTS	"LAYOUTS"
#define	SA_REGISTRY	"REGISTRY"

 
typedef struct sa_lot {
	avl_node_t lot_num_node;
	avl_node_t lot_hash_node;
	uint64_t lot_num;
	uint64_t lot_hash;
	sa_attr_type_t *lot_attrs;	 
	uint32_t lot_var_sizes;	 
	uint32_t lot_attr_count;	 
	list_t 	lot_idx_tab;	 
	int	lot_instance;	 
} sa_lot_t;

 
typedef struct sa_idx_tab {
	list_node_t	sa_next;
	sa_lot_t	*sa_layout;
	uint16_t	*sa_variable_lengths;
	zfs_refcount_t	sa_refcount;
	uint32_t	*sa_idx_tab;	 
} sa_idx_tab_t;

 
struct sa_os {
	kmutex_t 	sa_lock;
	boolean_t	sa_need_attr_registration;
	boolean_t	sa_force_spill;
	uint64_t	sa_master_obj;
	uint64_t	sa_reg_attr_obj;
	uint64_t	sa_layout_attr_obj;
	int		sa_num_attrs;
	sa_attr_table_t *sa_attr_table;	  
	sa_update_cb_t	*sa_update_cb;
	avl_tree_t	sa_layout_num_tree;   
	avl_tree_t	sa_layout_hash_tree;  
	int		sa_user_table_sz;
	sa_attr_type_t	*sa_user_table;  
};

 

#define	SA_MAGIC	0x2F505A   
typedef struct sa_hdr_phys {
	uint32_t sa_magic;
	 
	uint16_t sa_layout_info;
	uint16_t sa_lengths[1];	 
	 
} sa_hdr_phys_t;

#define	SA_HDR_LAYOUT_NUM(hdr) BF32_GET(hdr->sa_layout_info, 0, 10)
#define	SA_HDR_SIZE(hdr) BF32_GET_SB(hdr->sa_layout_info, 10, 6, 3, 0)
#define	SA_HDR_LAYOUT_INFO_ENCODE(x, num, size) \
{ \
	BF32_SET_SB(x, 10, 6, 3, 0, size); \
	BF32_SET(x, 0, 10, num); \
}

typedef enum sa_buf_type {
	SA_BONUS = 1,
	SA_SPILL = 2
} sa_buf_type_t;

typedef enum sa_data_op {
	SA_LOOKUP,
	SA_UPDATE,
	SA_ADD,
	SA_REPLACE,
	SA_REMOVE
} sa_data_op_t;

 

struct sa_handle {
	dmu_buf_user_t	sa_dbu;
	kmutex_t	sa_lock;
	dmu_buf_t	*sa_bonus;
	dmu_buf_t	*sa_spill;
	objset_t	*sa_os;
	void		*sa_userp;
	sa_idx_tab_t	*sa_bonus_tab;	  
	sa_idx_tab_t	*sa_spill_tab;  
};

#define	SA_GET_DB(hdl, type)	\
	(dmu_buf_impl_t *)((type == SA_BONUS) ? hdl->sa_bonus : hdl->sa_spill)

#define	SA_GET_HDR(hdl, type) \
	((sa_hdr_phys_t *)((dmu_buf_impl_t *)(SA_GET_DB(hdl, \
	type))->db.db_data))

#define	SA_IDX_TAB_GET(hdl, type) \
	(type == SA_BONUS ? hdl->sa_bonus_tab : hdl->sa_spill_tab)

#define	IS_SA_BONUSTYPE(a)	\
	((a == DMU_OT_SA) ? B_TRUE : B_FALSE)

#define	SA_BONUSTYPE_FROM_DB(db) \
	(dmu_get_bonustype((dmu_buf_t *)db))

#define	SA_BLKPTR_SPACE	(DN_OLD_MAX_BONUSLEN - sizeof (blkptr_t))

#define	SA_LAYOUT_NUM(x, type) \
	((!IS_SA_BONUSTYPE(type) ? 0 : (((IS_SA_BONUSTYPE(type)) && \
	((SA_HDR_LAYOUT_NUM(x)) == 0)) ? 1 : SA_HDR_LAYOUT_NUM(x))))


#define	SA_REGISTERED_LEN(sa, attr) sa->sa_attr_table[attr].sa_length

#define	SA_ATTR_LEN(sa, idx, attr, hdr) ((SA_REGISTERED_LEN(sa, attr) == 0) ?\
	hdr->sa_lengths[TOC_LEN_IDX(idx->sa_idx_tab[attr])] : \
	SA_REGISTERED_LEN(sa, attr))

#define	SA_SET_HDR(hdr, num, size) \
	{ \
		hdr->sa_magic = SA_MAGIC; \
		SA_HDR_LAYOUT_INFO_ENCODE(hdr->sa_layout_info, num, size); \
	}

#define	SA_ATTR_INFO(sa, idx, hdr, attr, bulk, type, hdl) \
	{ \
		bulk.sa_size = SA_ATTR_LEN(sa, idx, attr, hdr); \
		bulk.sa_buftype = type; \
		bulk.sa_addr = \
		    (void *)((uintptr_t)TOC_OFF(idx->sa_idx_tab[attr]) + \
		    (uintptr_t)hdr); \
}

#define	SA_HDR_SIZE_MATCH_LAYOUT(hdr, tb) \
	(SA_HDR_SIZE(hdr) == (sizeof (sa_hdr_phys_t) + \
	(tb->lot_var_sizes > 1 ? P2ROUNDUP((tb->lot_var_sizes - 1) * \
	sizeof (uint16_t), 8) : 0)))

int sa_add_impl(sa_handle_t *, sa_attr_type_t,
    uint32_t, sa_data_locator_t, void *, dmu_tx_t *);

void sa_register_update_callback_locked(objset_t *, sa_update_cb_t *);
int sa_size_locked(sa_handle_t *, sa_attr_type_t, int *);

void sa_default_locator(void **, uint32_t *, uint32_t, boolean_t, void *);
int sa_attr_size(sa_os_t *, sa_idx_tab_t *, sa_attr_type_t,
    uint16_t *, sa_hdr_phys_t *);

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	__cplusplus
}
#endif

#endif	 
