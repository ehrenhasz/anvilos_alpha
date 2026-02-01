 

 

#ifndef __MT8186_CLK_H
#define __MT8186_CLK_H

struct snd_sof_dev;

 
enum adsp_clk_id {
	CLK_TOP_AUDIODSP,
	CLK_TOP_ADSP_BUS,
	ADSP_CLK_MAX
};

int mt8186_adsp_init_clock(struct snd_sof_dev *sdev);
int mt8186_adsp_clock_on(struct snd_sof_dev *sdev);
void mt8186_adsp_clock_off(struct snd_sof_dev *sdev);
#endif
