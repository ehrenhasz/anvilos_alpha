




#include "dice.h"

int snd_dice_detect_harman_formats(struct snd_dice *dice)
{
	int i;

	
	
	
	
	for (i = 0; i < 2; ++i) {
		dice->tx_pcm_chs[0][i] = 12;
		dice->tx_midi_ports[0] = 1;
		dice->rx_pcm_chs[0][i] = 10;
		dice->rx_midi_ports[0] = 1;
	}

	return 0;
}
