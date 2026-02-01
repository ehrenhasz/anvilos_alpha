
 

#include "ia_css_mipi.h"
#include "sh_css_mipi.h"
#include <type_support.h>
#include "system_global.h"
#include "ia_css_err.h"
#include "ia_css_pipe.h"
#include "ia_css_stream_format.h"
#include "sh_css_stream_format.h"
#include "ia_css_stream_public.h"
#include "ia_css_frame_public.h"
#include "ia_css_input_port.h"
#include "ia_css_debug.h"
#include "sh_css_struct.h"
#include "sh_css_defs.h"
#include "sh_css_sp.h"  
#include "sw_event_global.h"  

static u32
ref_count_mipi_allocation[N_CSI_PORTS];  

 
int
ia_css_mipi_frame_calculate_size(const unsigned int width,
				 const unsigned int height,
				 const enum atomisp_input_format format,
				 const bool hasSOLandEOL,
				 const unsigned int embedded_data_size_words,
				 unsigned int *size_mem_words)
{
	int err = 0;

	unsigned int bits_per_pixel = 0;
	unsigned int even_line_bytes = 0;
	unsigned int odd_line_bytes = 0;
	unsigned int words_per_odd_line = 0;
	unsigned int words_for_first_line = 0;
	unsigned int words_per_even_line = 0;
	unsigned int mem_words_per_even_line = 0;
	unsigned int mem_words_per_odd_line = 0;
	unsigned int mem_words_for_first_line = 0;
	unsigned int mem_words_for_EOF = 0;
	unsigned int mem_words = 0;
	unsigned int width_padded = width;

	 
	if (IS_ISP2401)
		width_padded += (2 * ISP_VEC_NELEMS);

	IA_CSS_ENTER("padded_width=%d, height=%d, format=%d, hasSOLandEOL=%d, embedded_data_size_words=%d\n",
		     width_padded, height, format, hasSOLandEOL, embedded_data_size_words);

	switch (format) {
	case ATOMISP_INPUT_FORMAT_RAW_6:		 
		bits_per_pixel = 6;
		break;
	case ATOMISP_INPUT_FORMAT_RAW_7:		 
		bits_per_pixel = 7;
		break;
	case ATOMISP_INPUT_FORMAT_RAW_8:		 
	case ATOMISP_INPUT_FORMAT_BINARY_8:		 
	case ATOMISP_INPUT_FORMAT_YUV420_8:		 
		bits_per_pixel = 8;
		break;
	case ATOMISP_INPUT_FORMAT_YUV420_10:		 
	case ATOMISP_INPUT_FORMAT_RAW_10:		 
		 
		bits_per_pixel = 10;
		break;
	case ATOMISP_INPUT_FORMAT_YUV420_8_LEGACY:	 
	case ATOMISP_INPUT_FORMAT_RAW_12:		 
		bits_per_pixel = 12;
		break;
	case ATOMISP_INPUT_FORMAT_RAW_14:		 
		bits_per_pixel = 14;
		break;
	case ATOMISP_INPUT_FORMAT_RGB_444:		 
	case ATOMISP_INPUT_FORMAT_RGB_555:		 
	case ATOMISP_INPUT_FORMAT_RGB_565:		 
	case ATOMISP_INPUT_FORMAT_YUV422_8:		 
		bits_per_pixel = 16;
		break;
	case ATOMISP_INPUT_FORMAT_RGB_666:		 
		bits_per_pixel = 18;
		break;
	case ATOMISP_INPUT_FORMAT_YUV422_10:		 
		bits_per_pixel = 20;
		break;
	case ATOMISP_INPUT_FORMAT_RGB_888:		 
		bits_per_pixel = 24;
		break;

	case ATOMISP_INPUT_FORMAT_YUV420_16:		 
	case ATOMISP_INPUT_FORMAT_YUV422_16:		 
	case ATOMISP_INPUT_FORMAT_RAW_16:		 
	default:
		return -EINVAL;
	}

	odd_line_bytes = (width_padded * bits_per_pixel + 7) >> 3;  

	 
	if (format == ATOMISP_INPUT_FORMAT_YUV420_8
	    || format == ATOMISP_INPUT_FORMAT_YUV420_10
	    || format == ATOMISP_INPUT_FORMAT_YUV420_16) {
		even_line_bytes = (width_padded * 2 * bits_per_pixel + 7) >>
			3;  
	} else {
		even_line_bytes = odd_line_bytes;
	}

	 

	words_per_odd_line = (odd_line_bytes + 3) >> 2;
	 
	words_per_even_line  = (even_line_bytes  + 3) >> 2;
	words_for_first_line = words_per_odd_line + 2 + (hasSOLandEOL ? 1 : 0);
	 
	words_per_odd_line	+= (1 + (hasSOLandEOL ? 2 : 0));
	 
	words_per_even_line += (1 + (hasSOLandEOL ? 2 : 0));

	mem_words_per_odd_line	 = (words_per_odd_line + 7) >> 3;
	 
	mem_words_for_first_line = (words_for_first_line + 7) >> 3;
	mem_words_per_even_line  = (words_per_even_line + 7) >> 3;
	mem_words_for_EOF        = 1;  

	mem_words = ((embedded_data_size_words + 7) >> 3) +
	mem_words_for_first_line +
	(((height + 1) >> 1) - 1) * mem_words_per_odd_line +
	 
	(height      >> 1) * mem_words_per_even_line +  
	mem_words_for_EOF;

	*size_mem_words = mem_words;  
	 

	IA_CSS_LEAVE_ERR(err);
	return err;
}

 

#if !defined(ISP2401)
int
ia_css_mipi_frame_enable_check_on_size(const enum mipi_port_id port,
				       const unsigned int	size_mem_words)
{
	u32 idx;

	int err = -EBUSY;

	OP___assert(port < N_CSI_PORTS);
	OP___assert(size_mem_words != 0);

	for (idx = 0; idx < IA_CSS_MIPI_SIZE_CHECK_MAX_NOF_ENTRIES_PER_PORT &&
	     my_css.mipi_sizes_for_check[port][idx] != 0;
	     idx++) {  
	}
	if (idx < IA_CSS_MIPI_SIZE_CHECK_MAX_NOF_ENTRIES_PER_PORT) {
		my_css.mipi_sizes_for_check[port][idx] = size_mem_words;
		err = 0;
	}

	return err;
}
#endif

void
mipi_init(void)
{
	unsigned int i;

	for (i = 0; i < N_CSI_PORTS; i++)
		ref_count_mipi_allocation[i] = 0;
}

bool mipi_is_free(void)
{
	unsigned int i;

	for (i = 0; i < N_CSI_PORTS; i++)
		if (ref_count_mipi_allocation[i])
			return false;

	return true;
}

 
static int calculate_mipi_buff_size(struct ia_css_stream_config *stream_cfg,
				    unsigned int *size_mem_words)
{
	unsigned int width;
	unsigned int height;
	enum atomisp_input_format format;
	bool pack_raw_pixels;

	unsigned int width_padded;
	unsigned int bits_per_pixel = 0;

	unsigned int even_line_bytes = 0;
	unsigned int odd_line_bytes = 0;

	unsigned int words_per_odd_line = 0;
	unsigned int words_per_even_line = 0;

	unsigned int mem_words_per_even_line = 0;
	unsigned int mem_words_per_odd_line = 0;

	unsigned int mem_words_per_buff_line = 0;
	unsigned int mem_words_per_buff = 0;
	int err = 0;

	 
	width = stream_cfg->input_config.input_res.width;
	height = stream_cfg->input_config.input_res.height;
	format = stream_cfg->input_config.format;
	pack_raw_pixels = stream_cfg->pack_raw_pixels;
	 

	 
	 
	width_padded = width + (2 * ISP_VEC_NELEMS);
	 

	IA_CSS_ENTER("padded_width=%d, height=%d, format=%d\n",
		     width_padded, height, format);

	bits_per_pixel = sh_css_stream_format_2_bits_per_subpixel(format);
	bits_per_pixel =
	(format == ATOMISP_INPUT_FORMAT_RAW_10 && pack_raw_pixels) ? bits_per_pixel : 16;
	if (bits_per_pixel == 0)
		return -EINVAL;

	odd_line_bytes = (width_padded * bits_per_pixel + 7) >> 3;  

	 
	if (format == ATOMISP_INPUT_FORMAT_YUV420_8
	    || format == ATOMISP_INPUT_FORMAT_YUV420_10) {
		even_line_bytes = (width_padded * 2 * bits_per_pixel + 7) >>
			3;  
	} else {
		even_line_bytes = odd_line_bytes;
	}

	words_per_odd_line	 = (odd_line_bytes   + 3) >> 2;
	 
	words_per_even_line  = (even_line_bytes  + 3) >> 2;

	mem_words_per_odd_line	 = (words_per_odd_line + 7) >> 3;
	 
	mem_words_per_even_line  = (words_per_even_line + 7) >> 3;

	mem_words_per_buff_line =
	(mem_words_per_odd_line > mem_words_per_even_line) ? mem_words_per_odd_line : mem_words_per_even_line;
	mem_words_per_buff = mem_words_per_buff_line * height;

	*size_mem_words = mem_words_per_buff;

	IA_CSS_LEAVE_ERR(err);
	return err;
}

int
allocate_mipi_frames(struct ia_css_pipe *pipe,
		     struct ia_css_stream_info *info)
{
	int err = -EINVAL;
	unsigned int port;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
			    "allocate_mipi_frames(%p) enter:\n", pipe);

	if (IS_ISP2401 && pipe->stream->config.online) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
				    "allocate_mipi_frames(%p) exit: no buffers needed for 2401 pipe mode.\n",
				    pipe);
		return 0;
	}

	if (pipe->stream->config.mode != IA_CSS_INPUT_MODE_BUFFERED_SENSOR) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
				    "allocate_mipi_frames(%p) exit: no buffers needed for pipe mode.\n",
				    pipe);
		return 0;  
	}

	port = (unsigned int)pipe->stream->config.source.port.port;
	if (port >= N_CSI_PORTS) {
		IA_CSS_ERROR("allocate_mipi_frames(%p) exit: port is not correct (port=%d).",
			     pipe, port);
		return -EINVAL;
	}

	if (IS_ISP2401)
		err = calculate_mipi_buff_size(&pipe->stream->config,
					       &my_css.mipi_frame_size[port]);

	 
	if (ref_count_mipi_allocation[port] != 0) {
		if (IS_ISP2401)
			ref_count_mipi_allocation[port]++;

		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
				    "allocate_mipi_frames(%p) leave: nothing to do, already allocated for this port (port=%d).\n",
				    pipe, port);
		return 0;
	}

	ref_count_mipi_allocation[port]++;

	 
	my_css.num_mipi_frames[port] = NUM_MIPI_FRAMES_PER_STREAM;

	 
	{  
		unsigned int i, j;

		for (i = 0; i < my_css.num_mipi_frames[port]; i++) {
			 
			if (my_css.mipi_frames[port][i]) {
				ia_css_frame_free(my_css.mipi_frames[port][i]);
				my_css.mipi_frames[port][i] = NULL;
			}
			 
			if (i < my_css.num_mipi_frames[port]) {
				 
				err = ia_css_frame_allocate_with_buffer_size(
					  &my_css.mipi_frames[port][i],
					  my_css.mipi_frame_size[port] * HIVE_ISP_DDR_WORD_BYTES);
				if (err) {
					for (j = 0; j < i; j++) {
						if (my_css.mipi_frames[port][j]) {
							ia_css_frame_free(my_css.mipi_frames[port][j]);
							my_css.mipi_frames[port][j] = NULL;
						}
					}
					IA_CSS_ERROR("allocate_mipi_frames(%p, %d) exit: allocation failed.",
						     pipe, port);
					return err;
				}
			}
			if (info->metadata_info.size > 0) {
				 
				if (my_css.mipi_metadata[port][i]) {
					ia_css_metadata_free(my_css.mipi_metadata[port][i]);
					my_css.mipi_metadata[port][i] = NULL;
				}
				 
				if (i < my_css.num_mipi_frames[port]) {
					 
					my_css.mipi_metadata[port][i] = ia_css_metadata_allocate(&info->metadata_info);
					if (!my_css.mipi_metadata[port][i]) {
						ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
								    "allocate_mipi_metadata(%p, %d) failed.\n",
								    pipe, port);
						return err;
					}
				}
			}
		}
	}
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
			    "allocate_mipi_frames(%p) exit:\n", pipe);

	return err;
}

int
free_mipi_frames(struct ia_css_pipe *pipe)
{
	int err = -EINVAL;
	unsigned int port;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
			    "free_mipi_frames(%p) enter:\n", pipe);

	 
	if (pipe) {
		assert(pipe->stream);
		if ((!pipe) || (!pipe->stream)) {
			IA_CSS_ERROR("free_mipi_frames(%p) exit: pipe or stream is null.",
				     pipe);
			return -EINVAL;
		}

		if (pipe->stream->config.mode != IA_CSS_INPUT_MODE_BUFFERED_SENSOR) {
			IA_CSS_ERROR("free_mipi_frames(%p) exit: wrong mode.",
				     pipe);
			return err;
		}

		port = (unsigned int)pipe->stream->config.source.port.port;

		if (port >= N_CSI_PORTS) {
			IA_CSS_ERROR("free_mipi_frames(%p, %d) exit: pipe port is not correct.",
				     pipe, port);
			return err;
		}

		if (ref_count_mipi_allocation[port] > 0) {
			if (!IS_ISP2401) {
				assert(ref_count_mipi_allocation[port] == 1);
				if (ref_count_mipi_allocation[port] != 1) {
					IA_CSS_ERROR("free_mipi_frames(%p) exit: wrong ref_count (ref_count=%d).",
						     pipe, ref_count_mipi_allocation[port]);
					return err;
				}
			}

			ref_count_mipi_allocation[port]--;

			if (ref_count_mipi_allocation[port] == 0) {
				 
				unsigned int i;

				for (i = 0; i < my_css.num_mipi_frames[port]; i++) {
					if (my_css.mipi_frames[port][i]) {
						ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
								    "free_mipi_frames(port=%d, num=%d).\n", port, i);
						ia_css_frame_free(my_css.mipi_frames[port][i]);
						my_css.mipi_frames[port][i] = NULL;
					}
					if (my_css.mipi_metadata[port][i]) {
						ia_css_metadata_free(my_css.mipi_metadata[port][i]);
						my_css.mipi_metadata[port][i] = NULL;
					}
				}

				ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
						    "free_mipi_frames(%p) exit (deallocated).\n", pipe);
			}
		}
	} else {  
		 
		for (port = CSI_PORT0_ID; port < N_CSI_PORTS; port++) {
			unsigned int i;

			for (i = 0; i < my_css.num_mipi_frames[port]; i++) {
				if (my_css.mipi_frames[port][i]) {
					ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
							    "free_mipi_frames(port=%d, num=%d).\n", port, i);
					ia_css_frame_free(my_css.mipi_frames[port][i]);
					my_css.mipi_frames[port][i] = NULL;
				}
				if (my_css.mipi_metadata[port][i]) {
					ia_css_metadata_free(my_css.mipi_metadata[port][i]);
					my_css.mipi_metadata[port][i] = NULL;
				}
			}
			ref_count_mipi_allocation[port] = 0;
		}
	}
	return 0;
}

int
send_mipi_frames(struct ia_css_pipe *pipe)
{
	int err = -EINVAL;
	unsigned int i;
	unsigned int port;

	IA_CSS_ENTER_PRIVATE("pipe=%p", pipe);

	 
	 
	if (pipe->stream->config.mode != IA_CSS_INPUT_MODE_BUFFERED_SENSOR) {
		IA_CSS_LOG("nothing to be done for this mode");
		return 0;
		 
	}

	port = (unsigned int)pipe->stream->config.source.port.port;

	if (port >= N_CSI_PORTS) {
		IA_CSS_ERROR("send_mipi_frames(%p) exit: invalid port specified (port=%d).",
			     pipe, port);
		return err;
	}

	 
	for (i = 0; i < my_css.num_mipi_frames[port]; i++) {
		 
		sh_css_update_host2sp_mipi_frame(port * NUM_MIPI_FRAMES_PER_STREAM + i,
						 my_css.mipi_frames[port][i]);
		sh_css_update_host2sp_mipi_metadata(port * NUM_MIPI_FRAMES_PER_STREAM + i,
						    my_css.mipi_metadata[port][i]);
	}
	sh_css_update_host2sp_num_mipi_frames(my_css.num_mipi_frames[port]);

	 
	if (!sh_css_sp_is_running()) {
		 
		IA_CSS_ERROR("sp is not running");
		return err;
	}

	ia_css_bufq_enqueue_psys_event(
	    IA_CSS_PSYS_SW_EVENT_MIPI_BUFFERS_READY,
	    (uint8_t)port,
	    (uint8_t)my_css.num_mipi_frames[port],
	    0  );
	IA_CSS_LEAVE_ERR_PRIVATE(0);
	return 0;
}
