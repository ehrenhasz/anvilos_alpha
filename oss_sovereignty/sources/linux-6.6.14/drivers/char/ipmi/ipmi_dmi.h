

#include "ipmi_si.h"

#ifdef CONFIG_IPMI_DMI_DECODE
int ipmi_dmi_get_slave_addr(enum si_type si_type, unsigned int space,
			    unsigned long base_addr);
#endif
