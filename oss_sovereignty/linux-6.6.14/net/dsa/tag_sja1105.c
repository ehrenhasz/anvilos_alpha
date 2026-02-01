
 
#include <linux/if_vlan.h>
#include <linux/dsa/sja1105.h>
#include <linux/dsa/8021q.h>
#include <linux/packing.h>

#include "tag.h"
#include "tag_8021q.h"

#define SJA1105_NAME				"sja1105"
#define SJA1110_NAME				"sja1110"

 
#define SJA1110_HEADER_HOST_TO_SWITCH		BIT(15)

 
#define SJA1110_RX_HEADER_IS_METADATA		BIT(14)
#define SJA1110_RX_HEADER_HOST_ONLY		BIT(13)
#define SJA1110_RX_HEADER_HAS_TRAILER		BIT(12)

 
#define SJA1110_RX_HEADER_SRC_PORT(x)		(((x) & GENMASK(7, 4)) >> 4)
#define SJA1110_RX_HEADER_SWITCH_ID(x)		((x) & GENMASK(3, 0))

 
#define SJA1110_RX_HEADER_TRAILER_POS(x)	((x) & GENMASK(11, 0))

#define SJA1110_RX_TRAILER_SWITCH_ID(x)		(((x) & GENMASK(7, 4)) >> 4)
#define SJA1110_RX_TRAILER_SRC_PORT(x)		((x) & GENMASK(3, 0))

 
#define SJA1110_RX_HEADER_N_TS(x)		(((x) & GENMASK(8, 4)) >> 4)

 
#define SJA1110_TX_HEADER_UPDATE_TC		BIT(14)
#define SJA1110_TX_HEADER_TAKE_TS		BIT(13)
#define SJA1110_TX_HEADER_TAKE_TS_CASC		BIT(12)
#define SJA1110_TX_HEADER_HAS_TRAILER		BIT(11)

 
#define SJA1110_TX_HEADER_PRIO(x)		(((x) << 7) & GENMASK(10, 7))
#define SJA1110_TX_HEADER_TSTAMP_ID(x)		((x) & GENMASK(7, 0))

 
#define SJA1110_TX_HEADER_TRAILER_POS(x)	((x) & GENMASK(10, 0))

#define SJA1110_TX_TRAILER_TSTAMP_ID(x)		(((x) << 24) & GENMASK(31, 24))
#define SJA1110_TX_TRAILER_PRIO(x)		(((x) << 21) & GENMASK(23, 21))
#define SJA1110_TX_TRAILER_SWITCHID(x)		(((x) << 12) & GENMASK(15, 12))
#define SJA1110_TX_TRAILER_DESTPORTS(x)		(((x) << 1) & GENMASK(11, 1))

#define SJA1110_META_TSTAMP_SIZE		10

#define SJA1110_HEADER_LEN			4
#define SJA1110_RX_TRAILER_LEN			13
#define SJA1110_TX_TRAILER_LEN			4
#define SJA1110_MAX_PADDING_LEN			15

struct sja1105_tagger_private {
	struct sja1105_tagger_data data;  
	 
	spinlock_t meta_lock;
	struct sk_buff *stampable_skb;
	struct kthread_worker *xmit_worker;
};

static struct sja1105_tagger_private *
sja1105_tagger_private(struct dsa_switch *ds)
{
	return ds->tagger_data;
}

 
static inline bool sja1105_is_link_local(const struct sk_buff *skb)
{
	const struct ethhdr *hdr = eth_hdr(skb);
	u64 dmac = ether_addr_to_u64(hdr->h_dest);

	if (ntohs(hdr->h_proto) == ETH_P_SJA1105_META)
		return false;
	if ((dmac & SJA1105_LINKLOCAL_FILTER_A_MASK) ==
		    SJA1105_LINKLOCAL_FILTER_A)
		return true;
	if ((dmac & SJA1105_LINKLOCAL_FILTER_B_MASK) ==
		    SJA1105_LINKLOCAL_FILTER_B)
		return true;
	return false;
}

struct sja1105_meta {
	u64 tstamp;
	u64 dmac_byte_4;
	u64 dmac_byte_3;
	u64 source_port;
	u64 switch_id;
};

static void sja1105_meta_unpack(const struct sk_buff *skb,
				struct sja1105_meta *meta)
{
	u8 *buf = skb_mac_header(skb) + ETH_HLEN;

	 
	packing(buf,     &meta->tstamp,     31, 0, 4, UNPACK, 0);
	packing(buf + 4, &meta->dmac_byte_3, 7, 0, 1, UNPACK, 0);
	packing(buf + 5, &meta->dmac_byte_4, 7, 0, 1, UNPACK, 0);
	packing(buf + 6, &meta->source_port, 7, 0, 1, UNPACK, 0);
	packing(buf + 7, &meta->switch_id,   7, 0, 1, UNPACK, 0);
}

static inline bool sja1105_is_meta_frame(const struct sk_buff *skb)
{
	const struct ethhdr *hdr = eth_hdr(skb);
	u64 smac = ether_addr_to_u64(hdr->h_source);
	u64 dmac = ether_addr_to_u64(hdr->h_dest);

	if (smac != SJA1105_META_SMAC)
		return false;
	if (dmac != SJA1105_META_DMAC)
		return false;
	if (ntohs(hdr->h_proto) != ETH_P_SJA1105_META)
		return false;
	return true;
}

 
static struct sk_buff *sja1105_defer_xmit(struct dsa_port *dp,
					  struct sk_buff *skb)
{
	struct sja1105_tagger_data *tagger_data = sja1105_tagger_data(dp->ds);
	struct sja1105_tagger_private *priv = sja1105_tagger_private(dp->ds);
	void (*xmit_work_fn)(struct kthread_work *work);
	struct sja1105_deferred_xmit_work *xmit_work;
	struct kthread_worker *xmit_worker;

	xmit_work_fn = tagger_data->xmit_work_fn;
	xmit_worker = priv->xmit_worker;

	if (!xmit_work_fn || !xmit_worker)
		return NULL;

	xmit_work = kzalloc(sizeof(*xmit_work), GFP_ATOMIC);
	if (!xmit_work)
		return NULL;

	kthread_init_work(&xmit_work->work, xmit_work_fn);
	 
	xmit_work->dp = dp;
	xmit_work->skb = skb_get(skb);

	kthread_queue_work(xmit_worker, &xmit_work->work);

	return NULL;
}

 
static u16 sja1105_xmit_tpid(struct dsa_port *dp)
{
	struct dsa_switch *ds = dp->ds;
	struct dsa_port *other_dp;
	u16 proto;

	 
	if (!dsa_port_is_vlan_filtering(dp))
		return ETH_P_SJA1105;

	 
	dsa_switch_for_each_port(other_dp, ds) {
		struct net_device *br = dsa_port_bridge_dev_get(other_dp);

		if (!br)
			continue;

		 
		br_vlan_get_proto(br, &proto);

		return proto;
	}

	WARN_ONCE(1, "Port is VLAN-aware but cannot find associated bridge!\n");

	return ETH_P_SJA1105;
}

static struct sk_buff *sja1105_imprecise_xmit(struct sk_buff *skb,
					      struct net_device *netdev)
{
	struct dsa_port *dp = dsa_slave_to_port(netdev);
	unsigned int bridge_num = dsa_port_bridge_num_get(dp);
	struct net_device *br = dsa_port_bridge_dev_get(dp);
	u16 tx_vid;

	 
	if (br_vlan_enabled(br))
		return skb;

	 
	tx_vid = dsa_tag_8021q_bridge_vid(bridge_num);

	return dsa_8021q_xmit(skb, netdev, sja1105_xmit_tpid(dp), tx_vid);
}

 
static struct sk_buff *sja1105_pvid_tag_control_pkt(struct dsa_port *dp,
						    struct sk_buff *skb, u8 pcp)
{
	__be16 xmit_tpid = htons(sja1105_xmit_tpid(dp));
	struct vlan_ethhdr *hdr;

	 
	if (unlikely(skb_vlan_tag_present(skb))) {
		skb = __vlan_hwaccel_push_inside(skb);
		if (!skb)
			return NULL;
	}

	hdr = skb_vlan_eth_hdr(skb);

	 
	if (hdr->h_vlan_proto == xmit_tpid)
		return skb;

	return vlan_insert_tag(skb, xmit_tpid, (pcp << VLAN_PRIO_SHIFT) |
			       SJA1105_DEFAULT_VLAN);
}

static struct sk_buff *sja1105_xmit(struct sk_buff *skb,
				    struct net_device *netdev)
{
	struct dsa_port *dp = dsa_slave_to_port(netdev);
	u16 queue_mapping = skb_get_queue_mapping(skb);
	u8 pcp = netdev_txq_to_tc(netdev, queue_mapping);
	u16 tx_vid = dsa_tag_8021q_standalone_vid(dp);

	if (skb->offload_fwd_mark)
		return sja1105_imprecise_xmit(skb, netdev);

	 
	if (unlikely(sja1105_is_link_local(skb))) {
		skb = sja1105_pvid_tag_control_pkt(dp, skb, pcp);
		if (!skb)
			return NULL;

		return sja1105_defer_xmit(dp, skb);
	}

	return dsa_8021q_xmit(skb, netdev, sja1105_xmit_tpid(dp),
			     ((pcp << VLAN_PRIO_SHIFT) | tx_vid));
}

static struct sk_buff *sja1110_xmit(struct sk_buff *skb,
				    struct net_device *netdev)
{
	struct sk_buff *clone = SJA1105_SKB_CB(skb)->clone;
	struct dsa_port *dp = dsa_slave_to_port(netdev);
	u16 queue_mapping = skb_get_queue_mapping(skb);
	u8 pcp = netdev_txq_to_tc(netdev, queue_mapping);
	u16 tx_vid = dsa_tag_8021q_standalone_vid(dp);
	__be32 *tx_trailer;
	__be16 *tx_header;
	int trailer_pos;

	if (skb->offload_fwd_mark)
		return sja1105_imprecise_xmit(skb, netdev);

	 
	if (likely(!sja1105_is_link_local(skb)))
		return dsa_8021q_xmit(skb, netdev, sja1105_xmit_tpid(dp),
				     ((pcp << VLAN_PRIO_SHIFT) | tx_vid));

	skb = sja1105_pvid_tag_control_pkt(dp, skb, pcp);
	if (!skb)
		return NULL;

	skb_push(skb, SJA1110_HEADER_LEN);

	dsa_alloc_etype_header(skb, SJA1110_HEADER_LEN);

	trailer_pos = skb->len;

	tx_header = dsa_etype_header_pos_tx(skb);
	tx_trailer = skb_put(skb, SJA1110_TX_TRAILER_LEN);

	tx_header[0] = htons(ETH_P_SJA1110);
	tx_header[1] = htons(SJA1110_HEADER_HOST_TO_SWITCH |
			     SJA1110_TX_HEADER_HAS_TRAILER |
			     SJA1110_TX_HEADER_TRAILER_POS(trailer_pos));
	*tx_trailer = cpu_to_be32(SJA1110_TX_TRAILER_PRIO(pcp) |
				  SJA1110_TX_TRAILER_SWITCHID(dp->ds->index) |
				  SJA1110_TX_TRAILER_DESTPORTS(BIT(dp->index)));
	if (clone) {
		u8 ts_id = SJA1105_SKB_CB(clone)->ts_id;

		tx_header[1] |= htons(SJA1110_TX_HEADER_TAKE_TS);
		*tx_trailer |= cpu_to_be32(SJA1110_TX_TRAILER_TSTAMP_ID(ts_id));
	}

	return skb;
}

static void sja1105_transfer_meta(struct sk_buff *skb,
				  const struct sja1105_meta *meta)
{
	struct ethhdr *hdr = eth_hdr(skb);

	hdr->h_dest[3] = meta->dmac_byte_3;
	hdr->h_dest[4] = meta->dmac_byte_4;
	SJA1105_SKB_CB(skb)->tstamp = meta->tstamp;
}

 
static struct sk_buff
*sja1105_rcv_meta_state_machine(struct sk_buff *skb,
				struct sja1105_meta *meta,
				bool is_link_local,
				bool is_meta)
{
	 
	if (is_link_local) {
		struct dsa_port *dp = dsa_slave_to_port(skb->dev);
		struct sja1105_tagger_private *priv;
		struct dsa_switch *ds = dp->ds;

		priv = sja1105_tagger_private(ds);

		spin_lock(&priv->meta_lock);
		 
		if (priv->stampable_skb) {
			dev_err_ratelimited(ds->dev,
					    "Expected meta frame, is %12llx "
					    "in the DSA master multicast filter?\n",
					    SJA1105_META_DMAC);
			kfree_skb(priv->stampable_skb);
		}

		 
		priv->stampable_skb = skb_get(skb);
		spin_unlock(&priv->meta_lock);

		 
		return NULL;

	 
	} else if (is_meta) {
		struct dsa_port *dp = dsa_slave_to_port(skb->dev);
		struct sja1105_tagger_private *priv;
		struct dsa_switch *ds = dp->ds;
		struct sk_buff *stampable_skb;

		priv = sja1105_tagger_private(ds);

		spin_lock(&priv->meta_lock);

		stampable_skb = priv->stampable_skb;
		priv->stampable_skb = NULL;

		 
		if (!stampable_skb) {
			dev_err_ratelimited(ds->dev,
					    "Unexpected meta frame\n");
			spin_unlock(&priv->meta_lock);
			return NULL;
		}

		if (stampable_skb->dev != skb->dev) {
			dev_err_ratelimited(ds->dev,
					    "Meta frame on wrong port\n");
			spin_unlock(&priv->meta_lock);
			return NULL;
		}

		 
		kfree_skb(skb);
		skb = stampable_skb;
		sja1105_transfer_meta(skb, meta);

		spin_unlock(&priv->meta_lock);
	}

	return skb;
}

static bool sja1105_skb_has_tag_8021q(const struct sk_buff *skb)
{
	u16 tpid = ntohs(eth_hdr(skb)->h_proto);

	return tpid == ETH_P_SJA1105 || tpid == ETH_P_8021Q ||
	       skb_vlan_tag_present(skb);
}

static bool sja1110_skb_has_inband_control_extension(const struct sk_buff *skb)
{
	return ntohs(eth_hdr(skb)->h_proto) == ETH_P_SJA1110;
}

 
static void sja1105_vlan_rcv(struct sk_buff *skb, int *source_port,
			     int *switch_id, int *vbid, u16 *vid)
{
	struct vlan_ethhdr *hdr = vlan_eth_hdr(skb);
	u16 vlan_tci;

	if (skb_vlan_tag_present(skb))
		vlan_tci = skb_vlan_tag_get(skb);
	else
		vlan_tci = ntohs(hdr->h_vlan_TCI);

	if (vid_is_dsa_8021q(vlan_tci & VLAN_VID_MASK))
		return dsa_8021q_rcv(skb, source_port, switch_id, vbid);

	 
	*vid = vlan_tci & VLAN_VID_MASK;
}

static struct sk_buff *sja1105_rcv(struct sk_buff *skb,
				   struct net_device *netdev)
{
	int source_port = -1, switch_id = -1, vbid = -1;
	struct sja1105_meta meta = {0};
	struct ethhdr *hdr;
	bool is_link_local;
	bool is_meta;
	u16 vid;

	hdr = eth_hdr(skb);
	is_link_local = sja1105_is_link_local(skb);
	is_meta = sja1105_is_meta_frame(skb);

	if (is_link_local) {
		 
		source_port = hdr->h_dest[3];
		switch_id = hdr->h_dest[4];
	} else if (is_meta) {
		sja1105_meta_unpack(skb, &meta);
		source_port = meta.source_port;
		switch_id = meta.switch_id;
	}

	 
	if (sja1105_skb_has_tag_8021q(skb)) {
		int tmp_source_port = -1, tmp_switch_id = -1;

		sja1105_vlan_rcv(skb, &tmp_source_port, &tmp_switch_id, &vbid,
				 &vid);
		 
		if (vbid == 0 && source_port == -1)
			source_port = tmp_source_port;
		if (vbid == 0 && switch_id == -1)
			switch_id = tmp_switch_id;
	} else if (source_port == -1 && switch_id == -1) {
		 
		return NULL;
	}

	if (source_port != -1 && switch_id != -1)
		skb->dev = dsa_master_find_slave(netdev, switch_id, source_port);
	else if (vbid >= 1)
		skb->dev = dsa_tag_8021q_find_port_by_vbid(netdev, vbid);
	else
		skb->dev = dsa_find_designated_bridge_port_by_vid(netdev, vid);
	if (!skb->dev) {
		netdev_warn(netdev, "Couldn't decode source port\n");
		return NULL;
	}

	if (!is_link_local)
		dsa_default_offload_fwd_mark(skb);

	return sja1105_rcv_meta_state_machine(skb, &meta, is_link_local,
					      is_meta);
}

static struct sk_buff *sja1110_rcv_meta(struct sk_buff *skb, u16 rx_header)
{
	u8 *buf = dsa_etype_header_pos_rx(skb) + SJA1110_HEADER_LEN;
	int switch_id = SJA1110_RX_HEADER_SWITCH_ID(rx_header);
	int n_ts = SJA1110_RX_HEADER_N_TS(rx_header);
	struct sja1105_tagger_data *tagger_data;
	struct net_device *master = skb->dev;
	struct dsa_port *cpu_dp;
	struct dsa_switch *ds;
	int i;

	cpu_dp = master->dsa_ptr;
	ds = dsa_switch_find(cpu_dp->dst->index, switch_id);
	if (!ds) {
		net_err_ratelimited("%s: cannot find switch id %d\n",
				    master->name, switch_id);
		return NULL;
	}

	tagger_data = sja1105_tagger_data(ds);
	if (!tagger_data->meta_tstamp_handler)
		return NULL;

	for (i = 0; i <= n_ts; i++) {
		u8 ts_id, source_port, dir;
		u64 tstamp;

		ts_id = buf[0];
		source_port = (buf[1] & GENMASK(7, 4)) >> 4;
		dir = (buf[1] & BIT(3)) >> 3;
		tstamp = be64_to_cpu(*(__be64 *)(buf + 2));

		tagger_data->meta_tstamp_handler(ds, source_port, ts_id, dir,
						 tstamp);

		buf += SJA1110_META_TSTAMP_SIZE;
	}

	 
	return NULL;
}

static struct sk_buff *sja1110_rcv_inband_control_extension(struct sk_buff *skb,
							    int *source_port,
							    int *switch_id,
							    bool *host_only)
{
	u16 rx_header;

	if (unlikely(!pskb_may_pull(skb, SJA1110_HEADER_LEN)))
		return NULL;

	 
	rx_header = ntohs(*(__be16 *)skb->data);

	if (rx_header & SJA1110_RX_HEADER_HOST_ONLY)
		*host_only = true;

	if (rx_header & SJA1110_RX_HEADER_IS_METADATA)
		return sja1110_rcv_meta(skb, rx_header);

	 
	if (rx_header & SJA1110_RX_HEADER_HAS_TRAILER) {
		int start_of_padding = SJA1110_RX_HEADER_TRAILER_POS(rx_header);
		u8 *rx_trailer = skb_tail_pointer(skb) - SJA1110_RX_TRAILER_LEN;
		u64 *tstamp = &SJA1105_SKB_CB(skb)->tstamp;
		u8 last_byte = rx_trailer[12];

		 
		packing(rx_trailer, tstamp, 63, 0, 8, UNPACK, 0);

		*source_port = SJA1110_RX_TRAILER_SRC_PORT(last_byte);
		*switch_id = SJA1110_RX_TRAILER_SWITCH_ID(last_byte);

		 
		if (pskb_trim_rcsum(skb, start_of_padding - ETH_HLEN))
			return NULL;
	 
	} else {
		*source_port = SJA1110_RX_HEADER_SRC_PORT(rx_header);
		*switch_id = SJA1110_RX_HEADER_SWITCH_ID(rx_header);
	}

	 
	skb_pull_rcsum(skb, SJA1110_HEADER_LEN);

	dsa_strip_etype_header(skb, SJA1110_HEADER_LEN);

	 
	skb_set_mac_header(skb, -ETH_HLEN);

	return skb;
}

static struct sk_buff *sja1110_rcv(struct sk_buff *skb,
				   struct net_device *netdev)
{
	int source_port = -1, switch_id = -1, vbid = -1;
	bool host_only = false;
	u16 vid = 0;

	if (sja1110_skb_has_inband_control_extension(skb)) {
		skb = sja1110_rcv_inband_control_extension(skb, &source_port,
							   &switch_id,
							   &host_only);
		if (!skb)
			return NULL;
	}

	 
	if (likely(sja1105_skb_has_tag_8021q(skb)))
		sja1105_vlan_rcv(skb, &source_port, &switch_id, &vbid, &vid);

	if (vbid >= 1)
		skb->dev = dsa_tag_8021q_find_port_by_vbid(netdev, vbid);
	else if (source_port == -1 || switch_id == -1)
		skb->dev = dsa_find_designated_bridge_port_by_vid(netdev, vid);
	else
		skb->dev = dsa_master_find_slave(netdev, switch_id, source_port);
	if (!skb->dev) {
		netdev_warn(netdev, "Couldn't decode source port\n");
		return NULL;
	}

	if (!host_only)
		dsa_default_offload_fwd_mark(skb);

	return skb;
}

static void sja1105_flow_dissect(const struct sk_buff *skb, __be16 *proto,
				 int *offset)
{
	 
	if (unlikely(sja1105_is_link_local(skb)))
		return;

	dsa_tag_generic_flow_dissect(skb, proto, offset);
}

static void sja1110_flow_dissect(const struct sk_buff *skb, __be16 *proto,
				 int *offset)
{
	 
	if (unlikely(sja1105_is_link_local(skb)))
		return dsa_tag_generic_flow_dissect(skb, proto, offset);

	 
	*offset = VLAN_HLEN;
	*proto = ((__be16 *)skb->data)[(VLAN_HLEN / 2) - 1];
}

static void sja1105_disconnect(struct dsa_switch *ds)
{
	struct sja1105_tagger_private *priv = ds->tagger_data;

	kthread_destroy_worker(priv->xmit_worker);
	kfree(priv);
	ds->tagger_data = NULL;
}

static int sja1105_connect(struct dsa_switch *ds)
{
	struct sja1105_tagger_private *priv;
	struct kthread_worker *xmit_worker;
	int err;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	spin_lock_init(&priv->meta_lock);

	xmit_worker = kthread_create_worker(0, "dsa%d:%d_xmit",
					    ds->dst->index, ds->index);
	if (IS_ERR(xmit_worker)) {
		err = PTR_ERR(xmit_worker);
		kfree(priv);
		return err;
	}

	priv->xmit_worker = xmit_worker;
	ds->tagger_data = priv;

	return 0;
}

static const struct dsa_device_ops sja1105_netdev_ops = {
	.name = SJA1105_NAME,
	.proto = DSA_TAG_PROTO_SJA1105,
	.xmit = sja1105_xmit,
	.rcv = sja1105_rcv,
	.connect = sja1105_connect,
	.disconnect = sja1105_disconnect,
	.needed_headroom = VLAN_HLEN,
	.flow_dissect = sja1105_flow_dissect,
	.promisc_on_master = true,
};

DSA_TAG_DRIVER(sja1105_netdev_ops);
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_SJA1105, SJA1105_NAME);

static const struct dsa_device_ops sja1110_netdev_ops = {
	.name = SJA1110_NAME,
	.proto = DSA_TAG_PROTO_SJA1110,
	.xmit = sja1110_xmit,
	.rcv = sja1110_rcv,
	.connect = sja1105_connect,
	.disconnect = sja1105_disconnect,
	.flow_dissect = sja1110_flow_dissect,
	.needed_headroom = SJA1110_HEADER_LEN + VLAN_HLEN,
	.needed_tailroom = SJA1110_RX_TRAILER_LEN + SJA1110_MAX_PADDING_LEN,
};

DSA_TAG_DRIVER(sja1110_netdev_ops);
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_SJA1110, SJA1110_NAME);

static struct dsa_tag_driver *sja1105_tag_driver_array[] = {
	&DSA_TAG_DRIVER_NAME(sja1105_netdev_ops),
	&DSA_TAG_DRIVER_NAME(sja1110_netdev_ops),
};

module_dsa_tag_drivers(sja1105_tag_driver_array);

MODULE_LICENSE("GPL v2");
