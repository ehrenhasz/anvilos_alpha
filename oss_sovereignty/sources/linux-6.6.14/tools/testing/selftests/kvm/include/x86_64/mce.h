


#ifndef SELFTEST_KVM_MCE_H
#define SELFTEST_KVM_MCE_H

#define MCG_CTL_P		BIT_ULL(8)   
#define MCG_SER_P		BIT_ULL(24)  
#define MCG_LMCE_P		BIT_ULL(27)  
#define MCG_CMCI_P		BIT_ULL(10)  
#define KVM_MAX_MCE_BANKS 32
#define MCG_CAP_BANKS_MASK 0xff       
#define MCI_STATUS_VAL (1ULL << 63)   
#define MCI_STATUS_UC (1ULL << 61)    
#define MCI_STATUS_EN (1ULL << 60)    
#define MCI_STATUS_MISCV (1ULL << 59) 
#define MCI_STATUS_ADDRV (1ULL << 58) 
#define MCM_ADDR_PHYS 2    
#define MCI_CTL2_CMCI_EN		BIT_ULL(30)

#endif 
