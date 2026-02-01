













#include <asm/unaligned.h>
#include <linux/bitfield.h>

#include "mcp251xfd.h"

static inline struct
mcp251xfd_tx_obj *mcp251xfd_get_tx_obj_next(struct mcp251xfd_tx_ring *tx_ring)
{
	u8 tx_head;

	tx_head = mcp251xfd_get_tx_head(tx_ring);

	return &tx_ring->obj[tx_head];
}

static void
mcp251xfd_tx_obj_from_skb(const struct mcp251xfd_priv *priv,
			  struct mcp251xfd_tx_obj *tx_obj,
			  const struct sk_buff *skb,
			  unsigned int seq)
{
	const struct canfd_frame *cfd = (struct canfd_frame *)skb->data;
	struct mcp251xfd_hw_tx_obj_raw *hw_tx_obj;
	union mcp251xfd_tx_obj_load_buf *load_buf;
	u8 dlc;
	u32 id, flags;
	int len_sanitized = 0, len;

	if (cfd->can_id & CAN_EFF_FLAG) {
		u32 sid, eid;

		sid = FIELD_GET(MCP251XFD_REG_FRAME_EFF_SID_MASK, cfd->can_id);
		eid = FIELD_GET(MCP251XFD_REG_FRAME_EFF_EID_MASK, cfd->can_id);

		id = FIELD_PREP(MCP251XFD_OBJ_ID_EID_MASK, eid) |
			FIELD_PREP(MCP251XFD_OBJ_ID_SID_MASK, sid);

		flags = MCP251XFD_OBJ_FLAGS_IDE;
	} else {
		id = FIELD_PREP(MCP251XFD_OBJ_ID_SID_MASK, cfd->can_id);
		flags = 0;
	}

	 
	flags |= FIELD_PREP(MCP251XFD_OBJ_FLAGS_SEQ_MCP2518FD_MASK, seq);

	if (cfd->can_id & CAN_RTR_FLAG)
		flags |= MCP251XFD_OBJ_FLAGS_RTR;
	else
		len_sanitized = canfd_sanitize_len(cfd->len);

	 
	if (can_is_canfd_skb(skb)) {
		if (cfd->flags & CANFD_ESI)
			flags |= MCP251XFD_OBJ_FLAGS_ESI;

		flags |= MCP251XFD_OBJ_FLAGS_FDF;

		if (cfd->flags & CANFD_BRS)
			flags |= MCP251XFD_OBJ_FLAGS_BRS;

		dlc = can_fd_len2dlc(cfd->len);
	} else {
		dlc = can_get_cc_dlc((struct can_frame *)cfd,
				     priv->can.ctrlmode);
	}

	flags |= FIELD_PREP(MCP251XFD_OBJ_FLAGS_DLC_MASK, dlc);

	load_buf = &tx_obj->buf;
	if (priv->devtype_data.quirks & MCP251XFD_QUIRK_CRC_TX)
		hw_tx_obj = &load_buf->crc.hw_tx_obj;
	else
		hw_tx_obj = &load_buf->nocrc.hw_tx_obj;

	put_unaligned_le32(id, &hw_tx_obj->id);
	put_unaligned_le32(flags, &hw_tx_obj->flags);

	 
	memcpy(hw_tx_obj->data, cfd->data, cfd->len);

	 
	if (MCP251XFD_SANITIZE_CAN && len_sanitized) {
		int pad_len;

		pad_len = len_sanitized - cfd->len;
		if (pad_len)
			memset(hw_tx_obj->data + cfd->len, 0x0, pad_len);
	}

	 
	len = sizeof(hw_tx_obj->id) + sizeof(hw_tx_obj->flags);
	if (MCP251XFD_SANITIZE_CAN)
		len += round_up(len_sanitized, sizeof(u32));
	else
		len += round_up(cfd->len, sizeof(u32));

	if (priv->devtype_data.quirks & MCP251XFD_QUIRK_CRC_TX) {
		u16 crc;

		mcp251xfd_spi_cmd_crc_set_len_in_ram(&load_buf->crc.cmd,
						     len);
		 
		len += sizeof(load_buf->crc.cmd);
		crc = mcp251xfd_crc16_compute(&load_buf->crc, len);
		put_unaligned_be16(crc, (void *)load_buf + len);

		 
		len += sizeof(load_buf->crc.crc);
	} else {
		len += sizeof(load_buf->nocrc.cmd);
	}

	tx_obj->xfer[0].len = len;
}

static int mcp251xfd_tx_obj_write(const struct mcp251xfd_priv *priv,
				  struct mcp251xfd_tx_obj *tx_obj)
{
	return spi_async(priv->spi, &tx_obj->msg);
}

static bool mcp251xfd_tx_busy(const struct mcp251xfd_priv *priv,
			      struct mcp251xfd_tx_ring *tx_ring)
{
	if (mcp251xfd_get_tx_free(tx_ring) > 0)
		return false;

	netif_stop_queue(priv->ndev);

	 
	smp_mb();

	if (mcp251xfd_get_tx_free(tx_ring) == 0) {
		netdev_dbg(priv->ndev,
			   "Stopping tx-queue (tx_head=0x%08x, tx_tail=0x%08x, len=%d).\n",
			   tx_ring->head, tx_ring->tail,
			   tx_ring->head - tx_ring->tail);

		return true;
	}

	netif_start_queue(priv->ndev);

	return false;
}

netdev_tx_t mcp251xfd_start_xmit(struct sk_buff *skb,
				 struct net_device *ndev)
{
	struct mcp251xfd_priv *priv = netdev_priv(ndev);
	struct mcp251xfd_tx_ring *tx_ring = priv->tx;
	struct mcp251xfd_tx_obj *tx_obj;
	unsigned int frame_len;
	u8 tx_head;
	int err;

	if (can_dev_dropped_skb(ndev, skb))
		return NETDEV_TX_OK;

	if (mcp251xfd_tx_busy(priv, tx_ring))
		return NETDEV_TX_BUSY;

	tx_obj = mcp251xfd_get_tx_obj_next(tx_ring);
	mcp251xfd_tx_obj_from_skb(priv, tx_obj, skb, tx_ring->head);

	 
	tx_head = mcp251xfd_get_tx_head(tx_ring);
	tx_ring->head++;
	if (mcp251xfd_get_tx_free(tx_ring) == 0)
		netif_stop_queue(ndev);

	frame_len = can_skb_get_frame_len(skb);
	err = can_put_echo_skb(skb, ndev, tx_head, frame_len);
	if (!err)
		netdev_sent_queue(priv->ndev, frame_len);

	err = mcp251xfd_tx_obj_write(priv, tx_obj);
	if (err)
		goto out_err;

	return NETDEV_TX_OK;

 out_err:
	netdev_err(priv->ndev, "ERROR in %s: %d\n", __func__, err);

	return NETDEV_TX_OK;
}
