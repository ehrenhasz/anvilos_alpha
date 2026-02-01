 
 

#ifndef DRM_TEGRA_DP_H
#define DRM_TEGRA_DP_H 1

#include <linux/types.h>

struct drm_display_info;
struct drm_display_mode;
struct drm_dp_aux;
struct drm_dp_link;

 
struct drm_dp_link_caps {
	 
	bool enhanced_framing;

	 
	bool tps3_supported;

	 
	bool fast_training;

	 
	bool channel_coding;

	 
	bool alternate_scrambler_reset;
};

void drm_dp_link_caps_copy(struct drm_dp_link_caps *dest,
			   const struct drm_dp_link_caps *src);

 
struct drm_dp_link_ops {
	 
	int (*apply_training)(struct drm_dp_link *link);

	 
	int (*configure)(struct drm_dp_link *link);
};

#define DP_TRAIN_VOLTAGE_SWING_LEVEL(x) ((x) << 0)
#define DP_TRAIN_PRE_EMPHASIS_LEVEL(x) ((x) << 3)
#define DP_LANE_POST_CURSOR(i, x) (((x) & 0x3) << (((i) & 1) << 2))

 
struct drm_dp_link_train_set {
	unsigned int voltage_swing[4];
	unsigned int pre_emphasis[4];
	unsigned int post_cursor[4];
};

 
struct drm_dp_link_train {
	struct drm_dp_link_train_set request;
	struct drm_dp_link_train_set adjust;

	unsigned int pattern;

	bool clock_recovered;
	bool channel_equalized;
};

 
struct drm_dp_link {
	unsigned char revision;
	unsigned int max_rate;
	unsigned int max_lanes;

	struct drm_dp_link_caps caps;

	 
	struct {
		unsigned int cr;
		unsigned int ce;
	} aux_rd_interval;

	unsigned char edp;

	unsigned int rate;
	unsigned int lanes;

	unsigned long rates[DP_MAX_SUPPORTED_RATES];
	unsigned int num_rates;

	 
	const struct drm_dp_link_ops *ops;

	 
	struct drm_dp_aux *aux;

	 
	struct drm_dp_link_train train;
};

int drm_dp_link_add_rate(struct drm_dp_link *link, unsigned long rate);
int drm_dp_link_remove_rate(struct drm_dp_link *link, unsigned long rate);
void drm_dp_link_update_rates(struct drm_dp_link *link);

int drm_dp_link_probe(struct drm_dp_aux *aux, struct drm_dp_link *link);
int drm_dp_link_power_up(struct drm_dp_aux *aux, struct drm_dp_link *link);
int drm_dp_link_power_down(struct drm_dp_aux *aux, struct drm_dp_link *link);
int drm_dp_link_configure(struct drm_dp_aux *aux, struct drm_dp_link *link);
int drm_dp_link_choose(struct drm_dp_link *link,
		       const struct drm_display_mode *mode,
		       const struct drm_display_info *info);

void drm_dp_link_train_init(struct drm_dp_link_train *train);
int drm_dp_link_train(struct drm_dp_link *link);

#endif
