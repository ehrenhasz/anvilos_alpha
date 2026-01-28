#ifndef __DAL_GPIO_SERVICE_H__
#define __DAL_GPIO_SERVICE_H__
struct hw_translate;
struct hw_factory;
struct gpio_service {
	struct dc_context *ctx;
	struct hw_translate translate;
	struct hw_factory factory;
	char *busyness[GPIO_ID_COUNT];
};
enum gpio_result dal_gpio_service_open(
	struct gpio *gpio);
void dal_gpio_service_close(
	struct gpio_service *service,
	struct hw_gpio_pin **ptr);
enum gpio_result dal_gpio_service_lock(
	struct gpio_service *service,
	enum gpio_id id,
	uint32_t en);
enum gpio_result dal_gpio_service_unlock(
	struct gpio_service *service,
	enum gpio_id id,
	uint32_t en);
#endif
