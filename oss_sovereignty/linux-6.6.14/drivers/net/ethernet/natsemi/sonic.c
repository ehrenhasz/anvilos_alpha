
 

 

static unsigned int version_printed;

static int sonic_debug = -1;
module_param(sonic_debug, int, 0);
MODULE_PARM_DESC(sonic_debug, "debug message level");

static void sonic_msg_init(struct net_device *dev)
{
	struct sonic_local *lp = netdev_priv(dev);

	lp->msg_enable = netif_msg_init(sonic_debug, 0);

	if (version_printed++ == 0)
		netif_dbg(lp, drv, dev, "%s", version);
}

static int sonic_alloc_descriptors(struct net_device *dev)
{
	struct sonic_local *lp = netdev_priv(dev);

	 
	lp->descriptors =
		dma_alloc_coherent(lp->device,
				   SIZEOF_SONIC_DESC *
				   SONIC_BUS_SCALE(lp->dma_bitmode),
				   &lp->descriptors_laddr, GFP_KERNEL);

	if (!lp->descriptors)
		return -ENOMEM;

	lp->cda = lp->descriptors;
	lp->tda = lp->cda + SIZEOF_SONIC_CDA *
			    SONIC_BUS_SCALE(lp->dma_bitmode);
	lp->rda = lp->tda + SIZEOF_SONIC_TD * SONIC_NUM_TDS *
			    SONIC_BUS_SCALE(lp->dma_bitmode);
	lp->rra = lp->rda + SIZEOF_SONIC_RD * SONIC_NUM_RDS *
			    SONIC_BUS_SCALE(lp->dma_bitmode);

	lp->cda_laddr = lp->descriptors_laddr;
	lp->tda_laddr = lp->cda_laddr + SIZEOF_SONIC_CDA *
					SONIC_BUS_SCALE(lp->dma_bitmode);
	lp->rda_laddr = lp->tda_laddr + SIZEOF_SONIC_TD * SONIC_NUM_TDS *
					SONIC_BUS_SCALE(lp->dma_bitmode);
	lp->rra_laddr = lp->rda_laddr + SIZEOF_SONIC_RD * SONIC_NUM_RDS *
					SONIC_BUS_SCALE(lp->dma_bitmode);

	return 0;
}

 
static int sonic_open(struct net_device *dev)
{
	struct sonic_local *lp = netdev_priv(dev);
	int i;

	netif_dbg(lp, ifup, dev, "%s: initializing sonic driver\n", __func__);

	spin_lock_init(&lp->lock);

	for (i = 0; i < SONIC_NUM_RRS; i++) {
		struct sk_buff *skb = netdev_alloc_skb(dev, SONIC_RBSIZE + 2);
		if (skb == NULL) {
			while(i > 0) {  
				i--;
				dev_kfree_skb(lp->rx_skb[i]);
				lp->rx_skb[i] = NULL;
			}
			printk(KERN_ERR "%s: couldn't allocate receive buffers\n",
			       dev->name);
			return -ENOMEM;
		}
		 
		if (SONIC_BUS_SCALE(lp->dma_bitmode) == 2)
			skb_reserve(skb, 2);
		lp->rx_skb[i] = skb;
	}

	for (i = 0; i < SONIC_NUM_RRS; i++) {
		dma_addr_t laddr = dma_map_single(lp->device, skb_put(lp->rx_skb[i], SONIC_RBSIZE),
		                                  SONIC_RBSIZE, DMA_FROM_DEVICE);
		if (dma_mapping_error(lp->device, laddr)) {
			while(i > 0) {  
				i--;
				dma_unmap_single(lp->device, lp->rx_laddr[i], SONIC_RBSIZE, DMA_FROM_DEVICE);
				lp->rx_laddr[i] = (dma_addr_t)0;
			}
			for (i = 0; i < SONIC_NUM_RRS; i++) {
				dev_kfree_skb(lp->rx_skb[i]);
				lp->rx_skb[i] = NULL;
			}
			printk(KERN_ERR "%s: couldn't map rx DMA buffers\n",
			       dev->name);
			return -ENOMEM;
		}
		lp->rx_laddr[i] = laddr;
	}

	 
	sonic_init(dev, true);

	netif_start_queue(dev);

	netif_dbg(lp, ifup, dev, "%s: Initialization done\n", __func__);

	return 0;
}

 
static void sonic_quiesce(struct net_device *dev, u16 mask, bool may_sleep)
{
	struct sonic_local * __maybe_unused lp = netdev_priv(dev);
	int i;
	u16 bits;

	for (i = 0; i < 1000; ++i) {
		bits = SONIC_READ(SONIC_CMD) & mask;
		if (!bits)
			return;
		if (!may_sleep)
			udelay(20);
		else
			usleep_range(100, 200);
	}
	WARN_ONCE(1, "command deadline expired! 0x%04x\n", bits);
}

 
static int sonic_close(struct net_device *dev)
{
	struct sonic_local *lp = netdev_priv(dev);
	int i;

	netif_dbg(lp, ifdown, dev, "%s\n", __func__);

	netif_stop_queue(dev);

	 
	SONIC_WRITE(SONIC_CMD, SONIC_CR_RXDIS);
	sonic_quiesce(dev, SONIC_CR_ALL, true);

	SONIC_WRITE(SONIC_IMR, 0);
	SONIC_WRITE(SONIC_ISR, 0x7fff);
	SONIC_WRITE(SONIC_CMD, SONIC_CR_RST);

	 
	for (i = 0; i < SONIC_NUM_TDS; i++) {
		if(lp->tx_laddr[i]) {
			dma_unmap_single(lp->device, lp->tx_laddr[i], lp->tx_len[i], DMA_TO_DEVICE);
			lp->tx_laddr[i] = (dma_addr_t)0;
		}
		if(lp->tx_skb[i]) {
			dev_kfree_skb(lp->tx_skb[i]);
			lp->tx_skb[i] = NULL;
		}
	}

	 
	for (i = 0; i < SONIC_NUM_RRS; i++) {
		if(lp->rx_laddr[i]) {
			dma_unmap_single(lp->device, lp->rx_laddr[i], SONIC_RBSIZE, DMA_FROM_DEVICE);
			lp->rx_laddr[i] = (dma_addr_t)0;
		}
		if(lp->rx_skb[i]) {
			dev_kfree_skb(lp->rx_skb[i]);
			lp->rx_skb[i] = NULL;
		}
	}

	return 0;
}

static void sonic_tx_timeout(struct net_device *dev, unsigned int txqueue)
{
	struct sonic_local *lp = netdev_priv(dev);
	int i;
	 
	SONIC_WRITE(SONIC_CMD, SONIC_CR_RXDIS);
	sonic_quiesce(dev, SONIC_CR_ALL, false);

	SONIC_WRITE(SONIC_IMR, 0);
	SONIC_WRITE(SONIC_ISR, 0x7fff);
	SONIC_WRITE(SONIC_CMD, SONIC_CR_RST);
	 
	for (i = 0; i < SONIC_NUM_TDS; i++) {
		if(lp->tx_laddr[i]) {
			dma_unmap_single(lp->device, lp->tx_laddr[i], lp->tx_len[i], DMA_TO_DEVICE);
			lp->tx_laddr[i] = (dma_addr_t)0;
		}
		if(lp->tx_skb[i]) {
			dev_kfree_skb(lp->tx_skb[i]);
			lp->tx_skb[i] = NULL;
		}
	}
	 
	sonic_init(dev, false);
	lp->stats.tx_errors++;
	netif_trans_update(dev);  
	netif_wake_queue(dev);
}

 

static int sonic_send_packet(struct sk_buff *skb, struct net_device *dev)
{
	struct sonic_local *lp = netdev_priv(dev);
	dma_addr_t laddr;
	int length;
	int entry;
	unsigned long flags;

	netif_dbg(lp, tx_queued, dev, "%s: skb=%p\n", __func__, skb);

	length = skb->len;
	if (length < ETH_ZLEN) {
		if (skb_padto(skb, ETH_ZLEN))
			return NETDEV_TX_OK;
		length = ETH_ZLEN;
	}

	 

	laddr = dma_map_single(lp->device, skb->data, length, DMA_TO_DEVICE);
	if (dma_mapping_error(lp->device, laddr)) {
		pr_err_ratelimited("%s: failed to map tx DMA buffer.\n", dev->name);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	spin_lock_irqsave(&lp->lock, flags);

	entry = (lp->eol_tx + 1) & SONIC_TDS_MASK;

	sonic_tda_put(dev, entry, SONIC_TD_STATUS, 0);        
	sonic_tda_put(dev, entry, SONIC_TD_FRAG_COUNT, 1);    
	sonic_tda_put(dev, entry, SONIC_TD_PKTSIZE, length);  
	sonic_tda_put(dev, entry, SONIC_TD_FRAG_PTR_L, laddr & 0xffff);
	sonic_tda_put(dev, entry, SONIC_TD_FRAG_PTR_H, laddr >> 16);
	sonic_tda_put(dev, entry, SONIC_TD_FRAG_SIZE, length);
	sonic_tda_put(dev, entry, SONIC_TD_LINK,
		sonic_tda_get(dev, entry, SONIC_TD_LINK) | SONIC_EOL);

	sonic_tda_put(dev, lp->eol_tx, SONIC_TD_LINK, ~SONIC_EOL &
		      sonic_tda_get(dev, lp->eol_tx, SONIC_TD_LINK));

	netif_dbg(lp, tx_queued, dev, "%s: issuing Tx command\n", __func__);

	SONIC_WRITE(SONIC_CMD, SONIC_CR_TXP);

	lp->tx_len[entry] = length;
	lp->tx_laddr[entry] = laddr;
	lp->tx_skb[entry] = skb;

	lp->eol_tx = entry;

	entry = (entry + 1) & SONIC_TDS_MASK;
	if (lp->tx_skb[entry]) {
		 
		netif_dbg(lp, tx_queued, dev, "%s: stopping queue\n", __func__);
		netif_stop_queue(dev);
		 
	}

	spin_unlock_irqrestore(&lp->lock, flags);

	return NETDEV_TX_OK;
}

 
static irqreturn_t sonic_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct sonic_local *lp = netdev_priv(dev);
	int status;
	unsigned long flags;

	 
	spin_lock_irqsave(&lp->lock, flags);

	status = SONIC_READ(SONIC_ISR) & SONIC_IMR_DEFAULT;
	if (!status) {
		spin_unlock_irqrestore(&lp->lock, flags);

		return IRQ_NONE;
	}

	do {
		SONIC_WRITE(SONIC_ISR, status);  

		if (status & SONIC_INT_PKTRX) {
			netif_dbg(lp, intr, dev, "%s: packet rx\n", __func__);
			sonic_rx(dev);	 
		}

		if (status & SONIC_INT_TXDN) {
			int entry = lp->cur_tx;
			int td_status;
			int freed_some = 0;

			 

			netif_dbg(lp, intr, dev, "%s: tx done\n", __func__);

			while (lp->tx_skb[entry] != NULL) {
				if ((td_status = sonic_tda_get(dev, entry, SONIC_TD_STATUS)) == 0)
					break;

				if (td_status & SONIC_TCR_PTX) {
					lp->stats.tx_packets++;
					lp->stats.tx_bytes += sonic_tda_get(dev, entry, SONIC_TD_PKTSIZE);
				} else {
					if (td_status & (SONIC_TCR_EXD |
					    SONIC_TCR_EXC | SONIC_TCR_BCM))
						lp->stats.tx_aborted_errors++;
					if (td_status &
					    (SONIC_TCR_NCRS | SONIC_TCR_CRLS))
						lp->stats.tx_carrier_errors++;
					if (td_status & SONIC_TCR_OWC)
						lp->stats.tx_window_errors++;
					if (td_status & SONIC_TCR_FU)
						lp->stats.tx_fifo_errors++;
				}

				 
				dev_consume_skb_irq(lp->tx_skb[entry]);
				lp->tx_skb[entry] = NULL;
				 
				dma_unmap_single(lp->device, lp->tx_laddr[entry], lp->tx_len[entry], DMA_TO_DEVICE);
				lp->tx_laddr[entry] = (dma_addr_t)0;
				freed_some = 1;

				if (sonic_tda_get(dev, entry, SONIC_TD_LINK) & SONIC_EOL) {
					entry = (entry + 1) & SONIC_TDS_MASK;
					break;
				}
				entry = (entry + 1) & SONIC_TDS_MASK;
			}

			if (freed_some || lp->tx_skb[entry] == NULL)
				netif_wake_queue(dev);   
			lp->cur_tx = entry;
		}

		 
		if (status & SONIC_INT_RFO) {
			netif_dbg(lp, rx_err, dev, "%s: rx fifo overrun\n",
				  __func__);
		}
		if (status & SONIC_INT_RDE) {
			netif_dbg(lp, rx_err, dev, "%s: rx descriptors exhausted\n",
				  __func__);
		}
		if (status & SONIC_INT_RBAE) {
			netif_dbg(lp, rx_err, dev, "%s: rx buffer area exceeded\n",
				  __func__);
		}

		 
		if (status & SONIC_INT_FAE)
			lp->stats.rx_frame_errors += 65536;
		if (status & SONIC_INT_CRC)
			lp->stats.rx_crc_errors += 65536;
		if (status & SONIC_INT_MP)
			lp->stats.rx_missed_errors += 65536;

		 
		if (status & SONIC_INT_TXER) {
			u16 tcr = SONIC_READ(SONIC_TCR);

			netif_dbg(lp, tx_err, dev, "%s: TXER intr, TCR %04x\n",
				  __func__, tcr);

			if (tcr & (SONIC_TCR_EXD | SONIC_TCR_EXC |
				   SONIC_TCR_FU | SONIC_TCR_BCM)) {
				 
				netif_stop_queue(dev);
				SONIC_WRITE(SONIC_CMD, SONIC_CR_TXP);
			}
		}

		 
		if (status & SONIC_INT_BR) {
			printk(KERN_ERR "%s: Bus retry occurred! Device interrupt disabled.\n",
				dev->name);
			 
			 
			SONIC_WRITE(SONIC_IMR, 0);
		}

		status = SONIC_READ(SONIC_ISR) & SONIC_IMR_DEFAULT;
	} while (status);

	spin_unlock_irqrestore(&lp->lock, flags);

	return IRQ_HANDLED;
}

 
static int index_from_addr(struct sonic_local *lp, dma_addr_t addr,
			   unsigned int last)
{
	unsigned int i = last;

	do {
		i = (i + 1) & SONIC_RRS_MASK;
		if (addr == lp->rx_laddr[i])
			return i;
	} while (i != last);

	return -ENOENT;
}

 
static bool sonic_alloc_rb(struct net_device *dev, struct sonic_local *lp,
			   struct sk_buff **new_skb, dma_addr_t *new_addr)
{
	*new_skb = netdev_alloc_skb(dev, SONIC_RBSIZE + 2);
	if (!*new_skb)
		return false;

	if (SONIC_BUS_SCALE(lp->dma_bitmode) == 2)
		skb_reserve(*new_skb, 2);

	*new_addr = dma_map_single(lp->device, skb_put(*new_skb, SONIC_RBSIZE),
				   SONIC_RBSIZE, DMA_FROM_DEVICE);
	if (dma_mapping_error(lp->device, *new_addr)) {
		dev_kfree_skb(*new_skb);
		*new_skb = NULL;
		return false;
	}

	return true;
}

 
static void sonic_update_rra(struct net_device *dev, struct sonic_local *lp,
			     dma_addr_t old_addr, dma_addr_t new_addr)
{
	unsigned int entry = sonic_rr_entry(dev, SONIC_READ(SONIC_RWP));
	unsigned int end = sonic_rr_entry(dev, SONIC_READ(SONIC_RRP));
	u32 buf;

	 
	do {
		buf = (sonic_rra_get(dev, entry, SONIC_RR_BUFADR_H) << 16) |
		      sonic_rra_get(dev, entry, SONIC_RR_BUFADR_L);

		if (buf == old_addr)
			break;

		entry = (entry + 1) & SONIC_RRS_MASK;
	} while (entry != end);

	WARN_ONCE(buf != old_addr, "failed to find resource!\n");

	sonic_rra_put(dev, entry, SONIC_RR_BUFADR_H, new_addr >> 16);
	sonic_rra_put(dev, entry, SONIC_RR_BUFADR_L, new_addr & 0xffff);

	entry = (entry + 1) & SONIC_RRS_MASK;

	SONIC_WRITE(SONIC_RWP, sonic_rr_addr(dev, entry));
}

 
static void sonic_rx(struct net_device *dev)
{
	struct sonic_local *lp = netdev_priv(dev);
	int entry = lp->cur_rx;
	int prev_entry = lp->eol_rx;
	bool rbe = false;

	while (sonic_rda_get(dev, entry, SONIC_RD_IN_USE) == 0) {
		u16 status = sonic_rda_get(dev, entry, SONIC_RD_STATUS);

		 
		if ((status & SONIC_RCR_PRX) && (status & SONIC_RCR_LPKT)) {
			struct sk_buff *new_skb;
			dma_addr_t new_laddr;
			u32 addr = (sonic_rda_get(dev, entry,
						  SONIC_RD_PKTPTR_H) << 16) |
				   sonic_rda_get(dev, entry, SONIC_RD_PKTPTR_L);
			int i = index_from_addr(lp, addr, entry);

			if (i < 0) {
				WARN_ONCE(1, "failed to find buffer!\n");
				break;
			}

			if (sonic_alloc_rb(dev, lp, &new_skb, &new_laddr)) {
				struct sk_buff *used_skb = lp->rx_skb[i];
				int pkt_len;

				 
				dma_unmap_single(lp->device, addr, SONIC_RBSIZE,
						 DMA_FROM_DEVICE);

				pkt_len = sonic_rda_get(dev, entry,
							SONIC_RD_PKTLEN);
				skb_trim(used_skb, pkt_len);
				used_skb->protocol = eth_type_trans(used_skb,
								    dev);
				netif_rx(used_skb);
				lp->stats.rx_packets++;
				lp->stats.rx_bytes += pkt_len;

				lp->rx_skb[i] = new_skb;
				lp->rx_laddr[i] = new_laddr;
			} else {
				 
				new_laddr = addr;
				lp->stats.rx_dropped++;
			}
			 
			rbe = rbe || SONIC_READ(SONIC_ISR) & SONIC_INT_RBE;
			sonic_update_rra(dev, lp, addr, new_laddr);
		}
		 
		sonic_rda_put(dev, entry, SONIC_RD_STATUS, 0);
		sonic_rda_put(dev, entry, SONIC_RD_IN_USE, 1);

		prev_entry = entry;
		entry = (entry + 1) & SONIC_RDS_MASK;
	}

	lp->cur_rx = entry;

	if (prev_entry != lp->eol_rx) {
		 
		sonic_rda_put(dev, prev_entry, SONIC_RD_LINK, SONIC_EOL |
			      sonic_rda_get(dev, prev_entry, SONIC_RD_LINK));
		sonic_rda_put(dev, lp->eol_rx, SONIC_RD_LINK, ~SONIC_EOL &
			      sonic_rda_get(dev, lp->eol_rx, SONIC_RD_LINK));
		lp->eol_rx = prev_entry;
	}

	if (rbe)
		SONIC_WRITE(SONIC_ISR, SONIC_INT_RBE);
}


 
static struct net_device_stats *sonic_get_stats(struct net_device *dev)
{
	struct sonic_local *lp = netdev_priv(dev);

	 
	lp->stats.rx_crc_errors += SONIC_READ(SONIC_CRCT);
	SONIC_WRITE(SONIC_CRCT, 0xffff);
	lp->stats.rx_frame_errors += SONIC_READ(SONIC_FAET);
	SONIC_WRITE(SONIC_FAET, 0xffff);
	lp->stats.rx_missed_errors += SONIC_READ(SONIC_MPT);
	SONIC_WRITE(SONIC_MPT, 0xffff);

	return &lp->stats;
}


 
static void sonic_multicast_list(struct net_device *dev)
{
	struct sonic_local *lp = netdev_priv(dev);
	unsigned int rcr;
	struct netdev_hw_addr *ha;
	unsigned char *addr;
	int i;

	rcr = SONIC_READ(SONIC_RCR) & ~(SONIC_RCR_PRO | SONIC_RCR_AMC);
	rcr |= SONIC_RCR_BRD;	 

	if (dev->flags & IFF_PROMISC) {	 
		rcr |= SONIC_RCR_PRO;
	} else {
		if ((dev->flags & IFF_ALLMULTI) ||
		    (netdev_mc_count(dev) > 15)) {
			rcr |= SONIC_RCR_AMC;
		} else {
			unsigned long flags;

			netif_dbg(lp, ifup, dev, "%s: mc_count %d\n", __func__,
				  netdev_mc_count(dev));
			sonic_set_cam_enable(dev, 1);   
			i = 1;
			netdev_for_each_mc_addr(ha, dev) {
				addr = ha->addr;
				sonic_cda_put(dev, i, SONIC_CD_CAP0, addr[1] << 8 | addr[0]);
				sonic_cda_put(dev, i, SONIC_CD_CAP1, addr[3] << 8 | addr[2]);
				sonic_cda_put(dev, i, SONIC_CD_CAP2, addr[5] << 8 | addr[4]);
				sonic_set_cam_enable(dev, sonic_get_cam_enable(dev) | (1 << i));
				i++;
			}
			SONIC_WRITE(SONIC_CDC, 16);
			SONIC_WRITE(SONIC_CDP, lp->cda_laddr & 0xffff);

			 
			spin_lock_irqsave(&lp->lock, flags);
			sonic_quiesce(dev, SONIC_CR_TXP, false);
			SONIC_WRITE(SONIC_CMD, SONIC_CR_LCAM);
			sonic_quiesce(dev, SONIC_CR_LCAM, false);
			spin_unlock_irqrestore(&lp->lock, flags);
		}
	}

	netif_dbg(lp, ifup, dev, "%s: setting RCR=%x\n", __func__, rcr);

	SONIC_WRITE(SONIC_RCR, rcr);
}


 
static int sonic_init(struct net_device *dev, bool may_sleep)
{
	struct sonic_local *lp = netdev_priv(dev);
	int i;

	 
	SONIC_WRITE(SONIC_IMR, 0);
	SONIC_WRITE(SONIC_ISR, 0x7fff);
	SONIC_WRITE(SONIC_CMD, SONIC_CR_RST);

	 
	SONIC_WRITE(SONIC_CE, 0);

	 
	SONIC_WRITE(SONIC_CMD, 0);
	SONIC_WRITE(SONIC_CMD, SONIC_CR_RXDIS | SONIC_CR_STP);
	sonic_quiesce(dev, SONIC_CR_ALL, may_sleep);

	 
	netif_dbg(lp, ifup, dev, "%s: initialize receive resource area\n",
		  __func__);

	for (i = 0; i < SONIC_NUM_RRS; i++) {
		u16 bufadr_l = (unsigned long)lp->rx_laddr[i] & 0xffff;
		u16 bufadr_h = (unsigned long)lp->rx_laddr[i] >> 16;
		sonic_rra_put(dev, i, SONIC_RR_BUFADR_L, bufadr_l);
		sonic_rra_put(dev, i, SONIC_RR_BUFADR_H, bufadr_h);
		sonic_rra_put(dev, i, SONIC_RR_BUFSIZE_L, SONIC_RBSIZE >> 1);
		sonic_rra_put(dev, i, SONIC_RR_BUFSIZE_H, 0);
	}

	 
	SONIC_WRITE(SONIC_RSA, sonic_rr_addr(dev, 0));
	SONIC_WRITE(SONIC_REA, sonic_rr_addr(dev, SONIC_NUM_RRS));
	SONIC_WRITE(SONIC_RRP, sonic_rr_addr(dev, 0));
	SONIC_WRITE(SONIC_RWP, sonic_rr_addr(dev, SONIC_NUM_RRS - 1));
	SONIC_WRITE(SONIC_URRA, lp->rra_laddr >> 16);
	SONIC_WRITE(SONIC_EOBC, (SONIC_RBSIZE >> 1) - (lp->dma_bitmode ? 2 : 1));

	 
	netif_dbg(lp, ifup, dev, "%s: issuing RRRA command\n", __func__);

	SONIC_WRITE(SONIC_CMD, SONIC_CR_RRRA);
	sonic_quiesce(dev, SONIC_CR_RRRA, may_sleep);

	 
	netif_dbg(lp, ifup, dev, "%s: initialize receive descriptors\n",
		  __func__);

	for (i=0; i<SONIC_NUM_RDS; i++) {
		sonic_rda_put(dev, i, SONIC_RD_STATUS, 0);
		sonic_rda_put(dev, i, SONIC_RD_PKTLEN, 0);
		sonic_rda_put(dev, i, SONIC_RD_PKTPTR_L, 0);
		sonic_rda_put(dev, i, SONIC_RD_PKTPTR_H, 0);
		sonic_rda_put(dev, i, SONIC_RD_SEQNO, 0);
		sonic_rda_put(dev, i, SONIC_RD_IN_USE, 1);
		sonic_rda_put(dev, i, SONIC_RD_LINK,
			lp->rda_laddr +
			((i+1) * SIZEOF_SONIC_RD * SONIC_BUS_SCALE(lp->dma_bitmode)));
	}
	 
	sonic_rda_put(dev, SONIC_NUM_RDS - 1, SONIC_RD_LINK,
		(lp->rda_laddr & 0xffff) | SONIC_EOL);
	lp->eol_rx = SONIC_NUM_RDS - 1;
	lp->cur_rx = 0;
	SONIC_WRITE(SONIC_URDA, lp->rda_laddr >> 16);
	SONIC_WRITE(SONIC_CRDA, lp->rda_laddr & 0xffff);

	 
	netif_dbg(lp, ifup, dev, "%s: initialize transmit descriptors\n",
		  __func__);

	for (i = 0; i < SONIC_NUM_TDS; i++) {
		sonic_tda_put(dev, i, SONIC_TD_STATUS, 0);
		sonic_tda_put(dev, i, SONIC_TD_CONFIG, 0);
		sonic_tda_put(dev, i, SONIC_TD_PKTSIZE, 0);
		sonic_tda_put(dev, i, SONIC_TD_FRAG_COUNT, 0);
		sonic_tda_put(dev, i, SONIC_TD_LINK,
			(lp->tda_laddr & 0xffff) +
			(i + 1) * SIZEOF_SONIC_TD * SONIC_BUS_SCALE(lp->dma_bitmode));
		lp->tx_skb[i] = NULL;
	}
	 
	sonic_tda_put(dev, SONIC_NUM_TDS - 1, SONIC_TD_LINK,
		(lp->tda_laddr & 0xffff));

	SONIC_WRITE(SONIC_UTDA, lp->tda_laddr >> 16);
	SONIC_WRITE(SONIC_CTDA, lp->tda_laddr & 0xffff);
	lp->cur_tx = 0;
	lp->eol_tx = SONIC_NUM_TDS - 1;

	 
	sonic_cda_put(dev, 0, SONIC_CD_CAP0, dev->dev_addr[1] << 8 | dev->dev_addr[0]);
	sonic_cda_put(dev, 0, SONIC_CD_CAP1, dev->dev_addr[3] << 8 | dev->dev_addr[2]);
	sonic_cda_put(dev, 0, SONIC_CD_CAP2, dev->dev_addr[5] << 8 | dev->dev_addr[4]);
	sonic_set_cam_enable(dev, 1);

	for (i = 0; i < 16; i++)
		sonic_cda_put(dev, i, SONIC_CD_ENTRY_POINTER, i);

	 
	SONIC_WRITE(SONIC_CDP, lp->cda_laddr & 0xffff);
	SONIC_WRITE(SONIC_CDC, 16);

	 
	SONIC_WRITE(SONIC_CMD, SONIC_CR_LCAM);
	sonic_quiesce(dev, SONIC_CR_LCAM, may_sleep);

	 
	SONIC_WRITE(SONIC_RCR, SONIC_RCR_DEFAULT);
	SONIC_WRITE(SONIC_TCR, SONIC_TCR_DEFAULT);
	SONIC_WRITE(SONIC_ISR, 0x7fff);
	SONIC_WRITE(SONIC_IMR, SONIC_IMR_DEFAULT);
	SONIC_WRITE(SONIC_CMD, SONIC_CR_RXEN);

	netif_dbg(lp, ifup, dev, "%s: new status=%x\n", __func__,
		  SONIC_READ(SONIC_CMD));

	return 0;
}

MODULE_LICENSE("GPL");
