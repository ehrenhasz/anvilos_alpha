#ifndef _isp2400_support_h
#define _isp2400_support_h
#ifndef ISP2400_VECTOR_TYPES
typedef char *tmemvectors, *tmemvectoru, *tvector;
#endif
#define hrt_isp_vamem1_store_16(cell, addr, val) hrt_mem_store_16(cell, HRT_PROC_TYPE_PROP(cell, _simd_vamem1), addr, val)
#define hrt_isp_vamem2_store_16(cell, addr, val) hrt_mem_store_16(cell, HRT_PROC_TYPE_PROP(cell, _simd_vamem2), addr, val)
#define hrt_isp_dmem(cell) HRT_PROC_TYPE_PROP(cell, _base_dmem)
#define hrt_isp_vmem(cell) HRT_PROC_TYPE_PROP(cell, _simd_vmem)
#define hrt_isp_dmem_master_port_address(cell) hrt_mem_master_port_address(cell, hrt_isp_dmem(cell))
#define hrt_isp_vmem_master_port_address(cell) hrt_mem_master_port_address(cell, hrt_isp_vmem(cell))
#if ISP_HAS_HIST
#define hrt_isp_hist(cell) HRT_PROC_TYPE_PROP(cell, _simd_histogram)
#define hrt_isp_hist_master_port_address(cell) hrt_mem_master_port_address(cell, hrt_isp_hist(cell))
#endif
#endif  
