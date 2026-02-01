
 
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/stddef.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/export.h>

#include <asm/io.h>
#include <soc/fsl/qe/immap_qe.h>
#include <soc/fsl/qe/qe.h>

#include <soc/fsl/qe/ucc.h>
#include <soc/fsl/qe/ucc_slow.h>

u32 ucc_slow_get_qe_cr_subblock(int uccs_num)
{
	switch (uccs_num) {
	case 0: return QE_CR_SUBBLOCK_UCCSLOW1;
	case 1: return QE_CR_SUBBLOCK_UCCSLOW2;
	case 2: return QE_CR_SUBBLOCK_UCCSLOW3;
	case 3: return QE_CR_SUBBLOCK_UCCSLOW4;
	case 4: return QE_CR_SUBBLOCK_UCCSLOW5;
	case 5: return QE_CR_SUBBLOCK_UCCSLOW6;
	case 6: return QE_CR_SUBBLOCK_UCCSLOW7;
	case 7: return QE_CR_SUBBLOCK_UCCSLOW8;
	default: return QE_CR_SUBBLOCK_INVALID;
	}
}
EXPORT_SYMBOL(ucc_slow_get_qe_cr_subblock);

void ucc_slow_graceful_stop_tx(struct ucc_slow_private * uccs)
{
	struct ucc_slow_info *us_info = uccs->us_info;
	u32 id;

	id = ucc_slow_get_qe_cr_subblock(us_info->ucc_num);
	qe_issue_cmd(QE_GRACEFUL_STOP_TX, id,
			 QE_CR_PROTOCOL_UNSPECIFIED, 0);
}
EXPORT_SYMBOL(ucc_slow_graceful_stop_tx);

void ucc_slow_stop_tx(struct ucc_slow_private * uccs)
{
	struct ucc_slow_info *us_info = uccs->us_info;
	u32 id;

	id = ucc_slow_get_qe_cr_subblock(us_info->ucc_num);
	qe_issue_cmd(QE_STOP_TX, id, QE_CR_PROTOCOL_UNSPECIFIED, 0);
}
EXPORT_SYMBOL(ucc_slow_stop_tx);

void ucc_slow_restart_tx(struct ucc_slow_private * uccs)
{
	struct ucc_slow_info *us_info = uccs->us_info;
	u32 id;

	id = ucc_slow_get_qe_cr_subblock(us_info->ucc_num);
	qe_issue_cmd(QE_RESTART_TX, id, QE_CR_PROTOCOL_UNSPECIFIED, 0);
}
EXPORT_SYMBOL(ucc_slow_restart_tx);

void ucc_slow_enable(struct ucc_slow_private * uccs, enum comm_dir mode)
{
	struct ucc_slow __iomem *us_regs;
	u32 gumr_l;

	us_regs = uccs->us_regs;

	 
	gumr_l = ioread32be(&us_regs->gumr_l);
	if (mode & COMM_DIR_TX) {
		gumr_l |= UCC_SLOW_GUMR_L_ENT;
		uccs->enabled_tx = 1;
	}
	if (mode & COMM_DIR_RX) {
		gumr_l |= UCC_SLOW_GUMR_L_ENR;
		uccs->enabled_rx = 1;
	}
	iowrite32be(gumr_l, &us_regs->gumr_l);
}
EXPORT_SYMBOL(ucc_slow_enable);

void ucc_slow_disable(struct ucc_slow_private * uccs, enum comm_dir mode)
{
	struct ucc_slow __iomem *us_regs;
	u32 gumr_l;

	us_regs = uccs->us_regs;

	 
	gumr_l = ioread32be(&us_regs->gumr_l);
	if (mode & COMM_DIR_TX) {
		gumr_l &= ~UCC_SLOW_GUMR_L_ENT;
		uccs->enabled_tx = 0;
	}
	if (mode & COMM_DIR_RX) {
		gumr_l &= ~UCC_SLOW_GUMR_L_ENR;
		uccs->enabled_rx = 0;
	}
	iowrite32be(gumr_l, &us_regs->gumr_l);
}
EXPORT_SYMBOL(ucc_slow_disable);

 
int ucc_slow_init(struct ucc_slow_info * us_info, struct ucc_slow_private ** uccs_ret)
{
	struct ucc_slow_private *uccs;
	u32 i;
	struct ucc_slow __iomem *us_regs;
	u32 gumr;
	struct qe_bd __iomem *bd;
	u32 id;
	u32 command;
	int ret = 0;

	if (!us_info)
		return -EINVAL;

	 
	if ((us_info->ucc_num < 0) || (us_info->ucc_num > UCC_MAX_NUM - 1)) {
		printk(KERN_ERR "%s: illegal UCC number\n", __func__);
		return -EINVAL;
	}

	 
	if ((!us_info->rfw) &&
		(us_info->max_rx_buf_length & (UCC_SLOW_MRBLR_ALIGNMENT - 1))) {
		printk(KERN_ERR "max_rx_buf_length not aligned.\n");
		return -EINVAL;
	}

	uccs = kzalloc(sizeof(struct ucc_slow_private), GFP_KERNEL);
	if (!uccs) {
		printk(KERN_ERR "%s: Cannot allocate private data\n",
			__func__);
		return -ENOMEM;
	}
	uccs->rx_base_offset = -1;
	uccs->tx_base_offset = -1;
	uccs->us_pram_offset = -1;

	 
	uccs->us_info = us_info;
	 
	uccs->us_regs = ioremap(us_info->regs, sizeof(struct ucc_slow));
	if (uccs->us_regs == NULL) {
		printk(KERN_ERR "%s: Cannot map UCC registers\n", __func__);
		kfree(uccs);
		return -ENOMEM;
	}

	us_regs = uccs->us_regs;
	uccs->p_ucce = &us_regs->ucce;
	uccs->p_uccm = &us_regs->uccm;

	 
	uccs->us_pram_offset =
		qe_muram_alloc(UCC_SLOW_PRAM_SIZE, ALIGNMENT_OF_UCC_SLOW_PRAM);
	if (uccs->us_pram_offset < 0) {
		printk(KERN_ERR "%s: cannot allocate MURAM for PRAM", __func__);
		ucc_slow_free(uccs);
		return -ENOMEM;
	}
	id = ucc_slow_get_qe_cr_subblock(us_info->ucc_num);
	qe_issue_cmd(QE_ASSIGN_PAGE_TO_DEVICE, id, us_info->protocol,
		     uccs->us_pram_offset);

	uccs->us_pram = qe_muram_addr(uccs->us_pram_offset);

	 
	ret = ucc_set_type(us_info->ucc_num, UCC_SPEED_TYPE_SLOW);
	if (ret) {
		printk(KERN_ERR "%s: cannot set UCC type", __func__);
		ucc_slow_free(uccs);
		return ret;
	}

	iowrite16be(us_info->max_rx_buf_length, &uccs->us_pram->mrblr);

	INIT_LIST_HEAD(&uccs->confQ);

	 
	uccs->rx_base_offset =
		qe_muram_alloc(us_info->rx_bd_ring_len * sizeof(struct qe_bd),
				QE_ALIGNMENT_OF_BD);
	if (uccs->rx_base_offset < 0) {
		printk(KERN_ERR "%s: cannot allocate %u RX BDs\n", __func__,
			us_info->rx_bd_ring_len);
		ucc_slow_free(uccs);
		return -ENOMEM;
	}

	uccs->tx_base_offset =
		qe_muram_alloc(us_info->tx_bd_ring_len * sizeof(struct qe_bd),
			QE_ALIGNMENT_OF_BD);
	if (uccs->tx_base_offset < 0) {
		printk(KERN_ERR "%s: cannot allocate TX BDs", __func__);
		ucc_slow_free(uccs);
		return -ENOMEM;
	}

	 
	bd = uccs->confBd = uccs->tx_bd = qe_muram_addr(uccs->tx_base_offset);
	for (i = 0; i < us_info->tx_bd_ring_len - 1; i++) {
		 
		iowrite32be(0, &bd->buf);
		 
		iowrite32be(0, (u32 __iomem *)bd);
		bd++;
	}
	 
	iowrite32be(0, &bd->buf);
	iowrite32be(T_W, (u32 __iomem *)bd);

	 
	bd = uccs->rx_bd = qe_muram_addr(uccs->rx_base_offset);
	for (i = 0; i < us_info->rx_bd_ring_len - 1; i++) {
		 
		iowrite32be(0, (u32 __iomem *)bd);
		 
		iowrite32be(0, &bd->buf);
		bd++;
	}
	 
	iowrite32be(R_W, (u32 __iomem *)bd);
	iowrite32be(0, &bd->buf);

	 
	 
	gumr = us_info->tcrc;
	if (us_info->cdp)
		gumr |= UCC_SLOW_GUMR_H_CDP;
	if (us_info->ctsp)
		gumr |= UCC_SLOW_GUMR_H_CTSP;
	if (us_info->cds)
		gumr |= UCC_SLOW_GUMR_H_CDS;
	if (us_info->ctss)
		gumr |= UCC_SLOW_GUMR_H_CTSS;
	if (us_info->tfl)
		gumr |= UCC_SLOW_GUMR_H_TFL;
	if (us_info->rfw)
		gumr |= UCC_SLOW_GUMR_H_RFW;
	if (us_info->txsy)
		gumr |= UCC_SLOW_GUMR_H_TXSY;
	if (us_info->rtsm)
		gumr |= UCC_SLOW_GUMR_H_RTSM;
	iowrite32be(gumr, &us_regs->gumr_h);

	 
	gumr = (u32)us_info->tdcr | (u32)us_info->rdcr | (u32)us_info->tenc |
	       (u32)us_info->renc | (u32)us_info->diag | (u32)us_info->mode;
	if (us_info->tci)
		gumr |= UCC_SLOW_GUMR_L_TCI;
	if (us_info->rinv)
		gumr |= UCC_SLOW_GUMR_L_RINV;
	if (us_info->tinv)
		gumr |= UCC_SLOW_GUMR_L_TINV;
	if (us_info->tend)
		gumr |= UCC_SLOW_GUMR_L_TEND;
	iowrite32be(gumr, &us_regs->gumr_l);

	 

	 
	 
	iowrite8(UCC_BMR_BO_BE, &uccs->us_pram->tbmr);
	iowrite8(UCC_BMR_BO_BE, &uccs->us_pram->rbmr);

	 
	iowrite16be(uccs->rx_base_offset, &uccs->us_pram->rbase);
	iowrite16be(uccs->tx_base_offset, &uccs->us_pram->tbase);

	 
	 
	ucc_set_qe_mux_grant(us_info->ucc_num, us_info->grant_support);
	 
	ucc_set_qe_mux_bkpt(us_info->ucc_num, us_info->brkpt_support);
	 
	ucc_set_qe_mux_tsa(us_info->ucc_num, us_info->tsa);
	 
	if (!us_info->tsa) {
		 
		if (ucc_set_qe_mux_rxtx(us_info->ucc_num, us_info->rx_clock,
					COMM_DIR_RX)) {
			printk(KERN_ERR "%s: illegal value for RX clock\n",
			       __func__);
			ucc_slow_free(uccs);
			return -EINVAL;
		}
		 
		if (ucc_set_qe_mux_rxtx(us_info->ucc_num, us_info->tx_clock,
					COMM_DIR_TX)) {
			printk(KERN_ERR "%s: illegal value for TX clock\n",
			       __func__);
			ucc_slow_free(uccs);
			return -EINVAL;
		}
	}

	 
	iowrite16be(us_info->uccm_mask, &us_regs->uccm);

	 

	 
	iowrite16be(0xffff, &us_regs->ucce);

	 
	if (us_info->init_tx && us_info->init_rx)
		command = QE_INIT_TX_RX;
	else if (us_info->init_tx)
		command = QE_INIT_TX;
	else
		command = QE_INIT_RX;	 

	qe_issue_cmd(command, id, us_info->protocol, 0);

	*uccs_ret = uccs;
	return 0;
}
EXPORT_SYMBOL(ucc_slow_init);

void ucc_slow_free(struct ucc_slow_private * uccs)
{
	if (!uccs)
		return;

	qe_muram_free(uccs->rx_base_offset);
	qe_muram_free(uccs->tx_base_offset);
	qe_muram_free(uccs->us_pram_offset);

	if (uccs->us_regs)
		iounmap(uccs->us_regs);

	kfree(uccs);
}
EXPORT_SYMBOL(ucc_slow_free);

