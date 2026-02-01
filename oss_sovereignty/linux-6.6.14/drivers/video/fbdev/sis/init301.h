 
 
 

#ifndef  _INIT301_H_
#define  _INIT301_H_

#include "initdef.h"

#include "vgatypes.h"
#include "vstruct.h"
#ifdef SIS_CP
#undef SIS_CP
#endif
#include <linux/types.h>
#include <asm/io.h>
#include <linux/fb.h>
#include "sis.h"
#include <video/sisfb.h>

void		SiS_UnLockCRT2(struct SiS_Private *SiS_Pr);
void		SiS_EnableCRT2(struct SiS_Private *SiS_Pr);
unsigned short	SiS_GetRatePtr(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex);
void		SiS_WaitRetrace1(struct SiS_Private *SiS_Pr);
bool		SiS_IsDualEdge(struct SiS_Private *SiS_Pr);
bool		SiS_IsVAMode(struct SiS_Private *SiS_Pr);
void		SiS_GetVBInfo(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
			unsigned short ModeIdIndex, int checkcrt2mode);
void		SiS_SetYPbPr(struct SiS_Private *SiS_Pr);
void    	SiS_SetTVMode(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
			unsigned short ModeIdIndex);
void		SiS_GetLCDResInfo(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
		unsigned short ModeIdIndex);
unsigned short	SiS_GetVCLK2Ptr(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex,
			unsigned short RefreshRateTableIndex);
unsigned short	SiS_GetResInfo(struct SiS_Private *SiS_Pr,unsigned short ModeNo,unsigned short ModeIdIndex);
void		SiS_DisableBridge(struct SiS_Private *SiS_Pr);
bool		SiS_SetCRT2Group(struct SiS_Private *SiS_Pr, unsigned short ModeNo);
void		SiS_SiS30xBLOn(struct SiS_Private *SiS_Pr);
void		SiS_SiS30xBLOff(struct SiS_Private *SiS_Pr);

void		SiS_SetCH700x(struct SiS_Private *SiS_Pr, unsigned short reg, unsigned char val);
unsigned short	SiS_GetCH700x(struct SiS_Private *SiS_Pr, unsigned short tempax);
void		SiS_SetCH701x(struct SiS_Private *SiS_Pr, unsigned short reg, unsigned char val);
unsigned short	SiS_GetCH701x(struct SiS_Private *SiS_Pr, unsigned short tempax);
void		SiS_SetCH70xxANDOR(struct SiS_Private *SiS_Pr, unsigned short reg,
			unsigned char orval,unsigned short andval);
#ifdef CONFIG_FB_SIS_315
void		SiS_Chrontel701xBLOn(struct SiS_Private *SiS_Pr);
void		SiS_Chrontel701xBLOff(struct SiS_Private *SiS_Pr);
#endif  

#ifdef CONFIG_FB_SIS_300
void		SiS_SetChrontelGPIO(struct SiS_Private *SiS_Pr, unsigned short myvbinfo);
#endif

void		SiS_DDC2Delay(struct SiS_Private *SiS_Pr, unsigned int delaytime);
unsigned short	SiS_ReadDDC1Bit(struct SiS_Private *SiS_Pr);
unsigned short	SiS_HandleDDC(struct SiS_Private *SiS_Pr, unsigned int VBFlags, int VGAEngine,
			unsigned short adaptnum, unsigned short DDCdatatype,
			unsigned char *buffer, unsigned int VBFlags2);

extern void		SiS_DisplayOff(struct SiS_Private *SiS_Pr);
extern void		SiS_DisplayOn(struct SiS_Private *SiS_Pr);
extern bool		SiS_SearchModeID(struct SiS_Private *, unsigned short *, unsigned short *);
extern unsigned short	SiS_GetModeFlag(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
				unsigned short ModeIdIndex);
extern unsigned short	SiS_GetModePtr(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex);
extern unsigned short	SiS_GetColorDepth(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex);
extern unsigned short	SiS_GetOffset(struct SiS_Private *SiS_Pr, unsigned short ModeNo, unsigned short ModeIdIndex,
				unsigned short RefreshRateTableIndex);
extern void		SiS_LoadDAC(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
				unsigned short ModeIdIndex);
extern void		SiS_CalcLCDACRT1Timing(struct SiS_Private *SiS_Pr, unsigned short ModeNo,
				unsigned short ModeIdIndex);
extern void		SiS_CalcCRRegisters(struct SiS_Private *SiS_Pr, int depth);
extern unsigned short	SiS_GetRefCRTVCLK(struct SiS_Private *SiS_Pr, unsigned short Index, int UseWide);
extern unsigned short	SiS_GetRefCRT1CRTC(struct SiS_Private *SiS_Pr, unsigned short Index, int UseWide);
#ifdef CONFIG_FB_SIS_300
extern void		SiS_GetFIFOThresholdIndex300(struct SiS_Private *SiS_Pr, unsigned short *tempbx,
				unsigned short *tempcl);
extern unsigned short	SiS_GetFIFOThresholdB300(unsigned short tempbx, unsigned short tempcl);
extern unsigned short	SiS_GetLatencyFactor630(struct SiS_Private *SiS_Pr, unsigned short index);
extern unsigned int	sisfb_read_nbridge_pci_dword(struct SiS_Private *SiS_Pr, int reg);
extern unsigned int	sisfb_read_lpc_pci_dword(struct SiS_Private *SiS_Pr, int reg);
#endif

#endif
