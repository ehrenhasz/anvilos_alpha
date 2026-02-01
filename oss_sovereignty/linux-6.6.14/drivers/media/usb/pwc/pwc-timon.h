 
 



 

#ifndef PWC_TIMON_H
#define PWC_TIMON_H

#include "pwc.h"

#define PWC_FPS_MAX_TIMON 6

struct Timon_table_entry
{
	char alternate;			 
	unsigned short packetsize;	 
	unsigned short bandlength;	 
	unsigned char mode[13];		 
};

extern const struct Timon_table_entry Timon_table[PSZ_MAX][PWC_FPS_MAX_TIMON][4];
extern const unsigned int TimonRomTable [16][2][16][8];
extern const unsigned int Timon_fps_vector[PWC_FPS_MAX_TIMON];

#endif


