 
 

#ifndef __ODMEDCATURBOCHECK_H__
#define __ODMEDCATURBOCHECK_H__

struct edca_t {  
	bool bCurrentTurboEDCA;
	bool bIsCurRDLState;

	u32 prv_traffic_idx;  
};

void odm_EdcaTurboCheck(void *pDM_VOID);
void ODM_EdcaTurboInit(void *pDM_VOID);

void odm_EdcaTurboCheckCE(void *pDM_VOID);

#endif
