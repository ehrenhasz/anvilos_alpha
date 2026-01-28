#ifndef WFX_MAIN_H
#define WFX_MAIN_H
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include "hif_api_general.h"
struct wfx_dev;
struct wfx_hwbus_ops;
struct wfx_platform_data {
	const char *file_fw;
	const char *file_pds;
	struct gpio_desc *gpio_wakeup;
	bool use_rising_clk;
};
struct wfx_dev *wfx_init_common(struct device *dev, const struct wfx_platform_data *pdata,
				const struct wfx_hwbus_ops *hwbus_ops, void *hwbus_priv);
int wfx_probe(struct wfx_dev *wdev);
void wfx_release(struct wfx_dev *wdev);
bool wfx_api_older_than(struct wfx_dev *wdev, int major, int minor);
int wfx_send_pds(struct wfx_dev *wdev, u8 *buf, size_t len);
#endif
