



#ifndef	_SYS_DSL_CRYPT_H
#define	_SYS_DSL_CRYPT_H

#include <sys/dmu_tx.h>
#include <sys/dmu.h>
#include <sys/zio_crypt.h>
#include <sys/spa.h>
#include <sys/dsl_dataset.h>


#define	DSL_CRYPTO_KEY_CRYPTO_SUITE	"DSL_CRYPTO_SUITE"
#define	DSL_CRYPTO_KEY_GUID		"DSL_CRYPTO_GUID"
#define	DSL_CRYPTO_KEY_IV		"DSL_CRYPTO_IV"
#define	DSL_CRYPTO_KEY_MAC		"DSL_CRYPTO_MAC"
#define	DSL_CRYPTO_KEY_MASTER_KEY	"DSL_CRYPTO_MASTER_KEY_1"
#define	DSL_CRYPTO_KEY_HMAC_KEY		"DSL_CRYPTO_HMAC_KEY_1"
#define	DSL_CRYPTO_KEY_ROOT_DDOBJ	"DSL_CRYPTO_ROOT_DDOBJ"
#define	DSL_CRYPTO_KEY_REFCOUNT		"DSL_CRYPTO_REFCOUNT"
#define	DSL_CRYPTO_KEY_VERSION		"DSL_CRYPTO_VERSION"


typedef struct dsl_wrapping_key {
	
	avl_node_t wk_avl_link;

	
	zfs_keyformat_t wk_keyformat;

	
	uint64_t wk_salt;

	
	uint64_t wk_iters;

	
	crypto_key_t wk_key;

	
	zfs_refcount_t wk_refcnt;

	
	uint64_t wk_ddobj;
} dsl_wrapping_key_t;


typedef enum dcp_cmd {
	
	DCP_CMD_NONE = 0,	
	DCP_CMD_RAW_RECV,	

	
	DCP_CMD_NEW_KEY,	
	DCP_CMD_INHERIT,	
	DCP_CMD_FORCE_NEW_KEY,	
	DCP_CMD_FORCE_INHERIT,	

	DCP_CMD_MAX
} dcp_cmd_t;


typedef struct dsl_crypto_params {
	
	dcp_cmd_t cp_cmd;

	
	enum zio_encrypt cp_crypt;

	
	char *cp_keylocation;

	
	dsl_wrapping_key_t *cp_wkey;
} dsl_crypto_params_t;


typedef struct dsl_crypto_key {
	
	avl_node_t dck_avl_link;

	
	zfs_refcount_t dck_holds;

	
	zio_crypt_key_t dck_key;

	
	dsl_wrapping_key_t *dck_wkey;

	
	uint64_t dck_obj;
} dsl_crypto_key_t;


typedef struct dsl_key_mapping {
	
	avl_node_t km_avl_link;

	
	zfs_refcount_t km_refcnt;

	
	uint64_t km_dsobj;

	
	dsl_crypto_key_t *km_key;
} dsl_key_mapping_t;


typedef struct spa_keystore {
	
	krwlock_t sk_dk_lock;

	
	avl_tree_t sk_dsl_keys;

	
	krwlock_t sk_km_lock;

	
	avl_tree_t sk_key_mappings;

	
	krwlock_t sk_wkeys_lock;

	
	avl_tree_t sk_wkeys;
} spa_keystore_t;

int dsl_crypto_params_create_nvlist(dcp_cmd_t cmd, nvlist_t *props,
    nvlist_t *crypto_args, dsl_crypto_params_t **dcp_out);
void dsl_crypto_params_free(dsl_crypto_params_t *dcp, boolean_t unload);
void dsl_dataset_crypt_stats(struct dsl_dataset *ds, nvlist_t *nv);
int dsl_crypto_can_set_keylocation(const char *dsname, const char *keylocation);
boolean_t dsl_dir_incompatible_encryption_version(dsl_dir_t *dd);

void spa_keystore_init(spa_keystore_t *sk);
void spa_keystore_fini(spa_keystore_t *sk);

void spa_keystore_dsl_key_rele(spa_t *spa, dsl_crypto_key_t *dck,
    const void *tag);
int spa_keystore_load_wkey_impl(spa_t *spa, dsl_wrapping_key_t *wkey);
int spa_keystore_load_wkey(const char *dsname, dsl_crypto_params_t *dcp,
    boolean_t noop);
int spa_keystore_unload_wkey_impl(spa_t *spa, uint64_t ddobj);
int spa_keystore_unload_wkey(const char *dsname);

int spa_keystore_create_mapping(spa_t *spa, struct dsl_dataset *ds,
    const void *tag, dsl_key_mapping_t **km_out);
int spa_keystore_remove_mapping(spa_t *spa, uint64_t dsobj, const void *tag);
void key_mapping_add_ref(dsl_key_mapping_t *km, const void *tag);
void key_mapping_rele(spa_t *spa, dsl_key_mapping_t *km, const void *tag);
int spa_keystore_lookup_key(spa_t *spa, uint64_t dsobj, const void *tag,
    dsl_crypto_key_t **dck_out);

int dsl_crypto_populate_key_nvlist(struct objset *os,
    uint64_t from_ivset_guid, nvlist_t **nvl_out);
int dsl_crypto_recv_raw_key_check(struct dsl_dataset *ds,
    nvlist_t *nvl, dmu_tx_t *tx);
void dsl_crypto_recv_raw_key_sync(struct dsl_dataset *ds,
    nvlist_t *nvl, dmu_tx_t *tx);
int dsl_crypto_recv_raw(const char *poolname, uint64_t dsobj, uint64_t fromobj,
    dmu_objset_type_t ostype, nvlist_t *nvl, boolean_t do_key);

int spa_keystore_change_key(const char *dsname, dsl_crypto_params_t *dcp);
int dsl_dir_rename_crypt_check(dsl_dir_t *dd, dsl_dir_t *newparent);
int dsl_dataset_promote_crypt_check(dsl_dir_t *target, dsl_dir_t *origin);
void dsl_dataset_promote_crypt_sync(dsl_dir_t *target, dsl_dir_t *origin,
    dmu_tx_t *tx);
int dmu_objset_create_crypt_check(dsl_dir_t *parentdd,
    dsl_crypto_params_t *dcp, boolean_t *will_encrypt);
void dsl_dataset_create_crypt_sync(uint64_t dsobj, dsl_dir_t *dd,
    struct dsl_dataset *origin, dsl_crypto_params_t *dcp, dmu_tx_t *tx);
uint64_t dsl_crypto_key_create_sync(uint64_t crypt, dsl_wrapping_key_t *wkey,
    dmu_tx_t *tx);
uint64_t dsl_crypto_key_clone_sync(dsl_dir_t *origindd, dmu_tx_t *tx);
void dsl_crypto_key_destroy_sync(uint64_t dckobj, dmu_tx_t *tx);

int spa_crypt_get_salt(spa_t *spa, uint64_t dsobj, uint8_t *salt);
int spa_do_crypt_mac_abd(boolean_t generate, spa_t *spa, uint64_t dsobj,
    abd_t *abd, uint_t datalen, uint8_t *mac);
int spa_do_crypt_objset_mac_abd(boolean_t generate, spa_t *spa, uint64_t dsobj,
    abd_t *abd, uint_t datalen, boolean_t byteswap);
int spa_do_crypt_abd(boolean_t encrypt, spa_t *spa, const zbookmark_phys_t *zb,
    dmu_object_type_t ot, boolean_t dedup, boolean_t bswap, uint8_t *salt,
    uint8_t *iv, uint8_t *mac, uint_t datalen, abd_t *pabd, abd_t *cabd,
    boolean_t *no_crypt);
zfs_keystatus_t dsl_dataset_get_keystatus(dsl_dir_t *dd);

#endif
