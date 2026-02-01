
 

#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/swab.h>

#include <asm/byteorder.h>
#include <asm/errno.h>

#include <crypto/aes.h>
#include <crypto/gcm.h>

#include "ocs-aes.h"

#define AES_COMMAND_OFFSET			0x0000
#define AES_KEY_0_OFFSET			0x0004
#define AES_KEY_1_OFFSET			0x0008
#define AES_KEY_2_OFFSET			0x000C
#define AES_KEY_3_OFFSET			0x0010
#define AES_KEY_4_OFFSET			0x0014
#define AES_KEY_5_OFFSET			0x0018
#define AES_KEY_6_OFFSET			0x001C
#define AES_KEY_7_OFFSET			0x0020
#define AES_IV_0_OFFSET				0x0024
#define AES_IV_1_OFFSET				0x0028
#define AES_IV_2_OFFSET				0x002C
#define AES_IV_3_OFFSET				0x0030
#define AES_ACTIVE_OFFSET			0x0034
#define AES_STATUS_OFFSET			0x0038
#define AES_KEY_SIZE_OFFSET			0x0044
#define AES_IER_OFFSET				0x0048
#define AES_ISR_OFFSET				0x005C
#define AES_MULTIPURPOSE1_0_OFFSET		0x0200
#define AES_MULTIPURPOSE1_1_OFFSET		0x0204
#define AES_MULTIPURPOSE1_2_OFFSET		0x0208
#define AES_MULTIPURPOSE1_3_OFFSET		0x020C
#define AES_MULTIPURPOSE2_0_OFFSET		0x0220
#define AES_MULTIPURPOSE2_1_OFFSET		0x0224
#define AES_MULTIPURPOSE2_2_OFFSET		0x0228
#define AES_MULTIPURPOSE2_3_OFFSET		0x022C
#define AES_BYTE_ORDER_CFG_OFFSET		0x02C0
#define AES_TLEN_OFFSET				0x0300
#define AES_T_MAC_0_OFFSET			0x0304
#define AES_T_MAC_1_OFFSET			0x0308
#define AES_T_MAC_2_OFFSET			0x030C
#define AES_T_MAC_3_OFFSET			0x0310
#define AES_PLEN_OFFSET				0x0314
#define AES_A_DMA_SRC_ADDR_OFFSET		0x0400
#define AES_A_DMA_DST_ADDR_OFFSET		0x0404
#define AES_A_DMA_SRC_SIZE_OFFSET		0x0408
#define AES_A_DMA_DST_SIZE_OFFSET		0x040C
#define AES_A_DMA_DMA_MODE_OFFSET		0x0410
#define AES_A_DMA_NEXT_SRC_DESCR_OFFSET		0x0418
#define AES_A_DMA_NEXT_DST_DESCR_OFFSET		0x041C
#define AES_A_DMA_WHILE_ACTIVE_MODE_OFFSET	0x0420
#define AES_A_DMA_LOG_OFFSET			0x0424
#define AES_A_DMA_STATUS_OFFSET			0x0428
#define AES_A_DMA_PERF_CNTR_OFFSET		0x042C
#define AES_A_DMA_MSI_ISR_OFFSET		0x0480
#define AES_A_DMA_MSI_IER_OFFSET		0x0484
#define AES_A_DMA_MSI_MASK_OFFSET		0x0488
#define AES_A_DMA_INBUFFER_WRITE_FIFO_OFFSET	0x0600
#define AES_A_DMA_OUTBUFFER_READ_FIFO_OFFSET	0x0700

 
#define AES_A_DMA_DMA_MODE_ACTIVE		BIT(31)
#define AES_A_DMA_DMA_MODE_SRC_LINK_LIST_EN	BIT(25)
#define AES_A_DMA_DMA_MODE_DST_LINK_LIST_EN	BIT(24)

 
#define AES_ACTIVE_LAST_ADATA			BIT(9)
#define AES_ACTIVE_LAST_CCM_GCM			BIT(8)
#define AES_ACTIVE_TERMINATION			BIT(1)
#define AES_ACTIVE_TRIGGER			BIT(0)

#define AES_DISABLE_INT				0x00000000
#define AES_DMA_CPD_ERR_INT			BIT(8)
#define AES_DMA_OUTBUF_RD_ERR_INT		BIT(7)
#define AES_DMA_OUTBUF_WR_ERR_INT		BIT(6)
#define AES_DMA_INBUF_RD_ERR_INT		BIT(5)
#define AES_DMA_INBUF_WR_ERR_INT		BIT(4)
#define AES_DMA_BAD_COMP_INT			BIT(3)
#define AES_DMA_SAI_INT				BIT(2)
#define AES_DMA_SRC_DONE_INT			BIT(0)
#define AES_COMPLETE_INT			BIT(1)

#define AES_DMA_MSI_MASK_CLEAR			BIT(0)

#define AES_128_BIT_KEY				0x00000000
#define AES_256_BIT_KEY				BIT(0)

#define AES_DEACTIVATE_PERF_CNTR		0x00000000
#define AES_ACTIVATE_PERF_CNTR			BIT(0)

#define AES_MAX_TAG_SIZE_U32			4

#define OCS_LL_DMA_FLAG_TERMINATE		BIT(31)

 
#define AES_DMA_STATUS_INPUT_BUFFER_OCCUPANCY_MASK	0x3FF

 
#define CCM_DECRYPT_DELAY_TAG_CLK_COUNT		36UL

 
#define CCM_DECRYPT_DELAY_LAST_GCX_CLK_COUNT	42UL

 
#define L_PRIME_MIN (1)
#define L_PRIME_MAX (7)
 
#define L_PRIME_IDX		0
#define COUNTER_START(lprime)	(16 - ((lprime) + 1))
#define COUNTER_LEN(lprime)	((lprime) + 1)

enum aes_counter_mode {
	AES_CTR_M_NO_INC = 0,
	AES_CTR_M_32_INC = 1,
	AES_CTR_M_64_INC = 2,
	AES_CTR_M_128_INC = 3,
};

 
struct ocs_dma_linked_list {
	u32 src_addr;
	u32 src_len;
	u32 next;
	u32 ll_flags;
} __packed;

 
static inline void aes_a_set_endianness(const struct ocs_aes_dev *aes_dev)
{
	iowrite32(0x7FF, aes_dev->base_reg + AES_BYTE_ORDER_CFG_OFFSET);
}

 
static inline void aes_a_op_trigger(const struct ocs_aes_dev *aes_dev)
{
	iowrite32(AES_ACTIVE_TRIGGER, aes_dev->base_reg + AES_ACTIVE_OFFSET);
}

 
static inline void aes_a_op_termination(const struct ocs_aes_dev *aes_dev)
{
	iowrite32(AES_ACTIVE_TERMINATION,
		  aes_dev->base_reg + AES_ACTIVE_OFFSET);
}

 
static inline void aes_a_set_last_gcx(const struct ocs_aes_dev *aes_dev)
{
	iowrite32(AES_ACTIVE_LAST_CCM_GCM,
		  aes_dev->base_reg + AES_ACTIVE_OFFSET);
}

 
static inline void aes_a_wait_last_gcx(const struct ocs_aes_dev *aes_dev)
{
	u32 aes_active_reg;

	do {
		aes_active_reg = ioread32(aes_dev->base_reg +
					  AES_ACTIVE_OFFSET);
	} while (aes_active_reg & AES_ACTIVE_LAST_CCM_GCM);
}

 
static void aes_a_dma_wait_input_buffer_occupancy(const struct ocs_aes_dev *aes_dev)
{
	u32 reg;

	do {
		reg = ioread32(aes_dev->base_reg + AES_A_DMA_STATUS_OFFSET);
	} while (reg & AES_DMA_STATUS_INPUT_BUFFER_OCCUPANCY_MASK);
}

  
static inline void aes_a_set_last_gcx_and_adata(const struct ocs_aes_dev *aes_dev)
{
	iowrite32(AES_ACTIVE_LAST_ADATA | AES_ACTIVE_LAST_CCM_GCM,
		  aes_dev->base_reg + AES_ACTIVE_OFFSET);
}

 
static inline void aes_a_dma_set_xfer_size_zero(const struct ocs_aes_dev *aes_dev)
{
	iowrite32(0, aes_dev->base_reg + AES_A_DMA_SRC_SIZE_OFFSET);
	iowrite32(0, aes_dev->base_reg + AES_A_DMA_DST_SIZE_OFFSET);
}

 
static inline void aes_a_dma_active(const struct ocs_aes_dev *aes_dev)
{
	iowrite32(AES_A_DMA_DMA_MODE_ACTIVE,
		  aes_dev->base_reg + AES_A_DMA_DMA_MODE_OFFSET);
}

 
static inline void aes_a_dma_active_src_ll_en(const struct ocs_aes_dev *aes_dev)
{
	iowrite32(AES_A_DMA_DMA_MODE_ACTIVE |
		  AES_A_DMA_DMA_MODE_SRC_LINK_LIST_EN,
		  aes_dev->base_reg + AES_A_DMA_DMA_MODE_OFFSET);
}

 
static inline void aes_a_dma_active_dst_ll_en(const struct ocs_aes_dev *aes_dev)
{
	iowrite32(AES_A_DMA_DMA_MODE_ACTIVE |
		  AES_A_DMA_DMA_MODE_DST_LINK_LIST_EN,
		  aes_dev->base_reg + AES_A_DMA_DMA_MODE_OFFSET);
}

 
static inline void aes_a_dma_active_src_dst_ll_en(const struct ocs_aes_dev *aes_dev)
{
	iowrite32(AES_A_DMA_DMA_MODE_ACTIVE |
		  AES_A_DMA_DMA_MODE_SRC_LINK_LIST_EN |
		  AES_A_DMA_DMA_MODE_DST_LINK_LIST_EN,
		  aes_dev->base_reg + AES_A_DMA_DMA_MODE_OFFSET);
}

 
static inline void aes_a_dma_reset_and_activate_perf_cntr(const struct ocs_aes_dev *aes_dev)
{
	iowrite32(0x00000000, aes_dev->base_reg + AES_A_DMA_PERF_CNTR_OFFSET);
	iowrite32(AES_ACTIVATE_PERF_CNTR,
		  aes_dev->base_reg + AES_A_DMA_WHILE_ACTIVE_MODE_OFFSET);
}

 
static inline void aes_a_dma_wait_and_deactivate_perf_cntr(const struct ocs_aes_dev *aes_dev,
							   int delay)
{
	while (ioread32(aes_dev->base_reg + AES_A_DMA_PERF_CNTR_OFFSET) < delay)
		;
	iowrite32(AES_DEACTIVATE_PERF_CNTR,
		  aes_dev->base_reg + AES_A_DMA_WHILE_ACTIVE_MODE_OFFSET);
}

 
static void aes_irq_disable(struct ocs_aes_dev *aes_dev)
{
	u32 isr_val = 0;

	 
	iowrite32(AES_DISABLE_INT,
		  aes_dev->base_reg + AES_A_DMA_MSI_IER_OFFSET);
	iowrite32(AES_DISABLE_INT, aes_dev->base_reg + AES_IER_OFFSET);

	 
	isr_val = ioread32(aes_dev->base_reg + AES_A_DMA_MSI_ISR_OFFSET);
	if (isr_val)
		iowrite32(isr_val,
			  aes_dev->base_reg + AES_A_DMA_MSI_ISR_OFFSET);

	isr_val = ioread32(aes_dev->base_reg + AES_A_DMA_MSI_MASK_OFFSET);
	if (isr_val)
		iowrite32(isr_val,
			  aes_dev->base_reg + AES_A_DMA_MSI_MASK_OFFSET);

	isr_val = ioread32(aes_dev->base_reg + AES_ISR_OFFSET);
	if (isr_val)
		iowrite32(isr_val, aes_dev->base_reg + AES_ISR_OFFSET);
}

 
static void aes_irq_enable(struct ocs_aes_dev *aes_dev, u8 irq)
{
	if (irq == AES_COMPLETE_INT) {
		 
		iowrite32(AES_DMA_CPD_ERR_INT |
			  AES_DMA_OUTBUF_RD_ERR_INT |
			  AES_DMA_OUTBUF_WR_ERR_INT |
			  AES_DMA_INBUF_RD_ERR_INT |
			  AES_DMA_INBUF_WR_ERR_INT |
			  AES_DMA_BAD_COMP_INT |
			  AES_DMA_SAI_INT,
			  aes_dev->base_reg + AES_A_DMA_MSI_IER_OFFSET);
		 
		iowrite32(AES_COMPLETE_INT, aes_dev->base_reg + AES_IER_OFFSET);
		return;
	}
	if (irq == AES_DMA_SRC_DONE_INT) {
		 
		iowrite32(AES_DISABLE_INT, aes_dev->base_reg + AES_IER_OFFSET);
		 
		iowrite32(AES_DMA_CPD_ERR_INT |
			  AES_DMA_OUTBUF_RD_ERR_INT |
			  AES_DMA_OUTBUF_WR_ERR_INT |
			  AES_DMA_INBUF_RD_ERR_INT |
			  AES_DMA_INBUF_WR_ERR_INT |
			  AES_DMA_BAD_COMP_INT |
			  AES_DMA_SAI_INT |
			  AES_DMA_SRC_DONE_INT,
			  aes_dev->base_reg + AES_A_DMA_MSI_IER_OFFSET);
	}
}

 
static int ocs_aes_irq_enable_and_wait(struct ocs_aes_dev *aes_dev, u8 irq)
{
	int rc;

	reinit_completion(&aes_dev->irq_completion);
	aes_irq_enable(aes_dev, irq);
	rc = wait_for_completion_interruptible(&aes_dev->irq_completion);
	if (rc)
		return rc;

	return aes_dev->dma_err_mask ? -EIO : 0;
}

 
static inline void dma_to_ocs_aes_ll(struct ocs_aes_dev *aes_dev,
				     dma_addr_t dma_list)
{
	iowrite32(0, aes_dev->base_reg + AES_A_DMA_SRC_SIZE_OFFSET);
	iowrite32(dma_list,
		  aes_dev->base_reg + AES_A_DMA_NEXT_SRC_DESCR_OFFSET);
}

 
static inline void dma_from_ocs_aes_ll(struct ocs_aes_dev *aes_dev,
				       dma_addr_t dma_list)
{
	iowrite32(0, aes_dev->base_reg + AES_A_DMA_DST_SIZE_OFFSET);
	iowrite32(dma_list,
		  aes_dev->base_reg + AES_A_DMA_NEXT_DST_DESCR_OFFSET);
}

irqreturn_t ocs_aes_irq_handler(int irq, void *dev_id)
{
	struct ocs_aes_dev *aes_dev = dev_id;
	u32 aes_dma_isr;

	 
	aes_dma_isr = ioread32(aes_dev->base_reg + AES_A_DMA_MSI_ISR_OFFSET);

	 
	aes_irq_disable(aes_dev);

	 
	aes_dev->dma_err_mask = aes_dma_isr &
				(AES_DMA_CPD_ERR_INT |
				 AES_DMA_OUTBUF_RD_ERR_INT |
				 AES_DMA_OUTBUF_WR_ERR_INT |
				 AES_DMA_INBUF_RD_ERR_INT |
				 AES_DMA_INBUF_WR_ERR_INT |
				 AES_DMA_BAD_COMP_INT |
				 AES_DMA_SAI_INT);

	 
	complete(&aes_dev->irq_completion);

	return IRQ_HANDLED;
}

 
int ocs_aes_set_key(struct ocs_aes_dev *aes_dev, u32 key_size, const u8 *key,
		    enum ocs_cipher cipher)
{
	const u32 *key_u32;
	u32 val;
	int i;

	 
	if (cipher == OCS_AES && !(key_size == 32 || key_size == 16)) {
		dev_err(aes_dev->dev,
			"%d-bit keys not supported by AES cipher\n",
			key_size * 8);
		return -EINVAL;
	}
	 
	if (cipher == OCS_SM4 && key_size != 16) {
		dev_err(aes_dev->dev,
			"%d-bit keys not supported for SM4 cipher\n",
			key_size * 8);
		return -EINVAL;
	}

	if (!key)
		return -EINVAL;

	key_u32 = (const u32 *)key;

	 
	for (i = 0; i < (key_size / sizeof(u32)); i++) {
		iowrite32(key_u32[i],
			  aes_dev->base_reg + AES_KEY_0_OFFSET +
			  (i * sizeof(u32)));
	}
	 
	val = (key_size == 16) ? AES_128_BIT_KEY : AES_256_BIT_KEY;
	iowrite32(val, aes_dev->base_reg + AES_KEY_SIZE_OFFSET);

	return 0;
}

 
static inline void set_ocs_aes_command(struct ocs_aes_dev *aes_dev,
				       enum ocs_cipher cipher,
				       enum ocs_mode mode,
				       enum ocs_instruction instruction)
{
	u32 val;

	 
	val = (cipher << 14) | (mode << 8) | (instruction << 6) |
	      (AES_CTR_M_128_INC << 2);
	iowrite32(val, aes_dev->base_reg + AES_COMMAND_OFFSET);
}

static void ocs_aes_init(struct ocs_aes_dev *aes_dev,
			 enum ocs_mode mode,
			 enum ocs_cipher cipher,
			 enum ocs_instruction instruction)
{
	 
	aes_irq_disable(aes_dev);

	 
	aes_a_set_endianness(aes_dev);

	 
	set_ocs_aes_command(aes_dev, cipher, mode, instruction);
}

 
static inline void ocs_aes_write_last_data_blk_len(struct ocs_aes_dev *aes_dev,
						   u32 size)
{
	u32 val;

	if (size == 0) {
		val = 0;
		goto exit;
	}

	val = size % AES_BLOCK_SIZE;
	if (val == 0)
		val = AES_BLOCK_SIZE;

exit:
	iowrite32(val, aes_dev->base_reg + AES_PLEN_OFFSET);
}

 
static int ocs_aes_validate_inputs(dma_addr_t src_dma_list, u32 src_size,
				   const u8 *iv, u32 iv_size,
				   dma_addr_t aad_dma_list, u32 aad_size,
				   const u8 *tag, u32 tag_size,
				   enum ocs_cipher cipher, enum ocs_mode mode,
				   enum ocs_instruction instruction,
				   dma_addr_t dst_dma_list)
{
	 
	if (!(cipher == OCS_AES || cipher == OCS_SM4))
		return -EINVAL;

	if (mode != OCS_MODE_ECB && mode != OCS_MODE_CBC &&
	    mode != OCS_MODE_CTR && mode != OCS_MODE_CCM &&
	    mode != OCS_MODE_GCM && mode != OCS_MODE_CTS)
		return -EINVAL;

	if (instruction != OCS_ENCRYPT && instruction != OCS_DECRYPT &&
	    instruction != OCS_EXPAND  && instruction != OCS_BYPASS)
		return -EINVAL;

	 
	if (instruction == OCS_BYPASS) {
		if (src_dma_list == DMA_MAPPING_ERROR ||
		    dst_dma_list == DMA_MAPPING_ERROR)
			return -EINVAL;

		return 0;
	}

	 
	switch (mode) {
	case OCS_MODE_ECB:
		 
		if (src_size % AES_BLOCK_SIZE != 0)
			return -EINVAL;

		 
		if (src_dma_list == DMA_MAPPING_ERROR ||
		    dst_dma_list == DMA_MAPPING_ERROR)
			return -EINVAL;

		return 0;

	case OCS_MODE_CBC:
		 
		if (src_size % AES_BLOCK_SIZE != 0)
			return -EINVAL;

		 
		if (src_dma_list == DMA_MAPPING_ERROR ||
		    dst_dma_list == DMA_MAPPING_ERROR)
			return -EINVAL;

		 
		if (!iv || iv_size != AES_BLOCK_SIZE)
			return -EINVAL;

		return 0;

	case OCS_MODE_CTR:
		 
		if (src_size == 0)
			return -EINVAL;

		 
		if (src_dma_list == DMA_MAPPING_ERROR ||
		    dst_dma_list == DMA_MAPPING_ERROR)
			return -EINVAL;

		 
		if (!iv || iv_size != AES_BLOCK_SIZE)
			return -EINVAL;

		return 0;

	case OCS_MODE_CTS:
		 
		if (src_size < AES_BLOCK_SIZE)
			return -EINVAL;

		 
		if (src_dma_list == DMA_MAPPING_ERROR ||
		    dst_dma_list == DMA_MAPPING_ERROR)
			return -EINVAL;

		 
		if (!iv || iv_size != AES_BLOCK_SIZE)
			return -EINVAL;

		return 0;

	case OCS_MODE_GCM:
		 
		if (!iv || iv_size != GCM_AES_IV_SIZE)
			return -EINVAL;

		 
		if (src_size && (src_dma_list == DMA_MAPPING_ERROR ||
				 dst_dma_list == DMA_MAPPING_ERROR))
			return -EINVAL;

		 
		if (aad_size && aad_dma_list == DMA_MAPPING_ERROR)
			return -EINVAL;

		 
		if (!tag)
			return -EINVAL;

		 
		if (tag_size > (AES_MAX_TAG_SIZE_U32 * sizeof(u32)))
			return -EINVAL;

		return 0;

	case OCS_MODE_CCM:
		 
		if (!iv || iv_size != AES_BLOCK_SIZE)
			return -EINVAL;

		 
		if (iv[L_PRIME_IDX] < L_PRIME_MIN ||
		    iv[L_PRIME_IDX] > L_PRIME_MAX)
			return -EINVAL;

		 
		if (aad_size && aad_dma_list == DMA_MAPPING_ERROR)
			return -EINVAL;

		 
		if (tag_size > (AES_MAX_TAG_SIZE_U32 * sizeof(u32)))
			return -EINVAL;

		if (instruction == OCS_DECRYPT) {
			 
			if (src_size && (src_dma_list == DMA_MAPPING_ERROR ||
					 dst_dma_list == DMA_MAPPING_ERROR))
				return -EINVAL;

			 
			if (!tag)
				return -EINVAL;

			return 0;
		}

		 

		 
		if (dst_dma_list == DMA_MAPPING_ERROR)
			return -EINVAL;

		 
		if (src_size && src_dma_list == DMA_MAPPING_ERROR)
			return -EINVAL;

		return 0;

	default:
		return -EINVAL;
	}
}

 
int ocs_aes_op(struct ocs_aes_dev *aes_dev,
	       enum ocs_mode mode,
	       enum ocs_cipher cipher,
	       enum ocs_instruction instruction,
	       dma_addr_t dst_dma_list,
	       dma_addr_t src_dma_list,
	       u32 src_size,
	       u8 *iv,
	       u32 iv_size)
{
	u32 *iv32;
	int rc;

	rc = ocs_aes_validate_inputs(src_dma_list, src_size, iv, iv_size, 0, 0,
				     NULL, 0, cipher, mode, instruction,
				     dst_dma_list);
	if (rc)
		return rc;
	 
	if (mode == OCS_MODE_GCM || mode == OCS_MODE_CCM)
		return -EINVAL;

	 
	iv32 = (u32 *)iv;

	ocs_aes_init(aes_dev, mode, cipher, instruction);

	if (mode == OCS_MODE_CTS) {
		 
		ocs_aes_write_last_data_blk_len(aes_dev, src_size);
	}

	 
	if (mode != OCS_MODE_ECB) {
		iowrite32(iv32[0], aes_dev->base_reg + AES_IV_0_OFFSET);
		iowrite32(iv32[1], aes_dev->base_reg + AES_IV_1_OFFSET);
		iowrite32(iv32[2], aes_dev->base_reg + AES_IV_2_OFFSET);
		iowrite32(iv32[3], aes_dev->base_reg + AES_IV_3_OFFSET);
	}

	 
	aes_a_op_trigger(aes_dev);

	 
	dma_to_ocs_aes_ll(aes_dev, src_dma_list);
	dma_from_ocs_aes_ll(aes_dev, dst_dma_list);
	aes_a_dma_active_src_dst_ll_en(aes_dev);

	if (mode == OCS_MODE_CTS) {
		 
		aes_a_set_last_gcx(aes_dev);
	} else {
		 
		aes_a_op_termination(aes_dev);
	}

	 
	rc = ocs_aes_irq_enable_and_wait(aes_dev, AES_COMPLETE_INT);
	if (rc)
		return rc;

	if (mode == OCS_MODE_CTR) {
		 
		iv32[0] = ioread32(aes_dev->base_reg + AES_IV_0_OFFSET);
		iv32[1] = ioread32(aes_dev->base_reg + AES_IV_1_OFFSET);
		iv32[2] = ioread32(aes_dev->base_reg + AES_IV_2_OFFSET);
		iv32[3] = ioread32(aes_dev->base_reg + AES_IV_3_OFFSET);
	}

	return 0;
}

 
static void ocs_aes_gcm_write_j0(const struct ocs_aes_dev *aes_dev,
				 const u8 *iv)
{
	const u32 *j0 = (u32 *)iv;

	 
	iowrite32(0x00000001, aes_dev->base_reg + AES_IV_0_OFFSET);
	iowrite32(__swab32(j0[2]), aes_dev->base_reg + AES_IV_1_OFFSET);
	iowrite32(__swab32(j0[1]), aes_dev->base_reg + AES_IV_2_OFFSET);
	iowrite32(__swab32(j0[0]), aes_dev->base_reg + AES_IV_3_OFFSET);
}

 
static inline void ocs_aes_gcm_read_tag(struct ocs_aes_dev *aes_dev,
					u8 *tag, u32 tag_size)
{
	u32 tag_u32[AES_MAX_TAG_SIZE_U32];

	 
	tag_u32[0] = __swab32(ioread32(aes_dev->base_reg + AES_T_MAC_3_OFFSET));
	tag_u32[1] = __swab32(ioread32(aes_dev->base_reg + AES_T_MAC_2_OFFSET));
	tag_u32[2] = __swab32(ioread32(aes_dev->base_reg + AES_T_MAC_1_OFFSET));
	tag_u32[3] = __swab32(ioread32(aes_dev->base_reg + AES_T_MAC_0_OFFSET));

	memcpy(tag, tag_u32, tag_size);
}

 
int ocs_aes_gcm_op(struct ocs_aes_dev *aes_dev,
		   enum ocs_cipher cipher,
		   enum ocs_instruction instruction,
		   dma_addr_t dst_dma_list,
		   dma_addr_t src_dma_list,
		   u32 src_size,
		   const u8 *iv,
		   dma_addr_t aad_dma_list,
		   u32 aad_size,
		   u8 *out_tag,
		   u32 tag_size)
{
	u64 bit_len;
	u32 val;
	int rc;

	rc = ocs_aes_validate_inputs(src_dma_list, src_size, iv,
				     GCM_AES_IV_SIZE, aad_dma_list,
				     aad_size, out_tag, tag_size, cipher,
				     OCS_MODE_GCM, instruction,
				     dst_dma_list);
	if (rc)
		return rc;

	ocs_aes_init(aes_dev, OCS_MODE_GCM, cipher, instruction);

	 
	ocs_aes_gcm_write_j0(aes_dev, iv);

	 
	iowrite32(tag_size, aes_dev->base_reg + AES_TLEN_OFFSET);

	 
	ocs_aes_write_last_data_blk_len(aes_dev, src_size);

	 
	bit_len = (u64)src_size * 8;
	val = bit_len & 0xFFFFFFFF;
	iowrite32(val, aes_dev->base_reg + AES_MULTIPURPOSE2_0_OFFSET);
	val = bit_len >> 32;
	iowrite32(val, aes_dev->base_reg + AES_MULTIPURPOSE2_1_OFFSET);

	 
	bit_len = (u64)aad_size * 8;
	val = bit_len & 0xFFFFFFFF;
	iowrite32(val, aes_dev->base_reg + AES_MULTIPURPOSE2_2_OFFSET);
	val = bit_len >> 32;
	iowrite32(val, aes_dev->base_reg + AES_MULTIPURPOSE2_3_OFFSET);

	 
	aes_a_op_trigger(aes_dev);

	 
	if (aad_size) {
		 
		dma_to_ocs_aes_ll(aes_dev, aad_dma_list);
		aes_a_dma_active_src_ll_en(aes_dev);

		 
		aes_a_set_last_gcx_and_adata(aes_dev);

		 
		rc = ocs_aes_irq_enable_and_wait(aes_dev, AES_DMA_SRC_DONE_INT);
		if (rc)
			return rc;
	} else {
		aes_a_set_last_gcx_and_adata(aes_dev);
	}

	 
	aes_a_wait_last_gcx(aes_dev);
	aes_a_dma_wait_input_buffer_occupancy(aes_dev);

	 
	if (src_size) {
		 
		dma_to_ocs_aes_ll(aes_dev, src_dma_list);
		dma_from_ocs_aes_ll(aes_dev, dst_dma_list);
		aes_a_dma_active_src_dst_ll_en(aes_dev);
	} else {
		aes_a_dma_set_xfer_size_zero(aes_dev);
		aes_a_dma_active(aes_dev);
	}

	 
	aes_a_set_last_gcx(aes_dev);

	 
	rc = ocs_aes_irq_enable_and_wait(aes_dev, AES_COMPLETE_INT);
	if (rc)
		return rc;

	ocs_aes_gcm_read_tag(aes_dev, out_tag, tag_size);

	return 0;
}

 
static void ocs_aes_ccm_write_encrypted_tag(struct ocs_aes_dev *aes_dev,
					    const u8 *in_tag, u32 tag_size)
{
	int i;

	 
	aes_a_dma_wait_input_buffer_occupancy(aes_dev);

	 
	aes_a_dma_reset_and_activate_perf_cntr(aes_dev);
	aes_a_dma_wait_and_deactivate_perf_cntr(aes_dev,
						CCM_DECRYPT_DELAY_TAG_CLK_COUNT);

	 
	for (i = 0; i < tag_size; i++) {
		iowrite8(in_tag[i], aes_dev->base_reg +
				    AES_A_DMA_INBUFFER_WRITE_FIFO_OFFSET);
	}
}

 
static int ocs_aes_ccm_write_b0(const struct ocs_aes_dev *aes_dev,
				const u8 *iv, u32 adata_size, u32 tag_size,
				u32 cryptlen)
{
	u8 b0[16];  
	int i, q;

	 
	memset(b0, 0, sizeof(b0));

	 
	 
	if (adata_size)
		b0[0] |= BIT(6);
	 
	b0[0] |= (((tag_size - 2) / 2) & 0x7)  << 3;
	 
	b0[0] |= iv[0] & 0x7;
	 
	q = (iv[0] & 0x7) + 1;
	for (i = 1; i <= 15 - q; i++)
		b0[i] = iv[i];
	 
	i = sizeof(b0) - 1;
	while (q) {
		b0[i] = cryptlen & 0xff;
		cryptlen >>= 8;
		i--;
		q--;
	}
	 
	if (cryptlen)
		return -EOVERFLOW;
	 
	for (i = 0; i < sizeof(b0); i++)
		iowrite8(b0[i], aes_dev->base_reg +
				AES_A_DMA_INBUFFER_WRITE_FIFO_OFFSET);
	return 0;
}

 
static void ocs_aes_ccm_write_adata_len(const struct ocs_aes_dev *aes_dev,
					u64 adata_len)
{
	u8 enc_a[10];  
	int i, len;

	 
	if (adata_len < 65280) {
		len = 2;
		*(__be16 *)enc_a = cpu_to_be16(adata_len);
	} else if (adata_len <= 0xFFFFFFFF) {
		len = 6;
		*(__be16 *)enc_a = cpu_to_be16(0xfffe);
		*(__be32 *)&enc_a[2] = cpu_to_be32(adata_len);
	} else {  
		len = 10;
		*(__be16 *)enc_a = cpu_to_be16(0xffff);
		*(__be64 *)&enc_a[2] = cpu_to_be64(adata_len);
	}
	for (i = 0; i < len; i++)
		iowrite8(enc_a[i],
			 aes_dev->base_reg +
			 AES_A_DMA_INBUFFER_WRITE_FIFO_OFFSET);
}

static int ocs_aes_ccm_do_adata(struct ocs_aes_dev *aes_dev,
				dma_addr_t adata_dma_list, u32 adata_size)
{
	int rc;

	if (!adata_size) {
		 
		aes_a_set_last_gcx_and_adata(aes_dev);
		goto exit;
	}

	 

	 
	ocs_aes_ccm_write_adata_len(aes_dev, adata_size);

	 
	dma_to_ocs_aes_ll(aes_dev, adata_dma_list);

	 
	aes_a_dma_active_src_ll_en(aes_dev);

	 
	aes_a_set_last_gcx_and_adata(aes_dev);

	 
	rc = ocs_aes_irq_enable_and_wait(aes_dev, AES_DMA_SRC_DONE_INT);
	if (rc)
		return rc;

exit:
	 
	aes_a_wait_last_gcx(aes_dev);
	aes_a_dma_wait_input_buffer_occupancy(aes_dev);

	return 0;
}

static int ocs_aes_ccm_encrypt_do_payload(struct ocs_aes_dev *aes_dev,
					  dma_addr_t dst_dma_list,
					  dma_addr_t src_dma_list,
					  u32 src_size)
{
	if (src_size) {
		 
		dma_to_ocs_aes_ll(aes_dev, src_dma_list);
		dma_from_ocs_aes_ll(aes_dev, dst_dma_list);
		aes_a_dma_active_src_dst_ll_en(aes_dev);
	} else {
		 
		dma_from_ocs_aes_ll(aes_dev, dst_dma_list);
		aes_a_dma_active_dst_ll_en(aes_dev);
	}

	 
	aes_a_set_last_gcx(aes_dev);

	 
	return ocs_aes_irq_enable_and_wait(aes_dev, AES_COMPLETE_INT);
}

static int ocs_aes_ccm_decrypt_do_payload(struct ocs_aes_dev *aes_dev,
					  dma_addr_t dst_dma_list,
					  dma_addr_t src_dma_list,
					  u32 src_size)
{
	if (!src_size) {
		 
		aes_a_dma_set_xfer_size_zero(aes_dev);
		aes_a_dma_active(aes_dev);
		aes_a_set_last_gcx(aes_dev);

		return 0;
	}

	 
	dma_to_ocs_aes_ll(aes_dev, src_dma_list);
	dma_from_ocs_aes_ll(aes_dev, dst_dma_list);
	aes_a_dma_active_src_dst_ll_en(aes_dev);
	 
	aes_a_set_last_gcx(aes_dev);
	  
	return ocs_aes_irq_enable_and_wait(aes_dev, AES_DMA_SRC_DONE_INT);
}

 
static inline int ccm_compare_tag_to_yr(struct ocs_aes_dev *aes_dev,
					u8 tag_size_bytes)
{
	u32 tag[AES_MAX_TAG_SIZE_U32];
	u32 yr[AES_MAX_TAG_SIZE_U32];
	u8 i;

	 
	for (i = 0; i < AES_MAX_TAG_SIZE_U32; i++) {
		tag[i] = ioread32(aes_dev->base_reg +
				  AES_T_MAC_0_OFFSET + (i * sizeof(u32)));
		yr[i] = ioread32(aes_dev->base_reg +
				 AES_MULTIPURPOSE2_0_OFFSET +
				 (i * sizeof(u32)));
	}

	return memcmp(tag, yr, tag_size_bytes) ? -EBADMSG : 0;
}

 
int ocs_aes_ccm_op(struct ocs_aes_dev *aes_dev,
		   enum ocs_cipher cipher,
		   enum ocs_instruction instruction,
		   dma_addr_t dst_dma_list,
		   dma_addr_t src_dma_list,
		   u32 src_size,
		   u8 *iv,
		   dma_addr_t adata_dma_list,
		   u32 adata_size,
		   u8 *in_tag,
		   u32 tag_size)
{
	u32 *iv_32;
	u8 lprime;
	int rc;

	rc = ocs_aes_validate_inputs(src_dma_list, src_size, iv,
				     AES_BLOCK_SIZE, adata_dma_list, adata_size,
				     in_tag, tag_size, cipher, OCS_MODE_CCM,
				     instruction, dst_dma_list);
	if (rc)
		return rc;

	ocs_aes_init(aes_dev, OCS_MODE_CCM, cipher, instruction);

	 
	lprime = iv[L_PRIME_IDX];
	memset(&iv[COUNTER_START(lprime)], 0, COUNTER_LEN(lprime));

	 
	iv_32 = (u32 *)iv;
	iowrite32(__swab32(iv_32[0]),
		  aes_dev->base_reg + AES_MULTIPURPOSE1_3_OFFSET);
	iowrite32(__swab32(iv_32[1]),
		  aes_dev->base_reg + AES_MULTIPURPOSE1_2_OFFSET);
	iowrite32(__swab32(iv_32[2]),
		  aes_dev->base_reg + AES_MULTIPURPOSE1_1_OFFSET);
	iowrite32(__swab32(iv_32[3]),
		  aes_dev->base_reg + AES_MULTIPURPOSE1_0_OFFSET);

	 
	iowrite32(tag_size, aes_dev->base_reg + AES_TLEN_OFFSET);
	 
	ocs_aes_write_last_data_blk_len(aes_dev, src_size);

	 
	aes_a_op_trigger(aes_dev);

	aes_a_dma_reset_and_activate_perf_cntr(aes_dev);

	 
	rc = ocs_aes_ccm_write_b0(aes_dev, iv, adata_size, tag_size, src_size);
	if (rc)
		return rc;
	 
	aes_a_dma_wait_and_deactivate_perf_cntr(aes_dev,
						CCM_DECRYPT_DELAY_LAST_GCX_CLK_COUNT);

	 
	ocs_aes_ccm_do_adata(aes_dev, adata_dma_list, adata_size);

	 
	if (instruction == OCS_ENCRYPT) {
		return ocs_aes_ccm_encrypt_do_payload(aes_dev, dst_dma_list,
						      src_dma_list, src_size);
	}
	 
	rc = ocs_aes_ccm_decrypt_do_payload(aes_dev, dst_dma_list,
					    src_dma_list, src_size);
	if (rc)
		return rc;

	 
	ocs_aes_ccm_write_encrypted_tag(aes_dev, in_tag, tag_size);
	rc = ocs_aes_irq_enable_and_wait(aes_dev, AES_COMPLETE_INT);
	if (rc)
		return rc;

	return ccm_compare_tag_to_yr(aes_dev, tag_size);
}

 
int ocs_create_linked_list_from_sg(const struct ocs_aes_dev *aes_dev,
				   struct scatterlist *sg,
				   int sg_dma_count,
				   struct ocs_dll_desc *dll_desc,
				   size_t data_size, size_t data_offset)
{
	struct ocs_dma_linked_list *ll = NULL;
	struct scatterlist *sg_tmp;
	unsigned int tmp;
	int dma_nents;
	int i;

	if (!dll_desc || !sg || !aes_dev)
		return -EINVAL;

	 
	dll_desc->vaddr = NULL;
	dll_desc->dma_addr = DMA_MAPPING_ERROR;
	dll_desc->size = 0;

	if (data_size == 0)
		return 0;

	 
	while (data_offset >= sg_dma_len(sg)) {
		data_offset -= sg_dma_len(sg);
		sg_dma_count--;
		sg = sg_next(sg);
		 
		if (!sg || sg_dma_count == 0)
			return -EINVAL;
	}

	 
	dma_nents = 0;
	tmp = 0;
	sg_tmp = sg;
	while (tmp < data_offset + data_size) {
		 
		if (!sg_tmp)
			return -EINVAL;
		tmp += sg_dma_len(sg_tmp);
		dma_nents++;
		sg_tmp = sg_next(sg_tmp);
	}
	if (dma_nents > sg_dma_count)
		return -EINVAL;

	 
	dll_desc->size = sizeof(struct ocs_dma_linked_list) * dma_nents;
	dll_desc->vaddr = dma_alloc_coherent(aes_dev->dev, dll_desc->size,
					     &dll_desc->dma_addr, GFP_KERNEL);
	if (!dll_desc->vaddr)
		return -ENOMEM;

	 
	ll = dll_desc->vaddr;
	for (i = 0; i < dma_nents; i++, sg = sg_next(sg)) {
		ll[i].src_addr = sg_dma_address(sg) + data_offset;
		ll[i].src_len = (sg_dma_len(sg) - data_offset) < data_size ?
				(sg_dma_len(sg) - data_offset) : data_size;
		data_offset = 0;
		data_size -= ll[i].src_len;
		 
		ll[i].next = dll_desc->dma_addr + (sizeof(*ll) * (i + 1));
		ll[i].ll_flags = 0;
	}
	 
	ll[i - 1].next = 0;
	ll[i - 1].ll_flags = OCS_LL_DMA_FLAG_TERMINATE;

	return 0;
}
