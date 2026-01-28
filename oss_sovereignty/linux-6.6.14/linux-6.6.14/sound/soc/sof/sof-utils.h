#ifndef __SOC_SOF_UTILS_H
#define __SOC_SOF_UTILS_H
struct snd_dma_buffer;
struct device;
int snd_sof_create_page_table(struct device *dev,
			      struct snd_dma_buffer *dmab,
			      unsigned char *page_table, size_t size);
#endif
