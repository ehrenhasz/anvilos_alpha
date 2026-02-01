 
#ifndef __IRQSRCS_GFX_11_0_0_H__
#define __IRQSRCS_GFX_11_0_0_H__


#define GFX_11_0_0__SRCID__UTCL2_FAULT                          0       
#define GFX_11_0_0__SRCID__UTCL2_DATA_POISONING                 1       

#define GFX_11_0_0__SRCID__MEM_ACCES_MON		                10		

#define GFX_11_0_0__SRCID__SDMA_ATOMIC_RTN_DONE                 48      
#define GFX_11_0_0__SRCID__SDMA_TRAP                            49      
#define GFX_11_0_0__SRCID__SDMA_SRBMWRITE                       50      
#define GFX_11_0_0__SRCID__SDMA_CTXEMPTY                        51      
#define GFX_11_0_0__SRCID__SDMA_PREEMPT                         52      
#define GFX_11_0_0__SRCID__SDMA_IB_PREEMPT                      53      
#define GFX_11_0_0__SRCID__SDMA_DOORBELL_INVALID                54      
#define GFX_11_0_0__SRCID__SDMA_QUEUE_HANG                      55      
#define GFX_11_0_0__SRCID__SDMA_ATOMIC_TIMEOUT                  56      
#define GFX_11_0_0__SRCID__SDMA_POLL_TIMEOUT                    57      
#define GFX_11_0_0__SRCID__SDMA_PAGE_TIMEOUT                    58      
#define GFX_11_0_0__SRCID__SDMA_PAGE_NULL                       59      
#define GFX_11_0_0__SRCID__SDMA_PAGE_FAULT                      60      
#define GFX_11_0_0__SRCID__SDMA_VM_HOLE                         61      
#define GFX_11_0_0__SRCID__SDMA_ECC                             62      
#define GFX_11_0_0__SRCID__SDMA_FROZEN                          63      
#define GFX_11_0_0__SRCID__SDMA_SRAM_ECC                        64      
#define GFX_11_0_0__SRCID__SDMA_SEM_INCOMPLETE_TIMEOUT          65      
#define GFX_11_0_0__SRCID__SDMA_SEM_WAIT_FAIL_TIMEOUT           66      

#define GFX_11_0_0__SRCID__RLC_GC_FED_INTERRUPT                 128     

#define GFX_11_0_0__SRCID__CP_GENERIC_INT				        177		
#define GFX_11_0_0__SRCID__CP_PM4_PKT_RSVD_BIT_ERROR		    180		
#define GFX_11_0_0__SRCID__CP_EOP_INTERRUPT					    181		
#define GFX_11_0_0__SRCID__CP_BAD_OPCODE_ERROR				    183		
#define GFX_11_0_0__SRCID__CP_PRIV_REG_FAULT				    184		
#define GFX_11_0_0__SRCID__CP_PRIV_INSTR_FAULT				    185		
#define GFX_11_0_0__SRCID__CP_WAIT_MEM_SEM_FAULT			    186		
#define GFX_11_0_0__SRCID__CP_CTX_EMPTY_INTERRUPT			    187		
#define GFX_11_0_0__SRCID__CP_CTX_BUSY_INTERRUPT			    188		
#define GFX_11_0_0__SRCID__CP_ME_WAIT_REG_MEM_POLL_TIMEOUT	    192		
#define GFX_11_0_0__SRCID__CP_SIG_INCOMPLETE				    193		
#define GFX_11_0_0__SRCID__CP_PREEMPT_ACK					    194		
#define GFX_11_0_0__SRCID__CP_GPF					            195		
#define GFX_11_0_0__SRCID__CP_GDS_ALLOC_ERROR				    196		
#define GFX_11_0_0__SRCID__CP_ECC_ERROR					        197		
#define GFX_11_0_0__SRCID__CP_COMPUTE_QUERY_STATUS              199     
#define GFX_11_0_0__SRCID__CP_VM_DOORBELL					    200		
#define GFX_11_0_0__SRCID__CP_FUE_ERROR					        201		
#define GFX_11_0_0__SRCID__RLC_STRM_PERF_MONITOR_INTERRUPT	    202		
#define GFX_11_0_0__SRCID__GRBM_RD_TIMEOUT_ERROR			    232		
#define GFX_11_0_0__SRCID__GRBM_REG_GUI_IDLE				    233		

#define GFX_11_0_0__SRCID__SQ_INTERRUPT_ID					    239		


#endif
