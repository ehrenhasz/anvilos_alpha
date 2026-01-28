#ifndef __IA_CSS_MIPI_H
#define __IA_CSS_MIPI_H
#include <type_support.h>
#include "ia_css_err.h"
#include "ia_css_stream_format.h"
#include "ia_css_input_port.h"
int
ia_css_mipi_frame_enable_check_on_size(const enum mipi_port_id port,
				       const unsigned int	size_mem_words);
int
ia_css_mipi_frame_calculate_size(const unsigned int width,
				 const unsigned int height,
				 const enum atomisp_input_format format,
				 const bool hasSOLandEOL,
				 const unsigned int embedded_data_size_words,
				 unsigned int *size_mem_words);
#endif  
