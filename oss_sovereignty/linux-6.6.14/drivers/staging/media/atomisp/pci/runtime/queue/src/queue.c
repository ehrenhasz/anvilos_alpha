
 

#include "ia_css_queue.h"
#include <math_support.h>
#include <ia_css_circbuf.h>
#include <ia_css_circbuf_desc.h>
#include "queue_access.h"

 
int ia_css_queue_local_init(ia_css_queue_t *qhandle, ia_css_queue_local_t *desc)
{
	if (NULL == qhandle || NULL == desc
	    || NULL == desc->cb_elems || NULL == desc->cb_desc) {
		 
		return -EINVAL;
	}

	 
	qhandle->type = IA_CSS_QUEUE_TYPE_LOCAL;

	 
	ia_css_circbuf_create(&qhandle->desc.cb_local,
			      desc->cb_elems,
			      desc->cb_desc);

	return 0;
}

int ia_css_queue_remote_init(ia_css_queue_t *qhandle, ia_css_queue_remote_t *desc)
{
	if (NULL == qhandle || NULL == desc) {
		 
		return -EINVAL;
	}

	 
	qhandle->type = IA_CSS_QUEUE_TYPE_REMOTE;

	 
	qhandle->location = desc->location;
	qhandle->proc_id = desc->proc_id;
	qhandle->desc.remote.cb_desc_addr = desc->cb_desc_addr;
	qhandle->desc.remote.cb_elems_addr = desc->cb_elems_addr;

	 

	return 0;
}

int ia_css_queue_uninit(ia_css_queue_t *qhandle)
{
	if (!qhandle)
		return -EINVAL;

	 
	if (qhandle->type == IA_CSS_QUEUE_TYPE_LOCAL) {
		 
		ia_css_circbuf_destroy(&qhandle->desc.cb_local);
	}

	return 0;
}

int ia_css_queue_enqueue(ia_css_queue_t *qhandle, uint32_t item)
{
	int error = 0;

	if (!qhandle)
		return -EINVAL;

	 
	if (qhandle->type == IA_CSS_QUEUE_TYPE_LOCAL) {
		 
		if (ia_css_circbuf_is_full(&qhandle->desc.cb_local)) {
			 
			return -ENOBUFS;
		}

		 
		ia_css_circbuf_push(&qhandle->desc.cb_local, item);
	} else if (qhandle->type == IA_CSS_QUEUE_TYPE_REMOTE) {
		ia_css_circbuf_desc_t cb_desc;
		ia_css_circbuf_elem_t cb_elem;
		u32 ignore_desc_flags = QUEUE_IGNORE_STEP_FLAG;

		 
		QUEUE_CB_DESC_INIT(&cb_desc);
		error = ia_css_queue_load(qhandle, &cb_desc, ignore_desc_flags);
		if (error != 0)
			return error;

		 
		if (ia_css_circbuf_desc_is_full(&cb_desc))
			return -ENOBUFS;

		cb_elem.val = item;

		error = ia_css_queue_item_store(qhandle, cb_desc.end, &cb_elem);
		if (error != 0)
			return error;

		cb_desc.end = (cb_desc.end + 1) % cb_desc.size;

		 
		 
		ignore_desc_flags = QUEUE_IGNORE_SIZE_START_STEP_FLAGS;

		error = ia_css_queue_store(qhandle, &cb_desc, ignore_desc_flags);
		if (error != 0)
			return error;
	}

	return 0;
}

int ia_css_queue_dequeue(ia_css_queue_t *qhandle, uint32_t *item)
{
	int error = 0;

	if (!qhandle || NULL == item)
		return -EINVAL;

	 
	if (qhandle->type == IA_CSS_QUEUE_TYPE_LOCAL) {
		 
		if (ia_css_circbuf_is_empty(&qhandle->desc.cb_local)) {
			 
			return -ENODATA;
		}

		*item = ia_css_circbuf_pop(&qhandle->desc.cb_local);
	} else if (qhandle->type == IA_CSS_QUEUE_TYPE_REMOTE) {
		 
		ia_css_circbuf_desc_t cb_desc;
		ia_css_circbuf_elem_t cb_elem;
		u32 ignore_desc_flags = QUEUE_IGNORE_STEP_FLAG;

		QUEUE_CB_DESC_INIT(&cb_desc);

		error = ia_css_queue_load(qhandle, &cb_desc, ignore_desc_flags);
		if (error != 0)
			return error;

		 
		if (ia_css_circbuf_desc_is_empty(&cb_desc))
			return -ENODATA;

		error = ia_css_queue_item_load(qhandle, cb_desc.start, &cb_elem);
		if (error != 0)
			return error;

		*item = cb_elem.val;

		cb_desc.start = OP_std_modadd(cb_desc.start, 1, cb_desc.size);

		 
		 
		ignore_desc_flags = QUEUE_IGNORE_SIZE_END_STEP_FLAGS;
		error = ia_css_queue_store(qhandle, &cb_desc, ignore_desc_flags);
		if (error != 0)
			return error;
	}
	return 0;
}

int ia_css_queue_is_full(ia_css_queue_t *qhandle, bool *is_full)
{
	int error = 0;

	if ((!qhandle) || (!is_full))
		return -EINVAL;

	 
	if (qhandle->type == IA_CSS_QUEUE_TYPE_LOCAL) {
		 
		*is_full = ia_css_circbuf_is_full(&qhandle->desc.cb_local);
		return 0;
	} else if (qhandle->type == IA_CSS_QUEUE_TYPE_REMOTE) {
		 
		ia_css_circbuf_desc_t cb_desc;
		u32 ignore_desc_flags = QUEUE_IGNORE_STEP_FLAG;

		QUEUE_CB_DESC_INIT(&cb_desc);
		error = ia_css_queue_load(qhandle, &cb_desc, ignore_desc_flags);
		if (error != 0)
			return error;

		 
		*is_full = ia_css_circbuf_desc_is_full(&cb_desc);
		return 0;
	}

	return -EINVAL;
}

int ia_css_queue_get_free_space(ia_css_queue_t *qhandle, uint32_t *size)
{
	int error = 0;

	if ((!qhandle) || (!size))
		return -EINVAL;

	 
	if (qhandle->type == IA_CSS_QUEUE_TYPE_LOCAL) {
		 
		*size = ia_css_circbuf_get_free_elems(&qhandle->desc.cb_local);
		return 0;
	} else if (qhandle->type == IA_CSS_QUEUE_TYPE_REMOTE) {
		 
		ia_css_circbuf_desc_t cb_desc;
		u32 ignore_desc_flags = QUEUE_IGNORE_STEP_FLAG;

		QUEUE_CB_DESC_INIT(&cb_desc);
		error = ia_css_queue_load(qhandle, &cb_desc, ignore_desc_flags);
		if (error != 0)
			return error;

		 
		*size = ia_css_circbuf_desc_get_free_elems(&cb_desc);
		return 0;
	}

	return -EINVAL;
}

int ia_css_queue_get_used_space(ia_css_queue_t *qhandle, uint32_t *size)
{
	int error = 0;

	if ((!qhandle) || (!size))
		return -EINVAL;

	 
	if (qhandle->type == IA_CSS_QUEUE_TYPE_LOCAL) {
		 
		*size = ia_css_circbuf_get_num_elems(&qhandle->desc.cb_local);
		return 0;
	} else if (qhandle->type == IA_CSS_QUEUE_TYPE_REMOTE) {
		 
		ia_css_circbuf_desc_t cb_desc;
		u32 ignore_desc_flags = QUEUE_IGNORE_STEP_FLAG;

		QUEUE_CB_DESC_INIT(&cb_desc);
		error = ia_css_queue_load(qhandle, &cb_desc, ignore_desc_flags);
		if (error != 0)
			return error;

		 
		*size = ia_css_circbuf_desc_get_num_elems(&cb_desc);
		return 0;
	}

	return -EINVAL;
}

int ia_css_queue_peek(ia_css_queue_t *qhandle, u32 offset, uint32_t *element)
{
	u32 num_elems = 0;
	int error = 0;

	if ((!qhandle) || (!element))
		return -EINVAL;

	 
	if (qhandle->type == IA_CSS_QUEUE_TYPE_LOCAL) {
		 
		 
		num_elems = ia_css_circbuf_get_num_elems(&qhandle->desc.cb_local);
		if (offset > num_elems)
			return -EINVAL;

		*element = ia_css_circbuf_peek_from_start(&qhandle->desc.cb_local, (int)offset);
		return 0;
	} else if (qhandle->type == IA_CSS_QUEUE_TYPE_REMOTE) {
		 
		ia_css_circbuf_desc_t cb_desc;
		ia_css_circbuf_elem_t cb_elem;
		u32 ignore_desc_flags = QUEUE_IGNORE_STEP_FLAG;

		QUEUE_CB_DESC_INIT(&cb_desc);

		error =  ia_css_queue_load(qhandle, &cb_desc, ignore_desc_flags);
		if (error != 0)
			return error;

		 
		num_elems = ia_css_circbuf_desc_get_num_elems(&cb_desc);
		if (offset > num_elems)
			return -EINVAL;

		offset = OP_std_modadd(cb_desc.start, offset, cb_desc.size);
		error = ia_css_queue_item_load(qhandle, (uint8_t)offset, &cb_elem);
		if (error != 0)
			return error;

		*element = cb_elem.val;
		return 0;
	}

	return -EINVAL;
}

int ia_css_queue_is_empty(ia_css_queue_t *qhandle, bool *is_empty)
{
	int error = 0;

	if ((!qhandle) || (!is_empty))
		return -EINVAL;

	 
	if (qhandle->type == IA_CSS_QUEUE_TYPE_LOCAL) {
		 
		*is_empty = ia_css_circbuf_is_empty(&qhandle->desc.cb_local);
		return 0;
	} else if (qhandle->type == IA_CSS_QUEUE_TYPE_REMOTE) {
		 
		ia_css_circbuf_desc_t cb_desc;
		u32 ignore_desc_flags = QUEUE_IGNORE_STEP_FLAG;

		QUEUE_CB_DESC_INIT(&cb_desc);
		error = ia_css_queue_load(qhandle, &cb_desc, ignore_desc_flags);
		if (error != 0)
			return error;

		 
		*is_empty = ia_css_circbuf_desc_is_empty(&cb_desc);
		return 0;
	}

	return -EINVAL;
}

int ia_css_queue_get_size(ia_css_queue_t *qhandle, uint32_t *size)
{
	int error = 0;

	if ((!qhandle) || (!size))
		return -EINVAL;

	 
	if (qhandle->type == IA_CSS_QUEUE_TYPE_LOCAL) {
		 
		 
		*size = ia_css_circbuf_get_size(&qhandle->desc.cb_local);
	} else if (qhandle->type == IA_CSS_QUEUE_TYPE_REMOTE) {
		 
		ia_css_circbuf_desc_t cb_desc;
		u32 ignore_desc_flags = QUEUE_IGNORE_START_END_STEP_FLAGS;

		QUEUE_CB_DESC_INIT(&cb_desc);

		error = ia_css_queue_load(qhandle, &cb_desc, ignore_desc_flags);
		if (error != 0)
			return error;

		 
		*size = cb_desc.size;
	}

	return 0;
}
