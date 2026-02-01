
 

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/bbm.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/export.h>
#include <linux/string.h>

#include "internals.h"

#define BBT_BLOCK_GOOD		0x00
#define BBT_BLOCK_WORN		0x01
#define BBT_BLOCK_RESERVED	0x02
#define BBT_BLOCK_FACTORY_BAD	0x03

#define BBT_ENTRY_MASK		0x03
#define BBT_ENTRY_SHIFT		2

static inline uint8_t bbt_get_entry(struct nand_chip *chip, int block)
{
	uint8_t entry = chip->bbt[block >> BBT_ENTRY_SHIFT];
	entry >>= (block & BBT_ENTRY_MASK) * 2;
	return entry & BBT_ENTRY_MASK;
}

static inline void bbt_mark_entry(struct nand_chip *chip, int block,
		uint8_t mark)
{
	uint8_t msk = (mark & BBT_ENTRY_MASK) << ((block & BBT_ENTRY_MASK) * 2);
	chip->bbt[block >> BBT_ENTRY_SHIFT] |= msk;
}

static int check_pattern_no_oob(uint8_t *buf, struct nand_bbt_descr *td)
{
	if (memcmp(buf, td->pattern, td->len))
		return -1;
	return 0;
}

 
static int check_pattern(uint8_t *buf, int len, int paglen, struct nand_bbt_descr *td)
{
	if (td->options & NAND_BBT_NO_OOB)
		return check_pattern_no_oob(buf, td);

	 
	if (memcmp(buf + paglen + td->offs, td->pattern, td->len))
		return -1;

	return 0;
}

 
static int check_short_pattern(uint8_t *buf, struct nand_bbt_descr *td)
{
	 
	if (memcmp(buf + td->offs, td->pattern, td->len))
		return -1;
	return 0;
}

 
static u32 add_marker_len(struct nand_bbt_descr *td)
{
	u32 len;

	if (!(td->options & NAND_BBT_NO_OOB))
		return 0;

	len = td->len;
	if (td->options & NAND_BBT_VERSION)
		len++;
	return len;
}

 
static int read_bbt(struct nand_chip *this, uint8_t *buf, int page, int num,
		    struct nand_bbt_descr *td, int offs)
{
	struct mtd_info *mtd = nand_to_mtd(this);
	int res, ret = 0, i, j, act = 0;
	size_t retlen, len, totlen;
	loff_t from;
	int bits = td->options & NAND_BBT_NRBITS_MSK;
	uint8_t msk = (uint8_t)((1 << bits) - 1);
	u32 marker_len;
	int reserved_block_code = td->reserved_block_code;

	totlen = (num * bits) >> 3;
	marker_len = add_marker_len(td);
	from = ((loff_t)page) << this->page_shift;

	while (totlen) {
		len = min(totlen, (size_t)(1 << this->bbt_erase_shift));
		if (marker_len) {
			 
			len -= marker_len;
			from += marker_len;
			marker_len = 0;
		}
		res = mtd_read(mtd, from, len, &retlen, buf);
		if (res < 0) {
			if (mtd_is_eccerr(res)) {
				pr_info("nand_bbt: ECC error in BBT at 0x%012llx\n",
					from & ~mtd->writesize);
				return res;
			} else if (mtd_is_bitflip(res)) {
				pr_info("nand_bbt: corrected error in BBT at 0x%012llx\n",
					from & ~mtd->writesize);
				ret = res;
			} else {
				pr_info("nand_bbt: error reading BBT\n");
				return res;
			}
		}

		 
		for (i = 0; i < len; i++) {
			uint8_t dat = buf[i];
			for (j = 0; j < 8; j += bits, act++) {
				uint8_t tmp = (dat >> j) & msk;
				if (tmp == msk)
					continue;
				if (reserved_block_code && (tmp == reserved_block_code)) {
					pr_info("nand_read_bbt: reserved block at 0x%012llx\n",
						 (loff_t)(offs + act) <<
						 this->bbt_erase_shift);
					bbt_mark_entry(this, offs + act,
							BBT_BLOCK_RESERVED);
					mtd->ecc_stats.bbtblocks++;
					continue;
				}
				 
				pr_info("nand_read_bbt: bad block at 0x%012llx\n",
					 (loff_t)(offs + act) <<
					 this->bbt_erase_shift);
				 
				if (tmp == 0)
					bbt_mark_entry(this, offs + act,
							BBT_BLOCK_FACTORY_BAD);
				else
					bbt_mark_entry(this, offs + act,
							BBT_BLOCK_WORN);
				mtd->ecc_stats.badblocks++;
			}
		}
		totlen -= len;
		from += len;
	}
	return ret;
}

 
static int read_abs_bbt(struct nand_chip *this, uint8_t *buf,
			struct nand_bbt_descr *td, int chip)
{
	struct mtd_info *mtd = nand_to_mtd(this);
	u64 targetsize = nanddev_target_size(&this->base);
	int res = 0, i;

	if (td->options & NAND_BBT_PERCHIP) {
		int offs = 0;
		for (i = 0; i < nanddev_ntargets(&this->base); i++) {
			if (chip == -1 || chip == i)
				res = read_bbt(this, buf, td->pages[i],
					targetsize >> this->bbt_erase_shift,
					td, offs);
			if (res)
				return res;
			offs += targetsize >> this->bbt_erase_shift;
		}
	} else {
		res = read_bbt(this, buf, td->pages[0],
				mtd->size >> this->bbt_erase_shift, td, 0);
		if (res)
			return res;
	}
	return 0;
}

 
static int scan_read_data(struct nand_chip *this, uint8_t *buf, loff_t offs,
			  struct nand_bbt_descr *td)
{
	struct mtd_info *mtd = nand_to_mtd(this);
	size_t retlen;
	size_t len;

	len = td->len;
	if (td->options & NAND_BBT_VERSION)
		len++;

	return mtd_read(mtd, offs, len, &retlen, buf);
}

 
static int scan_read_oob(struct nand_chip *this, uint8_t *buf, loff_t offs,
			 size_t len)
{
	struct mtd_info *mtd = nand_to_mtd(this);
	struct mtd_oob_ops ops = { };
	int res, ret = 0;

	ops.mode = MTD_OPS_PLACE_OOB;
	ops.ooboffs = 0;
	ops.ooblen = mtd->oobsize;

	while (len > 0) {
		ops.datbuf = buf;
		ops.len = min(len, (size_t)mtd->writesize);
		ops.oobbuf = buf + ops.len;

		res = mtd_read_oob(mtd, offs, &ops);
		if (res) {
			if (!mtd_is_bitflip_or_eccerr(res))
				return res;
			else if (mtd_is_eccerr(res) || !ret)
				ret = res;
		}

		buf += mtd->oobsize + mtd->writesize;
		len -= mtd->writesize;
		offs += mtd->writesize;
	}
	return ret;
}

static int scan_read(struct nand_chip *this, uint8_t *buf, loff_t offs,
		     size_t len, struct nand_bbt_descr *td)
{
	if (td->options & NAND_BBT_NO_OOB)
		return scan_read_data(this, buf, offs, td);
	else
		return scan_read_oob(this, buf, offs, len);
}

 
static int scan_write_bbt(struct nand_chip *this, loff_t offs, size_t len,
			  uint8_t *buf, uint8_t *oob)
{
	struct mtd_info *mtd = nand_to_mtd(this);
	struct mtd_oob_ops ops = { };

	ops.mode = MTD_OPS_PLACE_OOB;
	ops.ooboffs = 0;
	ops.ooblen = mtd->oobsize;
	ops.datbuf = buf;
	ops.oobbuf = oob;
	ops.len = len;

	return mtd_write_oob(mtd, offs, &ops);
}

static u32 bbt_get_ver_offs(struct nand_chip *this, struct nand_bbt_descr *td)
{
	struct mtd_info *mtd = nand_to_mtd(this);
	u32 ver_offs = td->veroffs;

	if (!(td->options & NAND_BBT_NO_OOB))
		ver_offs += mtd->writesize;
	return ver_offs;
}

 
static void read_abs_bbts(struct nand_chip *this, uint8_t *buf,
			  struct nand_bbt_descr *td, struct nand_bbt_descr *md)
{
	struct mtd_info *mtd = nand_to_mtd(this);

	 
	if (td->options & NAND_BBT_VERSION) {
		scan_read(this, buf, (loff_t)td->pages[0] << this->page_shift,
			  mtd->writesize, td);
		td->version[0] = buf[bbt_get_ver_offs(this, td)];
		pr_info("Bad block table at page %d, version 0x%02X\n",
			 td->pages[0], td->version[0]);
	}

	 
	if (md && (md->options & NAND_BBT_VERSION)) {
		scan_read(this, buf, (loff_t)md->pages[0] << this->page_shift,
			  mtd->writesize, md);
		md->version[0] = buf[bbt_get_ver_offs(this, md)];
		pr_info("Bad block table at page %d, version 0x%02X\n",
			 md->pages[0], md->version[0]);
	}
}

 
static int scan_block_fast(struct nand_chip *this, struct nand_bbt_descr *bd,
			   loff_t offs, uint8_t *buf)
{
	struct mtd_info *mtd = nand_to_mtd(this);

	struct mtd_oob_ops ops = { };
	int ret, page_offset;

	ops.ooblen = mtd->oobsize;
	ops.oobbuf = buf;
	ops.ooboffs = 0;
	ops.datbuf = NULL;
	ops.mode = MTD_OPS_PLACE_OOB;

	page_offset = nand_bbm_get_next_page(this, 0);

	while (page_offset >= 0) {
		 
		ret = mtd_read_oob(mtd, offs + (page_offset * mtd->writesize),
				   &ops);
		 
		if (ret && !mtd_is_bitflip_or_eccerr(ret))
			return ret;

		if (check_short_pattern(buf, bd))
			return 1;

		page_offset = nand_bbm_get_next_page(this, page_offset + 1);
	}

	return 0;
}

 
static int bbt_block_checkbad(struct nand_chip *this, struct nand_bbt_descr *td,
			      loff_t offs, uint8_t *buf)
{
	struct nand_bbt_descr *bd = this->badblock_pattern;

	 
	if (!(td->options & NAND_BBT_NO_OOB) &&
	    td->offs >= bd->offs && td->offs < bd->offs + bd->len)
		return 0;

	 
	if (this->bbt_options & NAND_BBT_NO_OOB_BBM ||
	    this->options & NAND_NO_BBM_QUIRK)
		return 0;

	if (scan_block_fast(this, bd, offs, buf) > 0)
		return 1;

	return 0;
}

 
static int create_bbt(struct nand_chip *this, uint8_t *buf,
		      struct nand_bbt_descr *bd, int chip)
{
	u64 targetsize = nanddev_target_size(&this->base);
	struct mtd_info *mtd = nand_to_mtd(this);
	int i, numblocks, startblock;
	loff_t from;

	pr_info("Scanning device for bad blocks\n");

	if (chip == -1) {
		numblocks = mtd->size >> this->bbt_erase_shift;
		startblock = 0;
		from = 0;
	} else {
		if (chip >= nanddev_ntargets(&this->base)) {
			pr_warn("create_bbt(): chipnr (%d) > available chips (%d)\n",
			        chip + 1, nanddev_ntargets(&this->base));
			return -EINVAL;
		}
		numblocks = targetsize >> this->bbt_erase_shift;
		startblock = chip * numblocks;
		numblocks += startblock;
		from = (loff_t)startblock << this->bbt_erase_shift;
	}

	for (i = startblock; i < numblocks; i++) {
		int ret;

		BUG_ON(bd->options & NAND_BBT_NO_OOB);

		ret = scan_block_fast(this, bd, from, buf);
		if (ret < 0)
			return ret;

		if (ret) {
			bbt_mark_entry(this, i, BBT_BLOCK_FACTORY_BAD);
			pr_warn("Bad eraseblock %d at 0x%012llx\n",
				i, (unsigned long long)from);
			mtd->ecc_stats.badblocks++;
		}

		from += (1 << this->bbt_erase_shift);
	}
	return 0;
}

 
static int search_bbt(struct nand_chip *this, uint8_t *buf,
		      struct nand_bbt_descr *td)
{
	u64 targetsize = nanddev_target_size(&this->base);
	struct mtd_info *mtd = nand_to_mtd(this);
	int i, chips;
	int startblock, block, dir;
	int scanlen = mtd->writesize + mtd->oobsize;
	int bbtblocks;
	int blocktopage = this->bbt_erase_shift - this->page_shift;

	 
	if (td->options & NAND_BBT_LASTBLOCK) {
		startblock = (mtd->size >> this->bbt_erase_shift) - 1;
		dir = -1;
	} else {
		startblock = 0;
		dir = 1;
	}

	 
	if (td->options & NAND_BBT_PERCHIP) {
		chips = nanddev_ntargets(&this->base);
		bbtblocks = targetsize >> this->bbt_erase_shift;
		startblock &= bbtblocks - 1;
	} else {
		chips = 1;
		bbtblocks = mtd->size >> this->bbt_erase_shift;
	}

	for (i = 0; i < chips; i++) {
		 
		td->version[i] = 0;
		td->pages[i] = -1;
		 
		for (block = 0; block < td->maxblocks; block++) {

			int actblock = startblock + dir * block;
			loff_t offs = (loff_t)actblock << this->bbt_erase_shift;

			 
			if (bbt_block_checkbad(this, td, offs, buf))
				continue;

			 
			scan_read(this, buf, offs, mtd->writesize, td);
			if (!check_pattern(buf, scanlen, mtd->writesize, td)) {
				td->pages[i] = actblock << blocktopage;
				if (td->options & NAND_BBT_VERSION) {
					offs = bbt_get_ver_offs(this, td);
					td->version[i] = buf[offs];
				}
				break;
			}
		}
		startblock += targetsize >> this->bbt_erase_shift;
	}
	 
	for (i = 0; i < chips; i++) {
		if (td->pages[i] == -1)
			pr_warn("Bad block table not found for chip %d\n", i);
		else
			pr_info("Bad block table found at page %d, version 0x%02X\n",
				td->pages[i], td->version[i]);
	}
	return 0;
}

 
static void search_read_bbts(struct nand_chip *this, uint8_t *buf,
			     struct nand_bbt_descr *td,
			     struct nand_bbt_descr *md)
{
	 
	search_bbt(this, buf, td);

	 
	if (md)
		search_bbt(this, buf, md);
}

 
static int get_bbt_block(struct nand_chip *this, struct nand_bbt_descr *td,
			 struct nand_bbt_descr *md, int chip)
{
	u64 targetsize = nanddev_target_size(&this->base);
	int startblock, dir, page, numblocks, i;

	 
	if (td->pages[chip] != -1)
		return td->pages[chip] >>
				(this->bbt_erase_shift - this->page_shift);

	numblocks = (int)(targetsize >> this->bbt_erase_shift);
	if (!(td->options & NAND_BBT_PERCHIP))
		numblocks *= nanddev_ntargets(&this->base);

	 
	if (td->options & NAND_BBT_LASTBLOCK) {
		startblock = numblocks * (chip + 1) - 1;
		dir = -1;
	} else {
		startblock = chip * numblocks;
		dir = 1;
	}

	for (i = 0; i < td->maxblocks; i++) {
		int block = startblock + dir * i;

		 
		switch (bbt_get_entry(this, block)) {
		case BBT_BLOCK_WORN:
		case BBT_BLOCK_FACTORY_BAD:
			continue;
		}

		page = block << (this->bbt_erase_shift - this->page_shift);

		 
		if (!md || md->pages[chip] != page)
			return block;
	}

	return -ENOSPC;
}

 
static void mark_bbt_block_bad(struct nand_chip *this,
			       struct nand_bbt_descr *td,
			       int chip, int block)
{
	loff_t to;
	int res;

	bbt_mark_entry(this, block, BBT_BLOCK_WORN);

	to = (loff_t)block << this->bbt_erase_shift;
	res = nand_markbad_bbm(this, to);
	if (res)
		pr_warn("nand_bbt: error %d while marking block %d bad\n",
			res, block);

	td->pages[chip] = -1;
}

 
static int write_bbt(struct nand_chip *this, uint8_t *buf,
		     struct nand_bbt_descr *td, struct nand_bbt_descr *md,
		     int chipsel)
{
	u64 targetsize = nanddev_target_size(&this->base);
	struct mtd_info *mtd = nand_to_mtd(this);
	struct erase_info einfo;
	int i, res, chip = 0;
	int bits, page, offs, numblocks, sft, sftmsk;
	int nrchips, pageoffs, ooboffs;
	uint8_t msk[4];
	uint8_t rcode = td->reserved_block_code;
	size_t retlen, len = 0;
	loff_t to;
	struct mtd_oob_ops ops = { };

	ops.ooblen = mtd->oobsize;
	ops.ooboffs = 0;
	ops.datbuf = NULL;
	ops.mode = MTD_OPS_PLACE_OOB;

	if (!rcode)
		rcode = 0xff;
	 
	if (td->options & NAND_BBT_PERCHIP) {
		numblocks = (int)(targetsize >> this->bbt_erase_shift);
		 
		if (chipsel == -1) {
			nrchips = nanddev_ntargets(&this->base);
		} else {
			nrchips = chipsel + 1;
			chip = chipsel;
		}
	} else {
		numblocks = (int)(mtd->size >> this->bbt_erase_shift);
		nrchips = 1;
	}

	 
	while (chip < nrchips) {
		int block;

		block = get_bbt_block(this, td, md, chip);
		if (block < 0) {
			pr_err("No space left to write bad block table\n");
			res = block;
			goto outerr;
		}

		 
		page = block << (this->bbt_erase_shift - this->page_shift);

		 
		bits = td->options & NAND_BBT_NRBITS_MSK;
		msk[2] = ~rcode;
		switch (bits) {
		case 1: sft = 3; sftmsk = 0x07; msk[0] = 0x00; msk[1] = 0x01;
			msk[3] = 0x01;
			break;
		case 2: sft = 2; sftmsk = 0x06; msk[0] = 0x00; msk[1] = 0x01;
			msk[3] = 0x03;
			break;
		case 4: sft = 1; sftmsk = 0x04; msk[0] = 0x00; msk[1] = 0x0C;
			msk[3] = 0x0f;
			break;
		case 8: sft = 0; sftmsk = 0x00; msk[0] = 0x00; msk[1] = 0x0F;
			msk[3] = 0xff;
			break;
		default: return -EINVAL;
		}

		to = ((loff_t)page) << this->page_shift;

		 
		if (td->options & NAND_BBT_SAVECONTENT) {
			 
			to &= ~(((loff_t)1 << this->bbt_erase_shift) - 1);
			len = 1 << this->bbt_erase_shift;
			res = mtd_read(mtd, to, len, &retlen, buf);
			if (res < 0) {
				if (retlen != len) {
					pr_info("nand_bbt: error reading block for writing the bad block table\n");
					return res;
				}
				pr_warn("nand_bbt: ECC error while reading block for writing bad block table\n");
			}
			 
			ops.ooblen = (len >> this->page_shift) * mtd->oobsize;
			ops.oobbuf = &buf[len];
			res = mtd_read_oob(mtd, to + mtd->writesize, &ops);
			if (res < 0 || ops.oobretlen != ops.ooblen)
				goto outerr;

			 
			pageoffs = page - (int)(to >> this->page_shift);
			offs = pageoffs << this->page_shift;
			 
			memset(&buf[offs], 0xff, (size_t)(numblocks >> sft));
			ooboffs = len + (pageoffs * mtd->oobsize);

		} else if (td->options & NAND_BBT_NO_OOB) {
			ooboffs = 0;
			offs = td->len;
			 
			if (td->options & NAND_BBT_VERSION)
				offs++;
			 
			len = (size_t)(numblocks >> sft);
			len += offs;
			 
			len = ALIGN(len, mtd->writesize);
			 
			memset(buf, 0xff, len);
			 
			memcpy(buf, td->pattern, td->len);
		} else {
			 
			len = (size_t)(numblocks >> sft);
			 
			len = ALIGN(len, mtd->writesize);
			 
			memset(buf, 0xff, len +
			       (len >> this->page_shift)* mtd->oobsize);
			offs = 0;
			ooboffs = len;
			 
			memcpy(&buf[ooboffs + td->offs], td->pattern, td->len);
		}

		if (td->options & NAND_BBT_VERSION)
			buf[ooboffs + td->veroffs] = td->version[chip];

		 
		for (i = 0; i < numblocks; i++) {
			uint8_t dat;
			int sftcnt = (i << (3 - sft)) & sftmsk;
			dat = bbt_get_entry(this, chip * numblocks + i);
			 
			buf[offs + (i >> sft)] &= ~(msk[dat] << sftcnt);
		}

		memset(&einfo, 0, sizeof(einfo));
		einfo.addr = to;
		einfo.len = 1 << this->bbt_erase_shift;
		res = nand_erase_nand(this, &einfo, 1);
		if (res < 0) {
			pr_warn("nand_bbt: error while erasing BBT block %d\n",
				res);
			mark_bbt_block_bad(this, td, chip, block);
			continue;
		}

		res = scan_write_bbt(this, to, len, buf,
				     td->options & NAND_BBT_NO_OOB ?
				     NULL : &buf[len]);
		if (res < 0) {
			pr_warn("nand_bbt: error while writing BBT block %d\n",
				res);
			mark_bbt_block_bad(this, td, chip, block);
			continue;
		}

		pr_info("Bad block table written to 0x%012llx, version 0x%02X\n",
			 (unsigned long long)to, td->version[chip]);

		 
		td->pages[chip++] = page;
	}
	return 0;

 outerr:
	pr_warn("nand_bbt: error while writing bad block table %d\n", res);
	return res;
}

 
static inline int nand_memory_bbt(struct nand_chip *this,
				  struct nand_bbt_descr *bd)
{
	u8 *pagebuf = nand_get_data_buf(this);

	return create_bbt(this, pagebuf, bd, -1);
}

 
static int check_create(struct nand_chip *this, uint8_t *buf,
			struct nand_bbt_descr *bd)
{
	int i, chips, writeops, create, chipsel, res, res2;
	struct nand_bbt_descr *td = this->bbt_td;
	struct nand_bbt_descr *md = this->bbt_md;
	struct nand_bbt_descr *rd, *rd2;

	 
	if (td->options & NAND_BBT_PERCHIP)
		chips = nanddev_ntargets(&this->base);
	else
		chips = 1;

	for (i = 0; i < chips; i++) {
		writeops = 0;
		create = 0;
		rd = NULL;
		rd2 = NULL;
		res = res2 = 0;
		 
		chipsel = (td->options & NAND_BBT_PERCHIP) ? i : -1;
		 
		if (md) {
			if (td->pages[i] == -1 && md->pages[i] == -1) {
				create = 1;
				writeops = 0x03;
			} else if (td->pages[i] == -1) {
				rd = md;
				writeops = 0x01;
			} else if (md->pages[i] == -1) {
				rd = td;
				writeops = 0x02;
			} else if (td->version[i] == md->version[i]) {
				rd = td;
				if (!(td->options & NAND_BBT_VERSION))
					rd2 = md;
			} else if (((int8_t)(td->version[i] - md->version[i])) > 0) {
				rd = td;
				writeops = 0x02;
			} else {
				rd = md;
				writeops = 0x01;
			}
		} else {
			if (td->pages[i] == -1) {
				create = 1;
				writeops = 0x01;
			} else {
				rd = td;
			}
		}

		if (create) {
			 
			if (!(td->options & NAND_BBT_CREATE))
				continue;

			 
			if (!(this->bbt_options & NAND_BBT_CREATE_EMPTY))
				create_bbt(this, buf, bd, chipsel);

			td->version[i] = 1;
			if (md)
				md->version[i] = 1;
		}

		 
		if (rd) {
			res = read_abs_bbt(this, buf, rd, chipsel);
			if (mtd_is_eccerr(res)) {
				 
				rd->pages[i] = -1;
				rd->version[i] = 0;
				i--;
				continue;
			}
		}
		 
		if (rd2) {
			res2 = read_abs_bbt(this, buf, rd2, chipsel);
			if (mtd_is_eccerr(res2)) {
				 
				rd2->pages[i] = -1;
				rd2->version[i] = 0;
				i--;
				continue;
			}
		}

		 
		if (mtd_is_bitflip(res) || mtd_is_bitflip(res2))
			writeops = 0x03;

		 
		if (md) {
			td->version[i] = max(td->version[i], md->version[i]);
			md->version[i] = td->version[i];
		}

		 
		if ((writeops & 0x01) && (td->options & NAND_BBT_WRITE)) {
			res = write_bbt(this, buf, td, md, chipsel);
			if (res < 0)
				return res;
		}

		 
		if ((writeops & 0x02) && md && (md->options & NAND_BBT_WRITE)) {
			res = write_bbt(this, buf, md, td, chipsel);
			if (res < 0)
				return res;
		}
	}
	return 0;
}

 
static int nand_update_bbt(struct nand_chip *this, loff_t offs)
{
	struct mtd_info *mtd = nand_to_mtd(this);
	int len, res = 0;
	int chip, chipsel;
	uint8_t *buf;
	struct nand_bbt_descr *td = this->bbt_td;
	struct nand_bbt_descr *md = this->bbt_md;

	if (!this->bbt || !td)
		return -EINVAL;

	 
	len = (1 << this->bbt_erase_shift);
	len += (len >> this->page_shift) * mtd->oobsize;
	buf = kmalloc(len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	 
	if (td->options & NAND_BBT_PERCHIP) {
		chip = (int)(offs >> this->chip_shift);
		chipsel = chip;
	} else {
		chip = 0;
		chipsel = -1;
	}

	td->version[chip]++;
	if (md)
		md->version[chip]++;

	 
	if (td->options & NAND_BBT_WRITE) {
		res = write_bbt(this, buf, td, md, chipsel);
		if (res < 0)
			goto out;
	}
	 
	if (md && (md->options & NAND_BBT_WRITE)) {
		res = write_bbt(this, buf, md, td, chipsel);
	}

 out:
	kfree(buf);
	return res;
}

 
static void mark_bbt_region(struct nand_chip *this, struct nand_bbt_descr *td)
{
	u64 targetsize = nanddev_target_size(&this->base);
	struct mtd_info *mtd = nand_to_mtd(this);
	int i, j, chips, block, nrblocks, update;
	uint8_t oldval;

	 
	if (td->options & NAND_BBT_PERCHIP) {
		chips = nanddev_ntargets(&this->base);
		nrblocks = (int)(targetsize >> this->bbt_erase_shift);
	} else {
		chips = 1;
		nrblocks = (int)(mtd->size >> this->bbt_erase_shift);
	}

	for (i = 0; i < chips; i++) {
		if ((td->options & NAND_BBT_ABSPAGE) ||
		    !(td->options & NAND_BBT_WRITE)) {
			if (td->pages[i] == -1)
				continue;
			block = td->pages[i] >> (this->bbt_erase_shift - this->page_shift);
			oldval = bbt_get_entry(this, block);
			bbt_mark_entry(this, block, BBT_BLOCK_RESERVED);
			if ((oldval != BBT_BLOCK_RESERVED) &&
					td->reserved_block_code)
				nand_update_bbt(this, (loff_t)block <<
						this->bbt_erase_shift);
			continue;
		}
		update = 0;
		if (td->options & NAND_BBT_LASTBLOCK)
			block = ((i + 1) * nrblocks) - td->maxblocks;
		else
			block = i * nrblocks;
		for (j = 0; j < td->maxblocks; j++) {
			oldval = bbt_get_entry(this, block);
			bbt_mark_entry(this, block, BBT_BLOCK_RESERVED);
			if (oldval != BBT_BLOCK_RESERVED)
				update = 1;
			block++;
		}
		 
		if (update && td->reserved_block_code)
			nand_update_bbt(this, (loff_t)(block - 1) <<
					this->bbt_erase_shift);
	}
}

 
static void verify_bbt_descr(struct nand_chip *this, struct nand_bbt_descr *bd)
{
	u64 targetsize = nanddev_target_size(&this->base);
	struct mtd_info *mtd = nand_to_mtd(this);
	u32 pattern_len;
	u32 bits;
	u32 table_size;

	if (!bd)
		return;

	pattern_len = bd->len;
	bits = bd->options & NAND_BBT_NRBITS_MSK;

	BUG_ON((this->bbt_options & NAND_BBT_NO_OOB) &&
			!(this->bbt_options & NAND_BBT_USE_FLASH));
	BUG_ON(!bits);

	if (bd->options & NAND_BBT_VERSION)
		pattern_len++;

	if (bd->options & NAND_BBT_NO_OOB) {
		BUG_ON(!(this->bbt_options & NAND_BBT_USE_FLASH));
		BUG_ON(!(this->bbt_options & NAND_BBT_NO_OOB));
		BUG_ON(bd->offs);
		if (bd->options & NAND_BBT_VERSION)
			BUG_ON(bd->veroffs != bd->len);
		BUG_ON(bd->options & NAND_BBT_SAVECONTENT);
	}

	if (bd->options & NAND_BBT_PERCHIP)
		table_size = targetsize >> this->bbt_erase_shift;
	else
		table_size = mtd->size >> this->bbt_erase_shift;
	table_size >>= 3;
	table_size *= bits;
	if (bd->options & NAND_BBT_NO_OOB)
		table_size += pattern_len;
	BUG_ON(table_size > (1 << this->bbt_erase_shift));
}

 
static int nand_scan_bbt(struct nand_chip *this, struct nand_bbt_descr *bd)
{
	struct mtd_info *mtd = nand_to_mtd(this);
	int len, res;
	uint8_t *buf;
	struct nand_bbt_descr *td = this->bbt_td;
	struct nand_bbt_descr *md = this->bbt_md;

	len = (mtd->size >> (this->bbt_erase_shift + 2)) ? : 1;
	 
	this->bbt = kzalloc(len, GFP_KERNEL);
	if (!this->bbt)
		return -ENOMEM;

	 
	if (!td) {
		if ((res = nand_memory_bbt(this, bd))) {
			pr_err("nand_bbt: can't scan flash and build the RAM-based BBT\n");
			goto err_free_bbt;
		}
		return 0;
	}
	verify_bbt_descr(this, td);
	verify_bbt_descr(this, md);

	 
	len = (1 << this->bbt_erase_shift);
	len += (len >> this->page_shift) * mtd->oobsize;
	buf = vmalloc(len);
	if (!buf) {
		res = -ENOMEM;
		goto err_free_bbt;
	}

	 
	if (td->options & NAND_BBT_ABSPAGE) {
		read_abs_bbts(this, buf, td, md);
	} else {
		 
		search_read_bbts(this, buf, td, md);
	}

	res = check_create(this, buf, bd);
	if (res)
		goto err_free_buf;

	 
	mark_bbt_region(this, td);
	if (md)
		mark_bbt_region(this, md);

	vfree(buf);
	return 0;

err_free_buf:
	vfree(buf);
err_free_bbt:
	kfree(this->bbt);
	this->bbt = NULL;
	return res;
}

 
static uint8_t scan_ff_pattern[] = { 0xff, 0xff };

 
static uint8_t bbt_pattern[] = {'B', 'b', 't', '0' };
static uint8_t mirror_pattern[] = {'1', 't', 'b', 'B' };

static struct nand_bbt_descr bbt_main_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs =	8,
	.len = 4,
	.veroffs = 12,
	.maxblocks = NAND_BBT_SCAN_MAXBLOCKS,
	.pattern = bbt_pattern
};

static struct nand_bbt_descr bbt_mirror_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs =	8,
	.len = 4,
	.veroffs = 12,
	.maxblocks = NAND_BBT_SCAN_MAXBLOCKS,
	.pattern = mirror_pattern
};

static struct nand_bbt_descr bbt_main_no_oob_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP
		| NAND_BBT_NO_OOB,
	.len = 4,
	.veroffs = 4,
	.maxblocks = NAND_BBT_SCAN_MAXBLOCKS,
	.pattern = bbt_pattern
};

static struct nand_bbt_descr bbt_mirror_no_oob_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP
		| NAND_BBT_NO_OOB,
	.len = 4,
	.veroffs = 4,
	.maxblocks = NAND_BBT_SCAN_MAXBLOCKS,
	.pattern = mirror_pattern
};

#define BADBLOCK_SCAN_MASK (~NAND_BBT_NO_OOB)
 
static int nand_create_badblock_pattern(struct nand_chip *this)
{
	struct nand_bbt_descr *bd;
	if (this->badblock_pattern) {
		pr_warn("Bad block pattern already allocated; not replacing\n");
		return -EINVAL;
	}
	bd = kzalloc(sizeof(*bd), GFP_KERNEL);
	if (!bd)
		return -ENOMEM;
	bd->options = this->bbt_options & BADBLOCK_SCAN_MASK;
	bd->offs = this->badblockpos;
	bd->len = (this->options & NAND_BUSWIDTH_16) ? 2 : 1;
	bd->pattern = scan_ff_pattern;
	bd->options |= NAND_BBT_DYNAMICSTRUCT;
	this->badblock_pattern = bd;
	return 0;
}

 
int nand_create_bbt(struct nand_chip *this)
{
	int ret;

	 
	if (this->bbt_options & NAND_BBT_USE_FLASH) {
		 
		if (!this->bbt_td) {
			if (this->bbt_options & NAND_BBT_NO_OOB) {
				this->bbt_td = &bbt_main_no_oob_descr;
				this->bbt_md = &bbt_mirror_no_oob_descr;
			} else {
				this->bbt_td = &bbt_main_descr;
				this->bbt_md = &bbt_mirror_descr;
			}
		}
	} else {
		this->bbt_td = NULL;
		this->bbt_md = NULL;
	}

	if (!this->badblock_pattern) {
		ret = nand_create_badblock_pattern(this);
		if (ret)
			return ret;
	}

	return nand_scan_bbt(this, this->badblock_pattern);
}
EXPORT_SYMBOL(nand_create_bbt);

 
int nand_isreserved_bbt(struct nand_chip *this, loff_t offs)
{
	int block;

	block = (int)(offs >> this->bbt_erase_shift);
	return bbt_get_entry(this, block) == BBT_BLOCK_RESERVED;
}

 
int nand_isbad_bbt(struct nand_chip *this, loff_t offs, int allowbbt)
{
	int block, res;

	block = (int)(offs >> this->bbt_erase_shift);
	res = bbt_get_entry(this, block);

	pr_debug("nand_isbad_bbt(): bbt info for offs 0x%08x: (block %d) 0x%02x\n",
		 (unsigned int)offs, block, res);

	if (mtd_check_expert_analysis_mode())
		return 0;

	switch (res) {
	case BBT_BLOCK_GOOD:
		return 0;
	case BBT_BLOCK_WORN:
		return 1;
	case BBT_BLOCK_RESERVED:
		return allowbbt ? 0 : 1;
	}
	return 1;
}

 
int nand_markbad_bbt(struct nand_chip *this, loff_t offs)
{
	int block, ret = 0;

	block = (int)(offs >> this->bbt_erase_shift);

	 
	bbt_mark_entry(this, block, BBT_BLOCK_WORN);

	 
	if (this->bbt_options & NAND_BBT_USE_FLASH)
		ret = nand_update_bbt(this, offs);

	return ret;
}
