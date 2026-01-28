#ifndef _MTK_SOUNDCARD_DRIVER_H_
#define _MTK_SOUNDCARD_DRIVER_H_
int parse_dai_link_info(struct snd_soc_card *card);
void clean_card_reference(struct snd_soc_card *card);
#endif
