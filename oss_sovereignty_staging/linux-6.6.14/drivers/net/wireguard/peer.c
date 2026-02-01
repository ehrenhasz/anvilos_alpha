
 

#include "peer.h"
#include "device.h"
#include "queueing.h"
#include "timers.h"
#include "peerlookup.h"
#include "noise.h"

#include <linux/kref.h>
#include <linux/lockdep.h>
#include <linux/rcupdate.h>
#include <linux/list.h>

static struct kmem_cache *peer_cache;
static atomic64_t peer_counter = ATOMIC64_INIT(0);

struct wg_peer *wg_peer_create(struct wg_device *wg,
			       const u8 public_key[NOISE_PUBLIC_KEY_LEN],
			       const u8 preshared_key[NOISE_SYMMETRIC_KEY_LEN])
{
	struct wg_peer *peer;
	int ret = -ENOMEM;

	lockdep_assert_held(&wg->device_update_lock);

	if (wg->num_peers >= MAX_PEERS_PER_DEVICE)
		return ERR_PTR(ret);

	peer = kmem_cache_zalloc(peer_cache, GFP_KERNEL);
	if (unlikely(!peer))
		return ERR_PTR(ret);
	if (unlikely(dst_cache_init(&peer->endpoint_cache, GFP_KERNEL)))
		goto err;

	peer->device = wg;
	wg_noise_handshake_init(&peer->handshake, &wg->static_identity,
				public_key, preshared_key, peer);
	peer->internal_id = atomic64_inc_return(&peer_counter);
	peer->serial_work_cpu = nr_cpumask_bits;
	wg_cookie_init(&peer->latest_cookie);
	wg_timers_init(peer);
	wg_cookie_checker_precompute_peer_keys(peer);
	spin_lock_init(&peer->keypairs.keypair_update_lock);
	INIT_WORK(&peer->transmit_handshake_work, wg_packet_handshake_send_worker);
	INIT_WORK(&peer->transmit_packet_work, wg_packet_tx_worker);
	wg_prev_queue_init(&peer->tx_queue);
	wg_prev_queue_init(&peer->rx_queue);
	rwlock_init(&peer->endpoint_lock);
	kref_init(&peer->refcount);
	skb_queue_head_init(&peer->staged_packet_queue);
	wg_noise_reset_last_sent_handshake(&peer->last_sent_handshake);
	set_bit(NAPI_STATE_NO_BUSY_POLL, &peer->napi.state);
	netif_napi_add(wg->dev, &peer->napi, wg_packet_rx_poll);
	napi_enable(&peer->napi);
	list_add_tail(&peer->peer_list, &wg->peer_list);
	INIT_LIST_HEAD(&peer->allowedips_list);
	wg_pubkey_hashtable_add(wg->peer_hashtable, peer);
	++wg->num_peers;
	pr_debug("%s: Peer %llu created\n", wg->dev->name, peer->internal_id);
	return peer;

err:
	kmem_cache_free(peer_cache, peer);
	return ERR_PTR(ret);
}

struct wg_peer *wg_peer_get_maybe_zero(struct wg_peer *peer)
{
	RCU_LOCKDEP_WARN(!rcu_read_lock_bh_held(),
			 "Taking peer reference without holding the RCU read lock");
	if (unlikely(!peer || !kref_get_unless_zero(&peer->refcount)))
		return NULL;
	return peer;
}

static void peer_make_dead(struct wg_peer *peer)
{
	 
	list_del_init(&peer->peer_list);
	wg_allowedips_remove_by_peer(&peer->device->peer_allowedips, peer,
				     &peer->device->device_update_lock);
	wg_pubkey_hashtable_remove(peer->device->peer_hashtable, peer);

	 
	WRITE_ONCE(peer->is_dead, true);

	 
}

static void peer_remove_after_dead(struct wg_peer *peer)
{
	WARN_ON(!peer->is_dead);

	 
	wg_noise_keypairs_clear(&peer->keypairs);

	 
	wg_timers_stop(peer);

	 

	 
	flush_workqueue(peer->device->packet_crypt_wq);
	 
	flush_workqueue(peer->device->packet_crypt_wq);
	 
	napi_disable(&peer->napi);
	 
	netif_napi_del(&peer->napi);

	 
	flush_workqueue(peer->device->handshake_send_wq);

	 

	--peer->device->num_peers;
	wg_peer_put(peer);
}

 
void wg_peer_remove(struct wg_peer *peer)
{
	if (unlikely(!peer))
		return;
	lockdep_assert_held(&peer->device->device_update_lock);

	peer_make_dead(peer);
	synchronize_net();
	peer_remove_after_dead(peer);
}

void wg_peer_remove_all(struct wg_device *wg)
{
	struct wg_peer *peer, *temp;
	LIST_HEAD(dead_peers);

	lockdep_assert_held(&wg->device_update_lock);

	 
	wg_allowedips_free(&wg->peer_allowedips, &wg->device_update_lock);

	list_for_each_entry_safe(peer, temp, &wg->peer_list, peer_list) {
		peer_make_dead(peer);
		list_add_tail(&peer->peer_list, &dead_peers);
	}
	synchronize_net();
	list_for_each_entry_safe(peer, temp, &dead_peers, peer_list)
		peer_remove_after_dead(peer);
}

static void rcu_release(struct rcu_head *rcu)
{
	struct wg_peer *peer = container_of(rcu, struct wg_peer, rcu);

	dst_cache_destroy(&peer->endpoint_cache);
	WARN_ON(wg_prev_queue_peek(&peer->tx_queue) || wg_prev_queue_peek(&peer->rx_queue));

	 
	memzero_explicit(peer, sizeof(*peer));
	kmem_cache_free(peer_cache, peer);
}

static void kref_release(struct kref *refcount)
{
	struct wg_peer *peer = container_of(refcount, struct wg_peer, refcount);

	pr_debug("%s: Peer %llu (%pISpfsc) destroyed\n",
		 peer->device->dev->name, peer->internal_id,
		 &peer->endpoint.addr);

	 
	wg_index_hashtable_remove(peer->device->index_hashtable,
				  &peer->handshake.entry);

	 
	wg_packet_purge_staged_packets(peer);

	 
	call_rcu(&peer->rcu, rcu_release);
}

void wg_peer_put(struct wg_peer *peer)
{
	if (unlikely(!peer))
		return;
	kref_put(&peer->refcount, kref_release);
}

int __init wg_peer_init(void)
{
	peer_cache = KMEM_CACHE(wg_peer, 0);
	return peer_cache ? 0 : -ENOMEM;
}

void wg_peer_uninit(void)
{
	kmem_cache_destroy(peer_cache);
}
