
 

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/rawnand.h>
#include <linux/mtd/partitions.h>

#include <asm/io.h>
#include <asm/fsl_lbc.h>

#define MAX_BANKS 8
#define ERR_BYTE 0xFF  
#define FCM_TIMEOUT_MSECS 500  

 

struct fsl_elbc_mtd {
	struct nand_chip chip;
	struct fsl_lbc_ctrl *ctrl;

	struct device *dev;
	int bank;                
	u8 __iomem *vbase;       
	int page_size;           
	unsigned int fmr;        
};

 

struct fsl_elbc_fcm_ctrl {
	struct nand_controller controller;
	struct fsl_elbc_mtd *chips[MAX_BANKS];

	u8 __iomem *addr;         
	unsigned int page;        
	unsigned int read_bytes;  
	unsigned int column;      
	unsigned int index;       
	unsigned int status;      
	unsigned int mdr;         
	unsigned int use_mdr;     
	unsigned int oob;         
	unsigned int counter;	  
	unsigned int max_bitflips;   
};

 

static int fsl_elbc_ooblayout_ecc(struct mtd_info *mtd, int section,
				  struct mtd_oob_region *oobregion)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct fsl_elbc_mtd *priv = nand_get_controller_data(chip);

	if (section >= chip->ecc.steps)
		return -ERANGE;

	oobregion->offset = (16 * section) + 6;
	if (priv->fmr & FMR_ECCM)
		oobregion->offset += 2;

	oobregion->length = chip->ecc.bytes;

	return 0;
}

static int fsl_elbc_ooblayout_free(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *oobregion)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct fsl_elbc_mtd *priv = nand_get_controller_data(chip);

	if (section > chip->ecc.steps)
		return -ERANGE;

	if (!section) {
		oobregion->offset = 0;
		if (mtd->writesize > 512)
			oobregion->offset++;
		oobregion->length = (priv->fmr & FMR_ECCM) ? 7 : 5;
	} else {
		oobregion->offset = (16 * section) -
				    ((priv->fmr & FMR_ECCM) ? 5 : 7);
		if (section < chip->ecc.steps)
			oobregion->length = 13;
		else
			oobregion->length = mtd->oobsize - oobregion->offset;
	}

	return 0;
}

static const struct mtd_ooblayout_ops fsl_elbc_ooblayout_ops = {
	.ecc = fsl_elbc_ooblayout_ecc,
	.free = fsl_elbc_ooblayout_free,
};

 
static u8 bbt_pattern[] = {'B', 'b', 't', '0' };
static u8 mirror_pattern[] = {'1', 't', 'b', 'B' };

static struct nand_bbt_descr bbt_main_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE |
		   NAND_BBT_2BIT | NAND_BBT_VERSION,
	.offs =	11,
	.len = 4,
	.veroffs = 15,
	.maxblocks = 4,
	.pattern = bbt_pattern,
};

static struct nand_bbt_descr bbt_mirror_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE |
		   NAND_BBT_2BIT | NAND_BBT_VERSION,
	.offs =	11,
	.len = 4,
	.veroffs = 15,
	.maxblocks = 4,
	.pattern = mirror_pattern,
};

 

 
static void set_addr(struct mtd_info *mtd, int column, int page_addr, int oob)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct fsl_elbc_mtd *priv = nand_get_controller_data(chip);
	struct fsl_lbc_ctrl *ctrl = priv->ctrl;
	struct fsl_lbc_regs __iomem *lbc = ctrl->regs;
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = ctrl->nand;
	int buf_num;

	elbc_fcm_ctrl->page = page_addr;

	if (priv->page_size) {
		 
		out_be32(&lbc->fbar, page_addr >> 6);
		out_be32(&lbc->fpar,
		         ((page_addr << FPAR_LP_PI_SHIFT) & FPAR_LP_PI) |
		         (oob ? FPAR_LP_MS : 0) | column);
		buf_num = (page_addr & 1) << 2;
	} else {
		 
		out_be32(&lbc->fbar, page_addr >> 5);
		out_be32(&lbc->fpar,
		         ((page_addr << FPAR_SP_PI_SHIFT) & FPAR_SP_PI) |
		         (oob ? FPAR_SP_MS : 0) | column);
		buf_num = page_addr & 7;
	}

	elbc_fcm_ctrl->addr = priv->vbase + buf_num * 1024;
	elbc_fcm_ctrl->index = column;

	 
	if (oob)
		elbc_fcm_ctrl->index += priv->page_size ? 2048 : 512;

	dev_vdbg(priv->dev, "set_addr: bank=%d, "
			    "elbc_fcm_ctrl->addr=0x%p (0x%p), "
	                    "index %x, pes %d ps %d\n",
		 buf_num, elbc_fcm_ctrl->addr, priv->vbase,
		 elbc_fcm_ctrl->index,
	         chip->phys_erase_shift, chip->page_shift);
}

 
static int fsl_elbc_run_command(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct fsl_elbc_mtd *priv = nand_get_controller_data(chip);
	struct fsl_lbc_ctrl *ctrl = priv->ctrl;
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = ctrl->nand;
	struct fsl_lbc_regs __iomem *lbc = ctrl->regs;

	 
	out_be32(&lbc->fmr, priv->fmr | 3);
	if (elbc_fcm_ctrl->use_mdr)
		out_be32(&lbc->mdr, elbc_fcm_ctrl->mdr);

	dev_vdbg(priv->dev,
	         "fsl_elbc_run_command: fmr=%08x fir=%08x fcr=%08x\n",
	         in_be32(&lbc->fmr), in_be32(&lbc->fir), in_be32(&lbc->fcr));
	dev_vdbg(priv->dev,
	         "fsl_elbc_run_command: fbar=%08x fpar=%08x "
	         "fbcr=%08x bank=%d\n",
	         in_be32(&lbc->fbar), in_be32(&lbc->fpar),
	         in_be32(&lbc->fbcr), priv->bank);

	ctrl->irq_status = 0;
	 
	out_be32(&lbc->lsor, priv->bank);

	 
	wait_event_timeout(ctrl->irq_wait, ctrl->irq_status,
	                   FCM_TIMEOUT_MSECS * HZ/1000);
	elbc_fcm_ctrl->status = ctrl->irq_status;
	 
	if (elbc_fcm_ctrl->use_mdr)
		elbc_fcm_ctrl->mdr = in_be32(&lbc->mdr);

	elbc_fcm_ctrl->use_mdr = 0;

	if (elbc_fcm_ctrl->status != LTESR_CC) {
		dev_info(priv->dev,
		         "command failed: fir %x fcr %x status %x mdr %x\n",
		         in_be32(&lbc->fir), in_be32(&lbc->fcr),
			 elbc_fcm_ctrl->status, elbc_fcm_ctrl->mdr);
		return -EIO;
	}

	if (chip->ecc.engine_type != NAND_ECC_ENGINE_TYPE_ON_HOST)
		return 0;

	elbc_fcm_ctrl->max_bitflips = 0;

	if (elbc_fcm_ctrl->read_bytes == mtd->writesize + mtd->oobsize) {
		uint32_t lteccr = in_be32(&lbc->lteccr);
		 
		if (lteccr & 0x000F000F)
			out_be32(&lbc->lteccr, 0x000F000F);  
		if (lteccr & 0x000F0000) {
			mtd->ecc_stats.corrected++;
			elbc_fcm_ctrl->max_bitflips = 1;
		}
	}

	return 0;
}

static void fsl_elbc_do_read(struct nand_chip *chip, int oob)
{
	struct fsl_elbc_mtd *priv = nand_get_controller_data(chip);
	struct fsl_lbc_ctrl *ctrl = priv->ctrl;
	struct fsl_lbc_regs __iomem *lbc = ctrl->regs;

	if (priv->page_size) {
		out_be32(&lbc->fir,
		         (FIR_OP_CM0 << FIR_OP0_SHIFT) |
		         (FIR_OP_CA  << FIR_OP1_SHIFT) |
		         (FIR_OP_PA  << FIR_OP2_SHIFT) |
		         (FIR_OP_CM1 << FIR_OP3_SHIFT) |
		         (FIR_OP_RBW << FIR_OP4_SHIFT));

		out_be32(&lbc->fcr, (NAND_CMD_READ0 << FCR_CMD0_SHIFT) |
		                    (NAND_CMD_READSTART << FCR_CMD1_SHIFT));
	} else {
		out_be32(&lbc->fir,
		         (FIR_OP_CM0 << FIR_OP0_SHIFT) |
		         (FIR_OP_CA  << FIR_OP1_SHIFT) |
		         (FIR_OP_PA  << FIR_OP2_SHIFT) |
		         (FIR_OP_RBW << FIR_OP3_SHIFT));

		if (oob)
			out_be32(&lbc->fcr, NAND_CMD_READOOB << FCR_CMD0_SHIFT);
		else
			out_be32(&lbc->fcr, NAND_CMD_READ0 << FCR_CMD0_SHIFT);
	}
}

 
static void fsl_elbc_cmdfunc(struct nand_chip *chip, unsigned int command,
                             int column, int page_addr)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct fsl_elbc_mtd *priv = nand_get_controller_data(chip);
	struct fsl_lbc_ctrl *ctrl = priv->ctrl;
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = ctrl->nand;
	struct fsl_lbc_regs __iomem *lbc = ctrl->regs;

	elbc_fcm_ctrl->use_mdr = 0;

	 
	elbc_fcm_ctrl->read_bytes = 0;
	if (command != NAND_CMD_PAGEPROG)
		elbc_fcm_ctrl->index = 0;

	switch (command) {
	 
	case NAND_CMD_READ1:
		column += 256;
		fallthrough;
	case NAND_CMD_READ0:
		dev_dbg(priv->dev,
		        "fsl_elbc_cmdfunc: NAND_CMD_READ0, page_addr:"
		        " 0x%x, column: 0x%x.\n", page_addr, column);


		out_be32(&lbc->fbcr, 0);  
		set_addr(mtd, 0, page_addr, 0);

		elbc_fcm_ctrl->read_bytes = mtd->writesize + mtd->oobsize;
		elbc_fcm_ctrl->index += column;

		fsl_elbc_do_read(chip, 0);
		fsl_elbc_run_command(mtd);
		return;

	 
	case NAND_CMD_RNDOUT:
		dev_dbg(priv->dev,
			"fsl_elbc_cmdfunc: NAND_CMD_RNDOUT, column: 0x%x.\n",
			column);

		elbc_fcm_ctrl->index = column;
		return;

	 
	case NAND_CMD_READOOB:
		dev_vdbg(priv->dev,
		         "fsl_elbc_cmdfunc: NAND_CMD_READOOB, page_addr:"
			 " 0x%x, column: 0x%x.\n", page_addr, column);

		out_be32(&lbc->fbcr, mtd->oobsize - column);
		set_addr(mtd, column, page_addr, 1);

		elbc_fcm_ctrl->read_bytes = mtd->writesize + mtd->oobsize;

		fsl_elbc_do_read(chip, 1);
		fsl_elbc_run_command(mtd);
		return;

	case NAND_CMD_READID:
	case NAND_CMD_PARAM:
		dev_vdbg(priv->dev, "fsl_elbc_cmdfunc: NAND_CMD %x\n", command);

		out_be32(&lbc->fir, (FIR_OP_CM0 << FIR_OP0_SHIFT) |
		                    (FIR_OP_UA  << FIR_OP1_SHIFT) |
		                    (FIR_OP_RBW << FIR_OP2_SHIFT));
		out_be32(&lbc->fcr, command << FCR_CMD0_SHIFT);
		 
		out_be32(&lbc->fbcr, 256);
		elbc_fcm_ctrl->read_bytes = 256;
		elbc_fcm_ctrl->use_mdr = 1;
		elbc_fcm_ctrl->mdr = column;
		set_addr(mtd, 0, 0, 0);
		fsl_elbc_run_command(mtd);
		return;

	 
	case NAND_CMD_ERASE1:
		dev_vdbg(priv->dev,
		         "fsl_elbc_cmdfunc: NAND_CMD_ERASE1, "
		         "page_addr: 0x%x.\n", page_addr);
		set_addr(mtd, 0, page_addr, 0);
		return;

	 
	case NAND_CMD_ERASE2:
		dev_vdbg(priv->dev, "fsl_elbc_cmdfunc: NAND_CMD_ERASE2.\n");

		out_be32(&lbc->fir,
		         (FIR_OP_CM0 << FIR_OP0_SHIFT) |
		         (FIR_OP_PA  << FIR_OP1_SHIFT) |
		         (FIR_OP_CM2 << FIR_OP2_SHIFT) |
		         (FIR_OP_CW1 << FIR_OP3_SHIFT) |
		         (FIR_OP_RS  << FIR_OP4_SHIFT));

		out_be32(&lbc->fcr,
		         (NAND_CMD_ERASE1 << FCR_CMD0_SHIFT) |
		         (NAND_CMD_STATUS << FCR_CMD1_SHIFT) |
		         (NAND_CMD_ERASE2 << FCR_CMD2_SHIFT));

		out_be32(&lbc->fbcr, 0);
		elbc_fcm_ctrl->read_bytes = 0;
		elbc_fcm_ctrl->use_mdr = 1;

		fsl_elbc_run_command(mtd);
		return;

	 
	case NAND_CMD_SEQIN: {
		__be32 fcr;
		dev_vdbg(priv->dev,
			 "fsl_elbc_cmdfunc: NAND_CMD_SEQIN/PAGE_PROG, "
		         "page_addr: 0x%x, column: 0x%x.\n",
		         page_addr, column);

		elbc_fcm_ctrl->column = column;
		elbc_fcm_ctrl->use_mdr = 1;

		if (column >= mtd->writesize) {
			 
			column -= mtd->writesize;
			elbc_fcm_ctrl->oob = 1;
		} else {
			WARN_ON(column != 0);
			elbc_fcm_ctrl->oob = 0;
		}

		fcr = (NAND_CMD_STATUS   << FCR_CMD1_SHIFT) |
		      (NAND_CMD_SEQIN    << FCR_CMD2_SHIFT) |
		      (NAND_CMD_PAGEPROG << FCR_CMD3_SHIFT);

		if (priv->page_size) {
			out_be32(&lbc->fir,
			         (FIR_OP_CM2 << FIR_OP0_SHIFT) |
			         (FIR_OP_CA  << FIR_OP1_SHIFT) |
			         (FIR_OP_PA  << FIR_OP2_SHIFT) |
			         (FIR_OP_WB  << FIR_OP3_SHIFT) |
			         (FIR_OP_CM3 << FIR_OP4_SHIFT) |
			         (FIR_OP_CW1 << FIR_OP5_SHIFT) |
			         (FIR_OP_RS  << FIR_OP6_SHIFT));
		} else {
			out_be32(&lbc->fir,
			         (FIR_OP_CM0 << FIR_OP0_SHIFT) |
			         (FIR_OP_CM2 << FIR_OP1_SHIFT) |
			         (FIR_OP_CA  << FIR_OP2_SHIFT) |
			         (FIR_OP_PA  << FIR_OP3_SHIFT) |
			         (FIR_OP_WB  << FIR_OP4_SHIFT) |
			         (FIR_OP_CM3 << FIR_OP5_SHIFT) |
			         (FIR_OP_CW1 << FIR_OP6_SHIFT) |
			         (FIR_OP_RS  << FIR_OP7_SHIFT));

			if (elbc_fcm_ctrl->oob)
				 
				fcr |= NAND_CMD_READOOB << FCR_CMD0_SHIFT;
			else
				 
				fcr |= NAND_CMD_READ0 << FCR_CMD0_SHIFT;
		}

		out_be32(&lbc->fcr, fcr);
		set_addr(mtd, column, page_addr, elbc_fcm_ctrl->oob);
		return;
	}

	 
	case NAND_CMD_PAGEPROG: {
		dev_vdbg(priv->dev,
		         "fsl_elbc_cmdfunc: NAND_CMD_PAGEPROG "
			 "writing %d bytes.\n", elbc_fcm_ctrl->index);

		 
		if (elbc_fcm_ctrl->oob || elbc_fcm_ctrl->column != 0 ||
		    elbc_fcm_ctrl->index != mtd->writesize + mtd->oobsize)
			out_be32(&lbc->fbcr,
				elbc_fcm_ctrl->index - elbc_fcm_ctrl->column);
		else
			out_be32(&lbc->fbcr, 0);

		fsl_elbc_run_command(mtd);
		return;
	}

	 
	 
	case NAND_CMD_STATUS:
		out_be32(&lbc->fir,
		         (FIR_OP_CM0 << FIR_OP0_SHIFT) |
		         (FIR_OP_RBW << FIR_OP1_SHIFT));
		out_be32(&lbc->fcr, NAND_CMD_STATUS << FCR_CMD0_SHIFT);
		out_be32(&lbc->fbcr, 1);
		set_addr(mtd, 0, 0, 0);
		elbc_fcm_ctrl->read_bytes = 1;

		fsl_elbc_run_command(mtd);

		 
		setbits8(elbc_fcm_ctrl->addr, NAND_STATUS_WP);
		return;

	 
	case NAND_CMD_RESET:
		dev_dbg(priv->dev, "fsl_elbc_cmdfunc: NAND_CMD_RESET.\n");
		out_be32(&lbc->fir, FIR_OP_CM0 << FIR_OP0_SHIFT);
		out_be32(&lbc->fcr, NAND_CMD_RESET << FCR_CMD0_SHIFT);
		fsl_elbc_run_command(mtd);
		return;

	default:
		dev_err(priv->dev,
		        "fsl_elbc_cmdfunc: error, unsupported command 0x%x.\n",
		        command);
	}
}

static void fsl_elbc_select_chip(struct nand_chip *chip, int cs)
{
	 
}

 
static void fsl_elbc_write_buf(struct nand_chip *chip, const u8 *buf, int len)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct fsl_elbc_mtd *priv = nand_get_controller_data(chip);
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = priv->ctrl->nand;
	unsigned int bufsize = mtd->writesize + mtd->oobsize;

	if (len <= 0) {
		dev_err(priv->dev, "write_buf of %d bytes", len);
		elbc_fcm_ctrl->status = 0;
		return;
	}

	if ((unsigned int)len > bufsize - elbc_fcm_ctrl->index) {
		dev_err(priv->dev,
		        "write_buf beyond end of buffer "
		        "(%d requested, %u available)\n",
			len, bufsize - elbc_fcm_ctrl->index);
		len = bufsize - elbc_fcm_ctrl->index;
	}

	memcpy_toio(&elbc_fcm_ctrl->addr[elbc_fcm_ctrl->index], buf, len);
	 
	in_8(&elbc_fcm_ctrl->addr[elbc_fcm_ctrl->index] + len - 1);

	elbc_fcm_ctrl->index += len;
}

 
static u8 fsl_elbc_read_byte(struct nand_chip *chip)
{
	struct fsl_elbc_mtd *priv = nand_get_controller_data(chip);
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = priv->ctrl->nand;

	 
	if (elbc_fcm_ctrl->index < elbc_fcm_ctrl->read_bytes)
		return in_8(&elbc_fcm_ctrl->addr[elbc_fcm_ctrl->index++]);

	dev_err(priv->dev, "read_byte beyond end of buffer\n");
	return ERR_BYTE;
}

 
static void fsl_elbc_read_buf(struct nand_chip *chip, u8 *buf, int len)
{
	struct fsl_elbc_mtd *priv = nand_get_controller_data(chip);
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = priv->ctrl->nand;
	int avail;

	if (len < 0)
		return;

	avail = min((unsigned int)len,
			elbc_fcm_ctrl->read_bytes - elbc_fcm_ctrl->index);
	memcpy_fromio(buf, &elbc_fcm_ctrl->addr[elbc_fcm_ctrl->index], avail);
	elbc_fcm_ctrl->index += avail;

	if (len > avail)
		dev_err(priv->dev,
		        "read_buf beyond end of buffer "
		        "(%d requested, %d available)\n",
		        len, avail);
}

 
static int fsl_elbc_wait(struct nand_chip *chip)
{
	struct fsl_elbc_mtd *priv = nand_get_controller_data(chip);
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = priv->ctrl->nand;

	if (elbc_fcm_ctrl->status != LTESR_CC)
		return NAND_STATUS_FAIL;

	 
	return (elbc_fcm_ctrl->mdr & 0xff) | NAND_STATUS_WP;
}

static int fsl_elbc_read_page(struct nand_chip *chip, uint8_t *buf,
			      int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct fsl_elbc_mtd *priv = nand_get_controller_data(chip);
	struct fsl_lbc_ctrl *ctrl = priv->ctrl;
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = ctrl->nand;

	nand_read_page_op(chip, page, 0, buf, mtd->writesize);
	if (oob_required)
		fsl_elbc_read_buf(chip, chip->oob_poi, mtd->oobsize);

	if (fsl_elbc_wait(chip) & NAND_STATUS_FAIL)
		mtd->ecc_stats.failed++;

	return elbc_fcm_ctrl->max_bitflips;
}

 
static int fsl_elbc_write_page(struct nand_chip *chip, const uint8_t *buf,
			       int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);

	nand_prog_page_begin_op(chip, page, 0, buf, mtd->writesize);
	fsl_elbc_write_buf(chip, chip->oob_poi, mtd->oobsize);

	return nand_prog_page_end_op(chip);
}

 
static int fsl_elbc_write_subpage(struct nand_chip *chip, uint32_t offset,
				  uint32_t data_len, const uint8_t *buf,
				  int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);

	nand_prog_page_begin_op(chip, page, 0, NULL, 0);
	fsl_elbc_write_buf(chip, buf, mtd->writesize);
	fsl_elbc_write_buf(chip, chip->oob_poi, mtd->oobsize);
	return nand_prog_page_end_op(chip);
}

static int fsl_elbc_chip_init(struct fsl_elbc_mtd *priv)
{
	struct fsl_lbc_ctrl *ctrl = priv->ctrl;
	struct fsl_lbc_regs __iomem *lbc = ctrl->regs;
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = ctrl->nand;
	struct nand_chip *chip = &priv->chip;
	struct mtd_info *mtd = nand_to_mtd(chip);

	dev_dbg(priv->dev, "eLBC Set Information for bank %d\n", priv->bank);

	 
	mtd->dev.parent = priv->dev;
	nand_set_flash_node(chip, priv->dev->of_node);

	 
	priv->fmr = 15 << FMR_CWTO_SHIFT;
	if (in_be32(&lbc->bank[priv->bank].or) & OR_FCM_PGS)
		priv->fmr |= FMR_ECCM;

	 
	 
	chip->legacy.read_byte = fsl_elbc_read_byte;
	chip->legacy.write_buf = fsl_elbc_write_buf;
	chip->legacy.read_buf = fsl_elbc_read_buf;
	chip->legacy.select_chip = fsl_elbc_select_chip;
	chip->legacy.cmdfunc = fsl_elbc_cmdfunc;
	chip->legacy.waitfunc = fsl_elbc_wait;
	chip->legacy.set_features = nand_get_set_features_notsupp;
	chip->legacy.get_features = nand_get_set_features_notsupp;

	chip->bbt_td = &bbt_main_descr;
	chip->bbt_md = &bbt_mirror_descr;

	 
	chip->bbt_options = NAND_BBT_USE_FLASH;

	chip->controller = &elbc_fcm_ctrl->controller;
	nand_set_controller_data(chip, priv);

	return 0;
}

static int fsl_elbc_attach_chip(struct nand_chip *chip)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct fsl_elbc_mtd *priv = nand_get_controller_data(chip);
	struct fsl_lbc_ctrl *ctrl = priv->ctrl;
	struct fsl_lbc_regs __iomem *lbc = ctrl->regs;
	unsigned int al;
	u32 br;

	 
	if (chip->ecc.engine_type == NAND_ECC_ENGINE_TYPE_INVALID) {
		 
		if ((in_be32(&lbc->bank[priv->bank].br) & BR_DECC) ==
		    BR_DECC_CHK_GEN) {
			chip->ecc.engine_type = NAND_ECC_ENGINE_TYPE_ON_HOST;
		} else {
			 
			chip->ecc.engine_type = NAND_ECC_ENGINE_TYPE_SOFT;
			chip->ecc.algo = NAND_ECC_ALGO_HAMMING;
		}
	}

	switch (chip->ecc.engine_type) {
	 
	case NAND_ECC_ENGINE_TYPE_ON_HOST:
		chip->ecc.read_page = fsl_elbc_read_page;
		chip->ecc.write_page = fsl_elbc_write_page;
		chip->ecc.write_subpage = fsl_elbc_write_subpage;
		mtd_set_ooblayout(mtd, &fsl_elbc_ooblayout_ops);
		chip->ecc.size = 512;
		chip->ecc.bytes = 3;
		chip->ecc.strength = 1;
		break;

	 
	case NAND_ECC_ENGINE_TYPE_NONE:
	case NAND_ECC_ENGINE_TYPE_SOFT:
	case NAND_ECC_ENGINE_TYPE_ON_DIE:
		break;

	default:
		return -EINVAL;
	}

	 
	br = in_be32(&lbc->bank[priv->bank].br) & ~BR_DECC;
	if (chip->ecc.engine_type == NAND_ECC_ENGINE_TYPE_ON_HOST)
		out_be32(&lbc->bank[priv->bank].br, br | BR_DECC_CHK_GEN);
	else
		out_be32(&lbc->bank[priv->bank].br, br | BR_DECC_OFF);

	 
	al = 0;
	if (chip->pagemask & 0xffff0000)
		al++;
	if (chip->pagemask & 0xff000000)
		al++;

	priv->fmr |= al << FMR_AL_SHIFT;

	dev_dbg(priv->dev, "fsl_elbc_init: nand->numchips = %d\n",
	        nanddev_ntargets(&chip->base));
	dev_dbg(priv->dev, "fsl_elbc_init: nand->chipsize = %lld\n",
	        nanddev_target_size(&chip->base));
	dev_dbg(priv->dev, "fsl_elbc_init: nand->pagemask = %8x\n",
	        chip->pagemask);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->legacy.chip_delay = %d\n",
	        chip->legacy.chip_delay);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->badblockpos = %d\n",
	        chip->badblockpos);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->chip_shift = %d\n",
	        chip->chip_shift);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->page_shift = %d\n",
	        chip->page_shift);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->phys_erase_shift = %d\n",
	        chip->phys_erase_shift);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->ecc.engine_type = %d\n",
		chip->ecc.engine_type);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->ecc.steps = %d\n",
	        chip->ecc.steps);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->ecc.bytes = %d\n",
	        chip->ecc.bytes);
	dev_dbg(priv->dev, "fsl_elbc_init: nand->ecc.total = %d\n",
	        chip->ecc.total);
	dev_dbg(priv->dev, "fsl_elbc_init: mtd->ooblayout = %p\n",
		mtd->ooblayout);
	dev_dbg(priv->dev, "fsl_elbc_init: mtd->flags = %08x\n", mtd->flags);
	dev_dbg(priv->dev, "fsl_elbc_init: mtd->size = %lld\n", mtd->size);
	dev_dbg(priv->dev, "fsl_elbc_init: mtd->erasesize = %d\n",
	        mtd->erasesize);
	dev_dbg(priv->dev, "fsl_elbc_init: mtd->writesize = %d\n",
	        mtd->writesize);
	dev_dbg(priv->dev, "fsl_elbc_init: mtd->oobsize = %d\n",
	        mtd->oobsize);

	 
	if (mtd->writesize == 512) {
		priv->page_size = 0;
		clrbits32(&lbc->bank[priv->bank].or, OR_FCM_PGS);
	} else if (mtd->writesize == 2048) {
		priv->page_size = 1;
		setbits32(&lbc->bank[priv->bank].or, OR_FCM_PGS);
	} else {
		dev_err(priv->dev,
		        "fsl_elbc_init: page size %d is not supported\n",
		        mtd->writesize);
		return -ENOTSUPP;
	}

	return 0;
}

static const struct nand_controller_ops fsl_elbc_controller_ops = {
	.attach_chip = fsl_elbc_attach_chip,
};

static int fsl_elbc_chip_remove(struct fsl_elbc_mtd *priv)
{
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = priv->ctrl->nand;
	struct mtd_info *mtd = nand_to_mtd(&priv->chip);

	kfree(mtd->name);

	if (priv->vbase)
		iounmap(priv->vbase);

	elbc_fcm_ctrl->chips[priv->bank] = NULL;
	kfree(priv);
	return 0;
}

static DEFINE_MUTEX(fsl_elbc_nand_mutex);

static int fsl_elbc_nand_probe(struct platform_device *pdev)
{
	struct fsl_lbc_regs __iomem *lbc;
	struct fsl_elbc_mtd *priv;
	struct resource res;
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl;
	static const char *part_probe_types[]
		= { "cmdlinepart", "RedBoot", "ofpart", NULL };
	int ret;
	int bank;
	struct device *dev;
	struct device_node *node = pdev->dev.of_node;
	struct mtd_info *mtd;

	if (!fsl_lbc_ctrl_dev || !fsl_lbc_ctrl_dev->regs)
		return -ENODEV;
	lbc = fsl_lbc_ctrl_dev->regs;
	dev = fsl_lbc_ctrl_dev->dev;

	 
	ret = of_address_to_resource(node, 0, &res);
	if (ret) {
		dev_err(dev, "failed to get resource\n");
		return ret;
	}

	 
	for (bank = 0; bank < MAX_BANKS; bank++)
		if ((in_be32(&lbc->bank[bank].br) & BR_V) &&
		    (in_be32(&lbc->bank[bank].br) & BR_MSEL) == BR_MS_FCM &&
		    (in_be32(&lbc->bank[bank].br) &
		     in_be32(&lbc->bank[bank].or) & BR_BA)
		     == fsl_lbc_addr(res.start))
			break;

	if (bank >= MAX_BANKS) {
		dev_err(dev, "address did not match any chip selects\n");
		return -ENODEV;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	mutex_lock(&fsl_elbc_nand_mutex);
	if (!fsl_lbc_ctrl_dev->nand) {
		elbc_fcm_ctrl = kzalloc(sizeof(*elbc_fcm_ctrl), GFP_KERNEL);
		if (!elbc_fcm_ctrl) {
			mutex_unlock(&fsl_elbc_nand_mutex);
			ret = -ENOMEM;
			goto err;
		}
		elbc_fcm_ctrl->counter++;

		nand_controller_init(&elbc_fcm_ctrl->controller);
		fsl_lbc_ctrl_dev->nand = elbc_fcm_ctrl;
	} else {
		elbc_fcm_ctrl = fsl_lbc_ctrl_dev->nand;
	}
	mutex_unlock(&fsl_elbc_nand_mutex);

	elbc_fcm_ctrl->chips[bank] = priv;
	priv->bank = bank;
	priv->ctrl = fsl_lbc_ctrl_dev;
	priv->dev = &pdev->dev;
	dev_set_drvdata(priv->dev, priv);

	priv->vbase = ioremap(res.start, resource_size(&res));
	if (!priv->vbase) {
		dev_err(dev, "failed to map chip region\n");
		ret = -ENOMEM;
		goto err;
	}

	mtd = nand_to_mtd(&priv->chip);
	mtd->name = kasprintf(GFP_KERNEL, "%llx.flash", (u64)res.start);
	if (!nand_to_mtd(&priv->chip)->name) {
		ret = -ENOMEM;
		goto err;
	}

	ret = fsl_elbc_chip_init(priv);
	if (ret)
		goto err;

	priv->chip.controller->ops = &fsl_elbc_controller_ops;
	ret = nand_scan(&priv->chip, 1);
	if (ret)
		goto err;

	 
	ret = mtd_device_parse_register(mtd, part_probe_types, NULL, NULL, 0);
	if (ret)
		goto cleanup_nand;

	pr_info("eLBC NAND device at 0x%llx, bank %d\n",
		(unsigned long long)res.start, priv->bank);

	return 0;

cleanup_nand:
	nand_cleanup(&priv->chip);
err:
	fsl_elbc_chip_remove(priv);

	return ret;
}

static void fsl_elbc_nand_remove(struct platform_device *pdev)
{
	struct fsl_elbc_fcm_ctrl *elbc_fcm_ctrl = fsl_lbc_ctrl_dev->nand;
	struct fsl_elbc_mtd *priv = dev_get_drvdata(&pdev->dev);
	struct nand_chip *chip = &priv->chip;
	int ret;

	ret = mtd_device_unregister(nand_to_mtd(chip));
	WARN_ON(ret);
	nand_cleanup(chip);

	fsl_elbc_chip_remove(priv);

	mutex_lock(&fsl_elbc_nand_mutex);
	elbc_fcm_ctrl->counter--;
	if (!elbc_fcm_ctrl->counter) {
		fsl_lbc_ctrl_dev->nand = NULL;
		kfree(elbc_fcm_ctrl);
	}
	mutex_unlock(&fsl_elbc_nand_mutex);

}

static const struct of_device_id fsl_elbc_nand_match[] = {
	{ .compatible = "fsl,elbc-fcm-nand", },
	{}
};
MODULE_DEVICE_TABLE(of, fsl_elbc_nand_match);

static struct platform_driver fsl_elbc_nand_driver = {
	.driver = {
		.name = "fsl,elbc-fcm-nand",
		.of_match_table = fsl_elbc_nand_match,
	},
	.probe = fsl_elbc_nand_probe,
	.remove_new = fsl_elbc_nand_remove,
};

module_platform_driver(fsl_elbc_nand_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Freescale");
MODULE_DESCRIPTION("Freescale Enhanced Local Bus Controller MTD NAND driver");
