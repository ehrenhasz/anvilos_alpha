#ifndef _hive_isp_css_streaming_to_mipi_types_hrt_h_
#define _hive_isp_css_streaming_to_mipi_types_hrt_h_
#include <streaming_to_mipi_defs.h>
#define _HIVE_ISP_CH_ID_MASK    ((1U << HIVE_ISP_CH_ID_BITS) - 1)
#define _HIVE_ISP_FMT_TYPE_MASK ((1U << HIVE_ISP_FMT_TYPE_BITS) - 1)
#define _HIVE_STR_TO_MIPI_FMT_TYPE_LSB (HIVE_STR_TO_MIPI_CH_ID_LSB + HIVE_ISP_CH_ID_BITS)
#define _HIVE_STR_TO_MIPI_DATA_B_LSB   (HIVE_STR_TO_MIPI_DATA_A_LSB + HIVE_IF_PIXEL_WIDTH)
#endif  
