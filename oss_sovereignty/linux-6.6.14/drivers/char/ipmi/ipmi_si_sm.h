 
 

#ifndef __IPMI_SI_SM_H__
#define __IPMI_SI_SM_H__

#include "ipmi_si.h"

 
struct si_sm_data;

 
enum si_sm_result {
	SI_SM_CALL_WITHOUT_DELAY,  
	SI_SM_CALL_WITH_DELAY,	 
	SI_SM_CALL_WITH_TICK_DELAY, 
	SI_SM_TRANSACTION_COMPLETE,  
	SI_SM_IDLE,		 
	SI_SM_HOSED,		 

	 
	SI_SM_ATTN
};

 
struct si_sm_handlers {
	 
	char *version;

	 
	unsigned int (*init_data)(struct si_sm_data *smi,
				  struct si_sm_io   *io);

	 
	int (*start_transaction)(struct si_sm_data *smi,
				 unsigned char *data, unsigned int size);

	 
	int (*get_result)(struct si_sm_data *smi,
			  unsigned char *data, unsigned int length);

	 
	enum si_sm_result (*event)(struct si_sm_data *smi, long time);

	 
	int (*detect)(struct si_sm_data *smi);

	 
	void (*cleanup)(struct si_sm_data *smi);

	 
	int (*size)(void);
};

 
extern const struct si_sm_handlers kcs_smi_handlers;
extern const struct si_sm_handlers smic_smi_handlers;
extern const struct si_sm_handlers bt_smi_handlers;

#endif  
