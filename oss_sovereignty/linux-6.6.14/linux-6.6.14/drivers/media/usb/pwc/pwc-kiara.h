#ifndef PWC_KIARA_H
#define PWC_KIARA_H
#include "pwc.h"
#define PWC_FPS_MAX_KIARA 6
struct Kiara_table_entry
{
	char alternate;			 
	unsigned short packetsize;	 
	unsigned short bandlength;	 
	unsigned char mode[12];		 
};
extern const struct Kiara_table_entry Kiara_table[PSZ_MAX][PWC_FPS_MAX_KIARA][4];
extern const unsigned int KiaraRomTable[8][2][16][8];
extern const unsigned int Kiara_fps_vector[PWC_FPS_MAX_KIARA];
#endif
