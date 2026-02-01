 
 

#define RDI_DEV_ENTRY(rdi)   __string(dev, rvt_get_ibdev_name(rdi))
#define RDI_DEV_ASSIGN(rdi)  __assign_str(dev, rvt_get_ibdev_name(rdi))

#include "trace_rvt.h"
#include "trace_qp.h"
#include "trace_tx.h"
#include "trace_mr.h"
#include "trace_cq.h"
#include "trace_rc.h"
