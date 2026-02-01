 
 

#ifndef HIFACE_PCM_H
#define HIFACE_PCM_H

struct hiface_chip;

int hiface_pcm_init(struct hiface_chip *chip, u8 extra_freq);
void hiface_pcm_abort(struct hiface_chip *chip);
#endif  
