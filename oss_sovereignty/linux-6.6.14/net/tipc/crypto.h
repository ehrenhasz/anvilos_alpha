#ifdef CONFIG_TIPC_CRYPTO
#ifndef _TIPC_CRYPTO_H
#define _TIPC_CRYPTO_H
#include "core.h"
#include "node.h"
#include "msg.h"
#include "bearer.h"
#define TIPC_EVERSION			7
#define TIPC_AES_GCM_KEY_SIZE_128	16
#define TIPC_AES_GCM_KEY_SIZE_192	24
#define TIPC_AES_GCM_KEY_SIZE_256	32
#define TIPC_AES_GCM_SALT_SIZE		4
#define TIPC_AES_GCM_IV_SIZE		12
#define TIPC_AES_GCM_TAG_SIZE		16
enum {
	CLUSTER_KEY = 1,
	PER_NODE_KEY = (1 << 1),
};
extern int sysctl_tipc_max_tfms __read_mostly;
extern int sysctl_tipc_key_exchange_enabled __read_mostly;
struct tipc_ehdr {
	union {
		struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
			__u8	destined:1,
				user:4,
				version:3;
			__u8	reserved_1:1,
				rx_nokey:1,
				master_key:1,
				keepalive:1,
				rx_key_active:2,
				tx_key:2;
#elif defined(__BIG_ENDIAN_BITFIELD)
			__u8	version:3,
				user:4,
				destined:1;
			__u8	tx_key:2,
				rx_key_active:2,
				keepalive:1,
				master_key:1,
				rx_nokey:1,
				reserved_1:1;
#else
#error  "Please fix <asm/byteorder.h>"
#endif
			__be16	reserved_2;
		} __packed;
		__be32 w0;
	};
	__be64 seqno;
	union {
		__be32 addr;
		__u8 id[NODE_ID_LEN];  
	};
#define EHDR_SIZE	(offsetof(struct tipc_ehdr, addr) + sizeof(__be32))
#define EHDR_CFG_SIZE	(sizeof(struct tipc_ehdr))
#define EHDR_MIN_SIZE	(EHDR_SIZE)
#define EHDR_MAX_SIZE	(EHDR_CFG_SIZE)
#define EMSG_OVERHEAD	(EHDR_SIZE + TIPC_AES_GCM_TAG_SIZE)
} __packed;
int tipc_crypto_start(struct tipc_crypto **crypto, struct net *net,
		      struct tipc_node *node);
void tipc_crypto_stop(struct tipc_crypto **crypto);
void tipc_crypto_timeout(struct tipc_crypto *rx);
int tipc_crypto_xmit(struct net *net, struct sk_buff **skb,
		     struct tipc_bearer *b, struct tipc_media_addr *dst,
		     struct tipc_node *__dnode);
int tipc_crypto_rcv(struct net *net, struct tipc_crypto *rx,
		    struct sk_buff **skb, struct tipc_bearer *b);
int tipc_crypto_key_init(struct tipc_crypto *c, struct tipc_aead_key *ukey,
			 u8 mode, bool master_key);
void tipc_crypto_key_flush(struct tipc_crypto *c);
int tipc_crypto_key_distr(struct tipc_crypto *tx, u8 key,
			  struct tipc_node *dest);
void tipc_crypto_msg_rcv(struct net *net, struct sk_buff *skb);
void tipc_crypto_rekeying_sched(struct tipc_crypto *tx, bool changed,
				u32 new_intv);
int tipc_aead_key_validate(struct tipc_aead_key *ukey, struct genl_info *info);
bool tipc_ehdr_validate(struct sk_buff *skb);
static inline u32 msg_key_gen(struct tipc_msg *m)
{
	return msg_bits(m, 4, 16, 0xffff);
}
static inline void msg_set_key_gen(struct tipc_msg *m, u32 gen)
{
	msg_set_bits(m, 4, 16, 0xffff, gen);
}
static inline u32 msg_key_mode(struct tipc_msg *m)
{
	return msg_bits(m, 4, 0, 0xf);
}
static inline void msg_set_key_mode(struct tipc_msg *m, u32 mode)
{
	msg_set_bits(m, 4, 0, 0xf, mode);
}
#endif  
#endif
