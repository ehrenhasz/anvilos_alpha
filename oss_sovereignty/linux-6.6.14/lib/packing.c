
 
#include <linux/packing.h>
#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/bitrev.h>

static int get_le_offset(int offset)
{
	int closest_multiple_of_4;

	closest_multiple_of_4 = (offset / 4) * 4;
	offset -= closest_multiple_of_4;
	return closest_multiple_of_4 + (3 - offset);
}

static int get_reverse_lsw32_offset(int offset, size_t len)
{
	int closest_multiple_of_4;
	int word_index;

	word_index = offset / 4;
	closest_multiple_of_4 = word_index * 4;
	offset -= closest_multiple_of_4;
	word_index = (len / 4) - word_index - 1;
	return word_index * 4 + offset;
}

static void adjust_for_msb_right_quirk(u64 *to_write, int *box_start_bit,
				       int *box_end_bit, u8 *box_mask)
{
	int box_bit_width = *box_start_bit - *box_end_bit + 1;
	int new_box_start_bit, new_box_end_bit;

	*to_write >>= *box_end_bit;
	*to_write = bitrev8(*to_write) >> (8 - box_bit_width);
	*to_write <<= *box_end_bit;

	new_box_end_bit   = box_bit_width - *box_start_bit - 1;
	new_box_start_bit = box_bit_width - *box_end_bit - 1;
	*box_mask = GENMASK_ULL(new_box_start_bit, new_box_end_bit);
	*box_start_bit = new_box_start_bit;
	*box_end_bit   = new_box_end_bit;
}

 
int packing(void *pbuf, u64 *uval, int startbit, int endbit, size_t pbuflen,
	    enum packing_op op, u8 quirks)
{
	 
	u64 value_width;
	 
	int plogical_first_u8, plogical_last_u8, box;

	 
	if (startbit < endbit)
		 
		return -EINVAL;

	value_width = startbit - endbit + 1;
	if (value_width > 64)
		return -ERANGE;

	 
	if (op == PACK && value_width < 64 && (*uval >= (1ull << value_width)))
		 
		return -ERANGE;

	 
	if (op == UNPACK)
		*uval = 0;

	 
	plogical_first_u8 = startbit / 8;
	plogical_last_u8  = endbit / 8;

	for (box = plogical_first_u8; box >= plogical_last_u8; box--) {
		 
		int box_start_bit, box_end_bit, box_addr;
		u8  box_mask;
		 
		int proj_start_bit, proj_end_bit;
		u64 proj_mask;

		 
		if (box == plogical_first_u8)
			box_start_bit = startbit % 8;
		else
			box_start_bit = 7;
		if (box == plogical_last_u8)
			box_end_bit = endbit % 8;
		else
			box_end_bit = 0;

		 
		proj_start_bit = ((box * 8) + box_start_bit) - endbit;
		proj_end_bit   = ((box * 8) + box_end_bit) - endbit;
		proj_mask = GENMASK_ULL(proj_start_bit, proj_end_bit);
		box_mask  = GENMASK_ULL(box_start_bit, box_end_bit);

		 
		box_addr = pbuflen - box - 1;
		if (quirks & QUIRK_LITTLE_ENDIAN)
			box_addr = get_le_offset(box_addr);
		if (quirks & QUIRK_LSW32_IS_FIRST)
			box_addr = get_reverse_lsw32_offset(box_addr,
							    pbuflen);

		if (op == UNPACK) {
			u64 pval;

			 
			pval = ((u8 *)pbuf)[box_addr] & box_mask;
			if (quirks & QUIRK_MSB_ON_THE_RIGHT)
				adjust_for_msb_right_quirk(&pval,
							   &box_start_bit,
							   &box_end_bit,
							   &box_mask);

			pval >>= box_end_bit;
			pval <<= proj_end_bit;
			*uval &= ~proj_mask;
			*uval |= pval;
		} else {
			u64 pval;

			 
			pval = (*uval) & proj_mask;
			pval >>= proj_end_bit;
			if (quirks & QUIRK_MSB_ON_THE_RIGHT)
				adjust_for_msb_right_quirk(&pval,
							   &box_start_bit,
							   &box_end_bit,
							   &box_mask);

			pval <<= box_end_bit;
			((u8 *)pbuf)[box_addr] &= ~box_mask;
			((u8 *)pbuf)[box_addr] |= pval;
		}
	}
	return 0;
}
EXPORT_SYMBOL(packing);

MODULE_DESCRIPTION("Generic bitfield packing and unpacking");
