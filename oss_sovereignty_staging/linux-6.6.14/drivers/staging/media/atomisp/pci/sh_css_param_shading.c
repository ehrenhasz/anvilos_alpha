
 

#include <linux/math.h>
#include <linux/slab.h>

#include <math_support.h>
#include "sh_css_param_shading.h"
#include "ia_css_shading.h"
#include "assert_support.h"
#include "sh_css_defs.h"
#include "sh_css_internal.h"
#include "ia_css_debug.h"
#include "ia_css_pipe_binarydesc.h"

#include "sh_css_hrt.h"

#include "platform_support.h"

 
static void
crop_and_interpolate(unsigned int cropped_width,
		     unsigned int cropped_height,
		     unsigned int left_padding,
		     int right_padding,
		     int top_padding,
		     const struct ia_css_shading_table *in_table,
		     struct ia_css_shading_table *out_table,
		     enum ia_css_sc_color color)
{
	unsigned int i, j,
		 sensor_width,
		 sensor_height,
		 table_width,
		 table_height,
		 table_cell_h,
		 out_cell_size,
		 in_cell_size,
		 out_start_row,
		 padded_width;
	int out_start_col,  
	    table_cell_w;
	unsigned short *in_ptr,
		 *out_ptr;

	assert(in_table);
	assert(out_table);

	sensor_width  = in_table->sensor_width;
	sensor_height = in_table->sensor_height;
	table_width   = in_table->width;
	table_height  = in_table->height;
	in_ptr = in_table->data[color];
	out_ptr = out_table->data[color];

	padded_width = cropped_width + left_padding + right_padding;
	out_cell_size = CEIL_DIV(padded_width, out_table->width - 1);
	in_cell_size  = CEIL_DIV(sensor_width, table_width - 1);

	out_start_col = ((int)sensor_width - (int)cropped_width) / 2 - left_padding;
	out_start_row = ((int)sensor_height - (int)cropped_height) / 2 - top_padding;
	table_cell_w = (int)((table_width - 1) * in_cell_size);
	table_cell_h = (table_height - 1) * in_cell_size;

	for (i = 0; i < out_table->height; i++) {
		int ty, src_y0, src_y1;
		unsigned int sy0, sy1, dy0, dy1, divy;

		 
		ty = out_start_row + i * out_cell_size;

		 
		src_y0 = ty / (int)in_cell_size;
		if (in_cell_size < out_cell_size)
			src_y1 = (ty + out_cell_size) / in_cell_size;
		else
			src_y1 = src_y0 + 1;
		src_y0 = clamp(src_y0, 0, (int)table_height - 1);
		src_y1 = clamp(src_y1, 0, (int)table_height - 1);
		ty = min(clamp(ty, 0, (int)sensor_height - 1),
			 (int)table_cell_h);

		 
		sy0 = min(src_y0 * in_cell_size, sensor_height - 1);
		sy1 = min(src_y1 * in_cell_size, sensor_height - 1);
		 
		dy0 = ty - sy0;
		dy1 = sy1 - ty;
		divy = sy1 - sy0;
		if (divy == 0) {
			dy0 = 1;
			divy = 1;
		}

		for (j = 0; j < out_table->width; j++, out_ptr++) {
			int tx, src_x0, src_x1;
			unsigned int sx0, sx1, dx0, dx1, divx;
			unsigned short s_ul, s_ur, s_ll, s_lr;

			 
			tx = out_start_col + j * out_cell_size;
			 
			src_x0 = tx / (int)in_cell_size;
			if (in_cell_size < out_cell_size) {
				src_x1 = (tx + out_cell_size) /
					 (int)in_cell_size;
			} else {
				src_x1 = src_x0 + 1;
			}
			 
			src_x0 = clamp(src_x0, 0, (int)table_width - 1);
			src_x1 = clamp(src_x1, 0, (int)table_width - 1);
			tx = min(clamp(tx, 0, (int)sensor_width - 1),
				 (int)table_cell_w);
			 
			sx0 = min(src_x0 * in_cell_size, sensor_width - 1);
			sx1 = min(src_x1 * in_cell_size, sensor_width - 1);
			 
			dx0 = tx - sx0;
			dx1 = sx1 - tx;
			divx = sx1 - sx0;
			 
			if (divx == 0) {
				dx0 = 1;
				divx = 1;
			}

			 
			s_ul = in_ptr[(table_width * src_y0) + src_x0];
			s_ur = in_ptr[(table_width * src_y0) + src_x1];
			s_ll = in_ptr[(table_width * src_y1) + src_x0];
			s_lr = in_ptr[(table_width * src_y1) + src_x1];

			*out_ptr = (unsigned short)((dx0 * dy0 * s_lr + dx0 * dy1 * s_ur + dx1 * dy0 *
						     s_ll + dx1 * dy1 * s_ul) /
						    (divx * divy));
		}
	}
}

void
sh_css_params_shading_id_table_generate(
    struct ia_css_shading_table **target_table,
    unsigned int table_width,
    unsigned int table_height)
{
	 
	unsigned int i, j;
	struct ia_css_shading_table *result;

	assert(target_table);

	result = ia_css_shading_table_alloc(table_width, table_height);
	if (!result) {
		*target_table = NULL;
		return;
	}

	for (i = 0; i < IA_CSS_SC_NUM_COLORS; i++) {
		for (j = 0; j < table_height * table_width; j++)
			result->data[i][j] = 1;
	}
	result->fraction_bits = 0;
	*target_table = result;
}

void
prepare_shading_table(const struct ia_css_shading_table *in_table,
		      unsigned int sensor_binning,
		      struct ia_css_shading_table **target_table,
		      const struct ia_css_binary *binary,
		      unsigned int bds_factor)
{
	unsigned int input_width, input_height, table_width, table_height, i;
	unsigned int left_padding, top_padding, left_cropping;
	struct ia_css_shading_table *result;
	struct u32_fract bds;
	int right_padding;

	assert(target_table);
	assert(binary);

	if (!in_table) {
		sh_css_params_shading_id_table_generate(target_table,
							binary->sctbl_width_per_color,
							binary->sctbl_height);
		return;
	}

	 
	input_height  = binary->in_frame_info.res.height;
	input_width   = binary->in_frame_info.res.width;
	left_padding  = binary->left_padding;
	left_cropping = (binary->info->sp.pipeline.left_cropping == 0) ?
			binary->dvs_envelope.width : 2 * ISP_VEC_NELEMS;

	sh_css_bds_factor_get_fract(bds_factor, &bds);

	left_padding  = (left_padding + binary->info->sp.pipeline.left_cropping) *
			bds.numerator / bds.denominator -
			binary->info->sp.pipeline.left_cropping;
	right_padding = (binary->internal_frame_info.res.width -
			 binary->effective_in_frame_res.width * bds.denominator /
			 bds.numerator - left_cropping) * bds.numerator / bds.denominator;
	top_padding = binary->info->sp.pipeline.top_cropping * bds.numerator /
		      bds.denominator -
		      binary->info->sp.pipeline.top_cropping;

	 
	input_width  <<= sensor_binning;
	input_height <<= sensor_binning;
	 
	left_padding  <<= sensor_binning;
	right_padding <<= sensor_binning;
	top_padding   <<= sensor_binning;

	 
	input_width  = min(input_width,  in_table->sensor_width);
	input_height = min(input_height, in_table->sensor_height);

	 
	table_width  = binary->sctbl_width_per_color;
	table_height = binary->sctbl_height;

	result = ia_css_shading_table_alloc(table_width, table_height);
	if (!result) {
		*target_table = NULL;
		return;
	}
	result->sensor_width  = in_table->sensor_width;
	result->sensor_height = in_table->sensor_height;
	result->fraction_bits = in_table->fraction_bits;

	 
	for (i = 0; i < IA_CSS_SC_NUM_COLORS; i++) {
		crop_and_interpolate(input_width, input_height,
				     left_padding, right_padding, top_padding,
				     in_table,
				     result, i);
	}
	*target_table = result;
}

struct ia_css_shading_table *
ia_css_shading_table_alloc(
    unsigned int width,
    unsigned int height)
{
	unsigned int i;
	struct ia_css_shading_table *me;

	IA_CSS_ENTER("");

	me = kmalloc(sizeof(*me), GFP_KERNEL);
	if (!me)
		return me;

	me->width         = width;
	me->height        = height;
	me->sensor_width  = 0;
	me->sensor_height = 0;
	me->fraction_bits = 0;
	for (i = 0; i < IA_CSS_SC_NUM_COLORS; i++) {
		me->data[i] =
		    kvmalloc(width * height * sizeof(*me->data[0]),
			     GFP_KERNEL);
		if (!me->data[i]) {
			unsigned int j;

			for (j = 0; j < i; j++) {
				kvfree(me->data[j]);
				me->data[j] = NULL;
			}
			kfree(me);
			return NULL;
		}
	}

	IA_CSS_LEAVE("");
	return me;
}

void
ia_css_shading_table_free(struct ia_css_shading_table *table)
{
	unsigned int i;

	if (!table)
		return;

	 
	IA_CSS_ENTER("");

	for (i = 0; i < IA_CSS_SC_NUM_COLORS; i++) {
		if (table->data[i]) {
			kvfree(table->data[i]);
			table->data[i] = NULL;
		}
	}
	kfree(table);

	IA_CSS_LEAVE("");
}
