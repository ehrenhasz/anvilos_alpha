#ifndef _ASM_CSS_CHARS_H
#define _ASM_CSS_CHARS_H
#include <linux/types.h>
struct css_general_char {
	u64 : 12;
	u64 dynio : 1;	  
	u64 : 4;
	u64 eadm : 1;	  
	u64 : 23;
	u64 aif : 1;	  
	u64 : 3;
	u64 mcss : 1;	  
	u64 fcs : 1;	  
	u64 : 1;
	u64 ext_mb : 1;   
	u64 : 7;
	u64 aif_tdd : 1;  
	u64 : 1;
	u64 qebsm : 1;	  
	u64 : 2;
	u64 aiv : 1;	  
	u64 : 2;
	u64 : 3;
	u64 aif_osa : 1;  
	u64 : 12;
	u64 eadm_rf : 1;  
	u64 : 1;
	u64 cib : 1;	  
	u64 : 5;
	u64 fcx : 1;	  
	u64 : 19;
	u64 alt_ssi : 1;  
	u64 : 1;
	u64 narf : 1;	  
	u64 : 5;
	u64 enarf: 1;	  
	u64 : 6;
	u64 util_str : 1; 
} __packed;
extern struct css_general_char css_general_characteristics;
#endif
