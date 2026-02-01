 
 

#ifndef __SH_CSS_MIPI_H
#define __SH_CSS_MIPI_H

#include <ia_css_err.h>		   
#include <ia_css_types.h>	   
#include <ia_css_stream_public.h>  

void
mipi_init(void);

bool mipi_is_free(void);

int
allocate_mipi_frames(struct ia_css_pipe *pipe, struct ia_css_stream_info *info);

int
free_mipi_frames(struct ia_css_pipe *pipe);

int
send_mipi_frames(struct ia_css_pipe *pipe);

#endif  
