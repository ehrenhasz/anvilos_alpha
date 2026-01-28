#ifndef __LPASS_WSA_MACRO_H__
#define __LPASS_WSA_MACRO_H__
enum {
	WSA_MACRO_SPKR_MODE_DEFAULT,
	WSA_MACRO_SPKR_MODE_1,  
};
int wsa_macro_set_spkr_mode(struct snd_soc_component *component, int mode);
#endif  
