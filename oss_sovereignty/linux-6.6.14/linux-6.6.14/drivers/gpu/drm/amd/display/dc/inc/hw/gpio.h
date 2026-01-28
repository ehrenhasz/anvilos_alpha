#ifndef __DAL_GPIO_H__
#define __DAL_GPIO_H__
#include "gpio_types.h"
union gpio_hw_container {
	struct hw_ddc *ddc;
	struct hw_generic *generic;
	struct hw_hpd *hpd;
};
struct gpio {
	struct gpio_service *service;
	struct hw_gpio_pin *pin;
	enum gpio_id id;
	uint32_t en;
	union gpio_hw_container hw_container;
	enum gpio_mode mode;
	enum gpio_pin_output_state output_state;
};
#if 0
struct gpio_funcs {
	struct hw_gpio_pin *(*create_ddc_data)(
		struct dc_context *ctx,
		enum gpio_id id,
		uint32_t en);
	struct hw_gpio_pin *(*create_ddc_clock)(
		struct dc_context *ctx,
		enum gpio_id id,
		uint32_t en);
	struct hw_gpio_pin *(*create_generic)(
		struct dc_context *ctx,
		enum gpio_id id,
		uint32_t en);
	struct hw_gpio_pin *(*create_hpd)(
		struct dc_context *ctx,
		enum gpio_id id,
		uint32_t en);
	struct hw_gpio_pin *(*create_gpio_pad)(
		struct dc_context *ctx,
		enum gpio_id id,
		uint32_t en);
	struct hw_gpio_pin *(*create_sync)(
		struct dc_context *ctx,
		enum gpio_id id,
		uint32_t en);
	struct hw_gpio_pin *(*create_gsl)(
		struct dc_context *ctx,
		enum gpio_id id,
		uint32_t en);
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
#endif
#endif   
