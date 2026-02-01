
 

#include <linux/prefetch.h>
#include <linux/bpf_trace.h>
#include <net/mpls.h>
#include <net/xdp.h>
#include "i40e.h"
#include "i40e_trace.h"
#include "i40e_prototype.h"
#include "i40e_txrx_common.h"
#include "i40e_xsk.h"

#define I40E_TXD_CMD (I40E_TX_DESC_CMD_EOP | I40E_TX_DESC_CMD_RS)
 
static void i40e_fdir(struct i40e_ring *tx_ring,
		      struct i40e_fdir_filter *fdata, bool add)
{
	struct i40e_filter_program_desc *fdir_desc;
	struct i40e_pf *pf = tx_ring->vsi->back;
	u32 flex_ptype, dtype_cmd;
	u16 i;

	 
	i = tx_ring->next_to_use;
	fdir_desc = I40E_TX_FDIRDESC(tx_ring, i);

	i++;
	tx_ring->next_to_use = (i < tx_ring->count) ? i : 0;

	flex_ptype = I40E_TXD_FLTR_QW0_QINDEX_MASK &
		     (fdata->q_index << I40E_TXD_FLTR_QW0_QINDEX_SHIFT);

	flex_ptype |= I40E_TXD_FLTR_QW0_FLEXOFF_MASK &
		      (fdata->flex_off << I40E_TXD_FLTR_QW0_FLEXOFF_SHIFT);

	flex_ptype |= I40E_TXD_FLTR_QW0_PCTYPE_MASK &
		      (fdata->pctype << I40E_TXD_FLTR_QW0_PCTYPE_SHIFT);

	 
	flex_ptype |= I40E_TXD_FLTR_QW0_DEST_VSI_MASK &
		      ((u32)(fdata->dest_vsi ? : pf->vsi[pf->lan_vsi]->id) <<
		       I40E_TXD_FLTR_QW0_DEST_VSI_SHIFT);

	dtype_cmd = I40E_TX_DESC_DTYPE_FILTER_PROG;

	dtype_cmd |= add ?
		     I40E_FILTER_PROGRAM_DESC_PCMD_ADD_UPDATE <<
		     I40E_TXD_FLTR_QW1_PCMD_SHIFT :
		     I40E_FILTER_PROGRAM_DESC_PCMD_REMOVE <<
		     I40E_TXD_FLTR_QW1_PCMD_SHIFT;

	dtype_cmd |= I40E_TXD_FLTR_QW1_DEST_MASK &
		     (fdata->dest_ctl << I40E_TXD_FLTR_QW1_DEST_SHIFT);

	dtype_cmd |= I40E_TXD_FLTR_QW1_FD_STATUS_MASK &
		     (fdata->fd_status << I40E_TXD_FLTR_QW1_FD_STATUS_SHIFT);

	if (fdata->cnt_index) {
		dtype_cmd |= I40E_TXD_FLTR_QW1_CNT_ENA_MASK;
		dtype_cmd |= I40E_TXD_FLTR_QW1_CNTINDEX_MASK &
			     ((u32)fdata->cnt_index <<
			      I40E_TXD_FLTR_QW1_CNTINDEX_SHIFT);
	}

	fdir_desc->qindex_flex_ptype_vsi = cpu_to_le32(flex_ptype);
	fdir_desc->rsvd = cpu_to_le32(0);
	fdir_desc->dtype_cmd_cntindex = cpu_to_le32(dtype_cmd);
	fdir_desc->fd_id = cpu_to_le32(fdata->fd_id);
}

#define I40E_FD_CLEAN_DELAY 10
 
static int i40e_program_fdir_filter(struct i40e_fdir_filter *fdir_data,
				    u8 *raw_packet, struct i40e_pf *pf,
				    bool add)
{
	struct i40e_tx_buffer *tx_buf, *first;
	struct i40e_tx_desc *tx_desc;
	struct i40e_ring *tx_ring;
	struct i40e_vsi *vsi;
	struct device *dev;
	dma_addr_t dma;
	u32 td_cmd = 0;
	u16 i;

	 
	vsi = i40e_find_vsi_by_type(pf, I40E_VSI_FDIR);
	if (!vsi)
		return -ENOENT;

	tx_ring = vsi->tx_rings[0];
	dev = tx_ring->dev;

	 
	for (i = I40E_FD_CLEAN_DELAY; I40E_DESC_UNUSED(tx_ring) < 2; i--) {
		if (!i)
			return -EAGAIN;
		msleep_interruptible(1);
	}

	dma = dma_map_single(dev, raw_packet,
			     I40E_FDIR_MAX_RAW_PACKET_SIZE, DMA_TO_DEVICE);
	if (dma_mapping_error(dev, dma))
		goto dma_fail;

	 
	i = tx_ring->next_to_use;
	first = &tx_ring->tx_bi[i];
	i40e_fdir(tx_ring, fdir_data, add);

	 
	i = tx_ring->next_to_use;
	tx_desc = I40E_TX_DESC(tx_ring, i);
	tx_buf = &tx_ring->tx_bi[i];

	tx_ring->next_to_use = ((i + 1) < tx_ring->count) ? i + 1 : 0;

	memset(tx_buf, 0, sizeof(struct i40e_tx_buffer));

	 
	dma_unmap_len_set(tx_buf, len, I40E_FDIR_MAX_RAW_PACKET_SIZE);
	dma_unmap_addr_set(tx_buf, dma, dma);

	tx_desc->buffer_addr = cpu_to_le64(dma);
	td_cmd = I40E_TXD_CMD | I40E_TX_DESC_CMD_DUMMY;

	tx_buf->tx_flags = I40E_TX_FLAGS_FD_SB;
	tx_buf->raw_buf = (void *)raw_packet;

	tx_desc->cmd_type_offset_bsz =
		build_ctob(td_cmd, 0, I40E_FDIR_MAX_RAW_PACKET_SIZE, 0);

	 
	wmb();

	 
	first->next_to_watch = tx_desc;

	writel(tx_ring->next_to_use, tx_ring->tail);
	return 0;

dma_fail:
	return -1;
}

 
static char *i40e_create_dummy_packet(u8 *dummy_packet, bool ipv4, u8 l4proto,
				      struct i40e_fdir_filter *data)
{
	bool is_vlan = !!data->vlan_tag;
	struct vlan_hdr vlan = {};
	struct ipv6hdr ipv6 = {};
	struct ethhdr eth = {};
	struct iphdr ip = {};
	u8 *tmp;

	if (ipv4) {
		eth.h_proto = cpu_to_be16(ETH_P_IP);
		ip.protocol = l4proto;
		ip.version = 0x4;
		ip.ihl = 0x5;

		ip.daddr = data->dst_ip;
		ip.saddr = data->src_ip;
	} else {
		eth.h_proto = cpu_to_be16(ETH_P_IPV6);
		ipv6.nexthdr = l4proto;
		ipv6.version = 0x6;

		memcpy(&ipv6.saddr.in6_u.u6_addr32, data->src_ip6,
		       sizeof(__be32) * 4);
		memcpy(&ipv6.daddr.in6_u.u6_addr32, data->dst_ip6,
		       sizeof(__be32) * 4);
	}

	if (is_vlan) {
		vlan.h_vlan_TCI = data->vlan_tag;
		vlan.h_vlan_encapsulated_proto = eth.h_proto;
		eth.h_proto = data->vlan_etype;
	}

	tmp = dummy_packet;
	memcpy(tmp, &eth, sizeof(eth));
	tmp += sizeof(eth);

	if (is_vlan) {
		memcpy(tmp, &vlan, sizeof(vlan));
		tmp += sizeof(vlan);
	}

	if (ipv4) {
		memcpy(tmp, &ip, sizeof(ip));
		tmp += sizeof(ip);
	} else {
		memcpy(tmp, &ipv6, sizeof(ipv6));
		tmp += sizeof(ipv6);
	}

	return tmp;
}

 
static void i40e_create_dummy_udp_packet(u8 *raw_packet, bool ipv4, u8 l4proto,
					 struct i40e_fdir_filter *data)
{
	struct udphdr *udp;
	u8 *tmp;

	tmp = i40e_create_dummy_packet(raw_packet, ipv4, IPPROTO_UDP, data);
	udp = (struct udphdr *)(tmp);
	udp->dest = data->dst_port;
	udp->source = data->src_port;
}

 
static void i40e_create_dummy_tcp_packet(u8 *raw_packet, bool ipv4, u8 l4proto,
					 struct i40e_fdir_filter *data)
{
	struct tcphdr *tcp;
	u8 *tmp;
	 
	static const char tcp_packet[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0x50, 0x11, 0x0, 0x72, 0, 0, 0, 0};

	tmp = i40e_create_dummy_packet(raw_packet, ipv4, IPPROTO_TCP, data);

	tcp = (struct tcphdr *)tmp;
	memcpy(tcp, tcp_packet, sizeof(tcp_packet));
	tcp->dest = data->dst_port;
	tcp->source = data->src_port;
}

 
static void i40e_create_dummy_sctp_packet(u8 *raw_packet, bool ipv4,
					  u8 l4proto,
					  struct i40e_fdir_filter *data)
{
	struct sctphdr *sctp;
	u8 *tmp;

	tmp = i40e_create_dummy_packet(raw_packet, ipv4, IPPROTO_SCTP, data);

	sctp = (struct sctphdr *)tmp;
	sctp->dest = data->dst_port;
	sctp->source = data->src_port;
}

 
static int i40e_prepare_fdir_filter(struct i40e_pf *pf,
				    struct i40e_fdir_filter *fd_data,
				    bool add, char *packet_addr,
				    int payload_offset, u8 pctype)
{
	int ret;

	if (fd_data->flex_filter) {
		u8 *payload;
		__be16 pattern = fd_data->flex_word;
		u16 off = fd_data->flex_offset;

		payload = packet_addr + payload_offset;

		 
		if (!!fd_data->vlan_tag)
			payload += VLAN_HLEN;

		*((__force __be16 *)(payload + off)) = pattern;
	}

	fd_data->pctype = pctype;
	ret = i40e_program_fdir_filter(fd_data, packet_addr, pf, add);
	if (ret) {
		dev_info(&pf->pdev->dev,
			 "PCTYPE:%d, Filter command send failed for fd_id:%d (ret = %d)\n",
			 fd_data->pctype, fd_data->fd_id, ret);
		 
		return -EOPNOTSUPP;
	} else if (I40E_DEBUG_FD & pf->hw.debug_mask) {
		if (add)
			dev_info(&pf->pdev->dev,
				 "Filter OK for PCTYPE %d loc = %d\n",
				 fd_data->pctype, fd_data->fd_id);
		else
			dev_info(&pf->pdev->dev,
				 "Filter deleted for PCTYPE %d loc = %d\n",
				 fd_data->pctype, fd_data->fd_id);
	}

	return ret;
}

 
static void i40e_change_filter_num(bool ipv4, bool add, u16 *ipv4_filter_num,
				   u16 *ipv6_filter_num)
{
	if (add) {
		if (ipv4)
			(*ipv4_filter_num)++;
		else
			(*ipv6_filter_num)++;
	} else {
		if (ipv4)
			(*ipv4_filter_num)--;
		else
			(*ipv6_filter_num)--;
	}
}

#define I40E_UDPIP_DUMMY_PACKET_LEN	42
#define I40E_UDPIP6_DUMMY_PACKET_LEN	62
 
static int i40e_add_del_fdir_udp(struct i40e_vsi *vsi,
				 struct i40e_fdir_filter *fd_data,
				 bool add,
				 bool ipv4)
{
	struct i40e_pf *pf = vsi->back;
	u8 *raw_packet;
	int ret;

	raw_packet = kzalloc(I40E_FDIR_MAX_RAW_PACKET_SIZE, GFP_KERNEL);
	if (!raw_packet)
		return -ENOMEM;

	i40e_create_dummy_udp_packet(raw_packet, ipv4, IPPROTO_UDP, fd_data);

	if (ipv4)
		ret = i40e_prepare_fdir_filter
			(pf, fd_data, add, raw_packet,
			 I40E_UDPIP_DUMMY_PACKET_LEN,
			 I40E_FILTER_PCTYPE_NONF_IPV4_UDP);
	else
		ret = i40e_prepare_fdir_filter
			(pf, fd_data, add, raw_packet,
			 I40E_UDPIP6_DUMMY_PACKET_LEN,
			 I40E_FILTER_PCTYPE_NONF_IPV6_UDP);

	if (ret) {
		kfree(raw_packet);
		return ret;
	}

	i40e_change_filter_num(ipv4, add, &pf->fd_udp4_filter_cnt,
			       &pf->fd_udp6_filter_cnt);

	return 0;
}

#define I40E_TCPIP_DUMMY_PACKET_LEN	54
#define I40E_TCPIP6_DUMMY_PACKET_LEN	74
 
static int i40e_add_del_fdir_tcp(struct i40e_vsi *vsi,
				 struct i40e_fdir_filter *fd_data,
				 bool add,
				 bool ipv4)
{
	struct i40e_pf *pf = vsi->back;
	u8 *raw_packet;
	int ret;

	raw_packet = kzalloc(I40E_FDIR_MAX_RAW_PACKET_SIZE, GFP_KERNEL);
	if (!raw_packet)
		return -ENOMEM;

	i40e_create_dummy_tcp_packet(raw_packet, ipv4, IPPROTO_TCP, fd_data);
	if (ipv4)
		ret = i40e_prepare_fdir_filter
			(pf, fd_data, add, raw_packet,
			 I40E_TCPIP_DUMMY_PACKET_LEN,
			 I40E_FILTER_PCTYPE_NONF_IPV4_TCP);
	else
		ret = i40e_prepare_fdir_filter
			(pf, fd_data, add, raw_packet,
			 I40E_TCPIP6_DUMMY_PACKET_LEN,
			 I40E_FILTER_PCTYPE_NONF_IPV6_TCP);

	if (ret) {
		kfree(raw_packet);
		return ret;
	}

	i40e_change_filter_num(ipv4, add, &pf->fd_tcp4_filter_cnt,
			       &pf->fd_tcp6_filter_cnt);

	if (add) {
		if ((pf->flags & I40E_FLAG_FD_ATR_ENABLED) &&
		    I40E_DEBUG_FD & pf->hw.debug_mask)
			dev_info(&pf->pdev->dev, "Forcing ATR off, sideband rules for TCP/IPv4 flow being applied\n");
		set_bit(__I40E_FD_ATR_AUTO_DISABLED, pf->state);
	}
	return 0;
}

#define I40E_SCTPIP_DUMMY_PACKET_LEN	46
#define I40E_SCTPIP6_DUMMY_PACKET_LEN	66
 
static int i40e_add_del_fdir_sctp(struct i40e_vsi *vsi,
				  struct i40e_fdir_filter *fd_data,
				  bool add,
				  bool ipv4)
{
	struct i40e_pf *pf = vsi->back;
	u8 *raw_packet;
	int ret;

	raw_packet = kzalloc(I40E_FDIR_MAX_RAW_PACKET_SIZE, GFP_KERNEL);
	if (!raw_packet)
		return -ENOMEM;

	i40e_create_dummy_sctp_packet(raw_packet, ipv4, IPPROTO_SCTP, fd_data);

	if (ipv4)
		ret = i40e_prepare_fdir_filter
			(pf, fd_data, add, raw_packet,
			 I40E_SCTPIP_DUMMY_PACKET_LEN,
			 I40E_FILTER_PCTYPE_NONF_IPV4_SCTP);
	else
		ret = i40e_prepare_fdir_filter
			(pf, fd_data, add, raw_packet,
			 I40E_SCTPIP6_DUMMY_PACKET_LEN,
			 I40E_FILTER_PCTYPE_NONF_IPV6_SCTP);

	if (ret) {
		kfree(raw_packet);
		return ret;
	}

	i40e_change_filter_num(ipv4, add, &pf->fd_sctp4_filter_cnt,
			       &pf->fd_sctp6_filter_cnt);

	return 0;
}

#define I40E_IP_DUMMY_PACKET_LEN	34
#define I40E_IP6_DUMMY_PACKET_LEN	54
 
static int i40e_add_del_fdir_ip(struct i40e_vsi *vsi,
				struct i40e_fdir_filter *fd_data,
				bool add,
				bool ipv4)
{
	struct i40e_pf *pf = vsi->back;
	int payload_offset;
	u8 *raw_packet;
	int iter_start;
	int iter_end;
	int ret;
	int i;

	if (ipv4) {
		iter_start = I40E_FILTER_PCTYPE_NONF_IPV4_OTHER;
		iter_end = I40E_FILTER_PCTYPE_FRAG_IPV4;
	} else {
		iter_start = I40E_FILTER_PCTYPE_NONF_IPV6_OTHER;
		iter_end = I40E_FILTER_PCTYPE_FRAG_IPV6;
	}

	for (i = iter_start; i <= iter_end; i++) {
		raw_packet = kzalloc(I40E_FDIR_MAX_RAW_PACKET_SIZE, GFP_KERNEL);
		if (!raw_packet)
			return -ENOMEM;

		 
		(void)i40e_create_dummy_packet
			(raw_packet, ipv4, (ipv4) ? IPPROTO_IP : IPPROTO_NONE,
			 fd_data);

		payload_offset = (ipv4) ? I40E_IP_DUMMY_PACKET_LEN :
			I40E_IP6_DUMMY_PACKET_LEN;
		ret = i40e_prepare_fdir_filter(pf, fd_data, add, raw_packet,
					       payload_offset, i);
		if (ret)
			goto err;
	}

	i40e_change_filter_num(ipv4, add, &pf->fd_ip4_filter_cnt,
			       &pf->fd_ip6_filter_cnt);

	return 0;
err:
	kfree(raw_packet);
	return ret;
}

 
int i40e_add_del_fdir(struct i40e_vsi *vsi,
		      struct i40e_fdir_filter *input, bool add)
{
	enum ip_ver { ipv6 = 0, ipv4 = 1 };
	struct i40e_pf *pf = vsi->back;
	int ret;

	switch (input->flow_type & ~FLOW_EXT) {
	case TCP_V4_FLOW:
		ret = i40e_add_del_fdir_tcp(vsi, input, add, ipv4);
		break;
	case UDP_V4_FLOW:
		ret = i40e_add_del_fdir_udp(vsi, input, add, ipv4);
		break;
	case SCTP_V4_FLOW:
		ret = i40e_add_del_fdir_sctp(vsi, input, add, ipv4);
		break;
	case TCP_V6_FLOW:
		ret = i40e_add_del_fdir_tcp(vsi, input, add, ipv6);
		break;
	case UDP_V6_FLOW:
		ret = i40e_add_del_fdir_udp(vsi, input, add, ipv6);
		break;
	case SCTP_V6_FLOW:
		ret = i40e_add_del_fdir_sctp(vsi, input, add, ipv6);
		break;
	case IP_USER_FLOW:
		switch (input->ipl4_proto) {
		case IPPROTO_TCP:
			ret = i40e_add_del_fdir_tcp(vsi, input, add, ipv4);
			break;
		case IPPROTO_UDP:
			ret = i40e_add_del_fdir_udp(vsi, input, add, ipv4);
			break;
		case IPPROTO_SCTP:
			ret = i40e_add_del_fdir_sctp(vsi, input, add, ipv4);
			break;
		case IPPROTO_IP:
			ret = i40e_add_del_fdir_ip(vsi, input, add, ipv4);
			break;
		default:
			 
			dev_info(&pf->pdev->dev, "Unsupported IPv4 protocol 0x%02x\n",
				 input->ipl4_proto);
			return -EINVAL;
		}
		break;
	case IPV6_USER_FLOW:
		switch (input->ipl4_proto) {
		case IPPROTO_TCP:
			ret = i40e_add_del_fdir_tcp(vsi, input, add, ipv6);
			break;
		case IPPROTO_UDP:
			ret = i40e_add_del_fdir_udp(vsi, input, add, ipv6);
			break;
		case IPPROTO_SCTP:
			ret = i40e_add_del_fdir_sctp(vsi, input, add, ipv6);
			break;
		case IPPROTO_IP:
			ret = i40e_add_del_fdir_ip(vsi, input, add, ipv6);
			break;
		default:
			 
			dev_info(&pf->pdev->dev, "Unsupported IPv6 protocol 0x%02x\n",
				 input->ipl4_proto);
			return -EINVAL;
		}
		break;
	default:
		dev_info(&pf->pdev->dev, "Unsupported flow type 0x%02x\n",
			 input->flow_type);
		return -EINVAL;
	}

	 
	return ret;
}

 
static void i40e_fd_handle_status(struct i40e_ring *rx_ring, u64 qword0_raw,
				  u64 qword1, u8 prog_id)
{
	struct i40e_pf *pf = rx_ring->vsi->back;
	struct pci_dev *pdev = pf->pdev;
	struct i40e_16b_rx_wb_qw0 *qw0;
	u32 fcnt_prog, fcnt_avail;
	u32 error;

	qw0 = (struct i40e_16b_rx_wb_qw0 *)&qword0_raw;
	error = (qword1 & I40E_RX_PROG_STATUS_DESC_QW1_ERROR_MASK) >>
		I40E_RX_PROG_STATUS_DESC_QW1_ERROR_SHIFT;

	if (error == BIT(I40E_RX_PROG_STATUS_DESC_FD_TBL_FULL_SHIFT)) {
		pf->fd_inv = le32_to_cpu(qw0->hi_dword.fd_id);
		if (qw0->hi_dword.fd_id != 0 ||
		    (I40E_DEBUG_FD & pf->hw.debug_mask))
			dev_warn(&pdev->dev, "ntuple filter loc = %d, could not be added\n",
				 pf->fd_inv);

		 
		if (test_bit(__I40E_FD_FLUSH_REQUESTED, pf->state))
			return;

		pf->fd_add_err++;
		 
		pf->fd_atr_cnt = i40e_get_current_atr_cnt(pf);

		if (qw0->hi_dword.fd_id == 0 &&
		    test_bit(__I40E_FD_SB_AUTO_DISABLED, pf->state)) {
			 
			set_bit(__I40E_FD_ATR_AUTO_DISABLED, pf->state);
			set_bit(__I40E_FD_FLUSH_REQUESTED, pf->state);
		}

		 
		fcnt_prog = i40e_get_global_fd_count(pf);
		fcnt_avail = pf->fdir_pf_filter_count;
		 
		if (fcnt_prog >= (fcnt_avail - I40E_FDIR_BUFFER_FULL_MARGIN)) {
			if ((pf->flags & I40E_FLAG_FD_SB_ENABLED) &&
			    !test_and_set_bit(__I40E_FD_SB_AUTO_DISABLED,
					      pf->state))
				if (I40E_DEBUG_FD & pf->hw.debug_mask)
					dev_warn(&pdev->dev, "FD filter space full, new ntuple rules will not be added\n");
		}
	} else if (error == BIT(I40E_RX_PROG_STATUS_DESC_NO_FD_ENTRY_SHIFT)) {
		if (I40E_DEBUG_FD & pf->hw.debug_mask)
			dev_info(&pdev->dev, "ntuple filter fd_id = %d, could not be removed\n",
				 qw0->hi_dword.fd_id);
	}
}

 
static void i40e_unmap_and_free_tx_resource(struct i40e_ring *ring,
					    struct i40e_tx_buffer *tx_buffer)
{
	if (tx_buffer->skb) {
		if (tx_buffer->tx_flags & I40E_TX_FLAGS_FD_SB)
			kfree(tx_buffer->raw_buf);
		else if (ring_is_xdp(ring))
			xdp_return_frame(tx_buffer->xdpf);
		else
			dev_kfree_skb_any(tx_buffer->skb);
		if (dma_unmap_len(tx_buffer, len))
			dma_unmap_single(ring->dev,
					 dma_unmap_addr(tx_buffer, dma),
					 dma_unmap_len(tx_buffer, len),
					 DMA_TO_DEVICE);
	} else if (dma_unmap_len(tx_buffer, len)) {
		dma_unmap_page(ring->dev,
			       dma_unmap_addr(tx_buffer, dma),
			       dma_unmap_len(tx_buffer, len),
			       DMA_TO_DEVICE);
	}

	tx_buffer->next_to_watch = NULL;
	tx_buffer->skb = NULL;
	dma_unmap_len_set(tx_buffer, len, 0);
	 
}

 
void i40e_clean_tx_ring(struct i40e_ring *tx_ring)
{
	unsigned long bi_size;
	u16 i;

	if (ring_is_xdp(tx_ring) && tx_ring->xsk_pool) {
		i40e_xsk_clean_tx_ring(tx_ring);
	} else {
		 
		if (!tx_ring->tx_bi)
			return;

		 
		for (i = 0; i < tx_ring->count; i++)
			i40e_unmap_and_free_tx_resource(tx_ring,
							&tx_ring->tx_bi[i]);
	}

	bi_size = sizeof(struct i40e_tx_buffer) * tx_ring->count;
	memset(tx_ring->tx_bi, 0, bi_size);

	 
	memset(tx_ring->desc, 0, tx_ring->size);

	tx_ring->next_to_use = 0;
	tx_ring->next_to_clean = 0;

	if (!tx_ring->netdev)
		return;

	 
	netdev_tx_reset_queue(txring_txq(tx_ring));
}

 
void i40e_free_tx_resources(struct i40e_ring *tx_ring)
{
	i40e_clean_tx_ring(tx_ring);
	kfree(tx_ring->tx_bi);
	tx_ring->tx_bi = NULL;

	if (tx_ring->desc) {
		dma_free_coherent(tx_ring->dev, tx_ring->size,
				  tx_ring->desc, tx_ring->dma);
		tx_ring->desc = NULL;
	}
}

 
u32 i40e_get_tx_pending(struct i40e_ring *ring, bool in_sw)
{
	u32 head, tail;

	if (!in_sw) {
		head = i40e_get_head(ring);
		tail = readl(ring->tail);
	} else {
		head = ring->next_to_clean;
		tail = ring->next_to_use;
	}

	if (head != tail)
		return (head < tail) ?
			tail - head : (tail + ring->count - head);

	return 0;
}

 
void i40e_detect_recover_hung(struct i40e_vsi *vsi)
{
	struct i40e_ring *tx_ring = NULL;
	struct net_device *netdev;
	unsigned int i;
	int packets;

	if (!vsi)
		return;

	if (test_bit(__I40E_VSI_DOWN, vsi->state))
		return;

	netdev = vsi->netdev;
	if (!netdev)
		return;

	if (!netif_carrier_ok(netdev))
		return;

	for (i = 0; i < vsi->num_queue_pairs; i++) {
		tx_ring = vsi->tx_rings[i];
		if (tx_ring && tx_ring->desc) {
			 
			packets = tx_ring->stats.packets & INT_MAX;
			if (tx_ring->tx_stats.prev_pkt_ctr == packets) {
				i40e_force_wb(vsi, tx_ring->q_vector);
				continue;
			}

			 
			smp_rmb();
			tx_ring->tx_stats.prev_pkt_ctr =
			    i40e_get_tx_pending(tx_ring, true) ? packets : -1;
		}
	}
}

 
static bool i40e_clean_tx_irq(struct i40e_vsi *vsi,
			      struct i40e_ring *tx_ring, int napi_budget,
			      unsigned int *tx_cleaned)
{
	int i = tx_ring->next_to_clean;
	struct i40e_tx_buffer *tx_buf;
	struct i40e_tx_desc *tx_head;
	struct i40e_tx_desc *tx_desc;
	unsigned int total_bytes = 0, total_packets = 0;
	unsigned int budget = vsi->work_limit;

	tx_buf = &tx_ring->tx_bi[i];
	tx_desc = I40E_TX_DESC(tx_ring, i);
	i -= tx_ring->count;

	tx_head = I40E_TX_DESC(tx_ring, i40e_get_head(tx_ring));

	do {
		struct i40e_tx_desc *eop_desc = tx_buf->next_to_watch;

		 
		if (!eop_desc)
			break;

		 
		smp_rmb();

		i40e_trace(clean_tx_irq, tx_ring, tx_desc, tx_buf);
		 
		if (tx_head == tx_desc)
			break;

		 
		tx_buf->next_to_watch = NULL;

		 
		total_bytes += tx_buf->bytecount;
		total_packets += tx_buf->gso_segs;

		 
		if (ring_is_xdp(tx_ring))
			xdp_return_frame(tx_buf->xdpf);
		else
			napi_consume_skb(tx_buf->skb, napi_budget);

		 
		dma_unmap_single(tx_ring->dev,
				 dma_unmap_addr(tx_buf, dma),
				 dma_unmap_len(tx_buf, len),
				 DMA_TO_DEVICE);

		 
		tx_buf->skb = NULL;
		dma_unmap_len_set(tx_buf, len, 0);

		 
		while (tx_desc != eop_desc) {
			i40e_trace(clean_tx_irq_unmap,
				   tx_ring, tx_desc, tx_buf);

			tx_buf++;
			tx_desc++;
			i++;
			if (unlikely(!i)) {
				i -= tx_ring->count;
				tx_buf = tx_ring->tx_bi;
				tx_desc = I40E_TX_DESC(tx_ring, 0);
			}

			 
			if (dma_unmap_len(tx_buf, len)) {
				dma_unmap_page(tx_ring->dev,
					       dma_unmap_addr(tx_buf, dma),
					       dma_unmap_len(tx_buf, len),
					       DMA_TO_DEVICE);
				dma_unmap_len_set(tx_buf, len, 0);
			}
		}

		 
		tx_buf++;
		tx_desc++;
		i++;
		if (unlikely(!i)) {
			i -= tx_ring->count;
			tx_buf = tx_ring->tx_bi;
			tx_desc = I40E_TX_DESC(tx_ring, 0);
		}

		prefetch(tx_desc);

		 
		budget--;
	} while (likely(budget));

	i += tx_ring->count;
	tx_ring->next_to_clean = i;
	i40e_update_tx_stats(tx_ring, total_packets, total_bytes);
	i40e_arm_wb(tx_ring, vsi, budget);

	if (ring_is_xdp(tx_ring))
		return !!budget;

	 
	netdev_tx_completed_queue(txring_txq(tx_ring),
				  total_packets, total_bytes);

#define TX_WAKE_THRESHOLD ((s16)(DESC_NEEDED * 2))
	if (unlikely(total_packets && netif_carrier_ok(tx_ring->netdev) &&
		     (I40E_DESC_UNUSED(tx_ring) >= TX_WAKE_THRESHOLD))) {
		 
		smp_mb();
		if (__netif_subqueue_stopped(tx_ring->netdev,
					     tx_ring->queue_index) &&
		   !test_bit(__I40E_VSI_DOWN, vsi->state)) {
			netif_wake_subqueue(tx_ring->netdev,
					    tx_ring->queue_index);
			++tx_ring->tx_stats.restart_queue;
		}
	}

	*tx_cleaned = total_packets;
	return !!budget;
}

 
static void i40e_enable_wb_on_itr(struct i40e_vsi *vsi,
				  struct i40e_q_vector *q_vector)
{
	u16 flags = q_vector->tx.ring[0].flags;
	u32 val;

	if (!(flags & I40E_TXR_FLAGS_WB_ON_ITR))
		return;

	if (q_vector->arm_wb_state)
		return;

	if (vsi->back->flags & I40E_FLAG_MSIX_ENABLED) {
		val = I40E_PFINT_DYN_CTLN_WB_ON_ITR_MASK |
		      I40E_PFINT_DYN_CTLN_ITR_INDX_MASK;  

		wr32(&vsi->back->hw,
		     I40E_PFINT_DYN_CTLN(q_vector->reg_idx),
		     val);
	} else {
		val = I40E_PFINT_DYN_CTL0_WB_ON_ITR_MASK |
		      I40E_PFINT_DYN_CTL0_ITR_INDX_MASK;  

		wr32(&vsi->back->hw, I40E_PFINT_DYN_CTL0, val);
	}
	q_vector->arm_wb_state = true;
}

 
void i40e_force_wb(struct i40e_vsi *vsi, struct i40e_q_vector *q_vector)
{
	if (vsi->back->flags & I40E_FLAG_MSIX_ENABLED) {
		u32 val = I40E_PFINT_DYN_CTLN_INTENA_MASK |
			  I40E_PFINT_DYN_CTLN_ITR_INDX_MASK |  
			  I40E_PFINT_DYN_CTLN_SWINT_TRIG_MASK |
			  I40E_PFINT_DYN_CTLN_SW_ITR_INDX_ENA_MASK;
			   

		wr32(&vsi->back->hw,
		     I40E_PFINT_DYN_CTLN(q_vector->reg_idx), val);
	} else {
		u32 val = I40E_PFINT_DYN_CTL0_INTENA_MASK |
			  I40E_PFINT_DYN_CTL0_ITR_INDX_MASK |  
			  I40E_PFINT_DYN_CTL0_SWINT_TRIG_MASK |
			  I40E_PFINT_DYN_CTL0_SW_ITR_INDX_ENA_MASK;
			 

		wr32(&vsi->back->hw, I40E_PFINT_DYN_CTL0, val);
	}
}

static inline bool i40e_container_is_rx(struct i40e_q_vector *q_vector,
					struct i40e_ring_container *rc)
{
	return &q_vector->rx == rc;
}

static inline unsigned int i40e_itr_divisor(struct i40e_q_vector *q_vector)
{
	unsigned int divisor;

	switch (q_vector->vsi->back->hw.phy.link_info.link_speed) {
	case I40E_LINK_SPEED_40GB:
		divisor = I40E_ITR_ADAPTIVE_MIN_INC * 1024;
		break;
	case I40E_LINK_SPEED_25GB:
	case I40E_LINK_SPEED_20GB:
		divisor = I40E_ITR_ADAPTIVE_MIN_INC * 512;
		break;
	default:
	case I40E_LINK_SPEED_10GB:
		divisor = I40E_ITR_ADAPTIVE_MIN_INC * 256;
		break;
	case I40E_LINK_SPEED_1GB:
	case I40E_LINK_SPEED_100MB:
		divisor = I40E_ITR_ADAPTIVE_MIN_INC * 32;
		break;
	}

	return divisor;
}

 
static void i40e_update_itr(struct i40e_q_vector *q_vector,
			    struct i40e_ring_container *rc)
{
	unsigned int avg_wire_size, packets, bytes, itr;
	unsigned long next_update = jiffies;

	 
	if (!rc->ring || !ITR_IS_DYNAMIC(rc->ring->itr_setting))
		return;

	 
	itr = i40e_container_is_rx(q_vector, rc) ?
	      I40E_ITR_ADAPTIVE_MIN_USECS | I40E_ITR_ADAPTIVE_LATENCY :
	      I40E_ITR_ADAPTIVE_MAX_USECS | I40E_ITR_ADAPTIVE_LATENCY;

	 
	if (time_after(next_update, rc->next_update))
		goto clear_counts;

	 
	if (q_vector->itr_countdown) {
		itr = rc->target_itr;
		goto clear_counts;
	}

	packets = rc->total_packets;
	bytes = rc->total_bytes;

	if (i40e_container_is_rx(q_vector, rc)) {
		 
		if (packets && packets < 4 && bytes < 9000 &&
		    (q_vector->tx.target_itr & I40E_ITR_ADAPTIVE_LATENCY)) {
			itr = I40E_ITR_ADAPTIVE_LATENCY;
			goto adjust_by_size;
		}
	} else if (packets < 4) {
		 
		if (rc->target_itr == I40E_ITR_ADAPTIVE_MAX_USECS &&
		    (q_vector->rx.target_itr & I40E_ITR_MASK) ==
		     I40E_ITR_ADAPTIVE_MAX_USECS)
			goto clear_counts;
	} else if (packets > 32) {
		 
		rc->target_itr &= ~I40E_ITR_ADAPTIVE_LATENCY;
	}

	 
	if (packets < 56) {
		itr = rc->target_itr + I40E_ITR_ADAPTIVE_MIN_INC;
		if ((itr & I40E_ITR_MASK) > I40E_ITR_ADAPTIVE_MAX_USECS) {
			itr &= I40E_ITR_ADAPTIVE_LATENCY;
			itr += I40E_ITR_ADAPTIVE_MAX_USECS;
		}
		goto clear_counts;
	}

	if (packets <= 256) {
		itr = min(q_vector->tx.current_itr, q_vector->rx.current_itr);
		itr &= I40E_ITR_MASK;

		 
		if (packets <= 112)
			goto clear_counts;

		 
		itr /= 2;
		itr &= I40E_ITR_MASK;
		if (itr < I40E_ITR_ADAPTIVE_MIN_USECS)
			itr = I40E_ITR_ADAPTIVE_MIN_USECS;

		goto clear_counts;
	}

	 
	itr = I40E_ITR_ADAPTIVE_BULK;

adjust_by_size:
	 
	avg_wire_size = bytes / packets;

	 
	if (avg_wire_size <= 60) {
		 
		avg_wire_size = 4096;
	} else if (avg_wire_size <= 380) {
		 
		avg_wire_size *= 40;
		avg_wire_size += 1696;
	} else if (avg_wire_size <= 1084) {
		 
		avg_wire_size *= 15;
		avg_wire_size += 11452;
	} else if (avg_wire_size <= 1980) {
		 
		avg_wire_size *= 5;
		avg_wire_size += 22420;
	} else {
		 
		avg_wire_size = 32256;
	}

	 
	if (itr & I40E_ITR_ADAPTIVE_LATENCY)
		avg_wire_size /= 2;

	 
	itr += DIV_ROUND_UP(avg_wire_size, i40e_itr_divisor(q_vector)) *
	       I40E_ITR_ADAPTIVE_MIN_INC;

	if ((itr & I40E_ITR_MASK) > I40E_ITR_ADAPTIVE_MAX_USECS) {
		itr &= I40E_ITR_ADAPTIVE_LATENCY;
		itr += I40E_ITR_ADAPTIVE_MAX_USECS;
	}

clear_counts:
	 
	rc->target_itr = itr;

	 
	rc->next_update = next_update + 1;

	rc->total_bytes = 0;
	rc->total_packets = 0;
}

static struct i40e_rx_buffer *i40e_rx_bi(struct i40e_ring *rx_ring, u32 idx)
{
	return &rx_ring->rx_bi[idx];
}

 
static void i40e_reuse_rx_page(struct i40e_ring *rx_ring,
			       struct i40e_rx_buffer *old_buff)
{
	struct i40e_rx_buffer *new_buff;
	u16 nta = rx_ring->next_to_alloc;

	new_buff = i40e_rx_bi(rx_ring, nta);

	 
	nta++;
	rx_ring->next_to_alloc = (nta < rx_ring->count) ? nta : 0;

	 
	new_buff->dma		= old_buff->dma;
	new_buff->page		= old_buff->page;
	new_buff->page_offset	= old_buff->page_offset;
	new_buff->pagecnt_bias	= old_buff->pagecnt_bias;

	 
	old_buff->page = NULL;
}

 
void i40e_clean_programming_status(struct i40e_ring *rx_ring, u64 qword0_raw,
				   u64 qword1)
{
	u8 id;

	id = (qword1 & I40E_RX_PROG_STATUS_DESC_QW1_PROGID_MASK) >>
		  I40E_RX_PROG_STATUS_DESC_QW1_PROGID_SHIFT;

	if (id == I40E_RX_PROG_STATUS_DESC_FD_FILTER_STATUS)
		i40e_fd_handle_status(rx_ring, qword0_raw, qword1, id);
}

 
int i40e_setup_tx_descriptors(struct i40e_ring *tx_ring)
{
	struct device *dev = tx_ring->dev;
	int bi_size;

	if (!dev)
		return -ENOMEM;

	 
	WARN_ON(tx_ring->tx_bi);
	bi_size = sizeof(struct i40e_tx_buffer) * tx_ring->count;
	tx_ring->tx_bi = kzalloc(bi_size, GFP_KERNEL);
	if (!tx_ring->tx_bi)
		goto err;

	u64_stats_init(&tx_ring->syncp);

	 
	tx_ring->size = tx_ring->count * sizeof(struct i40e_tx_desc);
	 
	tx_ring->size += sizeof(u32);
	tx_ring->size = ALIGN(tx_ring->size, 4096);
	tx_ring->desc = dma_alloc_coherent(dev, tx_ring->size,
					   &tx_ring->dma, GFP_KERNEL);
	if (!tx_ring->desc) {
		dev_info(dev, "Unable to allocate memory for the Tx descriptor ring, size=%d\n",
			 tx_ring->size);
		goto err;
	}

	tx_ring->next_to_use = 0;
	tx_ring->next_to_clean = 0;
	tx_ring->tx_stats.prev_pkt_ctr = -1;
	return 0;

err:
	kfree(tx_ring->tx_bi);
	tx_ring->tx_bi = NULL;
	return -ENOMEM;
}

static void i40e_clear_rx_bi(struct i40e_ring *rx_ring)
{
	memset(rx_ring->rx_bi, 0, sizeof(*rx_ring->rx_bi) * rx_ring->count);
}

 
void i40e_clean_rx_ring(struct i40e_ring *rx_ring)
{
	u16 i;

	 
	if (!rx_ring->rx_bi)
		return;

	if (rx_ring->xsk_pool) {
		i40e_xsk_clean_rx_ring(rx_ring);
		goto skip_free;
	}

	 
	for (i = 0; i < rx_ring->count; i++) {
		struct i40e_rx_buffer *rx_bi = i40e_rx_bi(rx_ring, i);

		if (!rx_bi->page)
			continue;

		 
		dma_sync_single_range_for_cpu(rx_ring->dev,
					      rx_bi->dma,
					      rx_bi->page_offset,
					      rx_ring->rx_buf_len,
					      DMA_FROM_DEVICE);

		 
		dma_unmap_page_attrs(rx_ring->dev, rx_bi->dma,
				     i40e_rx_pg_size(rx_ring),
				     DMA_FROM_DEVICE,
				     I40E_RX_DMA_ATTR);

		__page_frag_cache_drain(rx_bi->page, rx_bi->pagecnt_bias);

		rx_bi->page = NULL;
		rx_bi->page_offset = 0;
	}

skip_free:
	if (rx_ring->xsk_pool)
		i40e_clear_rx_bi_zc(rx_ring);
	else
		i40e_clear_rx_bi(rx_ring);

	 
	memset(rx_ring->desc, 0, rx_ring->size);

	rx_ring->next_to_alloc = 0;
	rx_ring->next_to_clean = 0;
	rx_ring->next_to_process = 0;
	rx_ring->next_to_use = 0;
}

 
void i40e_free_rx_resources(struct i40e_ring *rx_ring)
{
	i40e_clean_rx_ring(rx_ring);
	if (rx_ring->vsi->type == I40E_VSI_MAIN)
		xdp_rxq_info_unreg(&rx_ring->xdp_rxq);
	rx_ring->xdp_prog = NULL;
	kfree(rx_ring->rx_bi);
	rx_ring->rx_bi = NULL;

	if (rx_ring->desc) {
		dma_free_coherent(rx_ring->dev, rx_ring->size,
				  rx_ring->desc, rx_ring->dma);
		rx_ring->desc = NULL;
	}
}

 
int i40e_setup_rx_descriptors(struct i40e_ring *rx_ring)
{
	struct device *dev = rx_ring->dev;
	int err;

	u64_stats_init(&rx_ring->syncp);

	 
	rx_ring->size = rx_ring->count * sizeof(union i40e_rx_desc);
	rx_ring->size = ALIGN(rx_ring->size, 4096);
	rx_ring->desc = dma_alloc_coherent(dev, rx_ring->size,
					   &rx_ring->dma, GFP_KERNEL);

	if (!rx_ring->desc) {
		dev_info(dev, "Unable to allocate memory for the Rx descriptor ring, size=%d\n",
			 rx_ring->size);
		return -ENOMEM;
	}

	rx_ring->next_to_alloc = 0;
	rx_ring->next_to_clean = 0;
	rx_ring->next_to_process = 0;
	rx_ring->next_to_use = 0;

	 
	if (rx_ring->vsi->type == I40E_VSI_MAIN) {
		err = xdp_rxq_info_reg(&rx_ring->xdp_rxq, rx_ring->netdev,
				       rx_ring->queue_index, rx_ring->q_vector->napi.napi_id);
		if (err < 0)
			return err;
	}

	rx_ring->xdp_prog = rx_ring->vsi->xdp_prog;

	rx_ring->rx_bi =
		kcalloc(rx_ring->count, sizeof(*rx_ring->rx_bi), GFP_KERNEL);
	if (!rx_ring->rx_bi)
		return -ENOMEM;

	return 0;
}

 
void i40e_release_rx_desc(struct i40e_ring *rx_ring, u32 val)
{
	rx_ring->next_to_use = val;

	 
	rx_ring->next_to_alloc = val;

	 
	wmb();
	writel(val, rx_ring->tail);
}

#if (PAGE_SIZE >= 8192)
static unsigned int i40e_rx_frame_truesize(struct i40e_ring *rx_ring,
					   unsigned int size)
{
	unsigned int truesize;

	truesize = rx_ring->rx_offset ?
		SKB_DATA_ALIGN(size + rx_ring->rx_offset) +
		SKB_DATA_ALIGN(sizeof(struct skb_shared_info)) :
		SKB_DATA_ALIGN(size);
	return truesize;
}
#endif

 
static bool i40e_alloc_mapped_page(struct i40e_ring *rx_ring,
				   struct i40e_rx_buffer *bi)
{
	struct page *page = bi->page;
	dma_addr_t dma;

	 
	if (likely(page)) {
		rx_ring->rx_stats.page_reuse_count++;
		return true;
	}

	 
	page = dev_alloc_pages(i40e_rx_pg_order(rx_ring));
	if (unlikely(!page)) {
		rx_ring->rx_stats.alloc_page_failed++;
		return false;
	}

	rx_ring->rx_stats.page_alloc_count++;

	 
	dma = dma_map_page_attrs(rx_ring->dev, page, 0,
				 i40e_rx_pg_size(rx_ring),
				 DMA_FROM_DEVICE,
				 I40E_RX_DMA_ATTR);

	 
	if (dma_mapping_error(rx_ring->dev, dma)) {
		__free_pages(page, i40e_rx_pg_order(rx_ring));
		rx_ring->rx_stats.alloc_page_failed++;
		return false;
	}

	bi->dma = dma;
	bi->page = page;
	bi->page_offset = rx_ring->rx_offset;
	page_ref_add(page, USHRT_MAX - 1);
	bi->pagecnt_bias = USHRT_MAX;

	return true;
}

 
bool i40e_alloc_rx_buffers(struct i40e_ring *rx_ring, u16 cleaned_count)
{
	u16 ntu = rx_ring->next_to_use;
	union i40e_rx_desc *rx_desc;
	struct i40e_rx_buffer *bi;

	 
	if (!rx_ring->netdev || !cleaned_count)
		return false;

	rx_desc = I40E_RX_DESC(rx_ring, ntu);
	bi = i40e_rx_bi(rx_ring, ntu);

	do {
		if (!i40e_alloc_mapped_page(rx_ring, bi))
			goto no_buffers;

		 
		dma_sync_single_range_for_device(rx_ring->dev, bi->dma,
						 bi->page_offset,
						 rx_ring->rx_buf_len,
						 DMA_FROM_DEVICE);

		 
		rx_desc->read.pkt_addr = cpu_to_le64(bi->dma + bi->page_offset);

		rx_desc++;
		bi++;
		ntu++;
		if (unlikely(ntu == rx_ring->count)) {
			rx_desc = I40E_RX_DESC(rx_ring, 0);
			bi = i40e_rx_bi(rx_ring, 0);
			ntu = 0;
		}

		 
		rx_desc->wb.qword1.status_error_len = 0;

		cleaned_count--;
	} while (cleaned_count);

	if (rx_ring->next_to_use != ntu)
		i40e_release_rx_desc(rx_ring, ntu);

	return false;

no_buffers:
	if (rx_ring->next_to_use != ntu)
		i40e_release_rx_desc(rx_ring, ntu);

	 
	return true;
}

 
static inline void i40e_rx_checksum(struct i40e_vsi *vsi,
				    struct sk_buff *skb,
				    union i40e_rx_desc *rx_desc)
{
	struct i40e_rx_ptype_decoded decoded;
	u32 rx_error, rx_status;
	bool ipv4, ipv6;
	u8 ptype;
	u64 qword;

	qword = le64_to_cpu(rx_desc->wb.qword1.status_error_len);
	ptype = (qword & I40E_RXD_QW1_PTYPE_MASK) >> I40E_RXD_QW1_PTYPE_SHIFT;
	rx_error = (qword & I40E_RXD_QW1_ERROR_MASK) >>
		   I40E_RXD_QW1_ERROR_SHIFT;
	rx_status = (qword & I40E_RXD_QW1_STATUS_MASK) >>
		    I40E_RXD_QW1_STATUS_SHIFT;
	decoded = decode_rx_desc_ptype(ptype);

	skb->ip_summed = CHECKSUM_NONE;

	skb_checksum_none_assert(skb);

	 
	if (!(vsi->netdev->features & NETIF_F_RXCSUM))
		return;

	 
	if (!(rx_status & BIT(I40E_RX_DESC_STATUS_L3L4P_SHIFT)))
		return;

	 
	if (!(decoded.known && decoded.outer_ip))
		return;

	ipv4 = (decoded.outer_ip == I40E_RX_PTYPE_OUTER_IP) &&
	       (decoded.outer_ip_ver == I40E_RX_PTYPE_OUTER_IPV4);
	ipv6 = (decoded.outer_ip == I40E_RX_PTYPE_OUTER_IP) &&
	       (decoded.outer_ip_ver == I40E_RX_PTYPE_OUTER_IPV6);

	if (ipv4 &&
	    (rx_error & (BIT(I40E_RX_DESC_ERROR_IPE_SHIFT) |
			 BIT(I40E_RX_DESC_ERROR_EIPE_SHIFT))))
		goto checksum_fail;

	 
	if (ipv6 &&
	    rx_status & BIT(I40E_RX_DESC_STATUS_IPV6EXADD_SHIFT))
		 
		return;

	 
	if (rx_error & BIT(I40E_RX_DESC_ERROR_L4E_SHIFT))
		goto checksum_fail;

	 
	if (rx_error & BIT(I40E_RX_DESC_ERROR_PPRS_SHIFT))
		return;

	 
	if (decoded.tunnel_type >= I40E_RX_PTYPE_TUNNEL_IP_GRENAT)
		skb->csum_level = 1;

	 
	switch (decoded.inner_prot) {
	case I40E_RX_PTYPE_INNER_PROT_TCP:
	case I40E_RX_PTYPE_INNER_PROT_UDP:
	case I40E_RX_PTYPE_INNER_PROT_SCTP:
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		fallthrough;
	default:
		break;
	}

	return;

checksum_fail:
	vsi->back->hw_csum_rx_error++;
}

 
static inline int i40e_ptype_to_htype(u8 ptype)
{
	struct i40e_rx_ptype_decoded decoded = decode_rx_desc_ptype(ptype);

	if (!decoded.known)
		return PKT_HASH_TYPE_NONE;

	if (decoded.outer_ip == I40E_RX_PTYPE_OUTER_IP &&
	    decoded.payload_layer == I40E_RX_PTYPE_PAYLOAD_LAYER_PAY4)
		return PKT_HASH_TYPE_L4;
	else if (decoded.outer_ip == I40E_RX_PTYPE_OUTER_IP &&
		 decoded.payload_layer == I40E_RX_PTYPE_PAYLOAD_LAYER_PAY3)
		return PKT_HASH_TYPE_L3;
	else
		return PKT_HASH_TYPE_L2;
}

 
static inline void i40e_rx_hash(struct i40e_ring *ring,
				union i40e_rx_desc *rx_desc,
				struct sk_buff *skb,
				u8 rx_ptype)
{
	u32 hash;
	const __le64 rss_mask =
		cpu_to_le64((u64)I40E_RX_DESC_FLTSTAT_RSS_HASH <<
			    I40E_RX_DESC_STATUS_FLTSTAT_SHIFT);

	if (!(ring->netdev->features & NETIF_F_RXHASH))
		return;

	if ((rx_desc->wb.qword1.status_error_len & rss_mask) == rss_mask) {
		hash = le32_to_cpu(rx_desc->wb.qword0.hi_dword.rss);
		skb_set_hash(skb, hash, i40e_ptype_to_htype(rx_ptype));
	}
}

 
void i40e_process_skb_fields(struct i40e_ring *rx_ring,
			     union i40e_rx_desc *rx_desc, struct sk_buff *skb)
{
	u64 qword = le64_to_cpu(rx_desc->wb.qword1.status_error_len);
	u32 rx_status = (qword & I40E_RXD_QW1_STATUS_MASK) >>
			I40E_RXD_QW1_STATUS_SHIFT;
	u32 tsynvalid = rx_status & I40E_RXD_QW1_STATUS_TSYNVALID_MASK;
	u32 tsyn = (rx_status & I40E_RXD_QW1_STATUS_TSYNINDX_MASK) >>
		   I40E_RXD_QW1_STATUS_TSYNINDX_SHIFT;
	u8 rx_ptype = (qword & I40E_RXD_QW1_PTYPE_MASK) >>
		      I40E_RXD_QW1_PTYPE_SHIFT;

	if (unlikely(tsynvalid))
		i40e_ptp_rx_hwtstamp(rx_ring->vsi->back, skb, tsyn);

	i40e_rx_hash(rx_ring, rx_desc, skb, rx_ptype);

	i40e_rx_checksum(rx_ring->vsi, skb, rx_desc);

	skb_record_rx_queue(skb, rx_ring->queue_index);

	if (qword & BIT(I40E_RX_DESC_STATUS_L2TAG1P_SHIFT)) {
		__le16 vlan_tag = rx_desc->wb.qword0.lo_dword.l2tag1;

		__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q),
				       le16_to_cpu(vlan_tag));
	}

	 
	skb->protocol = eth_type_trans(skb, rx_ring->netdev);
}

 
static bool i40e_cleanup_headers(struct i40e_ring *rx_ring, struct sk_buff *skb,
				 union i40e_rx_desc *rx_desc)

{
	 
	if (unlikely(i40e_test_staterr(rx_desc,
				       BIT(I40E_RXD_QW1_ERROR_SHIFT)))) {
		dev_kfree_skb_any(skb);
		return true;
	}

	 
	if (eth_skb_pad(skb))
		return true;

	return false;
}

 
static bool i40e_can_reuse_rx_page(struct i40e_rx_buffer *rx_buffer,
				   struct i40e_rx_queue_stats *rx_stats)
{
	unsigned int pagecnt_bias = rx_buffer->pagecnt_bias;
	struct page *page = rx_buffer->page;

	 
	if (!dev_page_is_reusable(page)) {
		rx_stats->page_waive_count++;
		return false;
	}

#if (PAGE_SIZE < 8192)
	 
	if (unlikely((rx_buffer->page_count - pagecnt_bias) > 1)) {
		rx_stats->page_busy_count++;
		return false;
	}
#else
#define I40E_LAST_OFFSET \
	(SKB_WITH_OVERHEAD(PAGE_SIZE) - I40E_RXBUFFER_2048)
	if (rx_buffer->page_offset > I40E_LAST_OFFSET) {
		rx_stats->page_busy_count++;
		return false;
	}
#endif

	 
	if (unlikely(pagecnt_bias == 1)) {
		page_ref_add(page, USHRT_MAX - 1);
		rx_buffer->pagecnt_bias = USHRT_MAX;
	}

	return true;
}

 
static void i40e_rx_buffer_flip(struct i40e_rx_buffer *rx_buffer,
				unsigned int truesize)
{
#if (PAGE_SIZE < 8192)
	rx_buffer->page_offset ^= truesize;
#else
	rx_buffer->page_offset += truesize;
#endif
}

 
static struct i40e_rx_buffer *i40e_get_rx_buffer(struct i40e_ring *rx_ring,
						 const unsigned int size)
{
	struct i40e_rx_buffer *rx_buffer;

	rx_buffer = i40e_rx_bi(rx_ring, rx_ring->next_to_process);
	rx_buffer->page_count =
#if (PAGE_SIZE < 8192)
		page_count(rx_buffer->page);
#else
		0;
#endif
	prefetch_page_address(rx_buffer->page);

	 
	dma_sync_single_range_for_cpu(rx_ring->dev,
				      rx_buffer->dma,
				      rx_buffer->page_offset,
				      size,
				      DMA_FROM_DEVICE);

	 
	rx_buffer->pagecnt_bias--;

	return rx_buffer;
}

 
static void i40e_put_rx_buffer(struct i40e_ring *rx_ring,
			       struct i40e_rx_buffer *rx_buffer)
{
	if (i40e_can_reuse_rx_page(rx_buffer, &rx_ring->rx_stats)) {
		 
		i40e_reuse_rx_page(rx_ring, rx_buffer);
	} else {
		 
		dma_unmap_page_attrs(rx_ring->dev, rx_buffer->dma,
				     i40e_rx_pg_size(rx_ring),
				     DMA_FROM_DEVICE, I40E_RX_DMA_ATTR);
		__page_frag_cache_drain(rx_buffer->page,
					rx_buffer->pagecnt_bias);
		 
		rx_buffer->page = NULL;
	}
}

 
static void i40e_process_rx_buffs(struct i40e_ring *rx_ring, int xdp_res,
				  struct xdp_buff *xdp)
{
	u32 next = rx_ring->next_to_clean;
	struct i40e_rx_buffer *rx_buffer;

	xdp->flags = 0;

	while (1) {
		rx_buffer = i40e_rx_bi(rx_ring, next);
		if (++next == rx_ring->count)
			next = 0;

		if (!rx_buffer->page)
			continue;

		if (xdp_res == I40E_XDP_CONSUMED)
			rx_buffer->pagecnt_bias++;
		else
			i40e_rx_buffer_flip(rx_buffer, xdp->frame_sz);

		 
		if (next == rx_ring->next_to_process)
			return;

		i40e_put_rx_buffer(rx_ring, rx_buffer);
	}
}

 
static struct sk_buff *i40e_construct_skb(struct i40e_ring *rx_ring,
					  struct xdp_buff *xdp,
					  u32 nr_frags)
{
	unsigned int size = xdp->data_end - xdp->data;
	struct i40e_rx_buffer *rx_buffer;
	unsigned int headlen;
	struct sk_buff *skb;

	 
	net_prefetch(xdp->data);

	 

	 
	skb = __napi_alloc_skb(&rx_ring->q_vector->napi,
			       I40E_RX_HDR_SIZE,
			       GFP_ATOMIC | __GFP_NOWARN);
	if (unlikely(!skb))
		return NULL;

	 
	headlen = size;
	if (headlen > I40E_RX_HDR_SIZE)
		headlen = eth_get_headlen(skb->dev, xdp->data,
					  I40E_RX_HDR_SIZE);

	 
	memcpy(__skb_put(skb, headlen), xdp->data,
	       ALIGN(headlen, sizeof(long)));

	rx_buffer = i40e_rx_bi(rx_ring, rx_ring->next_to_clean);
	 
	size -= headlen;
	if (size) {
		if (unlikely(nr_frags >= MAX_SKB_FRAGS)) {
			dev_kfree_skb(skb);
			return NULL;
		}
		skb_add_rx_frag(skb, 0, rx_buffer->page,
				rx_buffer->page_offset + headlen,
				size, xdp->frame_sz);
		 
		i40e_rx_buffer_flip(rx_buffer, xdp->frame_sz);
	} else {
		 
		rx_buffer->pagecnt_bias++;
	}

	if (unlikely(xdp_buff_has_frags(xdp))) {
		struct skb_shared_info *sinfo, *skinfo = skb_shinfo(skb);

		sinfo = xdp_get_shared_info_from_buff(xdp);
		memcpy(&skinfo->frags[skinfo->nr_frags], &sinfo->frags[0],
		       sizeof(skb_frag_t) * nr_frags);

		xdp_update_skb_shared_info(skb, skinfo->nr_frags + nr_frags,
					   sinfo->xdp_frags_size,
					   nr_frags * xdp->frame_sz,
					   xdp_buff_is_frag_pfmemalloc(xdp));

		 
		if (++rx_ring->next_to_clean == rx_ring->count)
			rx_ring->next_to_clean = 0;

		i40e_process_rx_buffs(rx_ring, I40E_XDP_PASS, xdp);
	}

	return skb;
}

 
static struct sk_buff *i40e_build_skb(struct i40e_ring *rx_ring,
				      struct xdp_buff *xdp,
				      u32 nr_frags)
{
	unsigned int metasize = xdp->data - xdp->data_meta;
	struct sk_buff *skb;

	 
	net_prefetch(xdp->data_meta);

	 
	skb = napi_build_skb(xdp->data_hard_start, xdp->frame_sz);
	if (unlikely(!skb))
		return NULL;

	 
	skb_reserve(skb, xdp->data - xdp->data_hard_start);
	__skb_put(skb, xdp->data_end - xdp->data);
	if (metasize)
		skb_metadata_set(skb, metasize);

	if (unlikely(xdp_buff_has_frags(xdp))) {
		struct skb_shared_info *sinfo;

		sinfo = xdp_get_shared_info_from_buff(xdp);
		xdp_update_skb_shared_info(skb, nr_frags,
					   sinfo->xdp_frags_size,
					   nr_frags * xdp->frame_sz,
					   xdp_buff_is_frag_pfmemalloc(xdp));

		i40e_process_rx_buffs(rx_ring, I40E_XDP_PASS, xdp);
	} else {
		struct i40e_rx_buffer *rx_buffer;

		rx_buffer = i40e_rx_bi(rx_ring, rx_ring->next_to_clean);
		 
		i40e_rx_buffer_flip(rx_buffer, xdp->frame_sz);
	}

	return skb;
}

 
bool i40e_is_non_eop(struct i40e_ring *rx_ring,
		     union i40e_rx_desc *rx_desc)
{
	 
#define I40E_RXD_EOF BIT(I40E_RX_DESC_STATUS_EOF_SHIFT)
	if (likely(i40e_test_staterr(rx_desc, I40E_RXD_EOF)))
		return false;

	rx_ring->rx_stats.non_eop_descs++;

	return true;
}

static int i40e_xmit_xdp_ring(struct xdp_frame *xdpf,
			      struct i40e_ring *xdp_ring);

int i40e_xmit_xdp_tx_ring(struct xdp_buff *xdp, struct i40e_ring *xdp_ring)
{
	struct xdp_frame *xdpf = xdp_convert_buff_to_frame(xdp);

	if (unlikely(!xdpf))
		return I40E_XDP_CONSUMED;

	return i40e_xmit_xdp_ring(xdpf, xdp_ring);
}

 
static int i40e_run_xdp(struct i40e_ring *rx_ring, struct xdp_buff *xdp, struct bpf_prog *xdp_prog)
{
	int err, result = I40E_XDP_PASS;
	struct i40e_ring *xdp_ring;
	u32 act;

	if (!xdp_prog)
		goto xdp_out;

	prefetchw(xdp->data_hard_start);  

	act = bpf_prog_run_xdp(xdp_prog, xdp);
	switch (act) {
	case XDP_PASS:
		break;
	case XDP_TX:
		xdp_ring = rx_ring->vsi->xdp_rings[rx_ring->queue_index];
		result = i40e_xmit_xdp_tx_ring(xdp, xdp_ring);
		if (result == I40E_XDP_CONSUMED)
			goto out_failure;
		break;
	case XDP_REDIRECT:
		err = xdp_do_redirect(rx_ring->netdev, xdp, xdp_prog);
		if (err)
			goto out_failure;
		result = I40E_XDP_REDIR;
		break;
	default:
		bpf_warn_invalid_xdp_action(rx_ring->netdev, xdp_prog, act);
		fallthrough;
	case XDP_ABORTED:
out_failure:
		trace_xdp_exception(rx_ring->netdev, xdp_prog, act);
		fallthrough;  
	case XDP_DROP:
		result = I40E_XDP_CONSUMED;
		break;
	}
xdp_out:
	return result;
}

 
void i40e_xdp_ring_update_tail(struct i40e_ring *xdp_ring)
{
	 
	wmb();
	writel_relaxed(xdp_ring->next_to_use, xdp_ring->tail);
}

 
void i40e_update_rx_stats(struct i40e_ring *rx_ring,
			  unsigned int total_rx_bytes,
			  unsigned int total_rx_packets)
{
	u64_stats_update_begin(&rx_ring->syncp);
	rx_ring->stats.packets += total_rx_packets;
	rx_ring->stats.bytes += total_rx_bytes;
	u64_stats_update_end(&rx_ring->syncp);
	rx_ring->q_vector->rx.total_packets += total_rx_packets;
	rx_ring->q_vector->rx.total_bytes += total_rx_bytes;
}

 
void i40e_finalize_xdp_rx(struct i40e_ring *rx_ring, unsigned int xdp_res)
{
	if (xdp_res & I40E_XDP_REDIR)
		xdp_do_flush_map();

	if (xdp_res & I40E_XDP_TX) {
		struct i40e_ring *xdp_ring =
			rx_ring->vsi->xdp_rings[rx_ring->queue_index];

		i40e_xdp_ring_update_tail(xdp_ring);
	}
}

 
static void i40e_inc_ntp(struct i40e_ring *rx_ring)
{
	u32 ntp = rx_ring->next_to_process + 1;

	ntp = (ntp < rx_ring->count) ? ntp : 0;
	rx_ring->next_to_process = ntp;
	prefetch(I40E_RX_DESC(rx_ring, ntp));
}

 
static int i40e_add_xdp_frag(struct xdp_buff *xdp, u32 *nr_frags,
			     struct i40e_rx_buffer *rx_buffer, u32 size)
{
	struct skb_shared_info *sinfo = xdp_get_shared_info_from_buff(xdp);

	if (!xdp_buff_has_frags(xdp)) {
		sinfo->nr_frags = 0;
		sinfo->xdp_frags_size = 0;
		xdp_buff_set_frags_flag(xdp);
	} else if (unlikely(sinfo->nr_frags >= MAX_SKB_FRAGS)) {
		 
		return -ENOMEM;
	}

	__skb_fill_page_desc_noacc(sinfo, sinfo->nr_frags++, rx_buffer->page,
				   rx_buffer->page_offset, size);

	sinfo->xdp_frags_size += size;

	if (page_is_pfmemalloc(rx_buffer->page))
		xdp_buff_set_frag_pfmemalloc(xdp);
	*nr_frags = sinfo->nr_frags;

	return 0;
}

 
static void i40e_consume_xdp_buff(struct i40e_ring *rx_ring,
				  struct xdp_buff *xdp,
				  struct i40e_rx_buffer *rx_buffer)
{
	i40e_process_rx_buffs(rx_ring, I40E_XDP_CONSUMED, xdp);
	i40e_put_rx_buffer(rx_ring, rx_buffer);
	rx_ring->next_to_clean = rx_ring->next_to_process;
	xdp->data = NULL;
}

 
static int i40e_clean_rx_irq(struct i40e_ring *rx_ring, int budget,
			     unsigned int *rx_cleaned)
{
	unsigned int total_rx_bytes = 0, total_rx_packets = 0;
	u16 cleaned_count = I40E_DESC_UNUSED(rx_ring);
	u16 clean_threshold = rx_ring->count / 2;
	unsigned int offset = rx_ring->rx_offset;
	struct xdp_buff *xdp = &rx_ring->xdp;
	unsigned int xdp_xmit = 0;
	struct bpf_prog *xdp_prog;
	bool failure = false;
	int xdp_res = 0;

	xdp_prog = READ_ONCE(rx_ring->xdp_prog);

	while (likely(total_rx_packets < (unsigned int)budget)) {
		u16 ntp = rx_ring->next_to_process;
		struct i40e_rx_buffer *rx_buffer;
		union i40e_rx_desc *rx_desc;
		struct sk_buff *skb;
		unsigned int size;
		u32 nfrags = 0;
		bool neop;
		u64 qword;

		 
		if (cleaned_count >= clean_threshold) {
			failure = failure ||
				  i40e_alloc_rx_buffers(rx_ring, cleaned_count);
			cleaned_count = 0;
		}

		rx_desc = I40E_RX_DESC(rx_ring, ntp);

		 
		qword = le64_to_cpu(rx_desc->wb.qword1.status_error_len);

		 
		dma_rmb();

		if (i40e_rx_is_programming_status(qword)) {
			i40e_clean_programming_status(rx_ring,
						      rx_desc->raw.qword[0],
						      qword);
			rx_buffer = i40e_rx_bi(rx_ring, ntp);
			i40e_inc_ntp(rx_ring);
			i40e_reuse_rx_page(rx_ring, rx_buffer);
			 
			if (rx_ring->next_to_clean == ntp) {
				rx_ring->next_to_clean =
					rx_ring->next_to_process;
				cleaned_count++;
			}
			continue;
		}

		size = (qword & I40E_RXD_QW1_LENGTH_PBUF_MASK) >>
		       I40E_RXD_QW1_LENGTH_PBUF_SHIFT;
		if (!size)
			break;

		i40e_trace(clean_rx_irq, rx_ring, rx_desc, xdp);
		 
		rx_buffer = i40e_get_rx_buffer(rx_ring, size);

		neop = i40e_is_non_eop(rx_ring, rx_desc);
		i40e_inc_ntp(rx_ring);

		if (!xdp->data) {
			unsigned char *hard_start;

			hard_start = page_address(rx_buffer->page) +
				     rx_buffer->page_offset - offset;
			xdp_prepare_buff(xdp, hard_start, offset, size, true);
#if (PAGE_SIZE > 4096)
			 
			xdp->frame_sz = i40e_rx_frame_truesize(rx_ring, size);
#endif
		} else if (i40e_add_xdp_frag(xdp, &nfrags, rx_buffer, size) &&
			   !neop) {
			 
			i40e_consume_xdp_buff(rx_ring, xdp, rx_buffer);
			break;
		}

		if (neop)
			continue;

		xdp_res = i40e_run_xdp(rx_ring, xdp, xdp_prog);

		if (xdp_res) {
			xdp_xmit |= xdp_res & (I40E_XDP_TX | I40E_XDP_REDIR);

			if (unlikely(xdp_buff_has_frags(xdp))) {
				i40e_process_rx_buffs(rx_ring, xdp_res, xdp);
				size = xdp_get_buff_len(xdp);
			} else if (xdp_res & (I40E_XDP_TX | I40E_XDP_REDIR)) {
				i40e_rx_buffer_flip(rx_buffer, xdp->frame_sz);
			} else {
				rx_buffer->pagecnt_bias++;
			}
			total_rx_bytes += size;
		} else {
			if (ring_uses_build_skb(rx_ring))
				skb = i40e_build_skb(rx_ring, xdp, nfrags);
			else
				skb = i40e_construct_skb(rx_ring, xdp, nfrags);

			 
			if (!skb) {
				rx_ring->rx_stats.alloc_buff_failed++;
				i40e_consume_xdp_buff(rx_ring, xdp, rx_buffer);
				break;
			}

			if (i40e_cleanup_headers(rx_ring, skb, rx_desc))
				goto process_next;

			 
			total_rx_bytes += skb->len;

			 
			i40e_process_skb_fields(rx_ring, rx_desc, skb);

			i40e_trace(clean_rx_irq_rx, rx_ring, rx_desc, xdp);
			napi_gro_receive(&rx_ring->q_vector->napi, skb);
		}

		 
		total_rx_packets++;
process_next:
		cleaned_count += nfrags + 1;
		i40e_put_rx_buffer(rx_ring, rx_buffer);
		rx_ring->next_to_clean = rx_ring->next_to_process;

		xdp->data = NULL;
	}

	i40e_finalize_xdp_rx(rx_ring, xdp_xmit);

	i40e_update_rx_stats(rx_ring, total_rx_bytes, total_rx_packets);

	*rx_cleaned = total_rx_packets;

	 
	return failure ? budget : (int)total_rx_packets;
}

static inline u32 i40e_buildreg_itr(const int type, u16 itr)
{
	u32 val;

	 
	itr &= I40E_ITR_MASK;

	val = I40E_PFINT_DYN_CTLN_INTENA_MASK |
	      (type << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT) |
	      (itr << (I40E_PFINT_DYN_CTLN_INTERVAL_SHIFT - 1));

	return val;
}

 
#define INTREG I40E_PFINT_DYN_CTLN

 
#define ITR_COUNTDOWN_START 3

 
static inline void i40e_update_enable_itr(struct i40e_vsi *vsi,
					  struct i40e_q_vector *q_vector)
{
	struct i40e_hw *hw = &vsi->back->hw;
	u32 intval;

	 
	if (!(vsi->back->flags & I40E_FLAG_MSIX_ENABLED)) {
		i40e_irq_dynamic_enable_icr0(vsi->back);
		return;
	}

	 
	i40e_update_itr(q_vector, &q_vector->tx);
	i40e_update_itr(q_vector, &q_vector->rx);

	 
	if (q_vector->rx.target_itr < q_vector->rx.current_itr) {
		 
		intval = i40e_buildreg_itr(I40E_RX_ITR,
					   q_vector->rx.target_itr);
		q_vector->rx.current_itr = q_vector->rx.target_itr;
		q_vector->itr_countdown = ITR_COUNTDOWN_START;
	} else if ((q_vector->tx.target_itr < q_vector->tx.current_itr) ||
		   ((q_vector->rx.target_itr - q_vector->rx.current_itr) <
		    (q_vector->tx.target_itr - q_vector->tx.current_itr))) {
		 
		intval = i40e_buildreg_itr(I40E_TX_ITR,
					   q_vector->tx.target_itr);
		q_vector->tx.current_itr = q_vector->tx.target_itr;
		q_vector->itr_countdown = ITR_COUNTDOWN_START;
	} else if (q_vector->rx.current_itr != q_vector->rx.target_itr) {
		 
		intval = i40e_buildreg_itr(I40E_RX_ITR,
					   q_vector->rx.target_itr);
		q_vector->rx.current_itr = q_vector->rx.target_itr;
		q_vector->itr_countdown = ITR_COUNTDOWN_START;
	} else {
		 
		intval = i40e_buildreg_itr(I40E_ITR_NONE, 0);
		if (q_vector->itr_countdown)
			q_vector->itr_countdown--;
	}

	if (!test_bit(__I40E_VSI_DOWN, vsi->state))
		wr32(hw, INTREG(q_vector->reg_idx), intval);
}

 
int i40e_napi_poll(struct napi_struct *napi, int budget)
{
	struct i40e_q_vector *q_vector =
			       container_of(napi, struct i40e_q_vector, napi);
	struct i40e_vsi *vsi = q_vector->vsi;
	struct i40e_ring *ring;
	bool tx_clean_complete = true;
	bool rx_clean_complete = true;
	unsigned int tx_cleaned = 0;
	unsigned int rx_cleaned = 0;
	bool clean_complete = true;
	bool arm_wb = false;
	int budget_per_ring;
	int work_done = 0;

	if (test_bit(__I40E_VSI_DOWN, vsi->state)) {
		napi_complete(napi);
		return 0;
	}

	 
	i40e_for_each_ring(ring, q_vector->tx) {
		bool wd = ring->xsk_pool ?
			  i40e_clean_xdp_tx_irq(vsi, ring) :
			  i40e_clean_tx_irq(vsi, ring, budget, &tx_cleaned);

		if (!wd) {
			clean_complete = tx_clean_complete = false;
			continue;
		}
		arm_wb |= ring->arm_wb;
		ring->arm_wb = false;
	}

	 
	if (budget <= 0)
		goto tx_only;

	 
	if (unlikely(q_vector->num_ringpairs > 1))
		 
		budget_per_ring = max_t(int, budget / q_vector->num_ringpairs, 1);
	else
		 
		budget_per_ring = budget;

	i40e_for_each_ring(ring, q_vector->rx) {
		int cleaned = ring->xsk_pool ?
			      i40e_clean_rx_irq_zc(ring, budget_per_ring) :
			      i40e_clean_rx_irq(ring, budget_per_ring, &rx_cleaned);

		work_done += cleaned;
		 
		if (cleaned >= budget_per_ring)
			clean_complete = rx_clean_complete = false;
	}

	if (!i40e_enabled_xdp_vsi(vsi))
		trace_i40e_napi_poll(napi, q_vector, budget, budget_per_ring, rx_cleaned,
				     tx_cleaned, rx_clean_complete, tx_clean_complete);

	 
	if (!clean_complete) {
		int cpu_id = smp_processor_id();

		 
		if (!cpumask_test_cpu(cpu_id, &q_vector->affinity_mask)) {
			 
			napi_complete_done(napi, work_done);

			 
			i40e_force_wb(vsi, q_vector);

			 
			return budget - 1;
		}
tx_only:
		if (arm_wb) {
			q_vector->tx.ring[0].tx_stats.tx_force_wb++;
			i40e_enable_wb_on_itr(vsi, q_vector);
		}
		return budget;
	}

	if (q_vector->tx.ring[0].flags & I40E_TXR_FLAGS_WB_ON_ITR)
		q_vector->arm_wb_state = false;

	 
	if (likely(napi_complete_done(napi, work_done)))
		i40e_update_enable_itr(vsi, q_vector);

	return min(work_done, budget - 1);
}

 
static void i40e_atr(struct i40e_ring *tx_ring, struct sk_buff *skb,
		     u32 tx_flags)
{
	struct i40e_filter_program_desc *fdir_desc;
	struct i40e_pf *pf = tx_ring->vsi->back;
	union {
		unsigned char *network;
		struct iphdr *ipv4;
		struct ipv6hdr *ipv6;
	} hdr;
	struct tcphdr *th;
	unsigned int hlen;
	u32 flex_ptype, dtype_cmd;
	int l4_proto;
	u16 i;

	 
	if (!(pf->flags & I40E_FLAG_FD_ATR_ENABLED))
		return;

	if (test_bit(__I40E_FD_ATR_AUTO_DISABLED, pf->state))
		return;

	 
	if (!tx_ring->atr_sample_rate)
		return;

	 
	if (!(tx_flags & (I40E_TX_FLAGS_IPV4 | I40E_TX_FLAGS_IPV6)))
		return;

	 
	hdr.network = (tx_flags & I40E_TX_FLAGS_UDP_TUNNEL) ?
		      skb_inner_network_header(skb) : skb_network_header(skb);

	 
	if (tx_flags & I40E_TX_FLAGS_IPV4) {
		 
		hlen = (hdr.network[0] & 0x0F) << 2;
		l4_proto = hdr.ipv4->protocol;
	} else {
		 
		unsigned int inner_hlen = hdr.network - skb->data;
		unsigned int h_offset = inner_hlen;

		 
		l4_proto =
		  ipv6_find_hdr(skb, &h_offset, IPPROTO_TCP, NULL, NULL);
		 
		hlen = h_offset - inner_hlen;
	}

	if (l4_proto != IPPROTO_TCP)
		return;

	th = (struct tcphdr *)(hdr.network + hlen);

	 
	if (th->syn && test_bit(__I40E_FD_ATR_AUTO_DISABLED, pf->state))
		return;
	if (pf->flags & I40E_FLAG_HW_ATR_EVICT_ENABLED) {
		 
		if (th->fin || th->rst)
			return;
	}

	tx_ring->atr_count++;

	 
	if (!th->fin &&
	    !th->syn &&
	    !th->rst &&
	    (tx_ring->atr_count < tx_ring->atr_sample_rate))
		return;

	tx_ring->atr_count = 0;

	 
	i = tx_ring->next_to_use;
	fdir_desc = I40E_TX_FDIRDESC(tx_ring, i);

	i++;
	tx_ring->next_to_use = (i < tx_ring->count) ? i : 0;

	flex_ptype = (tx_ring->queue_index << I40E_TXD_FLTR_QW0_QINDEX_SHIFT) &
		      I40E_TXD_FLTR_QW0_QINDEX_MASK;
	flex_ptype |= (tx_flags & I40E_TX_FLAGS_IPV4) ?
		      (I40E_FILTER_PCTYPE_NONF_IPV4_TCP <<
		       I40E_TXD_FLTR_QW0_PCTYPE_SHIFT) :
		      (I40E_FILTER_PCTYPE_NONF_IPV6_TCP <<
		       I40E_TXD_FLTR_QW0_PCTYPE_SHIFT);

	flex_ptype |= tx_ring->vsi->id << I40E_TXD_FLTR_QW0_DEST_VSI_SHIFT;

	dtype_cmd = I40E_TX_DESC_DTYPE_FILTER_PROG;

	dtype_cmd |= (th->fin || th->rst) ?
		     (I40E_FILTER_PROGRAM_DESC_PCMD_REMOVE <<
		      I40E_TXD_FLTR_QW1_PCMD_SHIFT) :
		     (I40E_FILTER_PROGRAM_DESC_PCMD_ADD_UPDATE <<
		      I40E_TXD_FLTR_QW1_PCMD_SHIFT);

	dtype_cmd |= I40E_FILTER_PROGRAM_DESC_DEST_DIRECT_PACKET_QINDEX <<
		     I40E_TXD_FLTR_QW1_DEST_SHIFT;

	dtype_cmd |= I40E_FILTER_PROGRAM_DESC_FD_STATUS_FD_ID <<
		     I40E_TXD_FLTR_QW1_FD_STATUS_SHIFT;

	dtype_cmd |= I40E_TXD_FLTR_QW1_CNT_ENA_MASK;
	if (!(tx_flags & I40E_TX_FLAGS_UDP_TUNNEL))
		dtype_cmd |=
			((u32)I40E_FD_ATR_STAT_IDX(pf->hw.pf_id) <<
			I40E_TXD_FLTR_QW1_CNTINDEX_SHIFT) &
			I40E_TXD_FLTR_QW1_CNTINDEX_MASK;
	else
		dtype_cmd |=
			((u32)I40E_FD_ATR_TUNNEL_STAT_IDX(pf->hw.pf_id) <<
			I40E_TXD_FLTR_QW1_CNTINDEX_SHIFT) &
			I40E_TXD_FLTR_QW1_CNTINDEX_MASK;

	if (pf->flags & I40E_FLAG_HW_ATR_EVICT_ENABLED)
		dtype_cmd |= I40E_TXD_FLTR_QW1_ATR_MASK;

	fdir_desc->qindex_flex_ptype_vsi = cpu_to_le32(flex_ptype);
	fdir_desc->rsvd = cpu_to_le32(0);
	fdir_desc->dtype_cmd_cntindex = cpu_to_le32(dtype_cmd);
	fdir_desc->fd_id = cpu_to_le32(0);
}

 
static inline int i40e_tx_prepare_vlan_flags(struct sk_buff *skb,
					     struct i40e_ring *tx_ring,
					     u32 *flags)
{
	__be16 protocol = skb->protocol;
	u32  tx_flags = 0;

	if (protocol == htons(ETH_P_8021Q) &&
	    !(tx_ring->netdev->features & NETIF_F_HW_VLAN_CTAG_TX)) {
		 
		skb->protocol = vlan_get_protocol(skb);
		goto out;
	}

	 
	if (skb_vlan_tag_present(skb)) {
		tx_flags |= skb_vlan_tag_get(skb) << I40E_TX_FLAGS_VLAN_SHIFT;
		tx_flags |= I40E_TX_FLAGS_HW_VLAN;
	 
	} else if (protocol == htons(ETH_P_8021Q)) {
		struct vlan_hdr *vhdr, _vhdr;

		vhdr = skb_header_pointer(skb, ETH_HLEN, sizeof(_vhdr), &_vhdr);
		if (!vhdr)
			return -EINVAL;

		protocol = vhdr->h_vlan_encapsulated_proto;
		tx_flags |= ntohs(vhdr->h_vlan_TCI) << I40E_TX_FLAGS_VLAN_SHIFT;
		tx_flags |= I40E_TX_FLAGS_SW_VLAN;
	}

	if (!(tx_ring->vsi->back->flags & I40E_FLAG_DCB_ENABLED))
		goto out;

	 
	if ((tx_flags & (I40E_TX_FLAGS_HW_VLAN | I40E_TX_FLAGS_SW_VLAN)) ||
	    (skb->priority != TC_PRIO_CONTROL)) {
		tx_flags &= ~I40E_TX_FLAGS_VLAN_PRIO_MASK;
		tx_flags |= (skb->priority & 0x7) <<
				I40E_TX_FLAGS_VLAN_PRIO_SHIFT;
		if (tx_flags & I40E_TX_FLAGS_SW_VLAN) {
			struct vlan_ethhdr *vhdr;
			int rc;

			rc = skb_cow_head(skb, 0);
			if (rc < 0)
				return rc;
			vhdr = skb_vlan_eth_hdr(skb);
			vhdr->h_vlan_TCI = htons(tx_flags >>
						 I40E_TX_FLAGS_VLAN_SHIFT);
		} else {
			tx_flags |= I40E_TX_FLAGS_HW_VLAN;
		}
	}

out:
	*flags = tx_flags;
	return 0;
}

 
static int i40e_tso(struct i40e_tx_buffer *first, u8 *hdr_len,
		    u64 *cd_type_cmd_tso_mss)
{
	struct sk_buff *skb = first->skb;
	u64 cd_cmd, cd_tso_len, cd_mss;
	__be16 protocol;
	union {
		struct iphdr *v4;
		struct ipv6hdr *v6;
		unsigned char *hdr;
	} ip;
	union {
		struct tcphdr *tcp;
		struct udphdr *udp;
		unsigned char *hdr;
	} l4;
	u32 paylen, l4_offset;
	u16 gso_size;
	int err;

	if (skb->ip_summed != CHECKSUM_PARTIAL)
		return 0;

	if (!skb_is_gso(skb))
		return 0;

	err = skb_cow_head(skb, 0);
	if (err < 0)
		return err;

	protocol = vlan_get_protocol(skb);

	if (eth_p_mpls(protocol))
		ip.hdr = skb_inner_network_header(skb);
	else
		ip.hdr = skb_network_header(skb);
	l4.hdr = skb_checksum_start(skb);

	 
	if (ip.v4->version == 4) {
		ip.v4->tot_len = 0;
		ip.v4->check = 0;

		first->tx_flags |= I40E_TX_FLAGS_TSO;
	} else {
		ip.v6->payload_len = 0;
		first->tx_flags |= I40E_TX_FLAGS_TSO;
	}

	if (skb_shinfo(skb)->gso_type & (SKB_GSO_GRE |
					 SKB_GSO_GRE_CSUM |
					 SKB_GSO_IPXIP4 |
					 SKB_GSO_IPXIP6 |
					 SKB_GSO_UDP_TUNNEL |
					 SKB_GSO_UDP_TUNNEL_CSUM)) {
		if (!(skb_shinfo(skb)->gso_type & SKB_GSO_PARTIAL) &&
		    (skb_shinfo(skb)->gso_type & SKB_GSO_UDP_TUNNEL_CSUM)) {
			l4.udp->len = 0;

			 
			l4_offset = l4.hdr - skb->data;

			 
			paylen = skb->len - l4_offset;
			csum_replace_by_diff(&l4.udp->check,
					     (__force __wsum)htonl(paylen));
		}

		 
		ip.hdr = skb_inner_network_header(skb);
		l4.hdr = skb_inner_transport_header(skb);

		 
		if (ip.v4->version == 4) {
			ip.v4->tot_len = 0;
			ip.v4->check = 0;
		} else {
			ip.v6->payload_len = 0;
		}
	}

	 
	l4_offset = l4.hdr - skb->data;

	 
	paylen = skb->len - l4_offset;

	if (skb_shinfo(skb)->gso_type & SKB_GSO_UDP_L4) {
		csum_replace_by_diff(&l4.udp->check, (__force __wsum)htonl(paylen));
		 
		*hdr_len = sizeof(*l4.udp) + l4_offset;
	} else {
		csum_replace_by_diff(&l4.tcp->check, (__force __wsum)htonl(paylen));
		 
		*hdr_len = (l4.tcp->doff * 4) + l4_offset;
	}

	 
	gso_size = skb_shinfo(skb)->gso_size;

	 
	first->gso_segs = skb_shinfo(skb)->gso_segs;
	first->bytecount += (first->gso_segs - 1) * *hdr_len;

	 
	cd_cmd = I40E_TX_CTX_DESC_TSO;
	cd_tso_len = skb->len - *hdr_len;
	cd_mss = gso_size;
	*cd_type_cmd_tso_mss |= (cd_cmd << I40E_TXD_CTX_QW1_CMD_SHIFT) |
				(cd_tso_len << I40E_TXD_CTX_QW1_TSO_LEN_SHIFT) |
				(cd_mss << I40E_TXD_CTX_QW1_MSS_SHIFT);
	return 1;
}

 
static int i40e_tsyn(struct i40e_ring *tx_ring, struct sk_buff *skb,
		     u32 tx_flags, u64 *cd_type_cmd_tso_mss)
{
	struct i40e_pf *pf;

	if (likely(!(skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP)))
		return 0;

	 
	if (tx_flags & I40E_TX_FLAGS_TSO)
		return 0;

	 
	pf = i40e_netdev_to_pf(tx_ring->netdev);
	if (!(pf->flags & I40E_FLAG_PTP))
		return 0;

	if (pf->ptp_tx &&
	    !test_and_set_bit_lock(__I40E_PTP_TX_IN_PROGRESS, pf->state)) {
		skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
		pf->ptp_tx_start = jiffies;
		pf->ptp_tx_skb = skb_get(skb);
	} else {
		pf->tx_hwtstamp_skipped++;
		return 0;
	}

	*cd_type_cmd_tso_mss |= (u64)I40E_TX_CTX_DESC_TSYN <<
				I40E_TXD_CTX_QW1_CMD_SHIFT;

	return 1;
}

 
static int i40e_tx_enable_csum(struct sk_buff *skb, u32 *tx_flags,
			       u32 *td_cmd, u32 *td_offset,
			       struct i40e_ring *tx_ring,
			       u32 *cd_tunneling)
{
	union {
		struct iphdr *v4;
		struct ipv6hdr *v6;
		unsigned char *hdr;
	} ip;
	union {
		struct tcphdr *tcp;
		struct udphdr *udp;
		unsigned char *hdr;
	} l4;
	unsigned char *exthdr;
	u32 offset, cmd = 0;
	__be16 frag_off;
	__be16 protocol;
	u8 l4_proto = 0;

	if (skb->ip_summed != CHECKSUM_PARTIAL)
		return 0;

	protocol = vlan_get_protocol(skb);

	if (eth_p_mpls(protocol)) {
		ip.hdr = skb_inner_network_header(skb);
		l4.hdr = skb_checksum_start(skb);
	} else {
		ip.hdr = skb_network_header(skb);
		l4.hdr = skb_transport_header(skb);
	}

	 
	if (ip.v4->version == 4)
		*tx_flags |= I40E_TX_FLAGS_IPV4;
	else
		*tx_flags |= I40E_TX_FLAGS_IPV6;

	 
	offset = ((ip.hdr - skb->data) / 2) << I40E_TX_DESC_LENGTH_MACLEN_SHIFT;

	if (skb->encapsulation) {
		u32 tunnel = 0;
		 
		if (*tx_flags & I40E_TX_FLAGS_IPV4) {
			tunnel |= (*tx_flags & I40E_TX_FLAGS_TSO) ?
				  I40E_TX_CTX_EXT_IP_IPV4 :
				  I40E_TX_CTX_EXT_IP_IPV4_NO_CSUM;

			l4_proto = ip.v4->protocol;
		} else if (*tx_flags & I40E_TX_FLAGS_IPV6) {
			int ret;

			tunnel |= I40E_TX_CTX_EXT_IP_IPV6;

			exthdr = ip.hdr + sizeof(*ip.v6);
			l4_proto = ip.v6->nexthdr;
			ret = ipv6_skip_exthdr(skb, exthdr - skb->data,
					       &l4_proto, &frag_off);
			if (ret < 0)
				return -1;
		}

		 
		switch (l4_proto) {
		case IPPROTO_UDP:
			tunnel |= I40E_TXD_CTX_UDP_TUNNELING;
			*tx_flags |= I40E_TX_FLAGS_UDP_TUNNEL;
			break;
		case IPPROTO_GRE:
			tunnel |= I40E_TXD_CTX_GRE_TUNNELING;
			*tx_flags |= I40E_TX_FLAGS_UDP_TUNNEL;
			break;
		case IPPROTO_IPIP:
		case IPPROTO_IPV6:
			*tx_flags |= I40E_TX_FLAGS_UDP_TUNNEL;
			l4.hdr = skb_inner_network_header(skb);
			break;
		default:
			if (*tx_flags & I40E_TX_FLAGS_TSO)
				return -1;

			skb_checksum_help(skb);
			return 0;
		}

		 
		tunnel |= ((l4.hdr - ip.hdr) / 4) <<
			  I40E_TXD_CTX_QW0_EXT_IPLEN_SHIFT;

		 
		ip.hdr = skb_inner_network_header(skb);

		 
		tunnel |= ((ip.hdr - l4.hdr) / 2) <<
			  I40E_TXD_CTX_QW0_NATLEN_SHIFT;

		 
		if ((*tx_flags & I40E_TX_FLAGS_TSO) &&
		    !(skb_shinfo(skb)->gso_type & SKB_GSO_PARTIAL) &&
		    (skb_shinfo(skb)->gso_type & SKB_GSO_UDP_TUNNEL_CSUM))
			tunnel |= I40E_TXD_CTX_QW0_L4T_CS_MASK;

		 
		*cd_tunneling |= tunnel;

		 
		l4.hdr = skb_inner_transport_header(skb);
		l4_proto = 0;

		 
		*tx_flags &= ~(I40E_TX_FLAGS_IPV4 | I40E_TX_FLAGS_IPV6);
		if (ip.v4->version == 4)
			*tx_flags |= I40E_TX_FLAGS_IPV4;
		if (ip.v6->version == 6)
			*tx_flags |= I40E_TX_FLAGS_IPV6;
	}

	 
	if (*tx_flags & I40E_TX_FLAGS_IPV4) {
		l4_proto = ip.v4->protocol;
		 
		cmd |= (*tx_flags & I40E_TX_FLAGS_TSO) ?
		       I40E_TX_DESC_CMD_IIPT_IPV4_CSUM :
		       I40E_TX_DESC_CMD_IIPT_IPV4;
	} else if (*tx_flags & I40E_TX_FLAGS_IPV6) {
		cmd |= I40E_TX_DESC_CMD_IIPT_IPV6;

		exthdr = ip.hdr + sizeof(*ip.v6);
		l4_proto = ip.v6->nexthdr;
		if (l4.hdr != exthdr)
			ipv6_skip_exthdr(skb, exthdr - skb->data,
					 &l4_proto, &frag_off);
	}

	 
	offset |= ((l4.hdr - ip.hdr) / 4) << I40E_TX_DESC_LENGTH_IPLEN_SHIFT;

	 
	switch (l4_proto) {
	case IPPROTO_TCP:
		 
		cmd |= I40E_TX_DESC_CMD_L4T_EOFT_TCP;
		offset |= l4.tcp->doff << I40E_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	case IPPROTO_SCTP:
		 
		cmd |= I40E_TX_DESC_CMD_L4T_EOFT_SCTP;
		offset |= (sizeof(struct sctphdr) >> 2) <<
			  I40E_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	case IPPROTO_UDP:
		 
		cmd |= I40E_TX_DESC_CMD_L4T_EOFT_UDP;
		offset |= (sizeof(struct udphdr) >> 2) <<
			  I40E_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	default:
		if (*tx_flags & I40E_TX_FLAGS_TSO)
			return -1;
		skb_checksum_help(skb);
		return 0;
	}

	*td_cmd |= cmd;
	*td_offset |= offset;

	return 1;
}

 
static void i40e_create_tx_ctx(struct i40e_ring *tx_ring,
			       const u64 cd_type_cmd_tso_mss,
			       const u32 cd_tunneling, const u32 cd_l2tag2)
{
	struct i40e_tx_context_desc *context_desc;
	int i = tx_ring->next_to_use;

	if ((cd_type_cmd_tso_mss == I40E_TX_DESC_DTYPE_CONTEXT) &&
	    !cd_tunneling && !cd_l2tag2)
		return;

	 
	context_desc = I40E_TX_CTXTDESC(tx_ring, i);

	i++;
	tx_ring->next_to_use = (i < tx_ring->count) ? i : 0;

	 
	context_desc->tunneling_params = cpu_to_le32(cd_tunneling);
	context_desc->l2tag2 = cpu_to_le16(cd_l2tag2);
	context_desc->rsvd = cpu_to_le16(0);
	context_desc->type_cmd_tso_mss = cpu_to_le64(cd_type_cmd_tso_mss);
}

 
int __i40e_maybe_stop_tx(struct i40e_ring *tx_ring, int size)
{
	netif_stop_subqueue(tx_ring->netdev, tx_ring->queue_index);
	 
	smp_mb();

	++tx_ring->tx_stats.tx_stopped;

	 
	if (likely(I40E_DESC_UNUSED(tx_ring) < size))
		return -EBUSY;

	 
	netif_start_subqueue(tx_ring->netdev, tx_ring->queue_index);
	++tx_ring->tx_stats.restart_queue;
	return 0;
}

 
bool __i40e_chk_linearize(struct sk_buff *skb)
{
	const skb_frag_t *frag, *stale;
	int nr_frags, sum;

	 
	nr_frags = skb_shinfo(skb)->nr_frags;
	if (nr_frags < (I40E_MAX_BUFFER_TXD - 1))
		return false;

	 
	nr_frags -= I40E_MAX_BUFFER_TXD - 2;
	frag = &skb_shinfo(skb)->frags[0];

	 
	sum = 1 - skb_shinfo(skb)->gso_size;

	 
	sum += skb_frag_size(frag++);
	sum += skb_frag_size(frag++);
	sum += skb_frag_size(frag++);
	sum += skb_frag_size(frag++);
	sum += skb_frag_size(frag++);

	 
	for (stale = &skb_shinfo(skb)->frags[0];; stale++) {
		int stale_size = skb_frag_size(stale);

		sum += skb_frag_size(frag++);

		 
		if (stale_size > I40E_MAX_DATA_PER_TXD) {
			int align_pad = -(skb_frag_off(stale)) &
					(I40E_MAX_READ_REQ_SIZE - 1);

			sum -= align_pad;
			stale_size -= align_pad;

			do {
				sum -= I40E_MAX_DATA_PER_TXD_ALIGNED;
				stale_size -= I40E_MAX_DATA_PER_TXD_ALIGNED;
			} while (stale_size > I40E_MAX_DATA_PER_TXD);
		}

		 
		if (sum < 0)
			return true;

		if (!nr_frags--)
			break;

		sum -= stale_size;
	}

	return false;
}

 
static inline int i40e_tx_map(struct i40e_ring *tx_ring, struct sk_buff *skb,
			      struct i40e_tx_buffer *first, u32 tx_flags,
			      const u8 hdr_len, u32 td_cmd, u32 td_offset)
{
	unsigned int data_len = skb->data_len;
	unsigned int size = skb_headlen(skb);
	skb_frag_t *frag;
	struct i40e_tx_buffer *tx_bi;
	struct i40e_tx_desc *tx_desc;
	u16 i = tx_ring->next_to_use;
	u32 td_tag = 0;
	dma_addr_t dma;
	u16 desc_count = 1;

	if (tx_flags & I40E_TX_FLAGS_HW_VLAN) {
		td_cmd |= I40E_TX_DESC_CMD_IL2TAG1;
		td_tag = (tx_flags & I40E_TX_FLAGS_VLAN_MASK) >>
			 I40E_TX_FLAGS_VLAN_SHIFT;
	}

	first->tx_flags = tx_flags;

	dma = dma_map_single(tx_ring->dev, skb->data, size, DMA_TO_DEVICE);

	tx_desc = I40E_TX_DESC(tx_ring, i);
	tx_bi = first;

	for (frag = &skb_shinfo(skb)->frags[0];; frag++) {
		unsigned int max_data = I40E_MAX_DATA_PER_TXD_ALIGNED;

		if (dma_mapping_error(tx_ring->dev, dma))
			goto dma_error;

		 
		dma_unmap_len_set(tx_bi, len, size);
		dma_unmap_addr_set(tx_bi, dma, dma);

		 
		max_data += -dma & (I40E_MAX_READ_REQ_SIZE - 1);
		tx_desc->buffer_addr = cpu_to_le64(dma);

		while (unlikely(size > I40E_MAX_DATA_PER_TXD)) {
			tx_desc->cmd_type_offset_bsz =
				build_ctob(td_cmd, td_offset,
					   max_data, td_tag);

			tx_desc++;
			i++;
			desc_count++;

			if (i == tx_ring->count) {
				tx_desc = I40E_TX_DESC(tx_ring, 0);
				i = 0;
			}

			dma += max_data;
			size -= max_data;

			max_data = I40E_MAX_DATA_PER_TXD_ALIGNED;
			tx_desc->buffer_addr = cpu_to_le64(dma);
		}

		if (likely(!data_len))
			break;

		tx_desc->cmd_type_offset_bsz = build_ctob(td_cmd, td_offset,
							  size, td_tag);

		tx_desc++;
		i++;
		desc_count++;

		if (i == tx_ring->count) {
			tx_desc = I40E_TX_DESC(tx_ring, 0);
			i = 0;
		}

		size = skb_frag_size(frag);
		data_len -= size;

		dma = skb_frag_dma_map(tx_ring->dev, frag, 0, size,
				       DMA_TO_DEVICE);

		tx_bi = &tx_ring->tx_bi[i];
	}

	netdev_tx_sent_queue(txring_txq(tx_ring), first->bytecount);

	i++;
	if (i == tx_ring->count)
		i = 0;

	tx_ring->next_to_use = i;

	i40e_maybe_stop_tx(tx_ring, DESC_NEEDED);

	 
	td_cmd |= I40E_TX_DESC_CMD_EOP;

	 
	desc_count |= ++tx_ring->packet_stride;

	if (desc_count >= WB_STRIDE) {
		 
		td_cmd |= I40E_TX_DESC_CMD_RS;
		tx_ring->packet_stride = 0;
	}

	tx_desc->cmd_type_offset_bsz =
			build_ctob(td_cmd, td_offset, size, td_tag);

	skb_tx_timestamp(skb);

	 
	wmb();

	 
	first->next_to_watch = tx_desc;

	 
	if (netif_xmit_stopped(txring_txq(tx_ring)) || !netdev_xmit_more()) {
		writel(i, tx_ring->tail);
	}

	return 0;

dma_error:
	dev_info(tx_ring->dev, "TX DMA map failed\n");

	 
	for (;;) {
		tx_bi = &tx_ring->tx_bi[i];
		i40e_unmap_and_free_tx_resource(tx_ring, tx_bi);
		if (tx_bi == first)
			break;
		if (i == 0)
			i = tx_ring->count;
		i--;
	}

	tx_ring->next_to_use = i;

	return -1;
}

static u16 i40e_swdcb_skb_tx_hash(struct net_device *dev,
				  const struct sk_buff *skb,
				  u16 num_tx_queues)
{
	u32 jhash_initval_salt = 0xd631614b;
	u32 hash;

	if (skb->sk && skb->sk->sk_hash)
		hash = skb->sk->sk_hash;
	else
		hash = (__force u16)skb->protocol ^ skb->hash;

	hash = jhash_1word(hash, jhash_initval_salt);

	return (u16)(((u64)hash * num_tx_queues) >> 32);
}

u16 i40e_lan_select_queue(struct net_device *netdev,
			  struct sk_buff *skb,
			  struct net_device __always_unused *sb_dev)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_vsi *vsi = np->vsi;
	struct i40e_hw *hw;
	u16 qoffset;
	u16 qcount;
	u8 tclass;
	u16 hash;
	u8 prio;

	 
	if (vsi->tc_config.numtc == 1 ||
	    i40e_is_tc_mqprio_enabled(vsi->back))
		return netdev_pick_tx(netdev, skb, sb_dev);

	prio = skb->priority;
	hw = &vsi->back->hw;
	tclass = hw->local_dcbx_config.etscfg.prioritytable[prio];
	 
	if (unlikely(!(vsi->tc_config.enabled_tc & BIT(tclass))))
		tclass = 0;

	 
	qcount = vsi->tc_config.tc_info[tclass].qcount;
	hash = i40e_swdcb_skb_tx_hash(netdev, skb, qcount);

	qoffset = vsi->tc_config.tc_info[tclass].qoffset;
	return qoffset + hash;
}

 
static int i40e_xmit_xdp_ring(struct xdp_frame *xdpf,
			      struct i40e_ring *xdp_ring)
{
	struct skb_shared_info *sinfo = xdp_get_shared_info_from_frame(xdpf);
	u8 nr_frags = unlikely(xdp_frame_has_frags(xdpf)) ? sinfo->nr_frags : 0;
	u16 i = 0, index = xdp_ring->next_to_use;
	struct i40e_tx_buffer *tx_head = &xdp_ring->tx_bi[index];
	struct i40e_tx_buffer *tx_bi = tx_head;
	struct i40e_tx_desc *tx_desc = I40E_TX_DESC(xdp_ring, index);
	void *data = xdpf->data;
	u32 size = xdpf->len;

	if (unlikely(I40E_DESC_UNUSED(xdp_ring) < 1 + nr_frags)) {
		xdp_ring->tx_stats.tx_busy++;
		return I40E_XDP_CONSUMED;
	}

	tx_head->bytecount = xdp_get_frame_len(xdpf);
	tx_head->gso_segs = 1;
	tx_head->xdpf = xdpf;

	for (;;) {
		dma_addr_t dma;

		dma = dma_map_single(xdp_ring->dev, data, size, DMA_TO_DEVICE);
		if (dma_mapping_error(xdp_ring->dev, dma))
			goto unmap;

		 
		dma_unmap_len_set(tx_bi, len, size);
		dma_unmap_addr_set(tx_bi, dma, dma);

		tx_desc->buffer_addr = cpu_to_le64(dma);
		tx_desc->cmd_type_offset_bsz =
			build_ctob(I40E_TX_DESC_CMD_ICRC, 0, size, 0);

		if (++index == xdp_ring->count)
			index = 0;

		if (i == nr_frags)
			break;

		tx_bi = &xdp_ring->tx_bi[index];
		tx_desc = I40E_TX_DESC(xdp_ring, index);

		data = skb_frag_address(&sinfo->frags[i]);
		size = skb_frag_size(&sinfo->frags[i]);
		i++;
	}

	tx_desc->cmd_type_offset_bsz |=
		cpu_to_le64(I40E_TXD_CMD << I40E_TXD_QW1_CMD_SHIFT);

	 
	smp_wmb();

	xdp_ring->xdp_tx_active++;

	tx_head->next_to_watch = tx_desc;
	xdp_ring->next_to_use = index;

	return I40E_XDP_TX;

unmap:
	for (;;) {
		tx_bi = &xdp_ring->tx_bi[index];
		if (dma_unmap_len(tx_bi, len))
			dma_unmap_page(xdp_ring->dev,
				       dma_unmap_addr(tx_bi, dma),
				       dma_unmap_len(tx_bi, len),
				       DMA_TO_DEVICE);
		dma_unmap_len_set(tx_bi, len, 0);
		if (tx_bi == tx_head)
			break;

		if (!index)
			index += xdp_ring->count;
		index--;
	}

	return I40E_XDP_CONSUMED;
}

 
static netdev_tx_t i40e_xmit_frame_ring(struct sk_buff *skb,
					struct i40e_ring *tx_ring)
{
	u64 cd_type_cmd_tso_mss = I40E_TX_DESC_DTYPE_CONTEXT;
	u32 cd_tunneling = 0, cd_l2tag2 = 0;
	struct i40e_tx_buffer *first;
	u32 td_offset = 0;
	u32 tx_flags = 0;
	u32 td_cmd = 0;
	u8 hdr_len = 0;
	int tso, count;
	int tsyn;

	 
	prefetch(skb->data);

	i40e_trace(xmit_frame_ring, skb, tx_ring);

	count = i40e_xmit_descriptor_count(skb);
	if (i40e_chk_linearize(skb, count)) {
		if (__skb_linearize(skb)) {
			dev_kfree_skb_any(skb);
			return NETDEV_TX_OK;
		}
		count = i40e_txd_use_count(skb->len);
		tx_ring->tx_stats.tx_linearize++;
	}

	 
	if (i40e_maybe_stop_tx(tx_ring, count + 4 + 1)) {
		tx_ring->tx_stats.tx_busy++;
		return NETDEV_TX_BUSY;
	}

	 
	first = &tx_ring->tx_bi[tx_ring->next_to_use];
	first->skb = skb;
	first->bytecount = skb->len;
	first->gso_segs = 1;

	 
	if (i40e_tx_prepare_vlan_flags(skb, tx_ring, &tx_flags))
		goto out_drop;

	tso = i40e_tso(first, &hdr_len, &cd_type_cmd_tso_mss);

	if (tso < 0)
		goto out_drop;
	else if (tso)
		tx_flags |= I40E_TX_FLAGS_TSO;

	 
	tso = i40e_tx_enable_csum(skb, &tx_flags, &td_cmd, &td_offset,
				  tx_ring, &cd_tunneling);
	if (tso < 0)
		goto out_drop;

	tsyn = i40e_tsyn(tx_ring, skb, tx_flags, &cd_type_cmd_tso_mss);

	if (tsyn)
		tx_flags |= I40E_TX_FLAGS_TSYN;

	 
	td_cmd |= I40E_TX_DESC_CMD_ICRC;

	i40e_create_tx_ctx(tx_ring, cd_type_cmd_tso_mss,
			   cd_tunneling, cd_l2tag2);

	 
	i40e_atr(tx_ring, skb, tx_flags);

	if (i40e_tx_map(tx_ring, skb, first, tx_flags, hdr_len,
			td_cmd, td_offset))
		goto cleanup_tx_tstamp;

	return NETDEV_TX_OK;

out_drop:
	i40e_trace(xmit_frame_ring_drop, first->skb, tx_ring);
	dev_kfree_skb_any(first->skb);
	first->skb = NULL;
cleanup_tx_tstamp:
	if (unlikely(tx_flags & I40E_TX_FLAGS_TSYN)) {
		struct i40e_pf *pf = i40e_netdev_to_pf(tx_ring->netdev);

		dev_kfree_skb_any(pf->ptp_tx_skb);
		pf->ptp_tx_skb = NULL;
		clear_bit_unlock(__I40E_PTP_TX_IN_PROGRESS, pf->state);
	}

	return NETDEV_TX_OK;
}

 
netdev_tx_t i40e_lan_xmit_frame(struct sk_buff *skb, struct net_device *netdev)
{
	struct i40e_netdev_priv *np = netdev_priv(netdev);
	struct i40e_vsi *vsi = np->vsi;
	struct i40e_ring *tx_ring = vsi->tx_rings[skb->queue_mapping];

	 
	if (skb_put_padto(skb, I40E_MIN_TX_LEN))
		return NETDEV_TX_OK;

	return i40e_xmit_frame_ring(skb, tx_ring);
}

 
int i40e_xdp_xmit(struct net_device *dev, int n, struct xdp_frame **frames,
		  u32 flags)
{
	struct i40e_netdev_priv *np = netdev_priv(dev);
	unsigned int queue_index = smp_processor_id();
	struct i40e_vsi *vsi = np->vsi;
	struct i40e_pf *pf = vsi->back;
	struct i40e_ring *xdp_ring;
	int nxmit = 0;
	int i;

	if (test_bit(__I40E_VSI_DOWN, vsi->state))
		return -ENETDOWN;

	if (!i40e_enabled_xdp_vsi(vsi) || queue_index >= vsi->num_queue_pairs ||
	    test_bit(__I40E_CONFIG_BUSY, pf->state))
		return -ENXIO;

	if (unlikely(flags & ~XDP_XMIT_FLAGS_MASK))
		return -EINVAL;

	xdp_ring = vsi->xdp_rings[queue_index];

	for (i = 0; i < n; i++) {
		struct xdp_frame *xdpf = frames[i];
		int err;

		err = i40e_xmit_xdp_ring(xdpf, xdp_ring);
		if (err != I40E_XDP_TX)
			break;
		nxmit++;
	}

	if (unlikely(flags & XDP_XMIT_FLUSH))
		i40e_xdp_ring_update_tail(xdp_ring);

	return nxmit;
}
