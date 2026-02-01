
 

#include "tc_counters.h"
#include "tc_encap_actions.h"
#include "mae_counter_format.h"
#include "mae.h"
#include "rx_common.h"

 

static const struct rhashtable_params efx_tc_counter_id_ht_params = {
	.key_len	= offsetof(struct efx_tc_counter_index, linkage),
	.key_offset	= 0,
	.head_offset	= offsetof(struct efx_tc_counter_index, linkage),
};

static const struct rhashtable_params efx_tc_counter_ht_params = {
	.key_len	= offsetof(struct efx_tc_counter, linkage),
	.key_offset	= 0,
	.head_offset	= offsetof(struct efx_tc_counter, linkage),
};

static void efx_tc_counter_free(void *ptr, void *__unused)
{
	struct efx_tc_counter *cnt = ptr;

	WARN_ON(!list_empty(&cnt->users));
	 
	flush_work(&cnt->work);
	EFX_WARN_ON_PARANOID(spin_is_locked(&cnt->lock));
	kfree(cnt);
}

static void efx_tc_counter_id_free(void *ptr, void *__unused)
{
	struct efx_tc_counter_index *ctr = ptr;

	WARN_ON(refcount_read(&ctr->ref));
	kfree(ctr);
}

int efx_tc_init_counters(struct efx_nic *efx)
{
	int rc;

	rc = rhashtable_init(&efx->tc->counter_id_ht, &efx_tc_counter_id_ht_params);
	if (rc < 0)
		goto fail_counter_id_ht;
	rc = rhashtable_init(&efx->tc->counter_ht, &efx_tc_counter_ht_params);
	if (rc < 0)
		goto fail_counter_ht;
	return 0;
fail_counter_ht:
	rhashtable_destroy(&efx->tc->counter_id_ht);
fail_counter_id_ht:
	return rc;
}

 
void efx_tc_destroy_counters(struct efx_nic *efx)
{
	rhashtable_destroy(&efx->tc->counter_ht);
	rhashtable_destroy(&efx->tc->counter_id_ht);
}

void efx_tc_fini_counters(struct efx_nic *efx)
{
	rhashtable_free_and_destroy(&efx->tc->counter_id_ht, efx_tc_counter_id_free, NULL);
	rhashtable_free_and_destroy(&efx->tc->counter_ht, efx_tc_counter_free, NULL);
}

static void efx_tc_counter_work(struct work_struct *work)
{
	struct efx_tc_counter *cnt = container_of(work, struct efx_tc_counter, work);
	struct efx_tc_encap_action *encap;
	struct efx_tc_action_set *act;
	unsigned long touched;
	struct neighbour *n;

	spin_lock_bh(&cnt->lock);
	touched = READ_ONCE(cnt->touched);

	list_for_each_entry(act, &cnt->users, count_user) {
		encap = act->encap_md;
		if (!encap)
			continue;
		if (!encap->neigh)  
			continue;
		if (time_after_eq(encap->neigh->used, touched))
			continue;
		encap->neigh->used = touched;
		 
		if (encap->neigh->dst_ip)
			n = neigh_lookup(&arp_tbl, &encap->neigh->dst_ip,
					 encap->neigh->egdev);
		else
#if IS_ENABLED(CONFIG_IPV6)
			n = neigh_lookup(ipv6_stub->nd_tbl,
					 &encap->neigh->dst_ip6,
					 encap->neigh->egdev);
#else
			n = NULL;
#endif
		if (!n)
			continue;

		neigh_event_send(n, NULL);
		neigh_release(n);
	}
	spin_unlock_bh(&cnt->lock);
}

 

struct efx_tc_counter *efx_tc_flower_allocate_counter(struct efx_nic *efx,
						      int type)
{
	struct efx_tc_counter *cnt;
	int rc, rc2;

	cnt = kzalloc(sizeof(*cnt), GFP_USER);
	if (!cnt)
		return ERR_PTR(-ENOMEM);

	spin_lock_init(&cnt->lock);
	INIT_WORK(&cnt->work, efx_tc_counter_work);
	cnt->touched = jiffies;
	cnt->type = type;

	rc = efx_mae_allocate_counter(efx, cnt);
	if (rc)
		goto fail1;
	INIT_LIST_HEAD(&cnt->users);
	rc = rhashtable_insert_fast(&efx->tc->counter_ht, &cnt->linkage,
				    efx_tc_counter_ht_params);
	if (rc)
		goto fail2;
	return cnt;
fail2:
	 
	rc2 = efx_mae_free_counter(efx, cnt);
	if (rc2)
		netif_warn(efx, hw, efx->net_dev,
			   "Failed to free MAE counter %u, rc %d\n",
			   cnt->fw_id, rc2);
fail1:
	kfree(cnt);
	return ERR_PTR(rc > 0 ? -EIO : rc);
}

void efx_tc_flower_release_counter(struct efx_nic *efx,
				   struct efx_tc_counter *cnt)
{
	int rc;

	rhashtable_remove_fast(&efx->tc->counter_ht, &cnt->linkage,
			       efx_tc_counter_ht_params);
	rc = efx_mae_free_counter(efx, cnt);
	if (rc)
		netif_warn(efx, hw, efx->net_dev,
			   "Failed to free MAE counter %u, rc %d\n",
			   cnt->fw_id, rc);
	WARN_ON(!list_empty(&cnt->users));
	 
	synchronize_rcu();
	flush_work(&cnt->work);
	EFX_WARN_ON_PARANOID(spin_is_locked(&cnt->lock));
	kfree(cnt);
}

static struct efx_tc_counter *efx_tc_flower_find_counter_by_fw_id(
				struct efx_nic *efx, int type, u32 fw_id)
{
	struct efx_tc_counter key = {};

	key.fw_id = fw_id;
	key.type = type;

	return rhashtable_lookup_fast(&efx->tc->counter_ht, &key,
				      efx_tc_counter_ht_params);
}

 

void efx_tc_flower_put_counter_index(struct efx_nic *efx,
				     struct efx_tc_counter_index *ctr)
{
	if (!refcount_dec_and_test(&ctr->ref))
		return;  
	rhashtable_remove_fast(&efx->tc->counter_id_ht, &ctr->linkage,
			       efx_tc_counter_id_ht_params);
	efx_tc_flower_release_counter(efx, ctr->cnt);
	kfree(ctr);
}

struct efx_tc_counter_index *efx_tc_flower_get_counter_index(
				struct efx_nic *efx, unsigned long cookie,
				enum efx_tc_counter_type type)
{
	struct efx_tc_counter_index *ctr, *old;
	struct efx_tc_counter *cnt;

	ctr = kzalloc(sizeof(*ctr), GFP_USER);
	if (!ctr)
		return ERR_PTR(-ENOMEM);
	ctr->cookie = cookie;
	old = rhashtable_lookup_get_insert_fast(&efx->tc->counter_id_ht,
						&ctr->linkage,
						efx_tc_counter_id_ht_params);
	if (old) {
		 
		kfree(ctr);
		if (IS_ERR(old))  
			return ERR_CAST(old);
		if (!refcount_inc_not_zero(&old->ref))
			return ERR_PTR(-EAGAIN);
		 
		ctr = old;
	} else {
		cnt = efx_tc_flower_allocate_counter(efx, type);
		if (IS_ERR(cnt)) {
			rhashtable_remove_fast(&efx->tc->counter_id_ht,
					       &ctr->linkage,
					       efx_tc_counter_id_ht_params);
			kfree(ctr);
			return (void *)cnt;  
		}
		ctr->cnt = cnt;
		refcount_set(&ctr->ref, 1);
	}
	return ctr;
}

struct efx_tc_counter_index *efx_tc_flower_find_counter_index(
				struct efx_nic *efx, unsigned long cookie)
{
	struct efx_tc_counter_index key = {};

	key.cookie = cookie;
	return rhashtable_lookup_fast(&efx->tc->counter_id_ht, &key,
				      efx_tc_counter_id_ht_params);
}

 

static void efx_tc_handle_no_channel(struct efx_nic *efx)
{
	netif_warn(efx, drv, efx->net_dev,
		   "MAE counters require MSI-X and 1 additional interrupt vector.\n");
}

static int efx_tc_probe_channel(struct efx_channel *channel)
{
	struct efx_rx_queue *rx_queue = &channel->rx_queue;

	channel->irq_moderation_us = 0;
	rx_queue->core_index = 0;

	INIT_WORK(&rx_queue->grant_work, efx_mae_counters_grant_credits);

	return 0;
}

static int efx_tc_start_channel(struct efx_channel *channel)
{
	struct efx_rx_queue *rx_queue = efx_channel_get_rx_queue(channel);
	struct efx_nic *efx = channel->efx;

	return efx_mae_start_counters(efx, rx_queue);
}

static void efx_tc_stop_channel(struct efx_channel *channel)
{
	struct efx_rx_queue *rx_queue = efx_channel_get_rx_queue(channel);
	struct efx_nic *efx = channel->efx;
	int rc;

	rc = efx_mae_stop_counters(efx, rx_queue);
	if (rc)
		netif_warn(efx, drv, efx->net_dev,
			   "Failed to stop MAE counters streaming, rc=%d.\n",
			   rc);
	rx_queue->grant_credits = false;
	flush_work(&rx_queue->grant_work);
}

static void efx_tc_remove_channel(struct efx_channel *channel)
{
}

static void efx_tc_get_channel_name(struct efx_channel *channel,
				    char *buf, size_t len)
{
	snprintf(buf, len, "%s-mae", channel->efx->name);
}

static void efx_tc_counter_update(struct efx_nic *efx,
				  enum efx_tc_counter_type counter_type,
				  u32 counter_idx, u64 packets, u64 bytes,
				  u32 mark)
{
	struct efx_tc_counter *cnt;

	rcu_read_lock();  
	cnt = efx_tc_flower_find_counter_by_fw_id(efx, counter_type, counter_idx);
	if (!cnt) {
		 
		if (net_ratelimit())
			netif_dbg(efx, drv, efx->net_dev,
				  "Got update for unwanted MAE counter %u type %u\n",
				  counter_idx, counter_type);
		goto out;
	}

	spin_lock_bh(&cnt->lock);
	if ((s32)mark - (s32)cnt->gen < 0) {
		 
	} else {
		 
		cnt->gen = mark;
		 
		cnt->packets += packets;
		cnt->bytes += bytes;
		cnt->touched = jiffies;
	}
	spin_unlock_bh(&cnt->lock);
	schedule_work(&cnt->work);
out:
	rcu_read_unlock();
}

static void efx_tc_rx_version_1(struct efx_nic *efx, const u8 *data, u32 mark)
{
	u16 n_counters, i;

	 

	n_counters = le16_to_cpu(*(const __le16 *)(data + 6));

	 
	for (i = 0; i < n_counters; i++) {
		const void *entry = data + 8 + 16 * i;
		u64 packet_count, byte_count;
		u32 counter_idx;

		counter_idx = le32_to_cpu(*(const __le32 *)entry);
		packet_count = le32_to_cpu(*(const __le32 *)(entry + 4)) |
			       ((u64)le16_to_cpu(*(const __le16 *)(entry + 8)) << 32);
		byte_count = le16_to_cpu(*(const __le16 *)(entry + 10)) |
			     ((u64)le32_to_cpu(*(const __le32 *)(entry + 12)) << 16);
		efx_tc_counter_update(efx, EFX_TC_COUNTER_TYPE_AR, counter_idx,
				      packet_count, byte_count, mark);
	}
}

#define TCV2_HDR_PTR(pkt, field)						\
	((void)BUILD_BUG_ON_ZERO(ERF_SC_PACKETISER_HEADER_##field##_LBN & 7),	\
	 (pkt) + ERF_SC_PACKETISER_HEADER_##field##_LBN / 8)
#define TCV2_HDR_BYTE(pkt, field)						\
	((void)BUILD_BUG_ON_ZERO(ERF_SC_PACKETISER_HEADER_##field##_WIDTH != 8),\
	 *TCV2_HDR_PTR(pkt, field))
#define TCV2_HDR_WORD(pkt, field)						\
	((void)BUILD_BUG_ON_ZERO(ERF_SC_PACKETISER_HEADER_##field##_WIDTH != 16),\
	 (void)BUILD_BUG_ON_ZERO(ERF_SC_PACKETISER_HEADER_##field##_LBN & 15),	\
	 *(__force const __le16 *)TCV2_HDR_PTR(pkt, field))
#define TCV2_PKT_PTR(pkt, poff, i, field)					\
	((void)BUILD_BUG_ON_ZERO(ERF_SC_PACKETISER_PAYLOAD_##field##_LBN & 7),	\
	 (pkt) + ERF_SC_PACKETISER_PAYLOAD_##field##_LBN/8 + poff +		\
	 i * ER_RX_SL_PACKETISER_PAYLOAD_WORD_SIZE)

 
static u64 efx_tc_read48(const __le16 *field)
{
	u64 out = 0;
	int i;

	for (i = 0; i < 3; i++)
		out |= (u64)le16_to_cpu(field[i]) << (i * 16);
	return out;
}

static enum efx_tc_counter_type efx_tc_rx_version_2(struct efx_nic *efx,
						    const u8 *data, u32 mark)
{
	u8 payload_offset, header_offset, ident;
	enum efx_tc_counter_type type;
	u16 n_counters, i;

	ident = TCV2_HDR_BYTE(data, IDENTIFIER);
	switch (ident) {
	case ERF_SC_PACKETISER_HEADER_IDENTIFIER_AR:
		type = EFX_TC_COUNTER_TYPE_AR;
		break;
	case ERF_SC_PACKETISER_HEADER_IDENTIFIER_CT:
		type = EFX_TC_COUNTER_TYPE_CT;
		break;
	case ERF_SC_PACKETISER_HEADER_IDENTIFIER_OR:
		type = EFX_TC_COUNTER_TYPE_OR;
		break;
	default:
		if (net_ratelimit())
			netif_err(efx, drv, efx->net_dev,
				  "ignored v2 MAE counter packet (bad identifier %u"
				  "), counters may be inaccurate\n", ident);
		return EFX_TC_COUNTER_TYPE_MAX;
	}
	header_offset = TCV2_HDR_BYTE(data, HEADER_OFFSET);
	 
	if (header_offset != ERF_SC_PACKETISER_HEADER_HEADER_OFFSET_DEFAULT) {
		if (net_ratelimit())
			netif_err(efx, drv, efx->net_dev,
				  "choked on v2 MAE counter packet (bad header_offset %u"
				  "), counters may be inaccurate\n", header_offset);
		return EFX_TC_COUNTER_TYPE_MAX;
	}
	payload_offset = TCV2_HDR_BYTE(data, PAYLOAD_OFFSET);
	n_counters = le16_to_cpu(TCV2_HDR_WORD(data, COUNT));

	for (i = 0; i < n_counters; i++) {
		const void *counter_idx_p, *packet_count_p, *byte_count_p;
		u64 packet_count, byte_count;
		u32 counter_idx;

		 
		counter_idx_p = TCV2_PKT_PTR(data, payload_offset, i, COUNTER_INDEX);
		BUILD_BUG_ON(ERF_SC_PACKETISER_PAYLOAD_COUNTER_INDEX_WIDTH != 24);
		BUILD_BUG_ON(ERF_SC_PACKETISER_PAYLOAD_COUNTER_INDEX_LBN & 31);
		counter_idx = le32_to_cpu(*(const __le32 *)counter_idx_p) & 0xffffff;
		 
		packet_count_p = TCV2_PKT_PTR(data, payload_offset, i, PACKET_COUNT);
		BUILD_BUG_ON(ERF_SC_PACKETISER_PAYLOAD_PACKET_COUNT_WIDTH != 48);
		BUILD_BUG_ON(ERF_SC_PACKETISER_PAYLOAD_PACKET_COUNT_LBN & 15);
		packet_count = efx_tc_read48((const __le16 *)packet_count_p);
		 
		byte_count_p = TCV2_PKT_PTR(data, payload_offset, i, BYTE_COUNT);
		BUILD_BUG_ON(ERF_SC_PACKETISER_PAYLOAD_BYTE_COUNT_WIDTH != 48);
		BUILD_BUG_ON(ERF_SC_PACKETISER_PAYLOAD_BYTE_COUNT_LBN & 15);
		byte_count = efx_tc_read48((const __le16 *)byte_count_p);

		if (type == EFX_TC_COUNTER_TYPE_CT) {
			 
			if (packet_count || byte_count != 1)
				netdev_warn_once(efx->net_dev,
						 "CT counter with inconsistent state (%llu, %llu)\n",
						 packet_count, byte_count);
			 
			byte_count = 0;
		}

		efx_tc_counter_update(efx, type, counter_idx, packet_count,
				      byte_count, mark);
	}
	return type;
}

 
static bool efx_tc_rx(struct efx_rx_queue *rx_queue, u32 mark)
{
	struct efx_channel *channel = efx_rx_queue_channel(rx_queue);
	struct efx_rx_buffer *rx_buf = efx_rx_buffer(rx_queue,
						     channel->rx_pkt_index);
	const u8 *data = efx_rx_buf_va(rx_buf);
	struct efx_nic *efx = rx_queue->efx;
	enum efx_tc_counter_type type;
	u8 version;

	 
	version = *data;
	switch (version) {
	case 1:
		type = EFX_TC_COUNTER_TYPE_AR;
		efx_tc_rx_version_1(efx, data, mark);
		break;
	case ERF_SC_PACKETISER_HEADER_VERSION_VALUE: 
		type = efx_tc_rx_version_2(efx, data, mark);
		break;
	default:
		if (net_ratelimit())
			netif_err(efx, drv, efx->net_dev,
				  "choked on MAE counter packet (bad version %u"
				  "); counters may be inaccurate\n",
				  version);
		goto out;
	}

	if (type < EFX_TC_COUNTER_TYPE_MAX) {
		 
		efx->tc->seen_gen[type] = mark;
		if (efx->tc->flush_counters &&
		    (s32)(efx->tc->flush_gen[type] - mark) <= 0)
			wake_up(&efx->tc->flush_wq);
	}
out:
	efx_free_rx_buffers(rx_queue, rx_buf, 1);
	channel->rx_pkt_n_frags = 0;
	return true;
}

const struct efx_channel_type efx_tc_channel_type = {
	.handle_no_channel	= efx_tc_handle_no_channel,
	.pre_probe		= efx_tc_probe_channel,
	.start			= efx_tc_start_channel,
	.stop			= efx_tc_stop_channel,
	.post_remove		= efx_tc_remove_channel,
	.get_name		= efx_tc_get_channel_name,
	.receive_raw		= efx_tc_rx,
	.keep_eventq		= true,
};
