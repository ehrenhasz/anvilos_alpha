
 

#include "decompress_common.h"
#include "lib.h"

#define XPRESS_NUM_SYMBOLS	512
#define XPRESS_MAX_CODEWORD_LEN	15
#define XPRESS_MIN_MATCH_LEN	3

 
#define XPRESS_TABLEBITS 12

 
struct xpress_decompressor {

	 
	u16 decode_table[(1 << XPRESS_TABLEBITS) + 2 * XPRESS_NUM_SYMBOLS];

	 
	u8 lens[XPRESS_NUM_SYMBOLS];

	 
	u16 working_space[2 * (1 + XPRESS_MAX_CODEWORD_LEN) +
			  XPRESS_NUM_SYMBOLS];
};

 
struct xpress_decompressor *xpress_allocate_decompressor(void)
{
	return kmalloc(sizeof(struct xpress_decompressor), GFP_NOFS);
}

 
int xpress_decompress(struct xpress_decompressor *decompressor,
		      const void *compressed_data, size_t compressed_size,
		      void *uncompressed_data, size_t uncompressed_size)
{
	struct xpress_decompressor *d = decompressor;
	const u8 * const in_begin = compressed_data;
	u8 * const out_begin = uncompressed_data;
	u8 *out_next = out_begin;
	u8 * const out_end = out_begin + uncompressed_size;
	struct input_bitstream is;
	u32 i;

	 
	if (compressed_size < XPRESS_NUM_SYMBOLS / 2)
		goto invalid;
	for (i = 0; i < XPRESS_NUM_SYMBOLS / 2; i++) {
		d->lens[i*2 + 0] = in_begin[i] & 0xF;
		d->lens[i*2 + 1] = in_begin[i] >> 4;
	}

	 
	if (make_huffman_decode_table(d->decode_table, XPRESS_NUM_SYMBOLS,
				      XPRESS_TABLEBITS, d->lens,
				      XPRESS_MAX_CODEWORD_LEN,
				      d->working_space))
		goto invalid;

	 

	init_input_bitstream(&is, in_begin + XPRESS_NUM_SYMBOLS / 2,
			     compressed_size - XPRESS_NUM_SYMBOLS / 2);

	while (out_next != out_end) {
		u32 sym;
		u32 log2_offset;
		u32 length;
		u32 offset;

		sym = read_huffsym(&is, d->decode_table,
				   XPRESS_TABLEBITS, XPRESS_MAX_CODEWORD_LEN);
		if (sym < 256) {
			 
			*out_next++ = sym;
		} else {
			 
			length = sym & 0xf;
			log2_offset = (sym >> 4) & 0xf;

			bitstream_ensure_bits(&is, 16);

			offset = ((u32)1 << log2_offset) |
				 bitstream_pop_bits(&is, log2_offset);

			if (length == 0xf) {
				length += bitstream_read_byte(&is);
				if (length == 0xf + 0xff)
					length = bitstream_read_u16(&is);
			}
			length += XPRESS_MIN_MATCH_LEN;

			if (offset > (size_t)(out_next - out_begin))
				goto invalid;

			if (length > (size_t)(out_end - out_next))
				goto invalid;

			out_next = lz_copy(out_next, length, offset, out_end,
					   XPRESS_MIN_MATCH_LEN);
		}
	}
	return 0;

invalid:
	return -1;
}

 
void xpress_free_decompressor(struct xpress_decompressor *decompressor)
{
	kfree(decompressor);
}
