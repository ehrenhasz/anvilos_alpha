
 

 

#include "ubi.h"

 
int ubi_calc_data_len(const struct ubi_device *ubi, const void *buf,
		      int length)
{
	int i;

	ubi_assert(!(length & (ubi->min_io_size - 1)));

	for (i = length - 1; i >= 0; i--)
		if (((const uint8_t *)buf)[i] != 0xFF)
			break;

	 
	length = ALIGN(i + 1, ubi->min_io_size);
	return length;
}

 
int ubi_check_volume(struct ubi_device *ubi, int vol_id)
{
	void *buf;
	int err = 0, i;
	struct ubi_volume *vol = ubi->volumes[vol_id];

	if (vol->vol_type != UBI_STATIC_VOLUME)
		return 0;

	buf = vmalloc(vol->usable_leb_size);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < vol->used_ebs; i++) {
		int size;

		cond_resched();

		if (i == vol->used_ebs - 1)
			size = vol->last_eb_bytes;
		else
			size = vol->usable_leb_size;

		err = ubi_eba_read_leb(ubi, vol, i, buf, 0, size, 1);
		if (err) {
			if (mtd_is_eccerr(err))
				err = 1;
			break;
		}
	}

	vfree(buf);
	return err;
}

 
void ubi_update_reserved(struct ubi_device *ubi)
{
	int need = ubi->beb_rsvd_level - ubi->beb_rsvd_pebs;

	if (need <= 0 || ubi->avail_pebs == 0)
		return;

	need = min_t(int, need, ubi->avail_pebs);
	ubi->avail_pebs -= need;
	ubi->rsvd_pebs += need;
	ubi->beb_rsvd_pebs += need;
	ubi_msg(ubi, "reserved more %d PEBs for bad PEB handling", need);
}

 
void ubi_calculate_reserved(struct ubi_device *ubi)
{
	 
	ubi->beb_rsvd_level = ubi->bad_peb_limit - ubi->bad_peb_count;
	if (ubi->beb_rsvd_level < 0) {
		ubi->beb_rsvd_level = 0;
		ubi_warn(ubi, "number of bad PEBs (%d) is above the expected limit (%d), not reserving any PEBs for bad PEB handling, will use available PEBs (if any)",
			 ubi->bad_peb_count, ubi->bad_peb_limit);
	}
}

 
int ubi_check_pattern(const void *buf, uint8_t patt, int size)
{
	int i;

	for (i = 0; i < size; i++)
		if (((const uint8_t *)buf)[i] != patt)
			return 0;
	return 1;
}

 
void ubi_msg(const struct ubi_device *ubi, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	pr_notice(UBI_NAME_STR "%d: %pV\n", ubi->ubi_num, &vaf);

	va_end(args);
}

 
void ubi_warn(const struct ubi_device *ubi, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	pr_warn(UBI_NAME_STR "%d warning: %ps: %pV\n",
		ubi->ubi_num, __builtin_return_address(0), &vaf);

	va_end(args);
}

 
void ubi_err(const struct ubi_device *ubi, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	pr_err(UBI_NAME_STR "%d error: %ps: %pV\n",
	       ubi->ubi_num, __builtin_return_address(0), &vaf);
	va_end(args);
}
