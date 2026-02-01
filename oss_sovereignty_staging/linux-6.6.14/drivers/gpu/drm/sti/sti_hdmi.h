 
 

#ifndef _STI_HDMI_H_
#define _STI_HDMI_H_

#include <linux/hdmi.h>
#include <linux/platform_device.h>

#include <media/cec-notifier.h>

#include <drm/drm_modes.h>
#include <drm/drm_property.h>

#define HDMI_STA           0x0010
#define HDMI_STA_DLL_LCK   BIT(5)
#define HDMI_STA_HOT_PLUG  BIT(4)

struct sti_hdmi;

struct hdmi_phy_ops {
	bool (*start)(struct sti_hdmi *hdmi);
	void (*stop)(struct sti_hdmi *hdmi);
};

struct hdmi_audio_params {
	bool enabled;
	unsigned int sample_width;
	unsigned int sample_rate;
	struct hdmi_audio_infoframe cea;
};

#define DEFAULT_COLORSPACE_MODE HDMI_COLORSPACE_RGB

 
struct sti_hdmi {
	struct device dev;
	struct drm_device *drm_dev;
	struct drm_display_mode mode;
	void __iomem *regs;
	void __iomem *syscfg;
	struct clk *clk_pix;
	struct clk *clk_tmds;
	struct clk *clk_phy;
	struct clk *clk_audio;
	int irq;
	u32 irq_status;
	struct hdmi_phy_ops *phy_ops;
	bool enabled;
	bool hpd;
	wait_queue_head_t wait_event;
	bool event_received;
	struct reset_control *reset;
	struct i2c_adapter *ddc_adapt;
	enum hdmi_colorspace colorspace;
	struct platform_device *audio_pdev;
	struct hdmi_audio_params audio;
	struct drm_connector *drm_connector;
	struct cec_notifier *notifier;
};

u32 hdmi_read(struct sti_hdmi *hdmi, int offset);
void hdmi_write(struct sti_hdmi *hdmi, u32 val, int offset);

 
struct hdmi_phy_config {
	u32 min_tmds_freq;
	u32 max_tmds_freq;
	u32 config[4];
};

#endif
