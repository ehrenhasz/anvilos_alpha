#ifndef HRT_GDC_v2_defs_h_
#define HRT_GDC_v2_defs_h_
#define HRT_GDC_IS_V2
#define HRT_GDC_N                     1024  
#define HRT_GDC_FRAC_BITS               10  
#define HRT_GDC_BLI_FRAC_BITS            4  
#define HRT_GDC_BLI_COEF_ONE             BIT(HRT_GDC_BLI_FRAC_BITS)
#define HRT_GDC_BCI_COEF_BITS           14  
#define HRT_GDC_BCI_COEF_ONE             (1 << (HRT_GDC_BCI_COEF_BITS - 2))   
#define HRT_GDC_BCI_COEF_MASK            ((1 << HRT_GDC_BCI_COEF_BITS) - 1)
#define HRT_GDC_LUT_BYTES                (HRT_GDC_N * 4 * 2)                 
#define _HRT_GDC_REG_ALIGN               4
#define HRT_GDC_CONFIG_CMD             1
#define HRT_GDC_DATA_CMD               0
#define HRT_GDC_CMD_POS               31
#define HRT_GDC_CMD_BITS               1
#define HRT_GDC_CRUN_POS              30
#define HRT_GDC_REG_ID_POS            25
#define HRT_GDC_REG_ID_BITS            5
#define HRT_GDC_DATA_POS               0
#define HRT_GDC_DATA_BITS             25
#define HRT_GDC_FRYIPXFRX_BITS        26
#define HRT_GDC_P0X_BITS              23
#define HRT_GDC_MAX_OXDIM           (8192 - 64)
#define HRT_GDC_MAX_OYDIM           4095
#define HRT_GDC_MAX_IXDIM           (8192 - 64)
#define HRT_GDC_MAX_IYDIM           4095
#define HRT_GDC_MAX_DS_FAC            16
#define HRT_GDC_MAX_DX                 (HRT_GDC_MAX_DS_FAC * HRT_GDC_N - 1)
#define HRT_GDC_MAX_DY                 HRT_GDC_MAX_DX
#define HRT_GDC_PERF_1_1_pix          0
#define HRT_GDC_PERF_2_1_pix          1
#define HRT_GDC_PERF_1_2_pix          2
#define HRT_GDC_PERF_2_2_pix          3
#define HRT_GDC_NND_MODE              0
#define HRT_GDC_BLI_MODE              1
#define HRT_GDC_BCI_MODE              2
#define HRT_GDC_LUT_MODE              3
#define HRT_GDC_SCAN_STB              0
#define HRT_GDC_SCAN_STR              1
#define HRT_GDC_MODE_SCALING          0
#define HRT_GDC_MODE_TETRAGON         1
#define HRT_GDC_LUT_COEFF_OFFSET     16
#define HRT_GDC_FRY_BIT_OFFSET       16
#define HRT_GDC_CE_FSM0_POS           0
#define HRT_GDC_CE_FSM0_LEN           2
#define HRT_GDC_CE_OPY_POS            2
#define HRT_GDC_CE_OPY_LEN           14
#define HRT_GDC_CE_OPX_POS           16
#define HRT_GDC_CE_OPX_LEN           16
#define HRT_GDC_CHK_ENGINE_IDX        0
#define HRT_GDC_WOIX_IDX              1
#define HRT_GDC_WOIY_IDX              2
#define HRT_GDC_BPP_IDX               3
#define HRT_GDC_FRYIPXFRX_IDX         4
#define HRT_GDC_OXDIM_IDX             5
#define HRT_GDC_OYDIM_IDX             6
#define HRT_GDC_SRC_ADDR_IDX          7
#define HRT_GDC_SRC_END_ADDR_IDX      8
#define HRT_GDC_SRC_WRAP_ADDR_IDX     9
#define HRT_GDC_SRC_STRIDE_IDX       10
#define HRT_GDC_DST_ADDR_IDX         11
#define HRT_GDC_DST_STRIDE_IDX       12
#define HRT_GDC_DX_IDX               13
#define HRT_GDC_DY_IDX               14
#define HRT_GDC_P0X_IDX              15
#define HRT_GDC_P0Y_IDX              16
#define HRT_GDC_P1X_IDX              17
#define HRT_GDC_P1Y_IDX              18
#define HRT_GDC_P2X_IDX              19
#define HRT_GDC_P2Y_IDX              20
#define HRT_GDC_P3X_IDX              21
#define HRT_GDC_P3Y_IDX              22
#define HRT_GDC_PERF_POINT_IDX       23   
#define HRT_GDC_INTERP_TYPE_IDX      24   
#define HRT_GDC_SCAN_IDX             25   
#define HRT_GDC_PROC_MODE_IDX        26   
#define HRT_GDC_LUT_IDX              32
#endif  
