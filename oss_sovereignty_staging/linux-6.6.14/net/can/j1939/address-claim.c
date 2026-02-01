









 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/netdevice.h>
#include <linux/skbuff.h>

#include "j1939-priv.h"

static inline name_t j1939_skb_to_name(const struct sk_buff *skb)
{
	return le64_to_cpup((__le64 *)skb->data);
}

static inline bool j1939_ac_msg_is_request(struct sk_buff *skb)
{
	struct j1939_sk_buff_cb *skcb = j1939_skb_to_cb(skb);
	int req_pgn;

	if (skb->len < 3 || skcb->addr.pgn != J1939_PGN_REQUEST)
		return false;

	req_pgn = skb->data[0] | (skb->data[1] << 8) | (skb->data[2] << 16);

	return req_pgn == J1939_PGN_ADDRESS_CLAIMED;
}

static int j1939_ac_verify_outgoing(struct j1939_priv *priv,
				    struct sk_buff *skb)
{
	struct j1939_sk_buff_cb *skcb = j1939_skb_to_cb(skb);

	if (skb->len != 8) {
		netdev_notice(priv->ndev, "tx address claim with dlc %i\n",
			      skb->len);
		return -EPROTO;
	}

	if (skcb->addr.src_name != j1939_skb_to_name(skb)) {
		netdev_notice(priv->ndev, "tx address claim with different name\n");
		return -EPROTO;
	}

	if (skcb->addr.sa == J1939_NO_ADDR) {
		netdev_notice(priv->ndev, "tx address claim with broadcast sa\n");
		return -EPROTO;
	}

	 
	if (skcb->addr.dst_name || skcb->addr.da != J1939_NO_ADDR) {
		netdev_notice(priv->ndev, "tx address claim with dest, not broadcast\n");
		return -EPROTO;
	}
	return 0;
}

int j1939_ac_fixup(struct j1939_priv *priv, struct sk_buff *skb)
{
	struct j1939_sk_buff_cb *skcb = j1939_skb_to_cb(skb);
	int ret;
	u8 addr;

	 
	if (skcb->addr.pgn == J1939_PGN_ADDRESS_CLAIMED) {
		struct j1939_ecu *ecu;

		ret = j1939_ac_verify_outgoing(priv, skb);
		 
		if (ret < 0)
			return ret;
		ecu = j1939_ecu_get_by_name(priv, skcb->addr.src_name);
		if (!ecu)
			return -ENODEV;

		if (ecu->addr != skcb->addr.sa)
			 
			j1939_ecu_unmap(ecu);
		j1939_ecu_put(ecu);
	} else if (skcb->addr.src_name) {
		 
		addr = j1939_name_to_addr(priv, skcb->addr.src_name);
		if (!j1939_address_is_unicast(addr) &&
		    !j1939_ac_msg_is_request(skb)) {
			netdev_notice(priv->ndev, "tx drop: invalid sa for name 0x%016llx\n",
				      skcb->addr.src_name);
			return -EADDRNOTAVAIL;
		}
		skcb->addr.sa = addr;
	}

	 
	if (skcb->addr.dst_name) {
		addr = j1939_name_to_addr(priv, skcb->addr.dst_name);
		if (!j1939_address_is_unicast(addr)) {
			netdev_notice(priv->ndev, "tx drop: invalid da for name 0x%016llx\n",
				      skcb->addr.dst_name);
			return -EADDRNOTAVAIL;
		}
		skcb->addr.da = addr;
	}
	return 0;
}

static void j1939_ac_process(struct j1939_priv *priv, struct sk_buff *skb)
{
	struct j1939_sk_buff_cb *skcb = j1939_skb_to_cb(skb);
	struct j1939_ecu *ecu, *prev;
	name_t name;

	if (skb->len != 8) {
		netdev_notice(priv->ndev, "rx address claim with wrong dlc %i\n",
			      skb->len);
		return;
	}

	name = j1939_skb_to_name(skb);
	skcb->addr.src_name = name;
	if (!name) {
		netdev_notice(priv->ndev, "rx address claim without name\n");
		return;
	}

	if (!j1939_address_is_valid(skcb->addr.sa)) {
		netdev_notice(priv->ndev, "rx address claim with broadcast sa\n");
		return;
	}

	write_lock_bh(&priv->lock);

	 
	ecu = j1939_ecu_get_by_name_locked(priv, name);

	if (ecu && ecu->addr == skcb->addr.sa) {
		 
		goto out_ecu_put;
	}

	if (!ecu && j1939_address_is_unicast(skcb->addr.sa))
		ecu = j1939_ecu_create_locked(priv, name);

	if (IS_ERR_OR_NULL(ecu))
		goto out_unlock_bh;

	 
	j1939_ecu_timer_cancel(ecu);

	if (j1939_address_is_idle(skcb->addr.sa)) {
		j1939_ecu_unmap_locked(ecu);
		goto out_ecu_put;
	}

	 
	if (ecu->addr != skcb->addr.sa)
		j1939_ecu_unmap_locked(ecu);
	ecu->addr = skcb->addr.sa;

	prev = j1939_ecu_get_by_addr_locked(priv, skcb->addr.sa);
	if (prev) {
		if (ecu->name > prev->name) {
			j1939_ecu_unmap_locked(ecu);
			j1939_ecu_put(prev);
			goto out_ecu_put;
		} else {
			 
			j1939_ecu_unmap_locked(prev);
			j1939_ecu_put(prev);
		}
	}

	j1939_ecu_timer_start(ecu);
 out_ecu_put:
	j1939_ecu_put(ecu);
 out_unlock_bh:
	write_unlock_bh(&priv->lock);
}

void j1939_ac_recv(struct j1939_priv *priv, struct sk_buff *skb)
{
	struct j1939_sk_buff_cb *skcb = j1939_skb_to_cb(skb);
	struct j1939_ecu *ecu;

	 
	if (skcb->addr.pgn == J1939_PGN_ADDRESS_CLAIMED) {
		j1939_ac_process(priv, skb);
	} else if (j1939_address_is_unicast(skcb->addr.sa)) {
		 
		ecu = j1939_ecu_get_by_addr(priv, skcb->addr.sa);
		if (ecu) {
			skcb->addr.src_name = ecu->name;
			j1939_ecu_put(ecu);
		}
	}

	 
	ecu = j1939_ecu_get_by_addr(priv, skcb->addr.da);
	if (ecu) {
		skcb->addr.dst_name = ecu->name;
		j1939_ecu_put(ecu);
	}
}
