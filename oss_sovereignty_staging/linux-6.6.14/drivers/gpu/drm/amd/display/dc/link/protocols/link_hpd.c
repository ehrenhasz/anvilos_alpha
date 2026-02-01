 

 
#include "link_hpd.h"
#include "gpio_service_interface.h"

bool link_get_hpd_state(struct dc_link *link)
{
	uint32_t state;

	dal_gpio_lock_pin(link->hpd_gpio);
	dal_gpio_get_value(link->hpd_gpio, &state);
	dal_gpio_unlock_pin(link->hpd_gpio);

	return state;
}

void link_enable_hpd(const struct dc_link *link)
{
	struct link_encoder *encoder = link->link_enc;

	if (encoder != NULL && encoder->funcs->enable_hpd != NULL)
		encoder->funcs->enable_hpd(encoder);
}

void link_disable_hpd(const struct dc_link *link)
{
	struct link_encoder *encoder = link->link_enc;

	if (encoder != NULL && encoder->funcs->enable_hpd != NULL)
		encoder->funcs->disable_hpd(encoder);
}

void link_enable_hpd_filter(struct dc_link *link, bool enable)
{
	struct gpio *hpd;

	if (enable) {
		link->is_hpd_filter_disabled = false;
		program_hpd_filter(link);
	} else {
		link->is_hpd_filter_disabled = true;
		 
		hpd = link_get_hpd_gpio(link->ctx->dc_bios, link->link_id, link->ctx->gpio_service);

		if (!hpd)
			return;

		 
		if (dal_gpio_open(hpd, GPIO_MODE_INTERRUPT) == GPIO_RESULT_OK) {
			struct gpio_hpd_config config;

			config.delay_on_connect = 0;
			config.delay_on_disconnect = 0;

			dal_irq_setup_hpd_filter(hpd, &config);

			dal_gpio_close(hpd);
		} else {
			ASSERT_CRITICAL(false);
		}
		 
		dal_gpio_destroy_irq(&hpd);
	}
}

struct gpio *link_get_hpd_gpio(struct dc_bios *dcb,
			  struct graphics_object_id link_id,
			  struct gpio_service *gpio_service)
{
	enum bp_result bp_result;
	struct graphics_object_hpd_info hpd_info;
	struct gpio_pin_info pin_info;

	if (dcb->funcs->get_hpd_info(dcb, link_id, &hpd_info) != BP_RESULT_OK)
		return NULL;

	bp_result = dcb->funcs->get_gpio_pin_info(dcb,
		hpd_info.hpd_int_gpio_uid, &pin_info);

	if (bp_result != BP_RESULT_OK) {
		ASSERT(bp_result == BP_RESULT_NORECORD);
		return NULL;
	}

	return dal_gpio_service_create_irq(gpio_service,
					   pin_info.offset,
					   pin_info.mask);
}

bool query_hpd_status(struct dc_link *link, uint32_t *is_hpd_high)
{
	struct gpio *hpd_pin = link_get_hpd_gpio(
			link->ctx->dc_bios, link->link_id,
			link->ctx->gpio_service);
	if (!hpd_pin)
		return false;

	dal_gpio_open(hpd_pin, GPIO_MODE_INTERRUPT);
	dal_gpio_get_value(hpd_pin, is_hpd_high);
	dal_gpio_close(hpd_pin);
	dal_gpio_destroy_irq(&hpd_pin);
	return true;
}

enum hpd_source_id get_hpd_line(struct dc_link *link)
{
	struct gpio *hpd;
	enum hpd_source_id hpd_id;

		hpd_id = HPD_SOURCEID_UNKNOWN;

	hpd = link_get_hpd_gpio(link->ctx->dc_bios, link->link_id,
			   link->ctx->gpio_service);

	if (hpd) {
		switch (dal_irq_get_source(hpd)) {
		case DC_IRQ_SOURCE_HPD1:
			hpd_id = HPD_SOURCEID1;
		break;
		case DC_IRQ_SOURCE_HPD2:
			hpd_id = HPD_SOURCEID2;
		break;
		case DC_IRQ_SOURCE_HPD3:
			hpd_id = HPD_SOURCEID3;
		break;
		case DC_IRQ_SOURCE_HPD4:
			hpd_id = HPD_SOURCEID4;
		break;
		case DC_IRQ_SOURCE_HPD5:
			hpd_id = HPD_SOURCEID5;
		break;
		case DC_IRQ_SOURCE_HPD6:
			hpd_id = HPD_SOURCEID6;
		break;
		default:
			BREAK_TO_DEBUGGER();
		break;
		}

		dal_gpio_destroy_irq(&hpd);
	}

	return hpd_id;
}

bool program_hpd_filter(const struct dc_link *link)
{
	bool result = false;
	struct gpio *hpd;
	int delay_on_connect_in_ms = 0;
	int delay_on_disconnect_in_ms = 0;

	if (link->is_hpd_filter_disabled)
		return false;
	 
	switch (link->connector_signal) {
	case SIGNAL_TYPE_DVI_SINGLE_LINK:
	case SIGNAL_TYPE_DVI_DUAL_LINK:
	case SIGNAL_TYPE_HDMI_TYPE_A:
		 
		delay_on_connect_in_ms = 500;
		delay_on_disconnect_in_ms = 100;
		break;
	case SIGNAL_TYPE_DISPLAY_PORT:
	case SIGNAL_TYPE_DISPLAY_PORT_MST:
		 
		 
		delay_on_connect_in_ms = 80;
		delay_on_disconnect_in_ms = 0;
		break;
	case SIGNAL_TYPE_LVDS:
	case SIGNAL_TYPE_EDP:
	default:
		 
		return false;
	}

	 
	hpd = link_get_hpd_gpio(link->ctx->dc_bios, link->link_id,
			   link->ctx->gpio_service);

	if (!hpd)
		return result;

	 
	if (dal_gpio_open(hpd, GPIO_MODE_INTERRUPT) == GPIO_RESULT_OK) {
		struct gpio_hpd_config config;

		config.delay_on_connect = delay_on_connect_in_ms;
		config.delay_on_disconnect = delay_on_disconnect_in_ms;

		dal_irq_setup_hpd_filter(hpd, &config);

		dal_gpio_close(hpd);

		result = true;
	} else {
		ASSERT_CRITICAL(false);
	}

	 
	dal_gpio_destroy_irq(&hpd);

	return result;
}
