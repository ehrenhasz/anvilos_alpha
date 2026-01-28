#ifndef CODEC_FWHT_H
#define CODEC_FWHT_H
#include <linux/types.h>
#include <linux/bitops.h>
#include <asm/byteorder.h>
#define FWHT_MAGIC1 0x4f4f4f4f
#define FWHT_MAGIC2 0xffffffff
#define vic_round_dim(dim, div) (round_up((dim) / (div), 8) * (div))
struct fwht_cframe_hdr {
	u32 magic1;
	u32 magic2;
	__be32 version;
	__be32 width, height;
	__be32 flags;
	__be32 colorspace;
	__be32 xfer_func;
	__be32 ycbcr_enc;
	__be32 quantization;
	__be32 size;
};
struct fwht_cframe {
	u16 i_frame_qp;
	u16 p_frame_qp;
	__be16 *rlc_data;
	s16 coeffs[8 * 8];
	s16 de_coeffs[8 * 8];
	s16 de_fwht[8 * 8];
	u32 size;
};
struct fwht_raw_frame {
	unsigned int width_div;
	unsigned int height_div;
	unsigned int luma_alpha_step;
	unsigned int chroma_step;
	unsigned int components_num;
	u8 *buf;
	u8 *luma, *cb, *cr, *alpha;
};
#define FWHT_FRAME_PCODED	BIT(0)
#define FWHT_FRAME_UNENCODED	BIT(1)
#define FWHT_LUMA_UNENCODED	BIT(2)
#define FWHT_CB_UNENCODED	BIT(3)
#define FWHT_CR_UNENCODED	BIT(4)
#define FWHT_ALPHA_UNENCODED	BIT(5)
u32 fwht_encode_frame(struct fwht_raw_frame *frm,
		      struct fwht_raw_frame *ref_frm,
		      struct fwht_cframe *cf,
		      bool is_intra, bool next_is_intra,
		      unsigned int width, unsigned int height,
		      unsigned int stride, unsigned int chroma_stride);
bool fwht_decode_frame(struct fwht_cframe *cf, u32 hdr_flags,
		unsigned int components_num, unsigned int width,
		unsigned int height, const struct fwht_raw_frame *ref,
		unsigned int ref_stride, unsigned int ref_chroma_stride,
		struct fwht_raw_frame *dst, unsigned int dst_stride,
		unsigned int dst_chroma_stride);
#endif
