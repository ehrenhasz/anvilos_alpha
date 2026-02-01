 
 

#ifndef __SH_CSS_STRUCT_H
#define __SH_CSS_STRUCT_H

 

#include <type_support.h>
#include <system_local.h>
#include "ia_css_pipeline.h"
#include "ia_css_pipe_public.h"
#include "ia_css_frame_public.h"
#include "ia_css_queue.h"
#include "ia_css_irq.h"

struct sh_css {
	struct ia_css_pipe            *active_pipes[IA_CSS_PIPELINE_NUM_MAX];
	 
	struct ia_css_pipe            *all_pipes[IA_CSS_PIPELINE_NUM_MAX];
	void *(*malloc)(size_t bytes, bool zero_mem);
	void (*free)(void *ptr);
	void (*flush)(struct ia_css_acc_fw *fw);

 
	void *(*malloc_ex)(size_t bytes, bool zero_mem, const char *caller_func,
			   int caller_line);
	void (*free_ex)(void *ptr, const char *caller_func, int caller_line);

 
	bool stop_copy_preview;

	bool                           check_system_idle;
	unsigned int                   num_cont_raw_frames;
	unsigned int                   num_mipi_frames[N_CSI_PORTS];
	struct ia_css_frame
		*mipi_frames[N_CSI_PORTS][NUM_MIPI_FRAMES_PER_STREAM];
	struct ia_css_metadata
		*mipi_metadata[N_CSI_PORTS][NUM_MIPI_FRAMES_PER_STREAM];
	unsigned int
	mipi_sizes_for_check[N_CSI_PORTS][IA_CSS_MIPI_SIZE_CHECK_MAX_NOF_ENTRIES_PER_PORT];
	unsigned int                   mipi_frame_size[N_CSI_PORTS];
	ia_css_ptr                   sp_bin_addr;
	hrt_data                       page_table_base_index;

	unsigned int
	size_mem_words;  
	enum ia_css_irq_type           irq_type;
	unsigned int                   pipe_counter;

	unsigned int		type;	 
};

#define IPU_2400		1
#define IPU_2401		2

#define IS_2400()		(my_css.type == IPU_2400)
#define IS_2401()		(my_css.type == IPU_2401)

extern struct sh_css my_css;

#endif  
