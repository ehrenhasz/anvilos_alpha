 
 

#ifndef	__ATOMISP_FOPS_H__
#define	__ATOMISP_FOPS_H__
#include "atomisp_subdev.h"

 

int atomisp_qbuffers_to_css(struct atomisp_sub_device *asd);

extern const struct vb2_ops atomisp_vb2_ops;
extern const struct v4l2_file_operations atomisp_fops;

#endif  
