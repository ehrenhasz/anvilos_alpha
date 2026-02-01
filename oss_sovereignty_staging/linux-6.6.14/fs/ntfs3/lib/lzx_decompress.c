
 

#include "decompress_common.h"
#include "lib.h"

 
#define LZX_NUM_CHARS			256

 
#define LZX_MIN_MATCH_LEN		2
#define LZX_MAX_MATCH_LEN		257

 
#define LZX_NUM_LENS			(LZX_MAX_MATCH_LEN - LZX_MIN_MATCH_LEN + 1)

 
#define LZX_NUM_PRIMARY_LENS		7
#define LZX_NUM_LEN_HEADERS		(LZX_NUM_PRIMARY_LENS + 1)

 
#define LZX_BLOCKTYPE_VERBATIM		1
#define LZX_BLOCKTYPE_ALIGNED		2
#define LZX_BLOCKTYPE_UNCOMPRESSED	3

 
#define LZX_NUM_OFFSET_SLOTS		30

 
#define LZX_MAINCODE_NUM_SYMBOLS	\
	(LZX_NUM_CHARS + (LZX_NUM_OFFSET_SLOTS * LZX_NUM_LEN_HEADERS))

 
#define LZX_LENCODE_NUM_SYMBOLS		(LZX_NUM_LENS - LZX_NUM_PRIMARY_LENS)

 
#define LZX_PRECODE_NUM_SYMBOLS		20

 
#define LZX_PRECODE_ELEMENT_SIZE	4

 
#define LZX_NUM_ALIGNED_OFFSET_BITS	3

 
#define LZX_ALIGNEDCODE_NUM_SYMBOLS	(1 << LZX_NUM_ALIGNED_OFFSET_BITS)

 
#define LZX_ALIGNED_OFFSET_BITMASK	((1 << LZX_NUM_ALIGNED_OFFSET_BITS) - 1)

 
#define LZX_ALIGNEDCODE_ELEMENT_SIZE	3

 
#define LZX_MAX_MAIN_CODEWORD_LEN	16
#define LZX_MAX_LEN_CODEWORD_LEN	16
#define LZX_MAX_PRE_CODEWORD_LEN	((1 << LZX_PRECODE_ELEMENT_SIZE) - 1)
#define LZX_MAX_ALIGNED_CODEWORD_LEN	((1 << LZX_ALIGNEDCODE_ELEMENT_SIZE) - 1)

 
#define LZX_DEFAULT_FILESIZE		12000000

 
#define LZX_DEFAULT_BLOCK_SIZE		32768

 
#define LZX_NUM_RECENT_OFFSETS		3

 
#define LZX_MAINCODE_TABLEBITS		11
#define LZX_LENCODE_TABLEBITS		10
#define LZX_PRECODE_TABLEBITS		6
#define LZX_ALIGNEDCODE_TABLEBITS	7

#define LZX_READ_LENS_MAX_OVERRUN	50

 
static const u32 lzx_offset_slot_base[LZX_NUM_OFFSET_SLOTS + 1] = {
	0,	1,	2,	3,	4,	 
	6,	8,	12,	16,	24,	 
	32,	48,	64,	96,	128,	 
	192,	256,	384,	512,	768,	 
	1024,	1536,	2048,	3072,	4096,    
	6144,	8192,	12288,	16384,	24576,	 
	32768,					 
};

 
static const u8 lzx_extra_offset_bits[LZX_NUM_OFFSET_SLOTS] = {
	0,	0,	0,	0,	1,
	1,	2,	2,	3,	3,
	4,	4,	5,	5,	6,
	6,	7,	7,	8,	8,
	9,	9,	10,	10,	11,
	11,	12,	12,	13,	13,
};

 
struct lzx_decompressor {

	 

	u16 maincode_decode_table[(1 << LZX_MAINCODE_TABLEBITS) +
					(LZX_MAINCODE_NUM_SYMBOLS * 2)];
	u8 maincode_lens[LZX_MAINCODE_NUM_SYMBOLS + LZX_READ_LENS_MAX_OVERRUN];


	u16 lencode_decode_table[(1 << LZX_LENCODE_TABLEBITS) +
					(LZX_LENCODE_NUM_SYMBOLS * 2)];
	u8 lencode_lens[LZX_LENCODE_NUM_SYMBOLS + LZX_READ_LENS_MAX_OVERRUN];


	u16 alignedcode_decode_table[(1 << LZX_ALIGNEDCODE_TABLEBITS) +
					(LZX_ALIGNEDCODE_NUM_SYMBOLS * 2)];
	u8 alignedcode_lens[LZX_ALIGNEDCODE_NUM_SYMBOLS];

	u16 precode_decode_table[(1 << LZX_PRECODE_TABLEBITS) +
				 (LZX_PRECODE_NUM_SYMBOLS * 2)];
	u8 precode_lens[LZX_PRECODE_NUM_SYMBOLS];

	 
	u16 working_space[2 * (1 + LZX_MAX_MAIN_CODEWORD_LEN) +
			  LZX_MAINCODE_NUM_SYMBOLS];
};

static void undo_e8_translation(void *target, s32 input_pos)
{
	s32 abs_offset, rel_offset;

	abs_offset = get_unaligned_le32(target);
	if (abs_offset >= 0) {
		if (abs_offset < LZX_DEFAULT_FILESIZE) {
			 
			rel_offset = abs_offset - input_pos;
			put_unaligned_le32(rel_offset, target);
		}
	} else {
		if (abs_offset >= -input_pos) {
			 
			rel_offset = abs_offset + LZX_DEFAULT_FILESIZE;
			put_unaligned_le32(rel_offset, target);
		}
	}
}

 
static void lzx_postprocess(u8 *data, u32 size)
{
	 
	u8 *tail;
	u8 saved_bytes[6];
	u8 *p;

	if (size <= 10)
		return;

	tail = &data[size - 6];
	memcpy(saved_bytes, tail, 6);
	memset(tail, 0xE8, 6);
	p = data;
	for (;;) {
		while (*p != 0xE8)
			p++;
		if (p >= tail)
			break;
		undo_e8_translation(p + 1, p - data);
		p += 5;
	}
	memcpy(tail, saved_bytes, 6);
}

 
static forceinline u32 read_presym(const struct lzx_decompressor *d,
					struct input_bitstream *is)
{
	return read_huffsym(is, d->precode_decode_table,
			    LZX_PRECODE_TABLEBITS, LZX_MAX_PRE_CODEWORD_LEN);
}

 
static forceinline u32 read_mainsym(const struct lzx_decompressor *d,
					 struct input_bitstream *is)
{
	return read_huffsym(is, d->maincode_decode_table,
			    LZX_MAINCODE_TABLEBITS, LZX_MAX_MAIN_CODEWORD_LEN);
}

 
static forceinline u32 read_lensym(const struct lzx_decompressor *d,
					struct input_bitstream *is)
{
	return read_huffsym(is, d->lencode_decode_table,
			    LZX_LENCODE_TABLEBITS, LZX_MAX_LEN_CODEWORD_LEN);
}

 
static forceinline u32 read_alignedsym(const struct lzx_decompressor *d,
					    struct input_bitstream *is)
{
	return read_huffsym(is, d->alignedcode_decode_table,
			    LZX_ALIGNEDCODE_TABLEBITS,
			    LZX_MAX_ALIGNED_CODEWORD_LEN);
}

 
static int lzx_read_codeword_lens(struct lzx_decompressor *d,
				  struct input_bitstream *is,
				  u8 *lens, u32 num_lens)
{
	u8 *len_ptr = lens;
	u8 *lens_end = lens + num_lens;
	int i;

	 
	for (i = 0; i < LZX_PRECODE_NUM_SYMBOLS; i++) {
		d->precode_lens[i] =
			bitstream_read_bits(is, LZX_PRECODE_ELEMENT_SIZE);
	}

	 
	if (make_huffman_decode_table(d->precode_decode_table,
				      LZX_PRECODE_NUM_SYMBOLS,
				      LZX_PRECODE_TABLEBITS,
				      d->precode_lens,
				      LZX_MAX_PRE_CODEWORD_LEN,
				      d->working_space))
		return -1;

	 
	do {
		u32 presym;
		u8 len;

		 
		presym = read_presym(d, is);
		if (presym < 17) {
			 
			len = *len_ptr - presym;
			if ((s8)len < 0)
				len += 17;
			*len_ptr++ = len;
		} else {
			 

			u32 run_len;

			if (presym == 17) {
				 
				run_len = 4 + bitstream_read_bits(is, 4);
				len = 0;
			} else if (presym == 18) {
				 
				run_len = 20 + bitstream_read_bits(is, 5);
				len = 0;
			} else {
				 
				run_len = 4 + bitstream_read_bits(is, 1);
				presym = read_presym(d, is);
				if (presym > 17)
					return -1;
				len = *len_ptr - presym;
				if ((s8)len < 0)
					len += 17;
			}

			do {
				*len_ptr++ = len;
			} while (--run_len);
			 
		}
	} while (len_ptr < lens_end);

	return 0;
}

 
static int lzx_read_block_header(struct lzx_decompressor *d,
				 struct input_bitstream *is,
				 int *block_type_ret,
				 u32 *block_size_ret,
				 u32 recent_offsets[])
{
	int block_type;
	u32 block_size;
	int i;

	bitstream_ensure_bits(is, 4);

	 
	block_type = bitstream_pop_bits(is, 3);

	 
	if (bitstream_pop_bits(is, 1)) {
		block_size = LZX_DEFAULT_BLOCK_SIZE;
	} else {
		block_size = 0;
		block_size |= bitstream_read_bits(is, 8);
		block_size <<= 8;
		block_size |= bitstream_read_bits(is, 8);
	}

	switch (block_type) {

	case LZX_BLOCKTYPE_ALIGNED:

		 

		for (i = 0; i < LZX_ALIGNEDCODE_NUM_SYMBOLS; i++) {
			d->alignedcode_lens[i] =
				bitstream_read_bits(is,
						    LZX_ALIGNEDCODE_ELEMENT_SIZE);
		}

		if (make_huffman_decode_table(d->alignedcode_decode_table,
					      LZX_ALIGNEDCODE_NUM_SYMBOLS,
					      LZX_ALIGNEDCODE_TABLEBITS,
					      d->alignedcode_lens,
					      LZX_MAX_ALIGNED_CODEWORD_LEN,
					      d->working_space))
			return -1;

		 
		fallthrough;

	case LZX_BLOCKTYPE_VERBATIM:

		 

		if (lzx_read_codeword_lens(d, is, d->maincode_lens,
					   LZX_NUM_CHARS))
			return -1;

		if (lzx_read_codeword_lens(d, is,
					   d->maincode_lens + LZX_NUM_CHARS,
					   LZX_MAINCODE_NUM_SYMBOLS - LZX_NUM_CHARS))
			return -1;

		if (make_huffman_decode_table(d->maincode_decode_table,
					      LZX_MAINCODE_NUM_SYMBOLS,
					      LZX_MAINCODE_TABLEBITS,
					      d->maincode_lens,
					      LZX_MAX_MAIN_CODEWORD_LEN,
					      d->working_space))
			return -1;

		 

		if (lzx_read_codeword_lens(d, is, d->lencode_lens,
					   LZX_LENCODE_NUM_SYMBOLS))
			return -1;

		if (make_huffman_decode_table(d->lencode_decode_table,
					      LZX_LENCODE_NUM_SYMBOLS,
					      LZX_LENCODE_TABLEBITS,
					      d->lencode_lens,
					      LZX_MAX_LEN_CODEWORD_LEN,
					      d->working_space))
			return -1;

		break;

	case LZX_BLOCKTYPE_UNCOMPRESSED:

		 
		bitstream_ensure_bits(is, 1);
		bitstream_align(is);

		recent_offsets[0] = bitstream_read_u32(is);
		recent_offsets[1] = bitstream_read_u32(is);
		recent_offsets[2] = bitstream_read_u32(is);

		 
		if (recent_offsets[0] == 0 || recent_offsets[1] == 0 ||
		    recent_offsets[2] == 0)
			return -1;
		break;

	default:
		 
		return -1;
	}

	*block_type_ret = block_type;
	*block_size_ret = block_size;
	return 0;
}

 
static int lzx_decompress_block(const struct lzx_decompressor *d,
				struct input_bitstream *is,
				int block_type, u32 block_size,
				u8 * const out_begin, u8 *out_next,
				u32 recent_offsets[])
{
	u8 * const block_end = out_next + block_size;
	u32 ones_if_aligned = 0U - (block_type == LZX_BLOCKTYPE_ALIGNED);

	do {
		u32 mainsym;
		u32 match_len;
		u32 match_offset;
		u32 offset_slot;
		u32 num_extra_bits;

		mainsym = read_mainsym(d, is);
		if (mainsym < LZX_NUM_CHARS) {
			 
			*out_next++ = mainsym;
			continue;
		}

		 

		 
		mainsym -= LZX_NUM_CHARS;
		match_len = mainsym % LZX_NUM_LEN_HEADERS;
		offset_slot = mainsym / LZX_NUM_LEN_HEADERS;

		 
		if (match_len == LZX_NUM_PRIMARY_LENS)
			match_len += read_lensym(d, is);
		match_len += LZX_MIN_MATCH_LEN;

		if (offset_slot < LZX_NUM_RECENT_OFFSETS) {
			 

			 
			match_offset = recent_offsets[offset_slot];
			recent_offsets[offset_slot] = recent_offsets[0];
			recent_offsets[0] = match_offset;
		} else {
			 

			 
			num_extra_bits = lzx_extra_offset_bits[offset_slot];

			 
			match_offset = lzx_offset_slot_base[offset_slot];

			 

			if ((num_extra_bits & ones_if_aligned) >= LZX_NUM_ALIGNED_OFFSET_BITS) {
				match_offset +=
					bitstream_read_bits(is, num_extra_bits -
								LZX_NUM_ALIGNED_OFFSET_BITS)
							<< LZX_NUM_ALIGNED_OFFSET_BITS;
				match_offset += read_alignedsym(d, is);
			} else {
				match_offset += bitstream_read_bits(is, num_extra_bits);
			}

			 
			match_offset -= (LZX_NUM_RECENT_OFFSETS - 1);

			 
			recent_offsets[2] = recent_offsets[1];
			recent_offsets[1] = recent_offsets[0];
			recent_offsets[0] = match_offset;
		}

		 

		if (match_len > (size_t)(block_end - out_next))
			return -1;

		if (match_offset > (size_t)(out_next - out_begin))
			return -1;

		out_next = lz_copy(out_next, match_len, match_offset,
				   block_end, LZX_MIN_MATCH_LEN);

	} while (out_next != block_end);

	return 0;
}

 
struct lzx_decompressor *lzx_allocate_decompressor(void)
{
	return kmalloc(sizeof(struct lzx_decompressor), GFP_NOFS);
}

 
int lzx_decompress(struct lzx_decompressor *decompressor,
		   const void *compressed_data, size_t compressed_size,
		   void *uncompressed_data, size_t uncompressed_size)
{
	struct lzx_decompressor *d = decompressor;
	u8 * const out_begin = uncompressed_data;
	u8 *out_next = out_begin;
	u8 * const out_end = out_begin + uncompressed_size;
	struct input_bitstream is;
	u32 recent_offsets[LZX_NUM_RECENT_OFFSETS] = {1, 1, 1};
	int e8_status = 0;

	init_input_bitstream(&is, compressed_data, compressed_size);

	 
	memset(d->maincode_lens, 0, LZX_MAINCODE_NUM_SYMBOLS);
	memset(d->lencode_lens, 0, LZX_LENCODE_NUM_SYMBOLS);

	 

	while (out_next != out_end) {
		int block_type;
		u32 block_size;

		if (lzx_read_block_header(d, &is, &block_type, &block_size,
					  recent_offsets))
			goto invalid;

		if (block_size < 1 || block_size > (size_t)(out_end - out_next))
			goto invalid;

		if (block_type != LZX_BLOCKTYPE_UNCOMPRESSED) {

			 

			if (lzx_decompress_block(d,
						 &is,
						 block_type,
						 block_size,
						 out_begin,
						 out_next,
						 recent_offsets))
				goto invalid;

			e8_status |= d->maincode_lens[0xe8];
			out_next += block_size;
		} else {
			 

			out_next = bitstream_read_bytes(&is, out_next,
							block_size);
			if (!out_next)
				goto invalid;

			if (block_size & 1)
				bitstream_read_byte(&is);

			e8_status = 1;
		}
	}

	 
	if (e8_status)
		lzx_postprocess(uncompressed_data, uncompressed_size);

	return 0;

invalid:
	return -1;
}

 
void lzx_free_decompressor(struct lzx_decompressor *decompressor)
{
	kfree(decompressor);
}
