



#include <linux/firmware.h>
#include <linux/delay.h>
#include <drm/drm_print.h>
#include "ast_drv.h"

bool ast_astdp_is_connected(struct ast_device *ast)
{
	if (!ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xD1, ASTDP_MCU_FW_EXECUTING))
		return false;
	if (!ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xDF, ASTDP_HPD))
		return false;
	if (!ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xDC, ASTDP_LINK_SUCCESS))
		return false;
	return true;
}

int ast_astdp_read_edid(struct drm_device *dev, u8 *ediddata)
{
	struct ast_device *ast = to_ast_device(dev);
	u8 i = 0, j = 0;

	 
	if (!(ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xD1, ASTDP_MCU_FW_EXECUTING) &&
		ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xDC, ASTDP_LINK_SUCCESS) &&
		ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xDF, ASTDP_HPD) &&
		ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xE5,
								ASTDP_HOST_EDID_READ_DONE_MASK))) {
		goto err_astdp_edid_not_ready;
	}

	ast_set_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xE5, (u8) ~ASTDP_HOST_EDID_READ_DONE_MASK,
							0x00);

	for (i = 0; i < 32; i++) {
		 
		ast_set_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xE4,
				       ASTDP_AND_CLEAR_MASK, (u8)i);
		j = 0;

		 
		while ((ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xD7,
				ASTDP_EDID_VALID_FLAG_MASK) != 0x01) ||
			(ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xD6,
						ASTDP_EDID_READ_POINTER_MASK) != i)) {
			 
			mdelay(j+1);

			if (!(ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xD1,
							ASTDP_MCU_FW_EXECUTING) &&
				ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xDC,
							ASTDP_LINK_SUCCESS) &&
				ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xDF, ASTDP_HPD))) {
				goto err_astdp_jump_out_loop_of_edid;
			}

			j++;
			if (j > 200)
				goto err_astdp_jump_out_loop_of_edid;
		}

		*(ediddata) = ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT,
							0xD8, ASTDP_EDID_READ_DATA_MASK);
		*(ediddata + 1) = ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xD9,
								ASTDP_EDID_READ_DATA_MASK);
		*(ediddata + 2) = ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xDA,
								ASTDP_EDID_READ_DATA_MASK);
		*(ediddata + 3) = ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xDB,
								ASTDP_EDID_READ_DATA_MASK);

		if (i == 31) {
			 
			*(ediddata + 3) = *(ediddata + 3) + *(ediddata + 2);
			*(ediddata + 2) = 0;
		}

		ediddata += 4;
	}

	ast_set_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xE5, (u8) ~ASTDP_HOST_EDID_READ_DONE_MASK,
							ASTDP_HOST_EDID_READ_DONE);

	return 0;

err_astdp_jump_out_loop_of_edid:
	ast_set_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xE5,
							(u8) ~ASTDP_HOST_EDID_READ_DONE_MASK,
							ASTDP_HOST_EDID_READ_DONE);
	return (~(j+256) + 1);

err_astdp_edid_not_ready:
	if (!(ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xD1, ASTDP_MCU_FW_EXECUTING)))
		return (~0xD1 + 1);
	if (!(ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xDC, ASTDP_LINK_SUCCESS)))
		return (~0xDC + 1);
	if (!(ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xDF, ASTDP_HPD)))
		return (~0xDF + 1);
	if (!(ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xE5, ASTDP_HOST_EDID_READ_DONE_MASK)))
		return (~0xE5 + 1);

	return	0;
}

 
void ast_dp_launch(struct drm_device *dev)
{
	u32 i = 0;
	u8 bDPExecute = 1;
	struct ast_device *ast = to_ast_device(dev);

	
	while (ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xD1, ASTDP_MCU_FW_EXECUTING) !=
		ASTDP_MCU_FW_EXECUTING) {
		i++;
		
		msleep(100);

		if (i >= 10) {
			
			bDPExecute = 0;
			break;
		}
	}

	if (!bDPExecute)
		drm_err(dev, "Wait DPMCU executing timeout\n");

	ast_set_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xE5,
			       (u8) ~ASTDP_HOST_EDID_READ_DONE_MASK,
			       ASTDP_HOST_EDID_READ_DONE);
}



void ast_dp_power_on_off(struct drm_device *dev, bool on)
{
	struct ast_device *ast = to_ast_device(dev);
	
	u8 bE3 = ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xE3, AST_DP_VIDEO_ENABLE);

	
	if (!on)
		bE3 |= AST_DP_PHY_SLEEP;

	
	ast_set_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xE3, (u8) ~AST_DP_PHY_SLEEP, bE3);
}



void ast_dp_set_on_off(struct drm_device *dev, bool on)
{
	struct ast_device *ast = to_ast_device(dev);
	u8 video_on_off = on;

	
	ast_set_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xE3, (u8) ~AST_DP_VIDEO_ENABLE, on);

	
	if (ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xDC, ASTDP_LINK_SUCCESS) &&
		ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xDF, ASTDP_HPD)) {
		video_on_off <<= 4;
		while (ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xDF,
						ASTDP_MIRROR_VIDEO_ENABLE) != video_on_off) {
			
			mdelay(1);
		}
	}
}

void ast_dp_set_mode(struct drm_crtc *crtc, struct ast_vbios_mode_info *vbios_mode)
{
	struct ast_device *ast = to_ast_device(crtc->dev);

	u32 ulRefreshRateIndex;
	u8 ModeIdx;

	ulRefreshRateIndex = vbios_mode->enh_table->refresh_rate_index - 1;

	switch (crtc->mode.crtc_hdisplay) {
	case 320:
		ModeIdx = ASTDP_320x240_60;
		break;
	case 400:
		ModeIdx = ASTDP_400x300_60;
		break;
	case 512:
		ModeIdx = ASTDP_512x384_60;
		break;
	case 640:
		ModeIdx = (ASTDP_640x480_60 + (u8) ulRefreshRateIndex);
		break;
	case 800:
		ModeIdx = (ASTDP_800x600_56 + (u8) ulRefreshRateIndex);
		break;
	case 1024:
		ModeIdx = (ASTDP_1024x768_60 + (u8) ulRefreshRateIndex);
		break;
	case 1152:
		ModeIdx = ASTDP_1152x864_75;
		break;
	case 1280:
		if (crtc->mode.crtc_vdisplay == 800)
			ModeIdx = (ASTDP_1280x800_60_RB - (u8) ulRefreshRateIndex);
		else		
			ModeIdx = (ASTDP_1280x1024_60 + (u8) ulRefreshRateIndex);
		break;
	case 1360:
	case 1366:
		ModeIdx = ASTDP_1366x768_60;
		break;
	case 1440:
		ModeIdx = (ASTDP_1440x900_60_RB - (u8) ulRefreshRateIndex);
		break;
	case 1600:
		if (crtc->mode.crtc_vdisplay == 900)
			ModeIdx = (ASTDP_1600x900_60_RB - (u8) ulRefreshRateIndex);
		else		
			ModeIdx = ASTDP_1600x1200_60;
		break;
	case 1680:
		ModeIdx = (ASTDP_1680x1050_60_RB - (u8) ulRefreshRateIndex);
		break;
	case 1920:
		if (crtc->mode.crtc_vdisplay == 1080)
			ModeIdx = ASTDP_1920x1080_60;
		else		
			ModeIdx = ASTDP_1920x1200_60;
		break;
	default:
		return;
	}

	 
	ast_set_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xE0, ASTDP_AND_CLEAR_MASK,
			       ASTDP_MISC0_24bpp);
	ast_set_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xE1, ASTDP_AND_CLEAR_MASK, ASTDP_MISC1);
	ast_set_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xE2, ASTDP_AND_CLEAR_MASK, ModeIdx);
}
