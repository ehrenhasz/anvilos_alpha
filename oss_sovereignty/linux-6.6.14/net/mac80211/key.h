 
 

#ifndef IEEE80211_KEY_H
#define IEEE80211_KEY_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/crypto.h>
#include <linux/rcupdate.h>
#include <crypto/arc4.h>
#include <net/mac80211.h>

#define NUM_DEFAULT_KEYS 4
#define NUM_DEFAULT_MGMT_KEYS 2
#define NUM_DEFAULT_BEACON_KEYS 2
#define INVALID_PTK_KEYIDX 2  

struct ieee80211_local;
struct ieee80211_sub_if_data;
struct ieee80211_link_data;
struct sta_info;

 
enum ieee80211_internal_key_flags {
	KEY_FLAG_UPLOADED_TO_HARDWARE	= BIT(0),
	KEY_FLAG_TAINTED		= BIT(1),
};

enum ieee80211_internal_tkip_state {
	TKIP_STATE_NOT_INIT,
	TKIP_STATE_PHASE1_DONE,
	TKIP_STATE_PHASE1_HW_UPLOADED,
};

struct tkip_ctx {
	u16 p1k[5];	 
	u32 p1k_iv32;	 
	enum ieee80211_internal_tkip_state state;
};

struct tkip_ctx_rx {
	struct tkip_ctx ctx;
	u32 iv32;	 
	u16 iv16;	 
};

struct ieee80211_key {
	struct ieee80211_local *local;
	struct ieee80211_sub_if_data *sdata;
	struct sta_info *sta;

	 
	struct list_head list;

	 
	unsigned int flags;

	union {
		struct {
			 
			spinlock_t txlock;

			 
			struct tkip_ctx tx;

			 
			struct tkip_ctx_rx rx[IEEE80211_NUM_TIDS];

			 
			u32 mic_failures;
		} tkip;
		struct {
			 
			u8 rx_pn[IEEE80211_NUM_TIDS + 1][IEEE80211_CCMP_PN_LEN];
			struct crypto_aead *tfm;
			u32 replays;  
		} ccmp;
		struct {
			u8 rx_pn[IEEE80211_CMAC_PN_LEN];
			struct crypto_shash *tfm;
			u32 replays;  
			u32 icverrors;  
		} aes_cmac;
		struct {
			u8 rx_pn[IEEE80211_GMAC_PN_LEN];
			struct crypto_aead *tfm;
			u32 replays;  
			u32 icverrors;  
		} aes_gmac;
		struct {
			 
			u8 rx_pn[IEEE80211_NUM_TIDS + 1][IEEE80211_GCMP_PN_LEN];
			struct crypto_aead *tfm;
			u32 replays;  
		} gcmp;
		struct {
			 
			u8 rx_pn[IEEE80211_NUM_TIDS + 1][IEEE80211_MAX_PN_LEN];
		} gen;
	} u;

#ifdef CONFIG_MAC80211_DEBUGFS
	struct {
		struct dentry *stalink;
		struct dentry *dir;
		int cnt;
	} debugfs;
#endif

	unsigned int color;

	 
	struct ieee80211_key_conf conf;
};

struct ieee80211_key *
ieee80211_key_alloc(u32 cipher, int idx, size_t key_len,
		    const u8 *key_data,
		    size_t seq_len, const u8 *seq);
 
int ieee80211_key_link(struct ieee80211_key *key,
		       struct ieee80211_link_data *link,
		       struct sta_info *sta);
int ieee80211_set_tx_key(struct ieee80211_key *key);
void ieee80211_key_free(struct ieee80211_key *key, bool delay_tailroom);
void ieee80211_key_free_unused(struct ieee80211_key *key);
void ieee80211_set_default_key(struct ieee80211_link_data *link, int idx,
			       bool uni, bool multi);
void ieee80211_set_default_mgmt_key(struct ieee80211_link_data *link,
				    int idx);
void ieee80211_set_default_beacon_key(struct ieee80211_link_data *link,
				      int idx);
void ieee80211_remove_link_keys(struct ieee80211_link_data *link,
				struct list_head *keys);
void ieee80211_free_key_list(struct ieee80211_local *local,
			     struct list_head *keys);
void ieee80211_free_keys(struct ieee80211_sub_if_data *sdata,
			 bool force_synchronize);
void ieee80211_free_sta_keys(struct ieee80211_local *local,
			     struct sta_info *sta);
void ieee80211_reenable_keys(struct ieee80211_sub_if_data *sdata);
int ieee80211_key_switch_links(struct ieee80211_sub_if_data *sdata,
			       unsigned long del_links_mask,
			       unsigned long add_links_mask);

#define key_mtx_dereference(local, ref) \
	rcu_dereference_protected(ref, lockdep_is_held(&((local)->key_mtx)))
#define rcu_dereference_check_key_mtx(local, ref) \
	rcu_dereference_check(ref, lockdep_is_held(&((local)->key_mtx)))

void ieee80211_delayed_tailroom_dec(struct work_struct *wk);

#endif  
