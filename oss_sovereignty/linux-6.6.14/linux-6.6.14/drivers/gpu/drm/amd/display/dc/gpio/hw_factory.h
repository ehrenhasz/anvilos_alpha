#ifndef __DAL_HW_FACTORY_H__
#define __DAL_HW_FACTORY_H__
struct hw_gpio_pin;
struct hw_hpd;
struct hw_ddc;
struct hw_generic;
struct gpio;
struct hw_factory {
	uint32_t number_of_pins[GPIO_ID_COUNT];
	const struct hw_factory_funcs {
		void (*init_ddc_data)(
				struct hw_ddc **hw_ddc,
				struct dc_context *ctx,
				enum gpio_id id,
				uint32_t en);
		void (*init_generic)(
				struct hw_generic **hw_generic,
				struct dc_context *ctx,
				enum gpio_id id,
				uint32_t en);
		void (*init_hpd)(
				struct hw_hpd **hw_hpd,
				struct dc_context *ctx,
				enum gpio_id id,
				uint32_t en);
		struct hw_gpio_pin *(*get_hpd_pin)(
				struct gpio *gpio);
		struct hw_gpio_pin *(*get_ddc_pin)(
				struct gpio *gpio);
		struct hw_gpio_pin *(*get_generic_pin)(
				struct gpio *gpio);
		void (*define_hpd_registers)(
				struct hw_gpio_pin *pin,
				uint32_t en);
		void (*define_ddc_registers)(
				struct hw_gpio_pin *pin,
				uint32_t en);
		void (*define_generic_registers)(
				struct hw_gpio_pin *pin,
				uint32_t en);
	} *funcs;
};
bool dal_hw_factory_init(
	struct hw_factory *factory,
	enum dce_version dce_version,
	enum dce_environment dce_environment);
#endif
