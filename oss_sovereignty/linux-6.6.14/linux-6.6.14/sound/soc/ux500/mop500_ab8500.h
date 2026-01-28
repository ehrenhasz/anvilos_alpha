#ifndef MOP500_AB8500_H
#define MOP500_AB8500_H
extern const struct snd_soc_ops mop500_ab8500_ops[];
int mop500_ab8500_machine_init(struct snd_soc_pcm_runtime *rtd);
void mop500_ab8500_remove(struct snd_soc_card *card);
#endif
