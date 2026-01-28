#ifndef __WIL_PLATFORM_H__
#define __WIL_PLATFORM_H__
struct device;
enum wil_platform_event {
	WIL_PLATFORM_EVT_FW_CRASH = 0,
	WIL_PLATFORM_EVT_PRE_RESET = 1,
	WIL_PLATFORM_EVT_FW_RDY = 2,
	WIL_PLATFORM_EVT_PRE_SUSPEND = 3,
	WIL_PLATFORM_EVT_POST_SUSPEND = 4,
};
enum wil_platform_features {
	WIL_PLATFORM_FEATURE_FW_EXT_CLK_CONTROL = 0,
	WIL_PLATFORM_FEATURE_TRIPLE_MSI = 1,
	WIL_PLATFORM_FEATURE_MAX,
};
enum wil_platform_capa {
	WIL_PLATFORM_CAPA_RADIO_ON_IN_SUSPEND = 0,
	WIL_PLATFORM_CAPA_T_PWR_ON_0 = 1,
	WIL_PLATFORM_CAPA_EXT_CLK = 2,
	WIL_PLATFORM_CAPA_MAX,
};
struct wil_platform_ops {
	int (*bus_request)(void *handle, uint32_t kbps  );
	int (*suspend)(void *handle, bool keep_device_power);
	int (*resume)(void *handle, bool device_powered_on);
	void (*uninit)(void *handle);
	int (*notify)(void *handle, enum wil_platform_event evt);
	int (*get_capa)(void *handle);
	void (*set_features)(void *handle, int features);
};
struct wil_platform_rops {
	int (*ramdump)(void *wil_handle, void *buf, uint32_t size);
	int (*fw_recovery)(void *wil_handle);
};
void *wil_platform_init(struct device *dev, struct wil_platform_ops *ops,
			const struct wil_platform_rops *rops, void *wil_handle);
int __init wil_platform_modinit(void);
void wil_platform_modexit(void);
#endif  
