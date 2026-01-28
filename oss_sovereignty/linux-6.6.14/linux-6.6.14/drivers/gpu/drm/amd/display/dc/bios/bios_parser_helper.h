#ifndef __DAL_BIOS_PARSER_HELPER_H__
#define __DAL_BIOS_PARSER_HELPER_H__
struct bios_parser;
uint8_t *bios_get_image(struct dc_bios *bp, uint32_t offset,
	uint32_t size);
bool bios_is_accelerated_mode(struct dc_bios *bios);
void bios_set_scratch_acc_mode_change(struct dc_bios *bios, uint32_t state);
void bios_set_scratch_critical_state(struct dc_bios *bios, bool state);
uint32_t bios_get_vga_enabled_displays(struct dc_bios *bios);
#define GET_IMAGE(type, offset) ((type *) bios_get_image(&bp->base, offset, sizeof(type)))
#endif
