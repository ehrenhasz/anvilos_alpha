#ifndef __VMEM_LOCAL_H_INCLUDED__
#define __VMEM_LOCAL_H_INCLUDED__
#include "type_support.h"
#include "vmem_global.h"
typedef u16 t_vmem_elem;
#define VMEM_ARRAY(x, s)    t_vmem_elem x[s / ISP_NWAY][ISP_NWAY]
void isp_vmem_load(
    const isp_ID_t		ID,
    const t_vmem_elem	*from,
    t_vmem_elem		*to,
    unsigned int elems);  
void isp_vmem_store(
    const isp_ID_t		ID,
    t_vmem_elem		*to,
    const t_vmem_elem	*from,
    unsigned int elems);  
void isp_vmem_2d_load(
    const isp_ID_t		ID,
    const t_vmem_elem	*from,
    t_vmem_elem		*to,
    unsigned int height,
    unsigned int width,
    unsigned int stride_to,   
    unsigned		stride_from  );
void isp_vmem_2d_store(
    const isp_ID_t		ID,
    t_vmem_elem		*to,
    const t_vmem_elem	*from,
    unsigned int height,
    unsigned int width,
    unsigned int stride_to,   
    unsigned		stride_from  );
#endif  
