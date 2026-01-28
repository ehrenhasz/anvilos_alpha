#ifndef __DAL_HW_GPIO_H__
#define __DAL_HW_GPIO_H__
#include "gpio_regs.h"
#define FROM_HW_GPIO_PIN(ptr) \
	container_of((ptr), struct hw_gpio, base)
struct addr_mask {
	uint32_t addr;
	uint32_t mask;
};
struct hw_gpio_pin {
	const struct hw_gpio_pin_funcs *funcs;
	enum gpio_id id;
	uint32_t en;
	enum gpio_mode mode;
	bool opened;
	struct dc_context *ctx;
};
struct hw_gpio_pin_funcs {
	void (*destroy)(
		struct hw_gpio_pin **ptr);
	bool (*open)(
		struct hw_gpio_pin *pin,
		enum gpio_mode mode);
	enum gpio_result (*get_value)(
		const struct hw_gpio_pin *pin,
		uint32_t *value);
	enum gpio_result (*set_value)(
		const struct hw_gpio_pin *pin,
		uint32_t value);
	enum gpio_result (*set_config)(
		struct hw_gpio_pin *pin,
		const struct gpio_config_data *config_data);
	enum gpio_result (*change_mode)(
		struct hw_gpio_pin *pin,
		enum gpio_mode mode);
	void (*close)(
		struct hw_gpio_pin *pin);
};
struct hw_gpio;
struct hw_gpio_pin_reg {
	struct addr_mask DC_GPIO_DATA_MASK;
	struct addr_mask DC_GPIO_DATA_A;
	struct addr_mask DC_GPIO_DATA_EN;
	struct addr_mask DC_GPIO_DATA_Y;
};
struct hw_gpio_mux_reg {
	struct addr_mask GPIO_MUX_CONTROL;
	struct addr_mask GPIO_MUX_STEREO_SEL;
};
struct hw_gpio {
	struct hw_gpio_pin base;
	struct {
		uint32_t mask;
		uint32_t a;
		uint32_t en;
		uint32_t mux;
	} store;
	bool mux_supported;
	const struct gpio_registers *regs;
};
#define HW_GPIO_FROM_BASE(hw_gpio_pin) \
	container_of((hw_gpio_pin), struct hw_gpio, base)
void dal_hw_gpio_construct(
	struct hw_gpio *pin,
	enum gpio_id id,
	uint32_t en,
	struct dc_context *ctx);
bool dal_hw_gpio_open(
	struct hw_gpio_pin *pin,
	enum gpio_mode mode);
enum gpio_result dal_hw_gpio_get_value(
	const struct hw_gpio_pin *pin,
	uint32_t *value);
enum gpio_result dal_hw_gpio_config_mode(
	struct hw_gpio *pin,
	enum gpio_mode mode);
void dal_hw_gpio_destruct(
	struct hw_gpio *pin);
enum gpio_result dal_hw_gpio_set_value(
	const struct hw_gpio_pin *ptr,
	uint32_t value);
enum gpio_result dal_hw_gpio_change_mode(
	struct hw_gpio_pin *ptr,
	enum gpio_mode mode);
void dal_hw_gpio_close(
	struct hw_gpio_pin *ptr);
#endif
