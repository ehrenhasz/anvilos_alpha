
 

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/nmi.h>

#include "internals.h"

 
static uint8_t nand_read_byte(struct nand_chip *chip)
{
	return readb(chip->legacy.IO_ADDR_R);
}

 
static uint8_t nand_read_byte16(struct nand_chip *chip)
{
	return (uint8_t) cpu_to_le16(readw(chip->legacy.IO_ADDR_R));
}

 
static void nand_select_chip(struct nand_chip *chip, int chipnr)
{
	switch (chipnr) {
	case -1:
		chip->legacy.cmd_ctrl(chip, NAND_CMD_NONE,
				      0 | NAND_CTRL_CHANGE);
		break;
	case 0:
		break;

	default:
		BUG();
	}
}

 
static void nand_write_byte(struct nand_chip *chip, uint8_t byte)
{
	chip->legacy.write_buf(chip, &byte, 1);
}

 
static void nand_write_byte16(struct nand_chip *chip, uint8_t byte)
{
	uint16_t word = byte;

	 
	chip->legacy.write_buf(chip, (uint8_t *)&word, 2);
}

 
static void nand_write_buf(struct nand_chip *chip, const uint8_t *buf, int len)
{
	iowrite8_rep(chip->legacy.IO_ADDR_W, buf, len);
}

 
static void nand_read_buf(struct nand_chip *chip, uint8_t *buf, int len)
{
	ioread8_rep(chip->legacy.IO_ADDR_R, buf, len);
}

 
static void nand_write_buf16(struct nand_chip *chip, const uint8_t *buf,
			     int len)
{
	u16 *p = (u16 *) buf;

	iowrite16_rep(chip->legacy.IO_ADDR_W, p, len >> 1);
}

 
static void nand_read_buf16(struct nand_chip *chip, uint8_t *buf, int len)
{
	u16 *p = (u16 *) buf;

	ioread16_rep(chip->legacy.IO_ADDR_R, p, len >> 1);
}

 
static void panic_nand_wait_ready(struct nand_chip *chip, unsigned long timeo)
{
	int i;

	 
	for (i = 0; i < timeo; i++) {
		if (chip->legacy.dev_ready(chip))
			break;
		touch_softlockup_watchdog();
		mdelay(1);
	}
}

 
void nand_wait_ready(struct nand_chip *chip)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	unsigned long timeo = 400;

	if (mtd->oops_panic_write)
		return panic_nand_wait_ready(chip, timeo);

	 
	timeo = jiffies + msecs_to_jiffies(timeo);
	do {
		if (chip->legacy.dev_ready(chip))
			return;
		cond_resched();
	} while (time_before(jiffies, timeo));

	if (!chip->legacy.dev_ready(chip))
		pr_warn_ratelimited("timeout while waiting for chip to become ready\n");
}
EXPORT_SYMBOL_GPL(nand_wait_ready);

 
static void nand_wait_status_ready(struct nand_chip *chip, unsigned long timeo)
{
	int ret;

	timeo = jiffies + msecs_to_jiffies(timeo);
	do {
		u8 status;

		ret = nand_read_data_op(chip, &status, sizeof(status), true,
					false);
		if (ret)
			return;

		if (status & NAND_STATUS_READY)
			break;
		touch_softlockup_watchdog();
	} while (time_before(jiffies, timeo));
};

 
static void nand_command(struct nand_chip *chip, unsigned int command,
			 int column, int page_addr)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int ctrl = NAND_CTRL_CLE | NAND_CTRL_CHANGE;

	 
	if (command == NAND_CMD_SEQIN) {
		int readcmd;

		if (column >= mtd->writesize) {
			 
			column -= mtd->writesize;
			readcmd = NAND_CMD_READOOB;
		} else if (column < 256) {
			 
			readcmd = NAND_CMD_READ0;
		} else {
			column -= 256;
			readcmd = NAND_CMD_READ1;
		}
		chip->legacy.cmd_ctrl(chip, readcmd, ctrl);
		ctrl &= ~NAND_CTRL_CHANGE;
	}
	if (command != NAND_CMD_NONE)
		chip->legacy.cmd_ctrl(chip, command, ctrl);

	 
	ctrl = NAND_CTRL_ALE | NAND_CTRL_CHANGE;
	 
	if (column != -1) {
		 
		if (chip->options & NAND_BUSWIDTH_16 &&
				!nand_opcode_8bits(command))
			column >>= 1;
		chip->legacy.cmd_ctrl(chip, column, ctrl);
		ctrl &= ~NAND_CTRL_CHANGE;
	}
	if (page_addr != -1) {
		chip->legacy.cmd_ctrl(chip, page_addr, ctrl);
		ctrl &= ~NAND_CTRL_CHANGE;
		chip->legacy.cmd_ctrl(chip, page_addr >> 8, ctrl);
		if (chip->options & NAND_ROW_ADDR_3)
			chip->legacy.cmd_ctrl(chip, page_addr >> 16, ctrl);
	}
	chip->legacy.cmd_ctrl(chip, NAND_CMD_NONE,
			      NAND_NCE | NAND_CTRL_CHANGE);

	 
	switch (command) {

	case NAND_CMD_NONE:
	case NAND_CMD_PAGEPROG:
	case NAND_CMD_ERASE1:
	case NAND_CMD_ERASE2:
	case NAND_CMD_SEQIN:
	case NAND_CMD_STATUS:
	case NAND_CMD_READID:
	case NAND_CMD_SET_FEATURES:
		return;

	case NAND_CMD_RESET:
		if (chip->legacy.dev_ready)
			break;
		udelay(chip->legacy.chip_delay);
		chip->legacy.cmd_ctrl(chip, NAND_CMD_STATUS,
				      NAND_CTRL_CLE | NAND_CTRL_CHANGE);
		chip->legacy.cmd_ctrl(chip, NAND_CMD_NONE,
				      NAND_NCE | NAND_CTRL_CHANGE);
		 
		nand_wait_status_ready(chip, 250);
		return;

		 
	case NAND_CMD_READ0:
		 
		if (column == -1 && page_addr == -1)
			return;
		fallthrough;
	default:
		 
		if (!chip->legacy.dev_ready) {
			udelay(chip->legacy.chip_delay);
			return;
		}
	}
	 
	ndelay(100);

	nand_wait_ready(chip);
}

static void nand_ccs_delay(struct nand_chip *chip)
{
	const struct nand_sdr_timings *sdr =
		nand_get_sdr_timings(nand_get_interface_config(chip));

	 
	if (!(chip->options & NAND_WAIT_TCCS))
		return;

	 
	if (!IS_ERR(sdr) && nand_controller_can_setup_interface(chip))
		ndelay(sdr->tCCS_min / 1000);
	else
		ndelay(500);
}

 
static void nand_command_lp(struct nand_chip *chip, unsigned int command,
			    int column, int page_addr)
{
	struct mtd_info *mtd = nand_to_mtd(chip);

	 
	if (command == NAND_CMD_READOOB) {
		column += mtd->writesize;
		command = NAND_CMD_READ0;
	}

	 
	if (command != NAND_CMD_NONE)
		chip->legacy.cmd_ctrl(chip, command,
				      NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);

	if (column != -1 || page_addr != -1) {
		int ctrl = NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE;

		 
		if (column != -1) {
			 
			if (chip->options & NAND_BUSWIDTH_16 &&
					!nand_opcode_8bits(command))
				column >>= 1;
			chip->legacy.cmd_ctrl(chip, column, ctrl);
			ctrl &= ~NAND_CTRL_CHANGE;

			 
			if (!nand_opcode_8bits(command))
				chip->legacy.cmd_ctrl(chip, column >> 8, ctrl);
		}
		if (page_addr != -1) {
			chip->legacy.cmd_ctrl(chip, page_addr, ctrl);
			chip->legacy.cmd_ctrl(chip, page_addr >> 8,
					     NAND_NCE | NAND_ALE);
			if (chip->options & NAND_ROW_ADDR_3)
				chip->legacy.cmd_ctrl(chip, page_addr >> 16,
						      NAND_NCE | NAND_ALE);
		}
	}
	chip->legacy.cmd_ctrl(chip, NAND_CMD_NONE,
			      NAND_NCE | NAND_CTRL_CHANGE);

	 
	switch (command) {

	case NAND_CMD_NONE:
	case NAND_CMD_CACHEDPROG:
	case NAND_CMD_PAGEPROG:
	case NAND_CMD_ERASE1:
	case NAND_CMD_ERASE2:
	case NAND_CMD_SEQIN:
	case NAND_CMD_STATUS:
	case NAND_CMD_READID:
	case NAND_CMD_SET_FEATURES:
		return;

	case NAND_CMD_RNDIN:
		nand_ccs_delay(chip);
		return;

	case NAND_CMD_RESET:
		if (chip->legacy.dev_ready)
			break;
		udelay(chip->legacy.chip_delay);
		chip->legacy.cmd_ctrl(chip, NAND_CMD_STATUS,
				      NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
		chip->legacy.cmd_ctrl(chip, NAND_CMD_NONE,
				      NAND_NCE | NAND_CTRL_CHANGE);
		 
		nand_wait_status_ready(chip, 250);
		return;

	case NAND_CMD_RNDOUT:
		 
		chip->legacy.cmd_ctrl(chip, NAND_CMD_RNDOUTSTART,
				      NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
		chip->legacy.cmd_ctrl(chip, NAND_CMD_NONE,
				      NAND_NCE | NAND_CTRL_CHANGE);

		nand_ccs_delay(chip);
		return;

	case NAND_CMD_READ0:
		 
		if (column == -1 && page_addr == -1)
			return;

		chip->legacy.cmd_ctrl(chip, NAND_CMD_READSTART,
				      NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
		chip->legacy.cmd_ctrl(chip, NAND_CMD_NONE,
				      NAND_NCE | NAND_CTRL_CHANGE);
		fallthrough;	 
	default:
		 
		if (!chip->legacy.dev_ready) {
			udelay(chip->legacy.chip_delay);
			return;
		}
	}

	 
	ndelay(100);

	nand_wait_ready(chip);
}

 
int nand_get_set_features_notsupp(struct nand_chip *chip, int addr,
				  u8 *subfeature_param)
{
	return -ENOTSUPP;
}
EXPORT_SYMBOL(nand_get_set_features_notsupp);

 
static int nand_wait(struct nand_chip *chip)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	unsigned long timeo = 400;
	u8 status;
	int ret;

	 
	ndelay(100);

	ret = nand_status_op(chip, NULL);
	if (ret)
		return ret;

	if (mtd->oops_panic_write) {
		panic_nand_wait(chip, timeo);
	} else {
		timeo = jiffies + msecs_to_jiffies(timeo);
		do {
			if (chip->legacy.dev_ready) {
				if (chip->legacy.dev_ready(chip))
					break;
			} else {
				ret = nand_read_data_op(chip, &status,
							sizeof(status), true,
							false);
				if (ret)
					return ret;

				if (status & NAND_STATUS_READY)
					break;
			}
			cond_resched();
		} while (time_before(jiffies, timeo));
	}

	ret = nand_read_data_op(chip, &status, sizeof(status), true, false);
	if (ret)
		return ret;

	 
	WARN_ON(!(status & NAND_STATUS_READY));
	return status;
}

void nand_legacy_set_defaults(struct nand_chip *chip)
{
	unsigned int busw = chip->options & NAND_BUSWIDTH_16;

	if (nand_has_exec_op(chip))
		return;

	 
	if (!chip->legacy.chip_delay)
		chip->legacy.chip_delay = 20;

	 
	if (!chip->legacy.cmdfunc)
		chip->legacy.cmdfunc = nand_command;

	 
	if (chip->legacy.waitfunc == NULL)
		chip->legacy.waitfunc = nand_wait;

	if (!chip->legacy.select_chip)
		chip->legacy.select_chip = nand_select_chip;

	 
	if (!chip->legacy.read_byte || chip->legacy.read_byte == nand_read_byte)
		chip->legacy.read_byte = busw ? nand_read_byte16 : nand_read_byte;
	if (!chip->legacy.write_buf || chip->legacy.write_buf == nand_write_buf)
		chip->legacy.write_buf = busw ? nand_write_buf16 : nand_write_buf;
	if (!chip->legacy.write_byte || chip->legacy.write_byte == nand_write_byte)
		chip->legacy.write_byte = busw ? nand_write_byte16 : nand_write_byte;
	if (!chip->legacy.read_buf || chip->legacy.read_buf == nand_read_buf)
		chip->legacy.read_buf = busw ? nand_read_buf16 : nand_read_buf;
}

void nand_legacy_adjust_cmdfunc(struct nand_chip *chip)
{
	struct mtd_info *mtd = nand_to_mtd(chip);

	 
	if (mtd->writesize > 512 && chip->legacy.cmdfunc == nand_command)
		chip->legacy.cmdfunc = nand_command_lp;
}

int nand_legacy_check_hooks(struct nand_chip *chip)
{
	 
	if (nand_has_exec_op(chip))
		return 0;

	 
	if ((!chip->legacy.cmdfunc || !chip->legacy.select_chip) &&
	    !chip->legacy.cmd_ctrl) {
		pr_err("->legacy.cmd_ctrl() should be provided\n");
		return -EINVAL;
	}

	return 0;
}
