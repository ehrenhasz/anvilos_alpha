 

#ifndef __DAL_HW_TRANSLATE_H__
#define __DAL_HW_TRANSLATE_H__

struct hw_translate_funcs {
	bool (*offset_to_id)(
		uint32_t offset,
		uint32_t mask,
		enum gpio_id *id,
		uint32_t *en);
	bool (*id_to_offset)(
		enum gpio_id id,
		uint32_t en,
		struct gpio_pin_info *info);
};

struct hw_translate {
	const struct hw_translate_funcs *funcs;
};

bool dal_hw_translate_init(
	struct hw_translate *translate,
	enum dce_version dce_version,
	enum dce_environment dce_environment);

#endif
