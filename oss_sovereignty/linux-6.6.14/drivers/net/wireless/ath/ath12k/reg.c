
 
#include <linux/rtnetlink.h>
#include "core.h"
#include "debug.h"

 
#define ATH12K_2GHZ_CH01_11      REG_RULE(2412 - 10, 2462 + 10, 40, 0, 20, 0)
#define ATH12K_5GHZ_5150_5350    REG_RULE(5150 - 10, 5350 + 10, 80, 0, 30,\
					  NL80211_RRF_NO_IR)
#define ATH12K_5GHZ_5725_5850    REG_RULE(5725 - 10, 5850 + 10, 80, 0, 30,\
					  NL80211_RRF_NO_IR)

#define ETSI_WEATHER_RADAR_BAND_LOW		5590
#define ETSI_WEATHER_RADAR_BAND_HIGH		5650
#define ETSI_WEATHER_RADAR_BAND_CAC_TIMEOUT	600000

static const struct ieee80211_regdomain ath12k_world_regd = {
	.n_reg_rules = 3,
	.alpha2 = "00",
	.reg_rules = {
		ATH12K_2GHZ_CH01_11,
		ATH12K_5GHZ_5150_5350,
		ATH12K_5GHZ_5725_5850,
	}
};

static bool ath12k_regdom_changes(struct ath12k *ar, char *alpha2)
{
	const struct ieee80211_regdomain *regd;

	regd = rcu_dereference_rtnl(ar->hw->wiphy->regd);
	 
	if (!regd)
		return true;

	return memcmp(regd->alpha2, alpha2, 2) != 0;
}

static void
ath12k_reg_notifier(struct wiphy *wiphy, struct regulatory_request *request)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_wmi_init_country_arg arg;
	struct ath12k *ar = hw->priv;
	int ret;

	ath12k_dbg(ar->ab, ATH12K_DBG_REG,
		   "Regulatory Notification received for %s\n", wiphy_name(wiphy));

	 
	if (!(request->initiator == NL80211_REGDOM_SET_BY_USER &&
	      request->user_reg_hint_type == NL80211_USER_REG_HINT_USER)) {
		ath12k_warn(ar->ab, "Unexpected Regulatory event for this wiphy\n");
		return;
	}

	if (!IS_ENABLED(CONFIG_ATH_REG_DYNAMIC_USER_REG_HINTS)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_REG,
			   "Country Setting is not allowed\n");
		return;
	}

	if (!ath12k_regdom_changes(ar, request->alpha2)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_REG, "Country is already set\n");
		return;
	}

	 
	arg.flags = ALPHA_IS_SET;
	memcpy(&arg.cc_info.alpha2, request->alpha2, 2);
	arg.cc_info.alpha2[2] = 0;

	ret = ath12k_wmi_send_init_country_cmd(ar, &arg);
	if (ret)
		ath12k_warn(ar->ab,
			    "INIT Country code set to fw failed : %d\n", ret);
}

int ath12k_reg_update_chan_list(struct ath12k *ar)
{
	struct ieee80211_supported_band **bands;
	struct ath12k_wmi_scan_chan_list_arg *arg;
	struct ieee80211_channel *channel;
	struct ieee80211_hw *hw = ar->hw;
	struct ath12k_wmi_channel_arg *ch;
	enum nl80211_band band;
	int num_channels = 0;
	int i, ret;

	bands = hw->wiphy->bands;
	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		if (!bands[band])
			continue;

		for (i = 0; i < bands[band]->n_channels; i++) {
			if (bands[band]->channels[i].flags &
			    IEEE80211_CHAN_DISABLED)
				continue;

			num_channels++;
		}
	}

	if (WARN_ON(!num_channels))
		return -EINVAL;

	arg = kzalloc(struct_size(arg, channel, num_channels), GFP_KERNEL);

	if (!arg)
		return -ENOMEM;

	arg->pdev_id = ar->pdev->pdev_id;
	arg->nallchans = num_channels;

	ch = arg->channel;

	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		if (!bands[band])
			continue;

		for (i = 0; i < bands[band]->n_channels; i++) {
			channel = &bands[band]->channels[i];

			if (channel->flags & IEEE80211_CHAN_DISABLED)
				continue;

			 
			ch->allow_ht = true;
			ch->allow_vht = true;
			ch->allow_he = true;

			ch->dfs_set =
				!!(channel->flags & IEEE80211_CHAN_RADAR);
			ch->is_chan_passive = !!(channel->flags &
						IEEE80211_CHAN_NO_IR);
			ch->is_chan_passive |= ch->dfs_set;
			ch->mhz = channel->center_freq;
			ch->cfreq1 = channel->center_freq;
			ch->minpower = 0;
			ch->maxpower = channel->max_power * 2;
			ch->maxregpower = channel->max_reg_power * 2;
			ch->antennamax = channel->max_antenna_gain * 2;

			 
			if (channel->band == NL80211_BAND_2GHZ)
				ch->phy_mode = MODE_11G;
			else
				ch->phy_mode = MODE_11A;

			if (channel->band == NL80211_BAND_6GHZ &&
			    cfg80211_channel_is_psc(channel))
				ch->psc_channel = true;

			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "mac channel [%d/%d] freq %d maxpower %d regpower %d antenna %d mode %d\n",
				   i, arg->nallchans,
				   ch->mhz, ch->maxpower, ch->maxregpower,
				   ch->antennamax, ch->phy_mode);

			ch++;
			 
		}
	}

	ret = ath12k_wmi_send_scan_chan_list_cmd(ar, arg);
	kfree(arg);

	return ret;
}

static void ath12k_copy_regd(struct ieee80211_regdomain *regd_orig,
			     struct ieee80211_regdomain *regd_copy)
{
	u8 i;

	 
	memcpy(regd_copy, regd_orig, sizeof(*regd_orig));

	for (i = 0; i < regd_orig->n_reg_rules; i++)
		memcpy(&regd_copy->reg_rules[i], &regd_orig->reg_rules[i],
		       sizeof(struct ieee80211_reg_rule));
}

int ath12k_regd_update(struct ath12k *ar, bool init)
{
	struct ieee80211_regdomain *regd, *regd_copy = NULL;
	int ret, regd_len, pdev_id;
	struct ath12k_base *ab;

	ab = ar->ab;
	pdev_id = ar->pdev_idx;

	spin_lock_bh(&ab->base_lock);

	if (init) {
		 
		if (ab->default_regd[pdev_id]) {
			regd = ab->default_regd[pdev_id];
		} else {
			ath12k_warn(ab,
				    "failed to receive default regd during init\n");
			regd = (struct ieee80211_regdomain *)&ath12k_world_regd;
		}
	} else {
		regd = ab->new_regd[pdev_id];
	}

	if (!regd) {
		ret = -EINVAL;
		spin_unlock_bh(&ab->base_lock);
		goto err;
	}

	regd_len = sizeof(*regd) + (regd->n_reg_rules *
		sizeof(struct ieee80211_reg_rule));

	regd_copy = kzalloc(regd_len, GFP_ATOMIC);
	if (regd_copy)
		ath12k_copy_regd(regd, regd_copy);

	spin_unlock_bh(&ab->base_lock);

	if (!regd_copy) {
		ret = -ENOMEM;
		goto err;
	}

	rtnl_lock();
	wiphy_lock(ar->hw->wiphy);
	ret = regulatory_set_wiphy_regd_sync(ar->hw->wiphy, regd_copy);
	wiphy_unlock(ar->hw->wiphy);
	rtnl_unlock();

	kfree(regd_copy);

	if (ret)
		goto err;

	if (ar->state == ATH12K_STATE_ON) {
		ret = ath12k_reg_update_chan_list(ar);
		if (ret)
			goto err;
	}

	return 0;
err:
	ath12k_warn(ab, "failed to perform regd update : %d\n", ret);
	return ret;
}

static enum nl80211_dfs_regions
ath12k_map_fw_dfs_region(enum ath12k_dfs_region dfs_region)
{
	switch (dfs_region) {
	case ATH12K_DFS_REG_FCC:
	case ATH12K_DFS_REG_CN:
		return NL80211_DFS_FCC;
	case ATH12K_DFS_REG_ETSI:
	case ATH12K_DFS_REG_KR:
		return NL80211_DFS_ETSI;
	case ATH12K_DFS_REG_MKK:
	case ATH12K_DFS_REG_MKK_N:
		return NL80211_DFS_JP;
	default:
		return NL80211_DFS_UNSET;
	}
}

static u32 ath12k_map_fw_reg_flags(u16 reg_flags)
{
	u32 flags = 0;

	if (reg_flags & REGULATORY_CHAN_NO_IR)
		flags = NL80211_RRF_NO_IR;

	if (reg_flags & REGULATORY_CHAN_RADAR)
		flags |= NL80211_RRF_DFS;

	if (reg_flags & REGULATORY_CHAN_NO_OFDM)
		flags |= NL80211_RRF_NO_OFDM;

	if (reg_flags & REGULATORY_CHAN_INDOOR_ONLY)
		flags |= NL80211_RRF_NO_OUTDOOR;

	if (reg_flags & REGULATORY_CHAN_NO_HT40)
		flags |= NL80211_RRF_NO_HT40;

	if (reg_flags & REGULATORY_CHAN_NO_80MHZ)
		flags |= NL80211_RRF_NO_80MHZ;

	if (reg_flags & REGULATORY_CHAN_NO_160MHZ)
		flags |= NL80211_RRF_NO_160MHZ;

	return flags;
}

static bool
ath12k_reg_can_intersect(struct ieee80211_reg_rule *rule1,
			 struct ieee80211_reg_rule *rule2)
{
	u32 start_freq1, end_freq1;
	u32 start_freq2, end_freq2;

	start_freq1 = rule1->freq_range.start_freq_khz;
	start_freq2 = rule2->freq_range.start_freq_khz;

	end_freq1 = rule1->freq_range.end_freq_khz;
	end_freq2 = rule2->freq_range.end_freq_khz;

	if ((start_freq1 >= start_freq2 &&
	     start_freq1 < end_freq2) ||
	    (start_freq2 > start_freq1 &&
	     start_freq2 < end_freq1))
		return true;

	 

	return false;
}

static void ath12k_reg_intersect_rules(struct ieee80211_reg_rule *rule1,
				       struct ieee80211_reg_rule *rule2,
				       struct ieee80211_reg_rule *new_rule)
{
	u32 start_freq1, end_freq1;
	u32 start_freq2, end_freq2;
	u32 freq_diff, max_bw;

	start_freq1 = rule1->freq_range.start_freq_khz;
	start_freq2 = rule2->freq_range.start_freq_khz;

	end_freq1 = rule1->freq_range.end_freq_khz;
	end_freq2 = rule2->freq_range.end_freq_khz;

	new_rule->freq_range.start_freq_khz = max_t(u32, start_freq1,
						    start_freq2);
	new_rule->freq_range.end_freq_khz = min_t(u32, end_freq1, end_freq2);

	freq_diff = new_rule->freq_range.end_freq_khz -
			new_rule->freq_range.start_freq_khz;
	max_bw = min_t(u32, rule1->freq_range.max_bandwidth_khz,
		       rule2->freq_range.max_bandwidth_khz);
	new_rule->freq_range.max_bandwidth_khz = min_t(u32, max_bw, freq_diff);

	new_rule->power_rule.max_antenna_gain =
		min_t(u32, rule1->power_rule.max_antenna_gain,
		      rule2->power_rule.max_antenna_gain);

	new_rule->power_rule.max_eirp = min_t(u32, rule1->power_rule.max_eirp,
					      rule2->power_rule.max_eirp);

	 
	new_rule->flags = rule1->flags | rule2->flags;

	 
	new_rule->dfs_cac_ms = max_t(u32, rule1->dfs_cac_ms,
				     rule2->dfs_cac_ms);
}

static struct ieee80211_regdomain *
ath12k_regd_intersect(struct ieee80211_regdomain *default_regd,
		      struct ieee80211_regdomain *curr_regd)
{
	u8 num_old_regd_rules, num_curr_regd_rules, num_new_regd_rules;
	struct ieee80211_reg_rule *old_rule, *curr_rule, *new_rule;
	struct ieee80211_regdomain *new_regd = NULL;
	u8 i, j, k;

	num_old_regd_rules = default_regd->n_reg_rules;
	num_curr_regd_rules = curr_regd->n_reg_rules;
	num_new_regd_rules = 0;

	 
	for (i = 0; i < num_old_regd_rules; i++) {
		old_rule = default_regd->reg_rules + i;
		for (j = 0; j < num_curr_regd_rules; j++) {
			curr_rule = curr_regd->reg_rules + j;

			if (ath12k_reg_can_intersect(old_rule, curr_rule))
				num_new_regd_rules++;
		}
	}

	if (!num_new_regd_rules)
		return NULL;

	new_regd = kzalloc(sizeof(*new_regd) + (num_new_regd_rules *
			sizeof(struct ieee80211_reg_rule)),
			GFP_ATOMIC);

	if (!new_regd)
		return NULL;

	 
	new_regd->n_reg_rules = num_new_regd_rules;
	memcpy(new_regd->alpha2, curr_regd->alpha2, sizeof(new_regd->alpha2));
	new_regd->dfs_region = curr_regd->dfs_region;
	new_rule = new_regd->reg_rules;

	for (i = 0, k = 0; i < num_old_regd_rules; i++) {
		old_rule = default_regd->reg_rules + i;
		for (j = 0; j < num_curr_regd_rules; j++) {
			curr_rule = curr_regd->reg_rules + j;

			if (ath12k_reg_can_intersect(old_rule, curr_rule))
				ath12k_reg_intersect_rules(old_rule, curr_rule,
							   (new_rule + k++));
		}
	}
	return new_regd;
}

static const char *
ath12k_reg_get_regdom_str(enum nl80211_dfs_regions dfs_region)
{
	switch (dfs_region) {
	case NL80211_DFS_FCC:
		return "FCC";
	case NL80211_DFS_ETSI:
		return "ETSI";
	case NL80211_DFS_JP:
		return "JP";
	default:
		return "UNSET";
	}
}

static u16
ath12k_reg_adjust_bw(u16 start_freq, u16 end_freq, u16 max_bw)
{
	u16 bw;

	bw = end_freq - start_freq;
	bw = min_t(u16, bw, max_bw);

	if (bw >= 80 && bw < 160)
		bw = 80;
	else if (bw >= 40 && bw < 80)
		bw = 40;
	else if (bw < 40)
		bw = 20;

	return bw;
}

static void
ath12k_reg_update_rule(struct ieee80211_reg_rule *reg_rule, u32 start_freq,
		       u32 end_freq, u32 bw, u32 ant_gain, u32 reg_pwr,
		       u32 reg_flags)
{
	reg_rule->freq_range.start_freq_khz = MHZ_TO_KHZ(start_freq);
	reg_rule->freq_range.end_freq_khz = MHZ_TO_KHZ(end_freq);
	reg_rule->freq_range.max_bandwidth_khz = MHZ_TO_KHZ(bw);
	reg_rule->power_rule.max_antenna_gain = DBI_TO_MBI(ant_gain);
	reg_rule->power_rule.max_eirp = DBM_TO_MBM(reg_pwr);
	reg_rule->flags = reg_flags;
}

static void
ath12k_reg_update_weather_radar_band(struct ath12k_base *ab,
				     struct ieee80211_regdomain *regd,
				     struct ath12k_reg_rule *reg_rule,
				     u8 *rule_idx, u32 flags, u16 max_bw)
{
	u32 end_freq;
	u16 bw;
	u8 i;

	i = *rule_idx;

	bw = ath12k_reg_adjust_bw(reg_rule->start_freq,
				  ETSI_WEATHER_RADAR_BAND_LOW, max_bw);

	ath12k_reg_update_rule(regd->reg_rules + i, reg_rule->start_freq,
			       ETSI_WEATHER_RADAR_BAND_LOW, bw,
			       reg_rule->ant_gain, reg_rule->reg_power,
			       flags);

	ath12k_dbg(ab, ATH12K_DBG_REG,
		   "\t%d. (%d - %d @ %d) (%d, %d) (%d ms) (FLAGS %d)\n",
		   i + 1, reg_rule->start_freq, ETSI_WEATHER_RADAR_BAND_LOW,
		   bw, reg_rule->ant_gain, reg_rule->reg_power,
		   regd->reg_rules[i].dfs_cac_ms,
		   flags);

	if (reg_rule->end_freq > ETSI_WEATHER_RADAR_BAND_HIGH)
		end_freq = ETSI_WEATHER_RADAR_BAND_HIGH;
	else
		end_freq = reg_rule->end_freq;

	bw = ath12k_reg_adjust_bw(ETSI_WEATHER_RADAR_BAND_LOW, end_freq,
				  max_bw);

	i++;

	ath12k_reg_update_rule(regd->reg_rules + i,
			       ETSI_WEATHER_RADAR_BAND_LOW, end_freq, bw,
			       reg_rule->ant_gain, reg_rule->reg_power,
			       flags);

	regd->reg_rules[i].dfs_cac_ms = ETSI_WEATHER_RADAR_BAND_CAC_TIMEOUT;

	ath12k_dbg(ab, ATH12K_DBG_REG,
		   "\t%d. (%d - %d @ %d) (%d, %d) (%d ms) (FLAGS %d)\n",
		   i + 1, ETSI_WEATHER_RADAR_BAND_LOW, end_freq,
		   bw, reg_rule->ant_gain, reg_rule->reg_power,
		   regd->reg_rules[i].dfs_cac_ms,
		   flags);

	if (end_freq == reg_rule->end_freq) {
		regd->n_reg_rules--;
		*rule_idx = i;
		return;
	}

	bw = ath12k_reg_adjust_bw(ETSI_WEATHER_RADAR_BAND_HIGH,
				  reg_rule->end_freq, max_bw);

	i++;

	ath12k_reg_update_rule(regd->reg_rules + i, ETSI_WEATHER_RADAR_BAND_HIGH,
			       reg_rule->end_freq, bw,
			       reg_rule->ant_gain, reg_rule->reg_power,
			       flags);

	ath12k_dbg(ab, ATH12K_DBG_REG,
		   "\t%d. (%d - %d @ %d) (%d, %d) (%d ms) (FLAGS %d)\n",
		   i + 1, ETSI_WEATHER_RADAR_BAND_HIGH, reg_rule->end_freq,
		   bw, reg_rule->ant_gain, reg_rule->reg_power,
		   regd->reg_rules[i].dfs_cac_ms,
		   flags);

	*rule_idx = i;
}

struct ieee80211_regdomain *
ath12k_reg_build_regd(struct ath12k_base *ab,
		      struct ath12k_reg_info *reg_info, bool intersect)
{
	struct ieee80211_regdomain *tmp_regd, *default_regd, *new_regd = NULL;
	struct ath12k_reg_rule *reg_rule;
	u8 i = 0, j = 0, k = 0;
	u8 num_rules;
	u16 max_bw;
	u32 flags;
	char alpha2[3];

	num_rules = reg_info->num_5g_reg_rules + reg_info->num_2g_reg_rules;

	 
	if (reg_info->is_ext_reg_event)
		num_rules += reg_info->num_6g_reg_rules_ap[WMI_REG_INDOOR_AP];

	if (!num_rules)
		goto ret;

	 
	if (reg_info->dfs_region == ATH12K_DFS_REG_ETSI)
		num_rules += 2;

	tmp_regd = kzalloc(sizeof(*tmp_regd) +
			   (num_rules * sizeof(struct ieee80211_reg_rule)),
			   GFP_ATOMIC);
	if (!tmp_regd)
		goto ret;

	memcpy(tmp_regd->alpha2, reg_info->alpha2, REG_ALPHA2_LEN + 1);
	memcpy(alpha2, reg_info->alpha2, REG_ALPHA2_LEN + 1);
	alpha2[2] = '\0';
	tmp_regd->dfs_region = ath12k_map_fw_dfs_region(reg_info->dfs_region);

	ath12k_dbg(ab, ATH12K_DBG_REG,
		   "\r\nCountry %s, CFG Regdomain %s FW Regdomain %d, num_reg_rules %d\n",
		   alpha2, ath12k_reg_get_regdom_str(tmp_regd->dfs_region),
		   reg_info->dfs_region, num_rules);
	 
	for (; i < num_rules; i++) {
		if (reg_info->num_2g_reg_rules &&
		    (i < reg_info->num_2g_reg_rules)) {
			reg_rule = reg_info->reg_rules_2g_ptr + i;
			max_bw = min_t(u16, reg_rule->max_bw,
				       reg_info->max_bw_2g);
			flags = 0;
		} else if (reg_info->num_5g_reg_rules &&
			   (j < reg_info->num_5g_reg_rules)) {
			reg_rule = reg_info->reg_rules_5g_ptr + j++;
			max_bw = min_t(u16, reg_rule->max_bw,
				       reg_info->max_bw_5g);

			 
			flags = NL80211_RRF_AUTO_BW;
		} else if (reg_info->is_ext_reg_event &&
			   reg_info->num_6g_reg_rules_ap[WMI_REG_INDOOR_AP] &&
			(k < reg_info->num_6g_reg_rules_ap[WMI_REG_INDOOR_AP])) {
			reg_rule = reg_info->reg_rules_6g_ap_ptr[WMI_REG_INDOOR_AP] + k++;
			max_bw = min_t(u16, reg_rule->max_bw,
				       reg_info->max_bw_6g_ap[WMI_REG_INDOOR_AP]);
			flags = NL80211_RRF_AUTO_BW;
		} else {
			break;
		}

		flags |= ath12k_map_fw_reg_flags(reg_rule->flags);

		ath12k_reg_update_rule(tmp_regd->reg_rules + i,
				       reg_rule->start_freq,
				       reg_rule->end_freq, max_bw,
				       reg_rule->ant_gain, reg_rule->reg_power,
				       flags);

		 
		if (flags & NL80211_RRF_DFS &&
		    reg_info->dfs_region == ATH12K_DFS_REG_ETSI &&
		    (reg_rule->end_freq > ETSI_WEATHER_RADAR_BAND_LOW &&
		    reg_rule->start_freq < ETSI_WEATHER_RADAR_BAND_HIGH)){
			ath12k_reg_update_weather_radar_band(ab, tmp_regd,
							     reg_rule, &i,
							     flags, max_bw);
			continue;
		}

		if (reg_info->is_ext_reg_event) {
			ath12k_dbg(ab, ATH12K_DBG_REG, "\t%d. (%d - %d @ %d) (%d, %d) (%d ms) (FLAGS %d) (%d, %d)\n",
				   i + 1, reg_rule->start_freq, reg_rule->end_freq,
				   max_bw, reg_rule->ant_gain, reg_rule->reg_power,
				   tmp_regd->reg_rules[i].dfs_cac_ms,
				   flags, reg_rule->psd_flag, reg_rule->psd_eirp);
		} else {
			ath12k_dbg(ab, ATH12K_DBG_REG,
				   "\t%d. (%d - %d @ %d) (%d, %d) (%d ms) (FLAGS %d)\n",
				   i + 1, reg_rule->start_freq, reg_rule->end_freq,
				   max_bw, reg_rule->ant_gain, reg_rule->reg_power,
				   tmp_regd->reg_rules[i].dfs_cac_ms,
				   flags);
		}
	}

	tmp_regd->n_reg_rules = i;

	if (intersect) {
		default_regd = ab->default_regd[reg_info->phy_id];

		 
		new_regd = ath12k_regd_intersect(default_regd, tmp_regd);
		kfree(tmp_regd);
		if (!new_regd) {
			ath12k_warn(ab, "Unable to create intersected regdomain\n");
			goto ret;
		}
	} else {
		new_regd = tmp_regd;
	}

ret:
	return new_regd;
}

void ath12k_regd_update_work(struct work_struct *work)
{
	struct ath12k *ar = container_of(work, struct ath12k,
					 regd_update_work);
	int ret;

	ret = ath12k_regd_update(ar, false);
	if (ret) {
		 
		 
	}
}

void ath12k_reg_init(struct ath12k *ar)
{
	ar->hw->wiphy->regulatory_flags = REGULATORY_WIPHY_SELF_MANAGED;
	ar->hw->wiphy->reg_notifier = ath12k_reg_notifier;
}

void ath12k_reg_free(struct ath12k_base *ab)
{
	int i;

	for (i = 0; i < ab->hw_params->max_radios; i++) {
		kfree(ab->default_regd[i]);
		kfree(ab->new_regd[i]);
	}
}
