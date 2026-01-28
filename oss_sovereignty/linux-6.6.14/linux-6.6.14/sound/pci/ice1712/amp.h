#ifndef __SOUND_AMP_H
#define __SOUND_AMP_H
#define  AMP_AUDIO2000_DEVICE_DESC 	       "{AMP Ltd,AUDIO2000},"\
					       "{Chaintech,AV-710},"
#if 0
#define VT1724_SUBDEVICE_AUDIO2000	0x12142417	 
#else
#define VT1724_SUBDEVICE_AUDIO2000	0x00030003	 
#endif
#define VT1724_SUBDEVICE_AV710		0x12142417	 
#define WM_DEV		0x36
#define WM_ATTEN_L	0x00
#define WM_ATTEN_R	0x01
#define WM_DAC_CTRL	0x02
#define WM_INT_CTRL	0x03
extern struct snd_ice1712_card_info  snd_vt1724_amp_cards[];
#endif  
