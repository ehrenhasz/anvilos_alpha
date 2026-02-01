
 

 

#include <linux/crc32.h>
#include <linux/err.h>
#include <linux/slab.h>
#include "ubi.h"

static int self_check_not_bad(const struct ubi_device *ubi, int pnum);
static int self_check_peb_ec_hdr(const struct ubi_device *ubi, int pnum);
static int self_check_ec_hdr(const struct ubi_device *ubi, int pnum,
			     const struct ubi_ec_hdr *ec_hdr);
static int self_check_peb_vid_hdr(const struct ubi_device *ubi, int pnum);
static int self_check_vid_hdr(const struct ubi_device *ubi, int pnum,
			      const struct ubi_vid_hdr *vid_hdr);
static int self_check_write(struct ubi_device *ubi, const void *buf, int pnum,
			    int offset, int len);

 
int ubi_io_read(const struct ubi_device *ubi, void *buf, int pnum, int offset,
		int len)
{
	int err, retries = 0;
	size_t read;
	loff_t addr;

	dbg_io("read %d bytes from PEB %d:%d", len, pnum, offset);

	ubi_assert(pnum >= 0 && pnum < ubi->peb_count);
	ubi_assert(offset >= 0 && offset + len <= ubi->peb_size);
	ubi_assert(len > 0);

	err = self_check_not_bad(ubi, pnum);
	if (err)
		return err;

	 
	*((uint8_t *)buf) ^= 0xFF;

	addr = (loff_t)pnum * ubi->peb_size + offset;
retry:
	err = mtd_read(ubi->mtd, addr, len, &read, buf);
	if (err) {
		const char *errstr = mtd_is_eccerr(err) ? " (ECC error)" : "";

		if (mtd_is_bitflip(err)) {
			 
			ubi_msg(ubi, "fixable bit-flip detected at PEB %d",
				pnum);
			ubi_assert(len == read);
			return UBI_IO_BITFLIPS;
		}

		if (retries++ < UBI_IO_RETRIES) {
			ubi_warn(ubi, "error %d%s while reading %d bytes from PEB %d:%d, read only %zd bytes, retry",
				 err, errstr, len, pnum, offset, read);
			yield();
			goto retry;
		}

		ubi_err(ubi, "error %d%s while reading %d bytes from PEB %d:%d, read %zd bytes",
			err, errstr, len, pnum, offset, read);
		dump_stack();

		 
		if (read != len && mtd_is_eccerr(err)) {
			ubi_assert(0);
			err = -EIO;
		}
	} else {
		ubi_assert(len == read);

		if (ubi_dbg_is_bitflip(ubi)) {
			dbg_gen("bit-flip (emulated)");
			err = UBI_IO_BITFLIPS;
		}
	}

	return err;
}

 
int ubi_io_write(struct ubi_device *ubi, const void *buf, int pnum, int offset,
		 int len)
{
	int err;
	size_t written;
	loff_t addr;

	dbg_io("write %d bytes to PEB %d:%d", len, pnum, offset);

	ubi_assert(pnum >= 0 && pnum < ubi->peb_count);
	ubi_assert(offset >= 0 && offset + len <= ubi->peb_size);
	ubi_assert(offset % ubi->hdrs_min_io_size == 0);
	ubi_assert(len > 0 && len % ubi->hdrs_min_io_size == 0);

	if (ubi->ro_mode) {
		ubi_err(ubi, "read-only mode");
		return -EROFS;
	}

	err = self_check_not_bad(ubi, pnum);
	if (err)
		return err;

	 
	err = ubi_self_check_all_ff(ubi, pnum, offset, len);
	if (err)
		return err;

	if (offset >= ubi->leb_start) {
		 
		err = self_check_peb_ec_hdr(ubi, pnum);
		if (err)
			return err;
		err = self_check_peb_vid_hdr(ubi, pnum);
		if (err)
			return err;
	}

	if (ubi_dbg_is_write_failure(ubi)) {
		ubi_err(ubi, "cannot write %d bytes to PEB %d:%d (emulated)",
			len, pnum, offset);
		dump_stack();
		return -EIO;
	}

	addr = (loff_t)pnum * ubi->peb_size + offset;
	err = mtd_write(ubi->mtd, addr, len, &written, buf);
	if (err) {
		ubi_err(ubi, "error %d while writing %d bytes to PEB %d:%d, written %zd bytes",
			err, len, pnum, offset, written);
		dump_stack();
		ubi_dump_flash(ubi, pnum, offset, len);
	} else
		ubi_assert(written == len);

	if (!err) {
		err = self_check_write(ubi, buf, pnum, offset, len);
		if (err)
			return err;

		 
		offset += len;
		len = ubi->peb_size - offset;
		if (len)
			err = ubi_self_check_all_ff(ubi, pnum, offset, len);
	}

	return err;
}

 
static int do_sync_erase(struct ubi_device *ubi, int pnum)
{
	int err, retries = 0;
	struct erase_info ei;

	dbg_io("erase PEB %d", pnum);
	ubi_assert(pnum >= 0 && pnum < ubi->peb_count);

	if (ubi->ro_mode) {
		ubi_err(ubi, "read-only mode");
		return -EROFS;
	}

retry:
	memset(&ei, 0, sizeof(struct erase_info));

	ei.addr     = (loff_t)pnum * ubi->peb_size;
	ei.len      = ubi->peb_size;

	err = mtd_erase(ubi->mtd, &ei);
	if (err) {
		if (retries++ < UBI_IO_RETRIES) {
			ubi_warn(ubi, "error %d while erasing PEB %d, retry",
				 err, pnum);
			yield();
			goto retry;
		}
		ubi_err(ubi, "cannot erase PEB %d, error %d", pnum, err);
		dump_stack();
		return err;
	}

	err = ubi_self_check_all_ff(ubi, pnum, 0, ubi->peb_size);
	if (err)
		return err;

	if (ubi_dbg_is_erase_failure(ubi)) {
		ubi_err(ubi, "cannot erase PEB %d (emulated)", pnum);
		return -EIO;
	}

	return 0;
}

 
static uint8_t patterns[] = {0xa5, 0x5a, 0x0};

 
static int torture_peb(struct ubi_device *ubi, int pnum)
{
	int err, i, patt_count;

	ubi_msg(ubi, "run torture test for PEB %d", pnum);
	patt_count = ARRAY_SIZE(patterns);
	ubi_assert(patt_count > 0);

	mutex_lock(&ubi->buf_mutex);
	for (i = 0; i < patt_count; i++) {
		err = do_sync_erase(ubi, pnum);
		if (err)
			goto out;

		 
		err = ubi_io_read(ubi, ubi->peb_buf, pnum, 0, ubi->peb_size);
		if (err)
			goto out;

		err = ubi_check_pattern(ubi->peb_buf, 0xFF, ubi->peb_size);
		if (err == 0) {
			ubi_err(ubi, "erased PEB %d, but a non-0xFF byte found",
				pnum);
			err = -EIO;
			goto out;
		}

		 
		memset(ubi->peb_buf, patterns[i], ubi->peb_size);
		err = ubi_io_write(ubi, ubi->peb_buf, pnum, 0, ubi->peb_size);
		if (err)
			goto out;

		memset(ubi->peb_buf, ~patterns[i], ubi->peb_size);
		err = ubi_io_read(ubi, ubi->peb_buf, pnum, 0, ubi->peb_size);
		if (err)
			goto out;

		err = ubi_check_pattern(ubi->peb_buf, patterns[i],
					ubi->peb_size);
		if (err == 0) {
			ubi_err(ubi, "pattern %x checking failed for PEB %d",
				patterns[i], pnum);
			err = -EIO;
			goto out;
		}
	}

	err = patt_count;
	ubi_msg(ubi, "PEB %d passed torture test, do not mark it as bad", pnum);

out:
	mutex_unlock(&ubi->buf_mutex);
	if (err == UBI_IO_BITFLIPS || mtd_is_eccerr(err)) {
		 
		ubi_err(ubi, "read problems on freshly erased PEB %d, must be bad",
			pnum);
		err = -EIO;
	}
	return err;
}

 
static int nor_erase_prepare(struct ubi_device *ubi, int pnum)
{
	int err;
	size_t written;
	loff_t addr;
	uint32_t data = 0;
	struct ubi_ec_hdr ec_hdr;
	struct ubi_vid_io_buf vidb;

	 
	struct ubi_vid_hdr vid_hdr;

	 
	addr = (loff_t)pnum * ubi->peb_size;
	err = ubi_io_read_ec_hdr(ubi, pnum, &ec_hdr, 0);
	if (err != UBI_IO_BAD_HDR_EBADMSG && err != UBI_IO_BAD_HDR &&
	    err != UBI_IO_FF){
		err = mtd_write(ubi->mtd, addr, 4, &written, (void *)&data);
		if(err)
			goto error;
	}

	ubi_init_vid_buf(ubi, &vidb, &vid_hdr);
	ubi_assert(&vid_hdr == ubi_get_vid_hdr(&vidb));

	err = ubi_io_read_vid_hdr(ubi, pnum, &vidb, 0);
	if (err != UBI_IO_BAD_HDR_EBADMSG && err != UBI_IO_BAD_HDR &&
	    err != UBI_IO_FF){
		addr += ubi->vid_hdr_aloffset;
		err = mtd_write(ubi->mtd, addr, 4, &written, (void *)&data);
		if (err)
			goto error;
	}
	return 0;

error:
	 
	ubi_err(ubi, "cannot invalidate PEB %d, write returned %d", pnum, err);
	ubi_dump_flash(ubi, pnum, 0, ubi->peb_size);
	return -EIO;
}

 
int ubi_io_sync_erase(struct ubi_device *ubi, int pnum, int torture)
{
	int err, ret = 0;

	ubi_assert(pnum >= 0 && pnum < ubi->peb_count);

	err = self_check_not_bad(ubi, pnum);
	if (err != 0)
		return err;

	if (ubi->ro_mode) {
		ubi_err(ubi, "read-only mode");
		return -EROFS;
	}

	 
	if (ubi->nor_flash && ubi->mtd->writesize == 1) {
		err = nor_erase_prepare(ubi, pnum);
		if (err)
			return err;
	}

	if (torture) {
		ret = torture_peb(ubi, pnum);
		if (ret < 0)
			return ret;
	}

	err = do_sync_erase(ubi, pnum);
	if (err)
		return err;

	return ret + 1;
}

 
int ubi_io_is_bad(const struct ubi_device *ubi, int pnum)
{
	struct mtd_info *mtd = ubi->mtd;

	ubi_assert(pnum >= 0 && pnum < ubi->peb_count);

	if (ubi->bad_allowed) {
		int ret;

		ret = mtd_block_isbad(mtd, (loff_t)pnum * ubi->peb_size);
		if (ret < 0)
			ubi_err(ubi, "error %d while checking if PEB %d is bad",
				ret, pnum);
		else if (ret)
			dbg_io("PEB %d is bad", pnum);
		return ret;
	}

	return 0;
}

 
int ubi_io_mark_bad(const struct ubi_device *ubi, int pnum)
{
	int err;
	struct mtd_info *mtd = ubi->mtd;

	ubi_assert(pnum >= 0 && pnum < ubi->peb_count);

	if (ubi->ro_mode) {
		ubi_err(ubi, "read-only mode");
		return -EROFS;
	}

	if (!ubi->bad_allowed)
		return 0;

	err = mtd_block_markbad(mtd, (loff_t)pnum * ubi->peb_size);
	if (err)
		ubi_err(ubi, "cannot mark PEB %d bad, error %d", pnum, err);
	return err;
}

 
static int validate_ec_hdr(const struct ubi_device *ubi,
			   const struct ubi_ec_hdr *ec_hdr)
{
	long long ec;
	int vid_hdr_offset, leb_start;

	ec = be64_to_cpu(ec_hdr->ec);
	vid_hdr_offset = be32_to_cpu(ec_hdr->vid_hdr_offset);
	leb_start = be32_to_cpu(ec_hdr->data_offset);

	if (ec_hdr->version != UBI_VERSION) {
		ubi_err(ubi, "node with incompatible UBI version found: this UBI version is %d, image version is %d",
			UBI_VERSION, (int)ec_hdr->version);
		goto bad;
	}

	if (vid_hdr_offset != ubi->vid_hdr_offset) {
		ubi_err(ubi, "bad VID header offset %d, expected %d",
			vid_hdr_offset, ubi->vid_hdr_offset);
		goto bad;
	}

	if (leb_start != ubi->leb_start) {
		ubi_err(ubi, "bad data offset %d, expected %d",
			leb_start, ubi->leb_start);
		goto bad;
	}

	if (ec < 0 || ec > UBI_MAX_ERASECOUNTER) {
		ubi_err(ubi, "bad erase counter %lld", ec);
		goto bad;
	}

	return 0;

bad:
	ubi_err(ubi, "bad EC header");
	ubi_dump_ec_hdr(ec_hdr);
	dump_stack();
	return 1;
}

 
int ubi_io_read_ec_hdr(struct ubi_device *ubi, int pnum,
		       struct ubi_ec_hdr *ec_hdr, int verbose)
{
	int err, read_err;
	uint32_t crc, magic, hdr_crc;

	dbg_io("read EC header from PEB %d", pnum);
	ubi_assert(pnum >= 0 && pnum < ubi->peb_count);

	read_err = ubi_io_read(ubi, ec_hdr, pnum, 0, UBI_EC_HDR_SIZE);
	if (read_err) {
		if (read_err != UBI_IO_BITFLIPS && !mtd_is_eccerr(read_err))
			return read_err;

		 
	}

	magic = be32_to_cpu(ec_hdr->magic);
	if (magic != UBI_EC_HDR_MAGIC) {
		if (mtd_is_eccerr(read_err))
			return UBI_IO_BAD_HDR_EBADMSG;

		 
		if (ubi_check_pattern(ec_hdr, 0xFF, UBI_EC_HDR_SIZE)) {
			 
			if (verbose)
				ubi_warn(ubi, "no EC header found at PEB %d, only 0xFF bytes",
					 pnum);
			dbg_bld("no EC header found at PEB %d, only 0xFF bytes",
				pnum);
			if (!read_err)
				return UBI_IO_FF;
			else
				return UBI_IO_FF_BITFLIPS;
		}

		 
		if (verbose) {
			ubi_warn(ubi, "bad magic number at PEB %d: %08x instead of %08x",
				 pnum, magic, UBI_EC_HDR_MAGIC);
			ubi_dump_ec_hdr(ec_hdr);
		}
		dbg_bld("bad magic number at PEB %d: %08x instead of %08x",
			pnum, magic, UBI_EC_HDR_MAGIC);
		return UBI_IO_BAD_HDR;
	}

	crc = crc32(UBI_CRC32_INIT, ec_hdr, UBI_EC_HDR_SIZE_CRC);
	hdr_crc = be32_to_cpu(ec_hdr->hdr_crc);

	if (hdr_crc != crc) {
		if (verbose) {
			ubi_warn(ubi, "bad EC header CRC at PEB %d, calculated %#08x, read %#08x",
				 pnum, crc, hdr_crc);
			ubi_dump_ec_hdr(ec_hdr);
		}
		dbg_bld("bad EC header CRC at PEB %d, calculated %#08x, read %#08x",
			pnum, crc, hdr_crc);

		if (!read_err)
			return UBI_IO_BAD_HDR;
		else
			return UBI_IO_BAD_HDR_EBADMSG;
	}

	 
	err = validate_ec_hdr(ubi, ec_hdr);
	if (err) {
		ubi_err(ubi, "validation failed for PEB %d", pnum);
		return -EINVAL;
	}

	 
	return read_err ? UBI_IO_BITFLIPS : 0;
}

 
int ubi_io_write_ec_hdr(struct ubi_device *ubi, int pnum,
			struct ubi_ec_hdr *ec_hdr)
{
	int err;
	uint32_t crc;

	dbg_io("write EC header to PEB %d", pnum);
	ubi_assert(pnum >= 0 &&  pnum < ubi->peb_count);

	ec_hdr->magic = cpu_to_be32(UBI_EC_HDR_MAGIC);
	ec_hdr->version = UBI_VERSION;
	ec_hdr->vid_hdr_offset = cpu_to_be32(ubi->vid_hdr_offset);
	ec_hdr->data_offset = cpu_to_be32(ubi->leb_start);
	ec_hdr->image_seq = cpu_to_be32(ubi->image_seq);
	crc = crc32(UBI_CRC32_INIT, ec_hdr, UBI_EC_HDR_SIZE_CRC);
	ec_hdr->hdr_crc = cpu_to_be32(crc);

	err = self_check_ec_hdr(ubi, pnum, ec_hdr);
	if (err)
		return err;

	if (ubi_dbg_power_cut(ubi, POWER_CUT_EC_WRITE))
		return -EROFS;

	err = ubi_io_write(ubi, ec_hdr, pnum, 0, ubi->ec_hdr_alsize);
	return err;
}

 
static int validate_vid_hdr(const struct ubi_device *ubi,
			    const struct ubi_vid_hdr *vid_hdr)
{
	int vol_type = vid_hdr->vol_type;
	int copy_flag = vid_hdr->copy_flag;
	int vol_id = be32_to_cpu(vid_hdr->vol_id);
	int lnum = be32_to_cpu(vid_hdr->lnum);
	int compat = vid_hdr->compat;
	int data_size = be32_to_cpu(vid_hdr->data_size);
	int used_ebs = be32_to_cpu(vid_hdr->used_ebs);
	int data_pad = be32_to_cpu(vid_hdr->data_pad);
	int data_crc = be32_to_cpu(vid_hdr->data_crc);
	int usable_leb_size = ubi->leb_size - data_pad;

	if (copy_flag != 0 && copy_flag != 1) {
		ubi_err(ubi, "bad copy_flag");
		goto bad;
	}

	if (vol_id < 0 || lnum < 0 || data_size < 0 || used_ebs < 0 ||
	    data_pad < 0) {
		ubi_err(ubi, "negative values");
		goto bad;
	}

	if (vol_id >= UBI_MAX_VOLUMES && vol_id < UBI_INTERNAL_VOL_START) {
		ubi_err(ubi, "bad vol_id");
		goto bad;
	}

	if (vol_id < UBI_INTERNAL_VOL_START && compat != 0) {
		ubi_err(ubi, "bad compat");
		goto bad;
	}

	if (vol_id >= UBI_INTERNAL_VOL_START && compat != UBI_COMPAT_DELETE &&
	    compat != UBI_COMPAT_RO && compat != UBI_COMPAT_PRESERVE &&
	    compat != UBI_COMPAT_REJECT) {
		ubi_err(ubi, "bad compat");
		goto bad;
	}

	if (vol_type != UBI_VID_DYNAMIC && vol_type != UBI_VID_STATIC) {
		ubi_err(ubi, "bad vol_type");
		goto bad;
	}

	if (data_pad >= ubi->leb_size / 2) {
		ubi_err(ubi, "bad data_pad");
		goto bad;
	}

	if (data_size > ubi->leb_size) {
		ubi_err(ubi, "bad data_size");
		goto bad;
	}

	if (vol_type == UBI_VID_STATIC) {
		 
		if (used_ebs == 0) {
			ubi_err(ubi, "zero used_ebs");
			goto bad;
		}
		if (data_size == 0) {
			ubi_err(ubi, "zero data_size");
			goto bad;
		}
		if (lnum < used_ebs - 1) {
			if (data_size != usable_leb_size) {
				ubi_err(ubi, "bad data_size");
				goto bad;
			}
		} else if (lnum > used_ebs - 1) {
			ubi_err(ubi, "too high lnum");
			goto bad;
		}
	} else {
		if (copy_flag == 0) {
			if (data_crc != 0) {
				ubi_err(ubi, "non-zero data CRC");
				goto bad;
			}
			if (data_size != 0) {
				ubi_err(ubi, "non-zero data_size");
				goto bad;
			}
		} else {
			if (data_size == 0) {
				ubi_err(ubi, "zero data_size of copy");
				goto bad;
			}
		}
		if (used_ebs != 0) {
			ubi_err(ubi, "bad used_ebs");
			goto bad;
		}
	}

	return 0;

bad:
	ubi_err(ubi, "bad VID header");
	ubi_dump_vid_hdr(vid_hdr);
	dump_stack();
	return 1;
}

 
int ubi_io_read_vid_hdr(struct ubi_device *ubi, int pnum,
			struct ubi_vid_io_buf *vidb, int verbose)
{
	int err, read_err;
	uint32_t crc, magic, hdr_crc;
	struct ubi_vid_hdr *vid_hdr = ubi_get_vid_hdr(vidb);
	void *p = vidb->buffer;

	dbg_io("read VID header from PEB %d", pnum);
	ubi_assert(pnum >= 0 &&  pnum < ubi->peb_count);

	read_err = ubi_io_read(ubi, p, pnum, ubi->vid_hdr_aloffset,
			  ubi->vid_hdr_shift + UBI_VID_HDR_SIZE);
	if (read_err && read_err != UBI_IO_BITFLIPS && !mtd_is_eccerr(read_err))
		return read_err;

	magic = be32_to_cpu(vid_hdr->magic);
	if (magic != UBI_VID_HDR_MAGIC) {
		if (mtd_is_eccerr(read_err))
			return UBI_IO_BAD_HDR_EBADMSG;

		if (ubi_check_pattern(vid_hdr, 0xFF, UBI_VID_HDR_SIZE)) {
			if (verbose)
				ubi_warn(ubi, "no VID header found at PEB %d, only 0xFF bytes",
					 pnum);
			dbg_bld("no VID header found at PEB %d, only 0xFF bytes",
				pnum);
			if (!read_err)
				return UBI_IO_FF;
			else
				return UBI_IO_FF_BITFLIPS;
		}

		if (verbose) {
			ubi_warn(ubi, "bad magic number at PEB %d: %08x instead of %08x",
				 pnum, magic, UBI_VID_HDR_MAGIC);
			ubi_dump_vid_hdr(vid_hdr);
		}
		dbg_bld("bad magic number at PEB %d: %08x instead of %08x",
			pnum, magic, UBI_VID_HDR_MAGIC);
		return UBI_IO_BAD_HDR;
	}

	crc = crc32(UBI_CRC32_INIT, vid_hdr, UBI_VID_HDR_SIZE_CRC);
	hdr_crc = be32_to_cpu(vid_hdr->hdr_crc);

	if (hdr_crc != crc) {
		if (verbose) {
			ubi_warn(ubi, "bad CRC at PEB %d, calculated %#08x, read %#08x",
				 pnum, crc, hdr_crc);
			ubi_dump_vid_hdr(vid_hdr);
		}
		dbg_bld("bad CRC at PEB %d, calculated %#08x, read %#08x",
			pnum, crc, hdr_crc);
		if (!read_err)
			return UBI_IO_BAD_HDR;
		else
			return UBI_IO_BAD_HDR_EBADMSG;
	}

	err = validate_vid_hdr(ubi, vid_hdr);
	if (err) {
		ubi_err(ubi, "validation failed for PEB %d", pnum);
		return -EINVAL;
	}

	return read_err ? UBI_IO_BITFLIPS : 0;
}

 
int ubi_io_write_vid_hdr(struct ubi_device *ubi, int pnum,
			 struct ubi_vid_io_buf *vidb)
{
	struct ubi_vid_hdr *vid_hdr = ubi_get_vid_hdr(vidb);
	int err;
	uint32_t crc;
	void *p = vidb->buffer;

	dbg_io("write VID header to PEB %d", pnum);
	ubi_assert(pnum >= 0 &&  pnum < ubi->peb_count);

	err = self_check_peb_ec_hdr(ubi, pnum);
	if (err)
		return err;

	vid_hdr->magic = cpu_to_be32(UBI_VID_HDR_MAGIC);
	vid_hdr->version = UBI_VERSION;
	crc = crc32(UBI_CRC32_INIT, vid_hdr, UBI_VID_HDR_SIZE_CRC);
	vid_hdr->hdr_crc = cpu_to_be32(crc);

	err = self_check_vid_hdr(ubi, pnum, vid_hdr);
	if (err)
		return err;

	if (ubi_dbg_power_cut(ubi, POWER_CUT_VID_WRITE))
		return -EROFS;

	err = ubi_io_write(ubi, p, pnum, ubi->vid_hdr_aloffset,
			   ubi->vid_hdr_alsize);
	return err;
}

 
static int self_check_not_bad(const struct ubi_device *ubi, int pnum)
{
	int err;

	if (!ubi_dbg_chk_io(ubi))
		return 0;

	err = ubi_io_is_bad(ubi, pnum);
	if (!err)
		return err;

	ubi_err(ubi, "self-check failed for PEB %d", pnum);
	dump_stack();
	return err > 0 ? -EINVAL : err;
}

 
static int self_check_ec_hdr(const struct ubi_device *ubi, int pnum,
			     const struct ubi_ec_hdr *ec_hdr)
{
	int err;
	uint32_t magic;

	if (!ubi_dbg_chk_io(ubi))
		return 0;

	magic = be32_to_cpu(ec_hdr->magic);
	if (magic != UBI_EC_HDR_MAGIC) {
		ubi_err(ubi, "bad magic %#08x, must be %#08x",
			magic, UBI_EC_HDR_MAGIC);
		goto fail;
	}

	err = validate_ec_hdr(ubi, ec_hdr);
	if (err) {
		ubi_err(ubi, "self-check failed for PEB %d", pnum);
		goto fail;
	}

	return 0;

fail:
	ubi_dump_ec_hdr(ec_hdr);
	dump_stack();
	return -EINVAL;
}

 
static int self_check_peb_ec_hdr(const struct ubi_device *ubi, int pnum)
{
	int err;
	uint32_t crc, hdr_crc;
	struct ubi_ec_hdr *ec_hdr;

	if (!ubi_dbg_chk_io(ubi))
		return 0;

	ec_hdr = kzalloc(ubi->ec_hdr_alsize, GFP_NOFS);
	if (!ec_hdr)
		return -ENOMEM;

	err = ubi_io_read(ubi, ec_hdr, pnum, 0, UBI_EC_HDR_SIZE);
	if (err && err != UBI_IO_BITFLIPS && !mtd_is_eccerr(err))
		goto exit;

	crc = crc32(UBI_CRC32_INIT, ec_hdr, UBI_EC_HDR_SIZE_CRC);
	hdr_crc = be32_to_cpu(ec_hdr->hdr_crc);
	if (hdr_crc != crc) {
		ubi_err(ubi, "bad CRC, calculated %#08x, read %#08x",
			crc, hdr_crc);
		ubi_err(ubi, "self-check failed for PEB %d", pnum);
		ubi_dump_ec_hdr(ec_hdr);
		dump_stack();
		err = -EINVAL;
		goto exit;
	}

	err = self_check_ec_hdr(ubi, pnum, ec_hdr);

exit:
	kfree(ec_hdr);
	return err;
}

 
static int self_check_vid_hdr(const struct ubi_device *ubi, int pnum,
			      const struct ubi_vid_hdr *vid_hdr)
{
	int err;
	uint32_t magic;

	if (!ubi_dbg_chk_io(ubi))
		return 0;

	magic = be32_to_cpu(vid_hdr->magic);
	if (magic != UBI_VID_HDR_MAGIC) {
		ubi_err(ubi, "bad VID header magic %#08x at PEB %d, must be %#08x",
			magic, pnum, UBI_VID_HDR_MAGIC);
		goto fail;
	}

	err = validate_vid_hdr(ubi, vid_hdr);
	if (err) {
		ubi_err(ubi, "self-check failed for PEB %d", pnum);
		goto fail;
	}

	return err;

fail:
	ubi_err(ubi, "self-check failed for PEB %d", pnum);
	ubi_dump_vid_hdr(vid_hdr);
	dump_stack();
	return -EINVAL;

}

 
static int self_check_peb_vid_hdr(const struct ubi_device *ubi, int pnum)
{
	int err;
	uint32_t crc, hdr_crc;
	struct ubi_vid_io_buf *vidb;
	struct ubi_vid_hdr *vid_hdr;
	void *p;

	if (!ubi_dbg_chk_io(ubi))
		return 0;

	vidb = ubi_alloc_vid_buf(ubi, GFP_NOFS);
	if (!vidb)
		return -ENOMEM;

	vid_hdr = ubi_get_vid_hdr(vidb);
	p = vidb->buffer;
	err = ubi_io_read(ubi, p, pnum, ubi->vid_hdr_aloffset,
			  ubi->vid_hdr_alsize);
	if (err && err != UBI_IO_BITFLIPS && !mtd_is_eccerr(err))
		goto exit;

	crc = crc32(UBI_CRC32_INIT, vid_hdr, UBI_VID_HDR_SIZE_CRC);
	hdr_crc = be32_to_cpu(vid_hdr->hdr_crc);
	if (hdr_crc != crc) {
		ubi_err(ubi, "bad VID header CRC at PEB %d, calculated %#08x, read %#08x",
			pnum, crc, hdr_crc);
		ubi_err(ubi, "self-check failed for PEB %d", pnum);
		ubi_dump_vid_hdr(vid_hdr);
		dump_stack();
		err = -EINVAL;
		goto exit;
	}

	err = self_check_vid_hdr(ubi, pnum, vid_hdr);

exit:
	ubi_free_vid_buf(vidb);
	return err;
}

 
static int self_check_write(struct ubi_device *ubi, const void *buf, int pnum,
			    int offset, int len)
{
	int err, i;
	size_t read;
	void *buf1;
	loff_t addr = (loff_t)pnum * ubi->peb_size + offset;

	if (!ubi_dbg_chk_io(ubi))
		return 0;

	buf1 = __vmalloc(len, GFP_NOFS);
	if (!buf1) {
		ubi_err(ubi, "cannot allocate memory to check writes");
		return 0;
	}

	err = mtd_read(ubi->mtd, addr, len, &read, buf1);
	if (err && !mtd_is_bitflip(err))
		goto out_free;

	for (i = 0; i < len; i++) {
		uint8_t c = ((uint8_t *)buf)[i];
		uint8_t c1 = ((uint8_t *)buf1)[i];
		int dump_len;

		if (c == c1)
			continue;

		ubi_err(ubi, "self-check failed for PEB %d:%d, len %d",
			pnum, offset, len);
		ubi_msg(ubi, "data differ at position %d", i);
		dump_len = max_t(int, 128, len - i);
		ubi_msg(ubi, "hex dump of the original buffer from %d to %d",
			i, i + dump_len);
		print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET, 32, 1,
			       buf + i, dump_len, 1);
		ubi_msg(ubi, "hex dump of the read buffer from %d to %d",
			i, i + dump_len);
		print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET, 32, 1,
			       buf1 + i, dump_len, 1);
		dump_stack();
		err = -EINVAL;
		goto out_free;
	}

	vfree(buf1);
	return 0;

out_free:
	vfree(buf1);
	return err;
}

 
int ubi_self_check_all_ff(struct ubi_device *ubi, int pnum, int offset, int len)
{
	size_t read;
	int err;
	void *buf;
	loff_t addr = (loff_t)pnum * ubi->peb_size + offset;

	if (!ubi_dbg_chk_io(ubi))
		return 0;

	buf = __vmalloc(len, GFP_NOFS);
	if (!buf) {
		ubi_err(ubi, "cannot allocate memory to check for 0xFFs");
		return 0;
	}

	err = mtd_read(ubi->mtd, addr, len, &read, buf);
	if (err && !mtd_is_bitflip(err)) {
		ubi_err(ubi, "err %d while reading %d bytes from PEB %d:%d, read %zd bytes",
			err, len, pnum, offset, read);
		goto error;
	}

	err = ubi_check_pattern(buf, 0xFF, len);
	if (err == 0) {
		ubi_err(ubi, "flash region at PEB %d:%d, length %d does not contain all 0xFF bytes",
			pnum, offset, len);
		goto fail;
	}

	vfree(buf);
	return 0;

fail:
	ubi_err(ubi, "self-check failed for PEB %d", pnum);
	ubi_msg(ubi, "hex dump of the %d-%d region", offset, offset + len);
	print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET, 32, 1, buf, len, 1);
	err = -EINVAL;
error:
	dump_stack();
	vfree(buf);
	return err;
}
