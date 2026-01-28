#ifndef	__ATOMISP_CMD_H__
#define	__ATOMISP_CMD_H__
#include "../../include/linux/atomisp.h"
#include <linux/interrupt.h>
#include <linux/videodev2.h>
#include <media/v4l2-subdev.h>
#include "atomisp_internal.h"
#include "ia_css_types.h"
#include "ia_css.h"
struct atomisp_device;
struct ia_css_frame;
#define MSI_ENABLE_BIT		16
#define INTR_DISABLE_BIT	10
#define BUS_MASTER_ENABLE	2
#define MEMORY_SPACE_ENABLE	1
#define INTR_IER		24
#define INTR_IIR		16
#define RUNMODE_MASK (ATOMISP_RUN_MODE_VIDEO | ATOMISP_RUN_MODE_STILL_CAPTURE \
			| ATOMISP_RUN_MODE_PREVIEW)
extern int atomisp_punit_hpll_freq;
void dump_sp_dmem(struct atomisp_device *isp, unsigned int addr,
		  unsigned int size);
struct camera_mipi_info *atomisp_to_sensor_mipi_info(struct v4l2_subdev *sd);
struct atomisp_video_pipe *atomisp_to_video_pipe(struct video_device *dev);
int atomisp_reset(struct atomisp_device *isp);
int atomisp_buffers_in_css(struct atomisp_video_pipe *pipe);
void atomisp_buffer_done(struct ia_css_frame *frame, enum vb2_buffer_state state);
void atomisp_flush_video_pipe(struct atomisp_video_pipe *pipe, enum vb2_buffer_state state,
			      bool warn_on_css_frames);
void atomisp_clear_css_buffer_counters(struct atomisp_sub_device *asd);
void atomisp_msi_irq_init(struct atomisp_device *isp);
void atomisp_msi_irq_uninit(struct atomisp_device *isp);
void atomisp_assert_recovery_work(struct work_struct *work);
void atomisp_setup_flash(struct atomisp_sub_device *asd);
irqreturn_t atomisp_isr(int irq, void *dev);
irqreturn_t atomisp_isr_thread(int irq, void *isp_ptr);
const struct atomisp_format_bridge *get_atomisp_format_bridge_from_mbus(
    u32 mbus_code);
bool atomisp_is_mbuscode_raw(uint32_t code);
bool atomisp_is_viewfinder_support(struct atomisp_device *isp);
int atomisp_set_sensor_runmode(struct atomisp_sub_device *asd,
			       struct atomisp_s_runmode *runmode);
int atomisp_gdc_cac(struct atomisp_sub_device *asd, int flag,
		    __s32 *value);
int atomisp_low_light(struct atomisp_sub_device *asd, int flag,
		      __s32 *value);
int atomisp_xnr(struct atomisp_sub_device *asd, int flag, int *arg);
int atomisp_formats(struct atomisp_sub_device *asd, int flag,
		    struct atomisp_formats_config *config);
int atomisp_nr(struct atomisp_sub_device *asd, int flag,
	       struct atomisp_nr_config *config);
int atomisp_tnr(struct atomisp_sub_device *asd, int flag,
		struct atomisp_tnr_config *config);
int atomisp_black_level(struct atomisp_sub_device *asd, int flag,
			struct atomisp_ob_config *config);
int atomisp_ee(struct atomisp_sub_device *asd, int flag,
	       struct atomisp_ee_config *config);
int atomisp_gamma(struct atomisp_sub_device *asd, int flag,
		  struct atomisp_gamma_table *config);
int atomisp_ctc(struct atomisp_sub_device *asd, int flag,
		struct atomisp_ctc_table *config);
int atomisp_gamma_correction(struct atomisp_sub_device *asd, int flag,
			     struct atomisp_gc_config *config);
int atomisp_gdc_cac_table(struct atomisp_sub_device *asd, int flag,
			  struct atomisp_morph_table *config);
int atomisp_macc_table(struct atomisp_sub_device *asd, int flag,
		       struct atomisp_macc_config *config);
int atomisp_get_dis_stat(struct atomisp_sub_device *asd,
			 struct atomisp_dis_statistics *stats);
int atomisp_get_dvs2_bq_resolutions(struct atomisp_sub_device *asd,
				    struct atomisp_dvs2_bq_resolutions *bq_res);
int atomisp_set_dis_coefs(struct atomisp_sub_device *asd,
			  struct atomisp_dis_coefficients *coefs);
int atomisp_set_dis_vector(struct atomisp_sub_device *asd,
			   struct atomisp_dis_vector *vector);
int atomisp_3a_stat(struct atomisp_sub_device *asd, int flag,
		    struct atomisp_3a_statistics *config);
int atomisp_set_parameters(struct video_device *vdev,
			   struct atomisp_parameters *arg);
int atomisp_param(struct atomisp_sub_device *asd, int flag,
		  struct atomisp_parm *config);
int atomisp_color_effect(struct atomisp_sub_device *asd, int flag,
			 __s32 *effect);
int atomisp_bad_pixel(struct atomisp_sub_device *asd, int flag,
		      __s32 *value);
int atomisp_bad_pixel_param(struct atomisp_sub_device *asd, int flag,
			    struct atomisp_dp_config *config);
int atomisp_video_stable(struct atomisp_sub_device *asd, int flag,
			 __s32 *value);
int atomisp_fixed_pattern(struct atomisp_sub_device *asd, int flag,
			  __s32 *value);
int atomisp_fixed_pattern_table(struct atomisp_sub_device *asd,
				struct v4l2_framebuffer *config);
int atomisp_false_color(struct atomisp_sub_device *asd, int flag,
			__s32 *value);
int atomisp_false_color_param(struct atomisp_sub_device *asd, int flag,
			      struct atomisp_de_config *config);
int atomisp_white_balance_param(struct atomisp_sub_device *asd, int flag,
				struct atomisp_wb_config *config);
int atomisp_3a_config_param(struct atomisp_sub_device *asd, int flag,
			    struct atomisp_3a_config *config);
int atomisp_digital_zoom(struct atomisp_sub_device *asd, int flag,
			 __s32 *value);
int atomisp_set_array_res(struct atomisp_sub_device *asd,
			  struct atomisp_resolution  *config);
int atomisp_calculate_real_zoom_region(struct atomisp_sub_device *asd,
				       struct ia_css_dz_config   *dz_config,
				       enum ia_css_pipe_id css_pipe_id);
int atomisp_cp_general_isp_parameters(struct atomisp_sub_device *asd,
				      struct atomisp_parameters *arg,
				      struct atomisp_css_params *css_param,
				      bool from_user);
int atomisp_cp_lsc_table(struct atomisp_sub_device *asd,
			 struct atomisp_shading_table *source_st,
			 struct atomisp_css_params *css_param,
			 bool from_user);
int atomisp_css_cp_dvs2_coefs(struct atomisp_sub_device *asd,
			      struct ia_css_dvs2_coefficients *coefs,
			      struct atomisp_css_params *css_param,
			      bool from_user);
int atomisp_cp_morph_table(struct atomisp_sub_device *asd,
			   struct atomisp_morph_table *source_morph_table,
			   struct atomisp_css_params *css_param,
			   bool from_user);
int atomisp_cp_dvs_6axis_config(struct atomisp_sub_device *asd,
				struct atomisp_dvs_6axis_config *user_6axis_config,
				struct atomisp_css_params *css_param,
				bool from_user);
int atomisp_makeup_css_parameters(struct atomisp_sub_device *asd,
				  struct atomisp_parameters *arg,
				  struct atomisp_css_params *css_param);
int atomisp_compare_grid(struct atomisp_sub_device *asd,
			 struct atomisp_grid_info *atomgrid);
void atomisp_get_padding(struct atomisp_device *isp, u32 width, u32 height,
			 u32 *padding_w, u32 *padding_h);
int atomisp_try_fmt(struct atomisp_device *isp, struct v4l2_pix_format *f,
		    const struct atomisp_format_bridge **fmt_ret,
		    const struct atomisp_format_bridge **snr_fmt_ret);
int atomisp_set_fmt(struct video_device *vdev, struct v4l2_format *f);
int atomisp_set_shading_table(struct atomisp_sub_device *asd,
			      struct atomisp_shading_table *shading_table);
void atomisp_free_internal_buffers(struct atomisp_sub_device *asd);
int  atomisp_flash_enable(struct atomisp_sub_device *asd,
			  int num_frames);
int atomisp_freq_scaling(struct atomisp_device *vdev,
			 enum atomisp_dfs_mode mode,
			 bool force);
void atomisp_buf_done(struct atomisp_sub_device *asd, int error,
		      enum ia_css_buffer_type buf_type,
		      enum ia_css_pipe_id css_pipe_id,
		      bool q_buffers, enum atomisp_input_stream_id stream_id);
void atomisp_eof_event(struct atomisp_sub_device *asd, uint8_t exp_id);
enum mipi_port_id atomisp_port_to_mipi_port(struct atomisp_device *isp,
					    enum atomisp_camera_port port);
void atomisp_apply_css_parameters(
    struct atomisp_sub_device *asd,
    struct atomisp_css_params *css_param);
void atomisp_free_css_parameters(struct atomisp_css_params *css_param);
void atomisp_handle_parameter_and_buffer(struct atomisp_video_pipe *pipe);
void atomisp_flush_params_queue(struct atomisp_video_pipe *asd);
int atomisp_exp_id_unlock(struct atomisp_sub_device *asd, int *exp_id);
int atomisp_exp_id_capture(struct atomisp_sub_device *asd, int *exp_id);
void atomisp_init_raw_buffer_bitmap(struct atomisp_sub_device *asd);
int atomisp_enable_dz_capt_pipe(struct atomisp_sub_device *asd,
				unsigned int *enable);
u32 atomisp_get_pixel_depth(u32 pixelformat);
int atomisp_inject_a_fake_event(struct atomisp_sub_device *asd, int *event);
int atomisp_get_invalid_frame_num(struct video_device *vdev,
				  int *invalid_frame_num);
int atomisp_power_off(struct device *dev);
int atomisp_power_on(struct device *dev);
#endif  
