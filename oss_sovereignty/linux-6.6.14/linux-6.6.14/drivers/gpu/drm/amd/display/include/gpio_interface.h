#ifndef __DAL_GPIO_INTERFACE_H__
#define __DAL_GPIO_INTERFACE_H__
#include "gpio_types.h"
#include "grph_object_defs.h"
struct gpio;
enum gpio_result dal_gpio_open(
	struct gpio *gpio,
	enum gpio_mode mode);
enum gpio_result dal_gpio_open_ex(
	struct gpio *gpio,
	enum gpio_mode mode);
enum gpio_result dal_gpio_get_value(
	const struct gpio *gpio,
	uint32_t *value);
enum gpio_result dal_gpio_set_value(
	const struct gpio *gpio,
	uint32_t value);
enum gpio_mode dal_gpio_get_mode(
	const struct gpio *gpio);
enum gpio_result dal_gpio_change_mode(
	struct gpio *gpio,
	enum gpio_mode mode);
enum gpio_result dal_gpio_lock_pin(
	struct gpio *gpio);
enum gpio_result dal_gpio_unlock_pin(
	struct gpio *gpio);
enum gpio_id dal_gpio_get_id(
	const struct gpio *gpio);
uint32_t dal_gpio_get_enum(
	const struct gpio *gpio);
enum gpio_result dal_gpio_set_config(
	struct gpio *gpio,
	const struct gpio_config_data *config_data);
enum gpio_result dal_gpio_get_pin_info(
	const struct gpio *gpio,
	struct gpio_pin_info *pin_info);
enum sync_source dal_gpio_get_sync_source(
	const struct gpio *gpio);
enum gpio_pin_output_state dal_gpio_get_output_state(
	const struct gpio *gpio);
struct hw_ddc *dal_gpio_get_ddc(struct gpio *gpio);
struct hw_hpd *dal_gpio_get_hpd(struct gpio *gpio);
struct hw_generic *dal_gpio_get_generic(struct gpio *gpio);
void dal_gpio_close(
	struct gpio *gpio);
#endif
