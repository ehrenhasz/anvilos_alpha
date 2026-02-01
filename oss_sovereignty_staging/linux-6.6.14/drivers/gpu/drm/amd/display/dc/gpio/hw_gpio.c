 

#include "dm_services.h"
#include "include/gpio_types.h"
#include "hw_gpio.h"

#include "reg_helper.h"
#include "gpio_regs.h"

#undef FN
#define FN(reg_name, field_name) \
	gpio->regs->field_name ## _shift, gpio->regs->field_name ## _mask

#define CTX \
	gpio->base.ctx
#define REG(reg)\
	(gpio->regs->reg)

static void store_registers(
	struct hw_gpio *gpio)
{
	REG_GET(MASK_reg, MASK, &gpio->store.mask);
	REG_GET(A_reg, A, &gpio->store.a);
	REG_GET(EN_reg, EN, &gpio->store.en);
	 
}

static void restore_registers(
	struct hw_gpio *gpio)
{
	REG_UPDATE(MASK_reg, MASK, gpio->store.mask);
	REG_UPDATE(A_reg, A, gpio->store.a);
	REG_UPDATE(EN_reg, EN, gpio->store.en);
	 
}

bool dal_hw_gpio_open(
	struct hw_gpio_pin *ptr,
	enum gpio_mode mode)
{
	struct hw_gpio *pin = FROM_HW_GPIO_PIN(ptr);

	store_registers(pin);

	ptr->opened = (dal_hw_gpio_config_mode(pin, mode) == GPIO_RESULT_OK);

	return ptr->opened;
}

enum gpio_result dal_hw_gpio_get_value(
	const struct hw_gpio_pin *ptr,
	uint32_t *value)
{
	const struct hw_gpio *gpio = FROM_HW_GPIO_PIN(ptr);

	enum gpio_result result = GPIO_RESULT_OK;

	switch (ptr->mode) {
	case GPIO_MODE_INPUT:
	case GPIO_MODE_OUTPUT:
	case GPIO_MODE_HARDWARE:
	case GPIO_MODE_FAST_OUTPUT:
		REG_GET(Y_reg, Y, value);
		break;
	default:
		result = GPIO_RESULT_NON_SPECIFIC_ERROR;
	}

	return result;
}

enum gpio_result dal_hw_gpio_set_value(
	const struct hw_gpio_pin *ptr,
	uint32_t value)
{
	struct hw_gpio *gpio = FROM_HW_GPIO_PIN(ptr);

	 

	switch (ptr->mode) {
	case GPIO_MODE_OUTPUT:
		REG_UPDATE(A_reg, A, value);
		return GPIO_RESULT_OK;
	case GPIO_MODE_FAST_OUTPUT:
		 
		REG_UPDATE(EN_reg, EN, ~value);
		return GPIO_RESULT_OK;
	default:
		return GPIO_RESULT_NON_SPECIFIC_ERROR;
	}
}

enum gpio_result dal_hw_gpio_change_mode(
	struct hw_gpio_pin *ptr,
	enum gpio_mode mode)
{
	struct hw_gpio *pin = FROM_HW_GPIO_PIN(ptr);

	return dal_hw_gpio_config_mode(pin, mode);
}

void dal_hw_gpio_close(
	struct hw_gpio_pin *ptr)
{
	struct hw_gpio *pin = FROM_HW_GPIO_PIN(ptr);

	restore_registers(pin);

	ptr->mode = GPIO_MODE_UNKNOWN;
	ptr->opened = false;
}

enum gpio_result dal_hw_gpio_config_mode(
	struct hw_gpio *gpio,
	enum gpio_mode mode)
{
	gpio->base.mode = mode;

	switch (mode) {
	case GPIO_MODE_INPUT:
		 
		REG_UPDATE(EN_reg, EN, 0);
		REG_UPDATE(MASK_reg, MASK, 1);
		return GPIO_RESULT_OK;
	case GPIO_MODE_OUTPUT:
		 
		REG_UPDATE(A_reg, A, 0);
		REG_UPDATE(MASK_reg, MASK, 1);
		return GPIO_RESULT_OK;
	case GPIO_MODE_FAST_OUTPUT:
		 
		REG_UPDATE(A_reg, A, 0);
		REG_UPDATE(MASK_reg, MASK, 1);
		return GPIO_RESULT_OK;
	case GPIO_MODE_HARDWARE:
		 
		REG_UPDATE(MASK_reg, MASK, 0);
		return GPIO_RESULT_OK;
	case GPIO_MODE_INTERRUPT:
		 
		REG_UPDATE(MASK_reg, MASK, 0);
		return GPIO_RESULT_OK;
	default:
		return GPIO_RESULT_NON_SPECIFIC_ERROR;
	}
}

void dal_hw_gpio_construct(
	struct hw_gpio *pin,
	enum gpio_id id,
	uint32_t en,
	struct dc_context *ctx)
{
	pin->base.ctx = ctx;
	pin->base.id = id;
	pin->base.en = en;
	pin->base.mode = GPIO_MODE_UNKNOWN;
	pin->base.opened = false;

	pin->store.mask = 0;
	pin->store.a = 0;
	pin->store.en = 0;
	pin->store.mux = 0;

	pin->mux_supported = false;
}

void dal_hw_gpio_destruct(
	struct hw_gpio *pin)
{
	ASSERT(!pin->base.opened);
}
