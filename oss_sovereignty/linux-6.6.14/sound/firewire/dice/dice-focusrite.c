




#include "dice.h"

int snd_dice_detect_focusrite_pro40_tcd3070_formats(struct snd_dice *dice)
{
	
	
	dice->tx_pcm_chs[0][0] = 20;
	dice->tx_midi_ports[0] = 1;
	dice->rx_pcm_chs[0][0] = 20;
	dice->rx_midi_ports[0] = 1;

	dice->tx_pcm_chs[0][1] = 16;
	dice->tx_midi_ports[1] = 1;
	dice->rx_pcm_chs[0][1] = 16;
	dice->rx_midi_ports[1] = 1;

	return 0;
}
