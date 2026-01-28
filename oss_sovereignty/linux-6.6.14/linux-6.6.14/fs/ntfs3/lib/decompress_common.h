#ifndef _LINUX_NTFS3_LIB_DECOMPRESS_COMMON_H
#define _LINUX_NTFS3_LIB_DECOMPRESS_COMMON_H
#include <linux/string.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#define forceinline __always_inline
#if defined(__i386__) || defined(__x86_64__) || defined(__ARM_FEATURE_UNALIGNED)
#  define FAST_UNALIGNED_ACCESS
#endif
#define WORDBYTES (sizeof(size_t))
static forceinline void
copy_unaligned_word(const void *src, void *dst)
{
	put_unaligned(get_unaligned((const size_t *)src), (size_t *)dst);
}
static forceinline size_t repeat_byte(u8 b)
{
	size_t v;
	v = b;
	v |= v << 8;
	v |= v << 16;
	v |= v << ((WORDBYTES == 8) ? 32 : 0);
	return v;
}
struct input_bitstream {
	u32 bitbuf;
	u32 bitsleft;
	const u8 *next;
	const u8 *end;
};
static forceinline void init_input_bitstream(struct input_bitstream *is,
					     const void *buffer, u32 size)
{
	is->bitbuf = 0;
	is->bitsleft = 0;
	is->next = buffer;
	is->end = is->next + size;
}
static forceinline void bitstream_ensure_bits(struct input_bitstream *is,
					      u32 num_bits)
{
	if (is->bitsleft < num_bits) {
		if (is->end - is->next >= 2) {
			is->bitbuf |= (u32)get_unaligned_le16(is->next)
					<< (16 - is->bitsleft);
			is->next += 2;
		}
		is->bitsleft += 16;
	}
}
static forceinline u32
bitstream_peek_bits(const struct input_bitstream *is, const u32 num_bits)
{
	return (is->bitbuf >> 1) >> (sizeof(is->bitbuf) * 8 - num_bits - 1);
}
static forceinline void
bitstream_remove_bits(struct input_bitstream *is, u32 num_bits)
{
	is->bitbuf <<= num_bits;
	is->bitsleft -= num_bits;
}
static forceinline u32
bitstream_pop_bits(struct input_bitstream *is, u32 num_bits)
{
	u32 bits = bitstream_peek_bits(is, num_bits);
	bitstream_remove_bits(is, num_bits);
	return bits;
}
static forceinline u32
bitstream_read_bits(struct input_bitstream *is, u32 num_bits)
{
	bitstream_ensure_bits(is, num_bits);
	return bitstream_pop_bits(is, num_bits);
}
static forceinline u8
bitstream_read_byte(struct input_bitstream *is)
{
	if (unlikely(is->end == is->next))
		return 0;
	return *is->next++;
}
static forceinline u16
bitstream_read_u16(struct input_bitstream *is)
{
	u16 v;
	if (unlikely(is->end - is->next < 2))
		return 0;
	v = get_unaligned_le16(is->next);
	is->next += 2;
	return v;
}
static forceinline u32
bitstream_read_u32(struct input_bitstream *is)
{
	u32 v;
	if (unlikely(is->end - is->next < 4))
		return 0;
	v = get_unaligned_le32(is->next);
	is->next += 4;
	return v;
}
static forceinline void *bitstream_read_bytes(struct input_bitstream *is,
					      void *dst_buffer, size_t count)
{
	if ((size_t)(is->end - is->next) < count)
		return NULL;
	memcpy(dst_buffer, is->next, count);
	is->next += count;
	return (u8 *)dst_buffer + count;
}
static forceinline void bitstream_align(struct input_bitstream *is)
{
	is->bitsleft = 0;
	is->bitbuf = 0;
}
extern int make_huffman_decode_table(u16 decode_table[], const u32 num_syms,
				     const u32 num_bits, const u8 lens[],
				     const u32 max_codeword_len,
				     u16 working_space[]);
static forceinline u32 read_huffsym(struct input_bitstream *istream,
					 const u16 decode_table[],
					 u32 table_bits,
					 u32 max_codeword_len)
{
	u32 entry;
	u32 key_bits;
	bitstream_ensure_bits(istream, max_codeword_len);
	key_bits = bitstream_peek_bits(istream, table_bits);
	entry = decode_table[key_bits];
	if (entry < 0xC000) {
		bitstream_remove_bits(istream, entry >> 11);
		return entry & 0x7FF;
	}
	bitstream_remove_bits(istream, table_bits);
	do {
		key_bits = (entry & 0x3FFF) + bitstream_pop_bits(istream, 1);
	} while ((entry = decode_table[key_bits]) >= 0xC000);
	return entry;
}
static forceinline u8 *lz_copy(u8 *dst, u32 length, u32 offset, const u8 *bufend,
			       u32 min_length)
{
	const u8 *src = dst - offset;
#ifdef FAST_UNALIGNED_ACCESS
	u8 * const end = dst + length;
	if (bufend - end >= (ptrdiff_t)(WORDBYTES - 1)) {
		if (offset >= WORDBYTES) {
			copy_unaligned_word(src, dst);
			src += WORDBYTES;
			dst += WORDBYTES;
			if (dst < end) {
				do {
					copy_unaligned_word(src, dst);
					src += WORDBYTES;
					dst += WORDBYTES;
				} while (dst < end);
			}
			return end;
		} else if (offset == 1) {
			size_t v = repeat_byte(*(dst - 1));
			do {
				put_unaligned(v, (size_t *)dst);
				src += WORDBYTES;
				dst += WORDBYTES;
			} while (dst < end);
			return end;
		}
	}
#endif  
	if (min_length >= 2) {
		*dst++ = *src++;
		length--;
	}
	if (min_length >= 3) {
		*dst++ = *src++;
		length--;
	}
	do {
		*dst++ = *src++;
	} while (--length);
	return dst;
}
#endif  
