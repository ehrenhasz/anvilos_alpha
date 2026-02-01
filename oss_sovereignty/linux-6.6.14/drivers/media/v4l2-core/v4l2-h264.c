
 

#include <linux/module.h>
#include <linux/sort.h>

#include <media/v4l2-h264.h>

 
static const int tmp_str_size = 1024;

 
void
v4l2_h264_init_reflist_builder(struct v4l2_h264_reflist_builder *b,
		const struct v4l2_ctrl_h264_decode_params *dec_params,
		const struct v4l2_ctrl_h264_sps *sps,
		const struct v4l2_h264_dpb_entry dpb[V4L2_H264_NUM_DPB_ENTRIES])
{
	int cur_frame_num, max_frame_num;
	unsigned int i;

	max_frame_num = 1 << (sps->log2_max_frame_num_minus4 + 4);
	cur_frame_num = dec_params->frame_num;

	memset(b, 0, sizeof(*b));
	if (!(dec_params->flags & V4L2_H264_DECODE_PARAM_FLAG_FIELD_PIC)) {
		b->cur_pic_order_count = min(dec_params->bottom_field_order_cnt,
					     dec_params->top_field_order_cnt);
		b->cur_pic_fields = V4L2_H264_FRAME_REF;
	} else if (dec_params->flags & V4L2_H264_DECODE_PARAM_FLAG_BOTTOM_FIELD) {
		b->cur_pic_order_count = dec_params->bottom_field_order_cnt;
		b->cur_pic_fields = V4L2_H264_BOTTOM_FIELD_REF;
	} else {
		b->cur_pic_order_count = dec_params->top_field_order_cnt;
		b->cur_pic_fields = V4L2_H264_TOP_FIELD_REF;
	}

	for (i = 0; i < V4L2_H264_NUM_DPB_ENTRIES; i++) {
		if (!(dpb[i].flags & V4L2_H264_DPB_ENTRY_FLAG_ACTIVE))
			continue;

		if (dpb[i].flags & V4L2_H264_DPB_ENTRY_FLAG_LONG_TERM)
			b->refs[i].longterm = true;

		 
		if (!b->refs[i].longterm && dpb[i].frame_num > cur_frame_num)
			b->refs[i].frame_num = (int)dpb[i].frame_num -
					       max_frame_num;
		else
			b->refs[i].frame_num = dpb[i].frame_num;

		b->refs[i].top_field_order_cnt = dpb[i].top_field_order_cnt;
		b->refs[i].bottom_field_order_cnt = dpb[i].bottom_field_order_cnt;

		if (b->cur_pic_fields == V4L2_H264_FRAME_REF) {
			u8 fields = V4L2_H264_FRAME_REF;

			b->unordered_reflist[b->num_valid].index = i;
			b->unordered_reflist[b->num_valid].fields = fields;
			b->num_valid++;
			continue;
		}

		if (dpb[i].fields & V4L2_H264_TOP_FIELD_REF) {
			u8 fields = V4L2_H264_TOP_FIELD_REF;

			b->unordered_reflist[b->num_valid].index = i;
			b->unordered_reflist[b->num_valid].fields = fields;
			b->num_valid++;
		}

		if (dpb[i].fields & V4L2_H264_BOTTOM_FIELD_REF) {
			u8 fields = V4L2_H264_BOTTOM_FIELD_REF;

			b->unordered_reflist[b->num_valid].index = i;
			b->unordered_reflist[b->num_valid].fields = fields;
			b->num_valid++;
		}
	}

	for (i = b->num_valid; i < ARRAY_SIZE(b->unordered_reflist); i++)
		b->unordered_reflist[i].index = i;
}
EXPORT_SYMBOL_GPL(v4l2_h264_init_reflist_builder);

static s32 v4l2_h264_get_poc(const struct v4l2_h264_reflist_builder *b,
			     const struct v4l2_h264_reference *ref)
{
	switch (ref->fields) {
	case V4L2_H264_FRAME_REF:
		return min(b->refs[ref->index].top_field_order_cnt,
				b->refs[ref->index].bottom_field_order_cnt);
	case V4L2_H264_TOP_FIELD_REF:
		return b->refs[ref->index].top_field_order_cnt;
	case V4L2_H264_BOTTOM_FIELD_REF:
		return b->refs[ref->index].bottom_field_order_cnt;
	}

	 
	return 0;
}

static int v4l2_h264_p_ref_list_cmp(const void *ptra, const void *ptrb,
				    const void *data)
{
	const struct v4l2_h264_reflist_builder *builder = data;
	u8 idxa, idxb;

	idxa = ((struct v4l2_h264_reference *)ptra)->index;
	idxb = ((struct v4l2_h264_reference *)ptrb)->index;

	if (WARN_ON(idxa >= V4L2_H264_NUM_DPB_ENTRIES ||
		    idxb >= V4L2_H264_NUM_DPB_ENTRIES))
		return 1;

	if (builder->refs[idxa].longterm != builder->refs[idxb].longterm) {
		 
		if (!builder->refs[idxa].longterm)
			return -1;
		else
			return 1;
	}

	 
	if (!builder->refs[idxa].longterm)
		return builder->refs[idxb].frame_num <
		       builder->refs[idxa].frame_num ?
		       -1 : 1;

	return builder->refs[idxa].frame_num < builder->refs[idxb].frame_num ?
	       -1 : 1;
}

static int v4l2_h264_b0_ref_list_cmp(const void *ptra, const void *ptrb,
				     const void *data)
{
	const struct v4l2_h264_reflist_builder *builder = data;
	s32 poca, pocb;
	u8 idxa, idxb;

	idxa = ((struct v4l2_h264_reference *)ptra)->index;
	idxb = ((struct v4l2_h264_reference *)ptrb)->index;

	if (WARN_ON(idxa >= V4L2_H264_NUM_DPB_ENTRIES ||
		    idxb >= V4L2_H264_NUM_DPB_ENTRIES))
		return 1;

	if (builder->refs[idxa].longterm != builder->refs[idxb].longterm) {
		 
		if (!builder->refs[idxa].longterm)
			return -1;
		else
			return 1;
	}

	 
	if (builder->refs[idxa].longterm)
		return builder->refs[idxa].frame_num <
		       builder->refs[idxb].frame_num ?
		       -1 : 1;

	poca = v4l2_h264_get_poc(builder, ptra);
	pocb = v4l2_h264_get_poc(builder, ptrb);

	 
	if ((poca < builder->cur_pic_order_count) !=
	     (pocb < builder->cur_pic_order_count))
		return poca < pocb ? -1 : 1;
	else if (poca < builder->cur_pic_order_count)
		return pocb < poca ? -1 : 1;

	return poca < pocb ? -1 : 1;
}

static int v4l2_h264_b1_ref_list_cmp(const void *ptra, const void *ptrb,
				     const void *data)
{
	const struct v4l2_h264_reflist_builder *builder = data;
	s32 poca, pocb;
	u8 idxa, idxb;

	idxa = ((struct v4l2_h264_reference *)ptra)->index;
	idxb = ((struct v4l2_h264_reference *)ptrb)->index;

	if (WARN_ON(idxa >= V4L2_H264_NUM_DPB_ENTRIES ||
		    idxb >= V4L2_H264_NUM_DPB_ENTRIES))
		return 1;

	if (builder->refs[idxa].longterm != builder->refs[idxb].longterm) {
		 
		if (!builder->refs[idxa].longterm)
			return -1;
		else
			return 1;
	}

	 
	if (builder->refs[idxa].longterm)
		return builder->refs[idxa].frame_num <
		       builder->refs[idxb].frame_num ?
		       -1 : 1;

	poca = v4l2_h264_get_poc(builder, ptra);
	pocb = v4l2_h264_get_poc(builder, ptrb);

	 
	if ((poca < builder->cur_pic_order_count) !=
	    (pocb < builder->cur_pic_order_count))
		return pocb < poca ? -1 : 1;
	else if (poca < builder->cur_pic_order_count)
		return pocb < poca ? -1 : 1;

	return poca < pocb ? -1 : 1;
}

 
static void reorder_field_reflist(const struct v4l2_h264_reflist_builder *b,
				  struct v4l2_h264_reference *reflist)
{
	struct v4l2_h264_reference tmplist[V4L2_H264_REF_LIST_LEN];
	u8 lt, i = 0, j = 0, k = 0;

	memcpy(tmplist, reflist, sizeof(tmplist[0]) * b->num_valid);

	for (lt = 0; lt <= 1; lt++) {
		do {
			for (; i < b->num_valid && b->refs[tmplist[i].index].longterm == lt; i++) {
				if (tmplist[i].fields == b->cur_pic_fields) {
					reflist[k++] = tmplist[i++];
					break;
				}
			}

			for (; j < b->num_valid && b->refs[tmplist[j].index].longterm == lt; j++) {
				if (tmplist[j].fields != b->cur_pic_fields) {
					reflist[k++] = tmplist[j++];
					break;
				}
			}
		} while ((i < b->num_valid && b->refs[tmplist[i].index].longterm == lt) ||
			 (j < b->num_valid && b->refs[tmplist[j].index].longterm == lt));
	}
}

static char ref_type_to_char(u8 ref_type)
{
	switch (ref_type) {
	case V4L2_H264_FRAME_REF:
		return 'f';
	case V4L2_H264_TOP_FIELD_REF:
		return 't';
	case V4L2_H264_BOTTOM_FIELD_REF:
		return 'b';
	}

	return '?';
}

static const char *format_ref_list_p(const struct v4l2_h264_reflist_builder *builder,
				     struct v4l2_h264_reference *reflist,
				     char **out_str)
{
	int n = 0, i;

	*out_str = kmalloc(tmp_str_size, GFP_KERNEL);
	if (!(*out_str))
		return NULL;

	n += snprintf(*out_str + n, tmp_str_size - n, "|");

	for (i = 0; i < builder->num_valid; i++) {
		 
		int frame_num = builder->refs[reflist[i].index].frame_num;
		bool longterm = builder->refs[reflist[i].index].longterm;

		n += scnprintf(*out_str + n, tmp_str_size - n, "%i%c%c|",
			       frame_num, longterm ? 'l' : 's',
			       ref_type_to_char(reflist[i].fields));
	}

	return *out_str;
}

static void print_ref_list_p(const struct v4l2_h264_reflist_builder *builder,
			     struct v4l2_h264_reference *reflist)
{
	char *buf = NULL;

	pr_debug("ref_pic_list_p (cur_poc %u%c) %s\n",
		 builder->cur_pic_order_count,
		 ref_type_to_char(builder->cur_pic_fields),
		 format_ref_list_p(builder, reflist, &buf));

	kfree(buf);
}

static const char *format_ref_list_b(const struct v4l2_h264_reflist_builder *builder,
				     struct v4l2_h264_reference *reflist,
				     char **out_str)
{
	int n = 0, i;

	*out_str = kmalloc(tmp_str_size, GFP_KERNEL);
	if (!(*out_str))
		return NULL;

	n += snprintf(*out_str + n, tmp_str_size - n, "|");

	for (i = 0; i < builder->num_valid; i++) {
		int frame_num = builder->refs[reflist[i].index].frame_num;
		u32 poc = v4l2_h264_get_poc(builder, reflist + i);
		bool longterm = builder->refs[reflist[i].index].longterm;

		n += scnprintf(*out_str + n, tmp_str_size - n, "%i%c%c|",
			       longterm ? frame_num : poc,
			       longterm ? 'l' : 's',
			       ref_type_to_char(reflist[i].fields));
	}

	return *out_str;
}

static void print_ref_list_b(const struct v4l2_h264_reflist_builder *builder,
			     struct v4l2_h264_reference *reflist, u8 list_num)
{
	char *buf = NULL;

	pr_debug("ref_pic_list_b%u (cur_poc %u%c) %s",
		 list_num, builder->cur_pic_order_count,
		 ref_type_to_char(builder->cur_pic_fields),
		 format_ref_list_b(builder, reflist, &buf));

	kfree(buf);
}

 
void
v4l2_h264_build_p_ref_list(const struct v4l2_h264_reflist_builder *builder,
			   struct v4l2_h264_reference *reflist)
{
	memcpy(reflist, builder->unordered_reflist,
	       sizeof(builder->unordered_reflist[0]) * builder->num_valid);
	sort_r(reflist, builder->num_valid, sizeof(*reflist),
	       v4l2_h264_p_ref_list_cmp, NULL, builder);

	if (builder->cur_pic_fields != V4L2_H264_FRAME_REF)
		reorder_field_reflist(builder, reflist);

	print_ref_list_p(builder, reflist);
}
EXPORT_SYMBOL_GPL(v4l2_h264_build_p_ref_list);

 
void
v4l2_h264_build_b_ref_lists(const struct v4l2_h264_reflist_builder *builder,
			    struct v4l2_h264_reference *b0_reflist,
			    struct v4l2_h264_reference *b1_reflist)
{
	memcpy(b0_reflist, builder->unordered_reflist,
	       sizeof(builder->unordered_reflist[0]) * builder->num_valid);
	sort_r(b0_reflist, builder->num_valid, sizeof(*b0_reflist),
	       v4l2_h264_b0_ref_list_cmp, NULL, builder);

	memcpy(b1_reflist, builder->unordered_reflist,
	       sizeof(builder->unordered_reflist[0]) * builder->num_valid);
	sort_r(b1_reflist, builder->num_valid, sizeof(*b1_reflist),
	       v4l2_h264_b1_ref_list_cmp, NULL, builder);

	if (builder->cur_pic_fields != V4L2_H264_FRAME_REF) {
		reorder_field_reflist(builder, b0_reflist);
		reorder_field_reflist(builder, b1_reflist);
	}

	if (builder->num_valid > 1 &&
	    !memcmp(b1_reflist, b0_reflist, builder->num_valid))
		swap(b1_reflist[0], b1_reflist[1]);

	print_ref_list_b(builder, b0_reflist, 0);
	print_ref_list_b(builder, b1_reflist, 1);
}
EXPORT_SYMBOL_GPL(v4l2_h264_build_b_ref_lists);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("V4L2 H264 Helpers");
MODULE_AUTHOR("Boris Brezillon <boris.brezillon@collabora.com>");
