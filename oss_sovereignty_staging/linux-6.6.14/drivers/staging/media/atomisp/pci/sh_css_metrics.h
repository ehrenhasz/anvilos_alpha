 
 

#ifndef _SH_CSS_METRICS_H_
#define _SH_CSS_METRICS_H_

#include <type_support.h>

struct sh_css_pc_histogram {
	unsigned int length;
	unsigned int *run;
	unsigned int *stall;
	unsigned int *msink;
};

struct sh_css_binary_metrics {
	unsigned int mode;
	unsigned int id;
	struct sh_css_pc_histogram isp_histogram;
	struct sh_css_pc_histogram sp_histogram;
	struct sh_css_binary_metrics *next;
};

struct ia_css_frame_metrics {
	unsigned int num_frames;
};

struct sh_css_metrics {
	struct sh_css_binary_metrics *binary_metrics;
	struct ia_css_frame_metrics   frame_metrics;
};

extern struct sh_css_metrics sh_css_metrics;

 
#include "ia_css_types.h"

 
void sh_css_metrics_enable_pc_histogram(bool enable);
void sh_css_metrics_start_frame(void);
void sh_css_metrics_start_binary(struct sh_css_binary_metrics *metrics);
void sh_css_metrics_sample_pcs(void);

#endif  
