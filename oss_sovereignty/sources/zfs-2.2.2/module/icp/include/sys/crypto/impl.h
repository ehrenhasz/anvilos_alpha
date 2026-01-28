


#ifndef	_SYS_CRYPTO_IMPL_H
#define	_SYS_CRYPTO_IMPL_H



#include <sys/zfs_context.h>
#include <sys/crypto/common.h>
#include <sys/crypto/api.h>
#include <sys/crypto/spi.h>
#include <sys/avl.h>

#ifdef	__cplusplus
extern "C" {
#endif





#define	KCF_OPS_CLASSSIZE	4
#define	KCF_MAXMECHTAB		32


typedef enum {
	KCF_PROV_ALLOCATED = 1,
	
	KCF_PROV_READY,
	
	KCF_PROV_FAILED,
	
	KCF_PROV_DISABLED,
	KCF_PROV_REMOVED,
	KCF_PROV_FREED
} kcf_prov_state_t;

#define	KCF_IS_PROV_USABLE(pd) ((pd)->pd_state == KCF_PROV_READY)
#define	KCF_IS_PROV_REMOVED(pd)	((pd)->pd_state >= KCF_PROV_REMOVED)


typedef struct kcf_provider_desc {
	uint_t				pd_refcnt;
	uint_t				pd_irefcnt;
	kmutex_t			pd_lock;
	kcf_prov_state_t		pd_state;
	const crypto_ops_t			*pd_ops_vector;
	ushort_t			pd_mech_indx[KCF_OPS_CLASSSIZE]\
					    [KCF_MAXMECHTAB];
	const crypto_mech_info_t		*pd_mechanisms;
	uint_t				pd_mech_list_count;
	kcondvar_t			pd_remove_cv;
	const char				*pd_description;
	crypto_kcf_provider_handle_t	pd_kcf_prov_handle;
	crypto_provider_id_t		pd_prov_id;
} kcf_provider_desc_t;


#define	KCF_PROV_REFHOLD(desc) {				\
	int newval = atomic_add_32_nv(&(desc)->pd_refcnt, 1);	\
	ASSERT(newval != 0);					\
}

#define	KCF_PROV_IREFHOLD(desc) {				\
	int newval = atomic_add_32_nv(&(desc)->pd_irefcnt, 1);	\
	ASSERT(newval != 0);					\
}

#define	KCF_PROV_IREFRELE(desc) {				\
	membar_producer();					\
	int newval = atomic_add_32_nv(&(desc)->pd_irefcnt, -1);	\
	ASSERT(newval != -1);					\
	if (newval == 0) {					\
		cv_broadcast(&(desc)->pd_remove_cv);		\
	}							\
}

#define	KCF_PROV_REFHELD(desc)	((desc)->pd_refcnt >= 1)

#define	KCF_PROV_REFRELE(desc) {				\
	membar_producer();					\
	int newval = atomic_add_32_nv(&(desc)->pd_refcnt, -1);	\
	ASSERT(newval != -1);					\
	if (newval == 0) {					\
		kcf_provider_zero_refcnt((desc));		\
	}							\
}




typedef struct kcf_prov_mech_desc {
	struct kcf_mech_entry		*pm_me;		
	struct kcf_prov_mech_desc	*pm_next;	
	crypto_mech_info_t		pm_mech_info;	
	kcf_provider_desc_t		*pm_prov_desc;	
} kcf_prov_mech_desc_t;


typedef	struct kcf_mech_entry {
	crypto_mech_name_t	me_name;	
	crypto_mech_type_t	me_mechid;	
	kcf_prov_mech_desc_t	*me_sw_prov;    
	avl_node_t	me_node;
} kcf_mech_entry_t;


#define	KCF_POLICY_REFHOLD(desc) {				\
	int newval = atomic_add_32_nv(&(desc)->pd_refcnt, 1);	\
	ASSERT(newval != 0);					\
}


#define	KCF_POLICY_REFRELE(desc) {				\
	membar_producer();					\
	int newval = atomic_add_32_nv(&(desc)->pd_refcnt, -1);	\
	ASSERT(newval != -1);					\
	if (newval == 0)					\
		kcf_policy_free_desc(desc);			\
}



#define	KCF_MAXDIGEST		16	
#define	KCF_MAXCIPHER		32	
#define	KCF_MAXMAC		40	

_Static_assert(KCF_MAXCIPHER == KCF_MAXMECHTAB,
	"KCF_MAXCIPHER != KCF_MAXMECHTAB");	

typedef	enum {
	KCF_DIGEST_CLASS = 1,
	KCF_CIPHER_CLASS,
	KCF_MAC_CLASS,
} kcf_ops_class_t;

#define	KCF_FIRST_OPSCLASS	KCF_DIGEST_CLASS
#define	KCF_LAST_OPSCLASS	KCF_MAC_CLASS
_Static_assert(
    KCF_OPS_CLASSSIZE == (KCF_LAST_OPSCLASS - KCF_FIRST_OPSCLASS + 2),
	"KCF_OPS_CLASSSIZE doesn't match kcf_ops_class_t!");



typedef	struct kcf_mech_entry_tab {
	int			met_size;	
	kcf_mech_entry_t	*met_tab;	
} kcf_mech_entry_tab_t;

extern const kcf_mech_entry_tab_t kcf_mech_tabs_tab[];

#define	KCF_MECHID(class, index)				\
	(((crypto_mech_type_t)(class) << 32) | (crypto_mech_type_t)(index))

#define	KCF_MECH2CLASS(mech_type) ((kcf_ops_class_t)((mech_type) >> 32))

#define	KCF_MECH2INDEX(mech_type) ((int)((mech_type) & 0xFFFFFFFF))

#define	KCF_TO_PROV_MECH_INDX(pd, mech_type) 			\
	((pd)->pd_mech_indx[KCF_MECH2CLASS(mech_type)] 		\
	[KCF_MECH2INDEX(mech_type)])

#define	KCF_TO_PROV_MECHINFO(pd, mech_type)			\
	((pd)->pd_mechanisms[KCF_TO_PROV_MECH_INDX(pd, mech_type)])

#define	KCF_TO_PROV_MECHNUM(pd, mech_type)			\
	(KCF_TO_PROV_MECHINFO(pd, mech_type).cm_mech_number)


#define	KCF_SUCCESS		0x0	
#define	KCF_INVALID_MECH_NUMBER	0x1	
#define	KCF_INVALID_MECH_NAME	0x2	
#define	KCF_INVALID_MECH_CLASS	0x3	
#define	KCF_MECH_TAB_FULL	0x4	
#define	KCF_INVALID_INDX	((ushort_t)-1)



#define	KCF_PROV_DIGEST_OPS(pd)		((pd)->pd_ops_vector->co_digest_ops)
#define	KCF_PROV_CIPHER_OPS(pd)		((pd)->pd_ops_vector->co_cipher_ops)
#define	KCF_PROV_MAC_OPS(pd)		((pd)->pd_ops_vector->co_mac_ops)
#define	KCF_PROV_CTX_OPS(pd)		((pd)->pd_ops_vector->co_ctx_ops)



#define	KCF_PROV_DIGEST_INIT(pd, ctx, mech) ( \
	(KCF_PROV_DIGEST_OPS(pd) && KCF_PROV_DIGEST_OPS(pd)->digest_init) ? \
	KCF_PROV_DIGEST_OPS(pd)->digest_init(ctx, mech) : \
	CRYPTO_NOT_SUPPORTED)



#define	KCF_PROV_ENCRYPT_INIT(pd, ctx, mech, key, template) ( \
	(KCF_PROV_CIPHER_OPS(pd) && KCF_PROV_CIPHER_OPS(pd)->encrypt_init) ? \
	KCF_PROV_CIPHER_OPS(pd)->encrypt_init(ctx, mech, key, template) : \
	CRYPTO_NOT_SUPPORTED)

#define	KCF_PROV_ENCRYPT_ATOMIC(pd, mech, key, plaintext, ciphertext, \
	    template) ( \
	(KCF_PROV_CIPHER_OPS(pd) && KCF_PROV_CIPHER_OPS(pd)->encrypt_atomic) ? \
	KCF_PROV_CIPHER_OPS(pd)->encrypt_atomic( \
	    mech, key, plaintext, ciphertext, template) : \
	CRYPTO_NOT_SUPPORTED)

#define	KCF_PROV_DECRYPT_ATOMIC(pd, mech, key, ciphertext, plaintext, \
	    template) ( \
	(KCF_PROV_CIPHER_OPS(pd) && KCF_PROV_CIPHER_OPS(pd)->decrypt_atomic) ? \
	KCF_PROV_CIPHER_OPS(pd)->decrypt_atomic( \
	    mech, key, ciphertext, plaintext, template) : \
	CRYPTO_NOT_SUPPORTED)



#define	KCF_PROV_MAC_INIT(pd, ctx, mech, key, template) ( \
	(KCF_PROV_MAC_OPS(pd) && KCF_PROV_MAC_OPS(pd)->mac_init) ? \
	KCF_PROV_MAC_OPS(pd)->mac_init(ctx, mech, key, template) \
	: CRYPTO_NOT_SUPPORTED)


#define	KCF_PROV_MAC_UPDATE(pd, ctx, data) ( \
	(KCF_PROV_MAC_OPS(pd) && KCF_PROV_MAC_OPS(pd)->mac_update) ? \
	KCF_PROV_MAC_OPS(pd)->mac_update(ctx, data) : \
	CRYPTO_NOT_SUPPORTED)

#define	KCF_PROV_MAC_FINAL(pd, ctx, mac) ( \
	(KCF_PROV_MAC_OPS(pd) && KCF_PROV_MAC_OPS(pd)->mac_final) ? \
	KCF_PROV_MAC_OPS(pd)->mac_final(ctx, mac) : \
	CRYPTO_NOT_SUPPORTED)

#define	KCF_PROV_MAC_ATOMIC(pd, mech, key, data, mac, template) ( \
	(KCF_PROV_MAC_OPS(pd) && KCF_PROV_MAC_OPS(pd)->mac_atomic) ? \
	KCF_PROV_MAC_OPS(pd)->mac_atomic( \
	    mech, key, data, mac, template) : \
	CRYPTO_NOT_SUPPORTED)



#define	KCF_PROV_CREATE_CTX_TEMPLATE(pd, mech, key, template, size) ( \
	(KCF_PROV_CTX_OPS(pd) && KCF_PROV_CTX_OPS(pd)->create_ctx_template) ? \
	KCF_PROV_CTX_OPS(pd)->create_ctx_template( \
	    mech, key, template, size) : \
	CRYPTO_NOT_SUPPORTED)

#define	KCF_PROV_FREE_CONTEXT(pd, ctx) ( \
	(KCF_PROV_CTX_OPS(pd) && KCF_PROV_CTX_OPS(pd)->free_context) ? \
	KCF_PROV_CTX_OPS(pd)->free_context(ctx) : CRYPTO_NOT_SUPPORTED)



extern void kcf_destroy_mech_tabs(void);
extern void kcf_init_mech_tabs(void);
extern int kcf_add_mech_provider(short, kcf_provider_desc_t *,
    kcf_prov_mech_desc_t **);
extern void kcf_remove_mech_provider(const char *, kcf_provider_desc_t *);
extern int kcf_get_mech_entry(crypto_mech_type_t, kcf_mech_entry_t **);
extern kcf_provider_desc_t *kcf_alloc_provider_desc(void);
extern void kcf_provider_zero_refcnt(kcf_provider_desc_t *);
extern void kcf_free_provider_desc(kcf_provider_desc_t *);
extern void undo_register_provider(kcf_provider_desc_t *, boolean_t);
extern int crypto_put_output_data(uchar_t *, crypto_data_t *, int);
extern int crypto_update_iov(void *, crypto_data_t *, crypto_data_t *,
    int (*cipher)(void *, caddr_t, size_t, crypto_data_t *));
extern int crypto_update_uio(void *, crypto_data_t *, crypto_data_t *,
    int (*cipher)(void *, caddr_t, size_t, crypto_data_t *));


extern void kcf_prov_tab_destroy(void);
extern void kcf_prov_tab_init(void);
extern int kcf_prov_tab_add_provider(kcf_provider_desc_t *);
extern int kcf_prov_tab_rem_provider(crypto_provider_id_t);
extern kcf_provider_desc_t *kcf_prov_tab_lookup(crypto_provider_id_t);
extern int kcf_get_sw_prov(crypto_mech_type_t, kcf_provider_desc_t **,
    kcf_mech_entry_t **, boolean_t);


#ifdef	__cplusplus
}
#endif

#endif	
