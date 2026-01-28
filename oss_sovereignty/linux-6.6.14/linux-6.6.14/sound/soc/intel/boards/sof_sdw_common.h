#ifndef SND_SOC_SOF_SDW_COMMON_H
#define SND_SOC_SOF_SDW_COMMON_H
#include <linux/bits.h>
#include <linux/types.h>
#include <sound/soc.h>
#define MAX_NO_PROPS 2
#define MAX_HDMI_NUM 4
#define SDW_UNUSED_DAI_ID -1
#define SDW_JACK_OUT_DAI_ID 0
#define SDW_JACK_IN_DAI_ID 1
#define SDW_AMP_OUT_DAI_ID 2
#define SDW_AMP_IN_DAI_ID 3
#define SDW_DMIC_DAI_ID 4
#define SDW_MAX_CPU_DAIS 16
#define SDW_INTEL_BIDIR_PDI_BASE 2
#define SDW_MAX_GROUPS 9
enum {
	SOF_PRE_TGL_HDMI_COUNT = 3,
	SOF_TGL_HDMI_COUNT = 4,
};
enum {
	SOF_I2S_SSP0 = BIT(0),
	SOF_I2S_SSP1 = BIT(1),
	SOF_I2S_SSP2 = BIT(2),
	SOF_I2S_SSP3 = BIT(3),
	SOF_I2S_SSP4 = BIT(4),
	SOF_I2S_SSP5 = BIT(5),
};
#define SOF_JACK_JDSRC(quirk)		((quirk) & GENMASK(3, 0))
#define SOF_SDW_FOUR_SPK		BIT(4)
#define SOF_SDW_TGL_HDMI		BIT(5)
#define SOF_SDW_PCH_DMIC		BIT(6)
#define SOF_SSP_PORT(x)		(((x) & GENMASK(5, 0)) << 7)
#define SOF_SSP_GET_PORT(quirk)	(((quirk) >> 7) & GENMASK(5, 0))
#define SOF_SDW_NO_AGGREGATION		BIT(14)
#define SOF_BT_OFFLOAD_SSP_SHIFT	15
#define SOF_BT_OFFLOAD_SSP_MASK	(GENMASK(17, 15))
#define SOF_BT_OFFLOAD_SSP(quirk)	\
	(((quirk) << SOF_BT_OFFLOAD_SSP_SHIFT) & SOF_BT_OFFLOAD_SSP_MASK)
#define SOF_SSP_BT_OFFLOAD_PRESENT	BIT(18)
#define SOF_SDW_DAI_TYPE_JACK		0
#define SOF_SDW_DAI_TYPE_AMP		1
#define SOF_SDW_DAI_TYPE_MIC		2
#define SOF_SDW_MAX_DAI_NUM		3
struct sof_sdw_codec_info;
struct sof_sdw_dai_info {
	const bool direction[2];  
	const char *dai_name;
	const int dai_type;
	const int dailink[2];  
	int  (*init)(struct snd_soc_card *card,
		     const struct snd_soc_acpi_link_adr *link,
		     struct snd_soc_dai_link *dai_links,
		     struct sof_sdw_codec_info *info,
		     bool playback);
	int (*exit)(struct snd_soc_card *card, struct snd_soc_dai_link *dai_link);
};
struct sof_sdw_codec_info {
	const int part_id;
	const int version_id;
	const char *codec_name;
	int amp_num;
	const u8 acpi_id[ACPI_ID_LEN];
	const bool ignore_pch_dmic;
	const struct snd_soc_ops *ops;
	struct sof_sdw_dai_info dais[SOF_SDW_MAX_DAI_NUM];
	const int dai_num;
	int (*codec_card_late_probe)(struct snd_soc_card *card);
};
struct mc_private {
	struct list_head hdmi_pcm_list;
	bool idisp_codec;
	struct snd_soc_jack sdw_headset;
	struct device *headset_codec_dev;  
	struct device *amp_dev1, *amp_dev2;
};
extern unsigned long sof_sdw_quirk;
int sdw_startup(struct snd_pcm_substream *substream);
int sdw_prepare(struct snd_pcm_substream *substream);
int sdw_trigger(struct snd_pcm_substream *substream, int cmd);
int sdw_hw_params(struct snd_pcm_substream *substream,
		  struct snd_pcm_hw_params *params);
int sdw_hw_free(struct snd_pcm_substream *substream);
void sdw_shutdown(struct snd_pcm_substream *substream);
int sof_sdw_hdmi_init(struct snd_soc_pcm_runtime *rtd);
int sof_sdw_hdmi_card_late_probe(struct snd_soc_card *card);
int sof_sdw_dmic_init(struct snd_soc_pcm_runtime *rtd);
int sof_sdw_rt711_init(struct snd_soc_card *card,
		       const struct snd_soc_acpi_link_adr *link,
		       struct snd_soc_dai_link *dai_links,
		       struct sof_sdw_codec_info *info,
		       bool playback);
int sof_sdw_rt711_exit(struct snd_soc_card *card, struct snd_soc_dai_link *dai_link);
int sof_sdw_rt_sdca_jack_init(struct snd_soc_card *card,
			      const struct snd_soc_acpi_link_adr *link,
			      struct snd_soc_dai_link *dai_links,
			      struct sof_sdw_codec_info *info,
			      bool playback);
int sof_sdw_rt_sdca_jack_exit(struct snd_soc_card *card, struct snd_soc_dai_link *dai_link);
int sof_sdw_rt712_sdca_init(struct snd_soc_card *card,
			    const struct snd_soc_acpi_link_adr *link,
			    struct snd_soc_dai_link *dai_links,
			    struct sof_sdw_codec_info *info,
			    bool playback);
int sof_sdw_rt712_sdca_exit(struct snd_soc_card *card, struct snd_soc_dai_link *dai_link);
int sof_sdw_rt712_spk_init(struct snd_soc_card *card,
			   const struct snd_soc_acpi_link_adr *link,
			   struct snd_soc_dai_link *dai_links,
			   struct sof_sdw_codec_info *info,
			   bool playback);
int sof_sdw_rt712_sdca_dmic_init(struct snd_soc_card *card,
				 const struct snd_soc_acpi_link_adr *link,
				 struct snd_soc_dai_link *dai_links,
				 struct sof_sdw_codec_info *info,
				 bool playback);
int sof_sdw_rt700_init(struct snd_soc_card *card,
		       const struct snd_soc_acpi_link_adr *link,
		       struct snd_soc_dai_link *dai_links,
		       struct sof_sdw_codec_info *info,
		       bool playback);
extern struct snd_soc_ops sof_sdw_rt1308_i2s_ops;
int sof_sdw_rt_amp_init(struct snd_soc_card *card,
			const struct snd_soc_acpi_link_adr *link,
			struct snd_soc_dai_link *dai_links,
			struct sof_sdw_codec_info *info,
			bool playback);
int sof_sdw_rt_amp_exit(struct snd_soc_card *card, struct snd_soc_dai_link *dai_link);
int sof_sdw_rt715_init(struct snd_soc_card *card,
		       const struct snd_soc_acpi_link_adr *link,
		       struct snd_soc_dai_link *dai_links,
		       struct sof_sdw_codec_info *info,
		       bool playback);
int sof_sdw_rt715_sdca_init(struct snd_soc_card *card,
			    const struct snd_soc_acpi_link_adr *link,
			    struct snd_soc_dai_link *dai_links,
			    struct sof_sdw_codec_info *info,
			    bool playback);
int sof_sdw_maxim_init(struct snd_soc_card *card,
		       const struct snd_soc_acpi_link_adr *link,
		       struct snd_soc_dai_link *dai_links,
		       struct sof_sdw_codec_info *info,
		       bool playback);
int sof_sdw_rt5682_init(struct snd_soc_card *card,
			const struct snd_soc_acpi_link_adr *link,
			struct snd_soc_dai_link *dai_links,
			struct sof_sdw_codec_info *info,
			bool playback);
int sof_sdw_cs42l42_init(struct snd_soc_card *card,
			 const struct snd_soc_acpi_link_adr *link,
			 struct snd_soc_dai_link *dai_links,
			 struct sof_sdw_codec_info *info,
			 bool playback);
int sof_sdw_cs_amp_init(struct snd_soc_card *card,
			const struct snd_soc_acpi_link_adr *link,
			struct snd_soc_dai_link *dai_links,
			struct sof_sdw_codec_info *info,
			bool playback);
#endif
