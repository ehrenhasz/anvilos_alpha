 
 

#ifndef __MESON_VDEC_HELPERS_H_
#define __MESON_VDEC_HELPERS_H_

#include "vdec.h"

 
int amvdec_set_canvases(struct amvdec_session *sess,
			u32 reg_base[], u32 reg_num[]);

 
u32 amvdec_read_dos(struct amvdec_core *core, u32 reg);
void amvdec_write_dos(struct amvdec_core *core, u32 reg, u32 val);
void amvdec_write_dos_bits(struct amvdec_core *core, u32 reg, u32 val);
void amvdec_clear_dos_bits(struct amvdec_core *core, u32 reg, u32 val);
u32 amvdec_read_parser(struct amvdec_core *core, u32 reg);
void amvdec_write_parser(struct amvdec_core *core, u32 reg, u32 val);

u32 amvdec_am21c_body_size(u32 width, u32 height);
u32 amvdec_am21c_head_size(u32 width, u32 height);
u32 amvdec_am21c_size(u32 width, u32 height);

 
void amvdec_dst_buf_done_idx(struct amvdec_session *sess, u32 buf_idx,
			     u32 offset, u32 field);
void amvdec_dst_buf_done(struct amvdec_session *sess,
			 struct vb2_v4l2_buffer *vbuf, u32 field);
void amvdec_dst_buf_done_offset(struct amvdec_session *sess,
				struct vb2_v4l2_buffer *vbuf,
				u32 offset, u32 field, bool allow_drop);

 
int amvdec_add_ts(struct amvdec_session *sess, u64 ts,
		  struct v4l2_timecode tc, u32 offset, u32 flags);
void amvdec_remove_ts(struct amvdec_session *sess, u64 ts);

 
void amvdec_set_par_from_dar(struct amvdec_session *sess,
			     u32 dar_num, u32 dar_den);

 
void amvdec_src_change(struct amvdec_session *sess, u32 width,
		       u32 height, u32 dpb_size);

 
void amvdec_abort(struct amvdec_session *sess);
#endif
