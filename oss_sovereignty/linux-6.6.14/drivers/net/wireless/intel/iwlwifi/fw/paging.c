
 
#include "iwl-drv.h"
#include "runtime.h"
#include "fw/api/commands.h"

void iwl_free_fw_paging(struct iwl_fw_runtime *fwrt)
{
	int i;

	if (!fwrt->fw_paging_db[0].fw_paging_block)
		return;

	for (i = 0; i < NUM_OF_FW_PAGING_BLOCKS; i++) {
		struct iwl_fw_paging *paging = &fwrt->fw_paging_db[i];

		if (!paging->fw_paging_block) {
			IWL_DEBUG_FW(fwrt,
				     "Paging: block %d already freed, continue to next page\n",
				     i);

			continue;
		}
		dma_unmap_page(fwrt->trans->dev, paging->fw_paging_phys,
			       paging->fw_paging_size, DMA_BIDIRECTIONAL);

		__free_pages(paging->fw_paging_block,
			     get_order(paging->fw_paging_size));
		paging->fw_paging_block = NULL;
	}

	memset(fwrt->fw_paging_db, 0, sizeof(fwrt->fw_paging_db));
}
IWL_EXPORT_SYMBOL(iwl_free_fw_paging);

static int iwl_alloc_fw_paging_mem(struct iwl_fw_runtime *fwrt,
				   const struct fw_img *image)
{
	struct page *block;
	dma_addr_t phys = 0;
	int blk_idx, order, num_of_pages, size;

	if (fwrt->fw_paging_db[0].fw_paging_block)
		return 0;

	 
	BUILD_BUG_ON(BIT(BLOCK_2_EXP_SIZE) != PAGING_BLOCK_SIZE);

	num_of_pages = image->paging_mem_size / FW_PAGING_SIZE;
	fwrt->num_of_paging_blk =
		DIV_ROUND_UP(num_of_pages, NUM_OF_PAGE_PER_GROUP);
	fwrt->num_of_pages_in_last_blk =
		num_of_pages -
		NUM_OF_PAGE_PER_GROUP * (fwrt->num_of_paging_blk - 1);

	IWL_DEBUG_FW(fwrt,
		     "Paging: allocating mem for %d paging blocks, each block holds 8 pages, last block holds %d pages\n",
		     fwrt->num_of_paging_blk,
		     fwrt->num_of_pages_in_last_blk);

	 
	for (blk_idx = 0; blk_idx < fwrt->num_of_paging_blk + 1; blk_idx++) {
		 
		size = blk_idx ? PAGING_BLOCK_SIZE : FW_PAGING_SIZE;
		order = get_order(size);
		block = alloc_pages(GFP_KERNEL, order);
		if (!block) {
			 
			iwl_free_fw_paging(fwrt);
			return -ENOMEM;
		}

		fwrt->fw_paging_db[blk_idx].fw_paging_block = block;
		fwrt->fw_paging_db[blk_idx].fw_paging_size = size;

		phys = dma_map_page(fwrt->trans->dev, block, 0,
				    PAGE_SIZE << order,
				    DMA_BIDIRECTIONAL);
		if (dma_mapping_error(fwrt->trans->dev, phys)) {
			 
			iwl_free_fw_paging(fwrt);
			return -ENOMEM;
		}
		fwrt->fw_paging_db[blk_idx].fw_paging_phys = phys;

		if (!blk_idx)
			IWL_DEBUG_FW(fwrt,
				     "Paging: allocated 4K(CSS) bytes (order %d) for firmware paging.\n",
				     order);
		else
			IWL_DEBUG_FW(fwrt,
				     "Paging: allocated 32K bytes (order %d) for firmware paging.\n",
				     order);
	}

	return 0;
}

static int iwl_fill_paging_mem(struct iwl_fw_runtime *fwrt,
			       const struct fw_img *image)
{
	int sec_idx, idx, ret;
	u32 offset = 0;

	 
	for (sec_idx = 0; sec_idx < image->num_sec; sec_idx++) {
		if (image->sec[sec_idx].offset == PAGING_SEPARATOR_SECTION) {
			sec_idx++;
			break;
		}
	}

	 
	if (sec_idx >= image->num_sec - 1) {
		IWL_ERR(fwrt, "Paging: Missing CSS and/or paging sections\n");
		ret = -EINVAL;
		goto err;
	}

	 
	IWL_DEBUG_FW(fwrt, "Paging: load paging CSS to FW, sec = %d\n",
		     sec_idx);

	if (image->sec[sec_idx].len > fwrt->fw_paging_db[0].fw_paging_size) {
		IWL_ERR(fwrt, "CSS block is larger than paging size\n");
		ret = -EINVAL;
		goto err;
	}

	memcpy(page_address(fwrt->fw_paging_db[0].fw_paging_block),
	       image->sec[sec_idx].data,
	       image->sec[sec_idx].len);
	fwrt->fw_paging_db[0].fw_offs = image->sec[sec_idx].offset;
	dma_sync_single_for_device(fwrt->trans->dev,
				   fwrt->fw_paging_db[0].fw_paging_phys,
				   fwrt->fw_paging_db[0].fw_paging_size,
				   DMA_BIDIRECTIONAL);

	IWL_DEBUG_FW(fwrt,
		     "Paging: copied %d CSS bytes to first block\n",
		     fwrt->fw_paging_db[0].fw_paging_size);

	sec_idx++;

	 
	for (idx = 1; idx < fwrt->num_of_paging_blk + 1; idx++) {
		struct iwl_fw_paging *block = &fwrt->fw_paging_db[idx];
		int remaining = image->sec[sec_idx].len - offset;
		int len = block->fw_paging_size;

		 
		if (idx == fwrt->num_of_paging_blk) {
			len = remaining;
			if (remaining !=
			    fwrt->num_of_pages_in_last_blk * FW_PAGING_SIZE) {
				IWL_ERR(fwrt,
					"Paging: last block contains more data than expected %d\n",
					remaining);
				ret = -EINVAL;
				goto err;
			}
		} else if (block->fw_paging_size > remaining) {
			IWL_ERR(fwrt,
				"Paging: not enough data in other in block %d (%d)\n",
				idx, remaining);
			ret = -EINVAL;
			goto err;
		}

		memcpy(page_address(block->fw_paging_block),
		       (const u8 *)image->sec[sec_idx].data + offset, len);
		block->fw_offs = image->sec[sec_idx].offset + offset;
		dma_sync_single_for_device(fwrt->trans->dev,
					   block->fw_paging_phys,
					   block->fw_paging_size,
					   DMA_BIDIRECTIONAL);

		IWL_DEBUG_FW(fwrt,
			     "Paging: copied %d paging bytes to block %d\n",
			     len, idx);

		offset += block->fw_paging_size;
	}

	return 0;

err:
	iwl_free_fw_paging(fwrt);
	return ret;
}

static int iwl_save_fw_paging(struct iwl_fw_runtime *fwrt,
			      const struct fw_img *fw)
{
	int ret;

	ret = iwl_alloc_fw_paging_mem(fwrt, fw);
	if (ret)
		return ret;

	return iwl_fill_paging_mem(fwrt, fw);
}

 
static int iwl_send_paging_cmd(struct iwl_fw_runtime *fwrt,
			       const struct fw_img *fw)
{
	struct iwl_fw_paging_cmd paging_cmd = {
		.flags = cpu_to_le32(PAGING_CMD_IS_SECURED |
				     PAGING_CMD_IS_ENABLED |
				     (fwrt->num_of_pages_in_last_blk <<
				      PAGING_CMD_NUM_OF_PAGES_IN_LAST_GRP_POS)),
		.block_size = cpu_to_le32(BLOCK_2_EXP_SIZE),
		.block_num = cpu_to_le32(fwrt->num_of_paging_blk),
	};
	struct iwl_host_cmd hcmd = {
		.id = WIDE_ID(IWL_ALWAYS_LONG_GROUP, FW_PAGING_BLOCK_CMD),
		.len = { sizeof(paging_cmd), },
		.data = { &paging_cmd, },
	};
	int blk_idx;

	 
	for (blk_idx = 0; blk_idx < fwrt->num_of_paging_blk + 1; blk_idx++) {
		dma_addr_t addr = fwrt->fw_paging_db[blk_idx].fw_paging_phys;
		__le32 phy_addr;

		addr = addr >> PAGE_2_EXP_SIZE;
		phy_addr = cpu_to_le32(addr);
		paging_cmd.device_phy_addr[blk_idx] = phy_addr;
	}

	return iwl_trans_send_cmd(fwrt->trans, &hcmd);
}

int iwl_init_paging(struct iwl_fw_runtime *fwrt, enum iwl_ucode_type type)
{
	const struct fw_img *fw = &fwrt->fw->img[type];
	int ret;

	if (fwrt->trans->trans_cfg->gen2)
		return 0;

	 
	if (!fw->paging_mem_size)
		return 0;

	ret = iwl_save_fw_paging(fwrt, fw);
	if (ret) {
		IWL_ERR(fwrt, "failed to save the FW paging image\n");
		return ret;
	}

	ret = iwl_send_paging_cmd(fwrt, fw);
	if (ret) {
		IWL_ERR(fwrt, "failed to send the paging cmd\n");
		iwl_free_fw_paging(fwrt);
		return ret;
	}

	return 0;
}
IWL_EXPORT_SYMBOL(iwl_init_paging);
