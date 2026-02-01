
 

 

#include "hid-uclogic-params.h"
#include "hid-uclogic-rdesc.h"
#include "usbhid/usbhid.h"
#include "hid-ids.h"
#include <linux/ctype.h>
#include <linux/string.h>
#include <asm/unaligned.h>

 
static const char *uclogic_params_pen_inrange_to_str(
				enum uclogic_params_pen_inrange inrange)
{
	switch (inrange) {
	case UCLOGIC_PARAMS_PEN_INRANGE_NORMAL:
		return "normal";
	case UCLOGIC_PARAMS_PEN_INRANGE_INVERTED:
		return "inverted";
	case UCLOGIC_PARAMS_PEN_INRANGE_NONE:
		return "none";
	default:
		return NULL;
	}
}

 
static void uclogic_params_pen_hid_dbg(const struct hid_device *hdev,
					const struct uclogic_params_pen *pen)
{
	size_t i;

	hid_dbg(hdev, "\t.usage_invalid = %s\n",
		(pen->usage_invalid ? "true" : "false"));
	hid_dbg(hdev, "\t.desc_ptr = %p\n", pen->desc_ptr);
	hid_dbg(hdev, "\t.desc_size = %u\n", pen->desc_size);
	hid_dbg(hdev, "\t.id = %u\n", pen->id);
	hid_dbg(hdev, "\t.subreport_list = {\n");
	for (i = 0; i < ARRAY_SIZE(pen->subreport_list); i++) {
		hid_dbg(hdev, "\t\t{0x%02hhx, %hhu}%s\n",
			pen->subreport_list[i].value,
			pen->subreport_list[i].id,
			i < (ARRAY_SIZE(pen->subreport_list) - 1) ? "," : "");
	}
	hid_dbg(hdev, "\t}\n");
	hid_dbg(hdev, "\t.inrange = %s\n",
		uclogic_params_pen_inrange_to_str(pen->inrange));
	hid_dbg(hdev, "\t.fragmented_hires = %s\n",
		(pen->fragmented_hires ? "true" : "false"));
	hid_dbg(hdev, "\t.tilt_y_flipped = %s\n",
		(pen->tilt_y_flipped ? "true" : "false"));
}

 
static void uclogic_params_frame_hid_dbg(
				const struct hid_device *hdev,
				const struct uclogic_params_frame *frame)
{
	hid_dbg(hdev, "\t\t.desc_ptr = %p\n", frame->desc_ptr);
	hid_dbg(hdev, "\t\t.desc_size = %u\n", frame->desc_size);
	hid_dbg(hdev, "\t\t.id = %u\n", frame->id);
	hid_dbg(hdev, "\t\t.suffix = %s\n", frame->suffix);
	hid_dbg(hdev, "\t\t.re_lsb = %u\n", frame->re_lsb);
	hid_dbg(hdev, "\t\t.dev_id_byte = %u\n", frame->dev_id_byte);
	hid_dbg(hdev, "\t\t.touch_byte = %u\n", frame->touch_byte);
	hid_dbg(hdev, "\t\t.touch_max = %hhd\n", frame->touch_max);
	hid_dbg(hdev, "\t\t.touch_flip_at = %hhd\n",
		frame->touch_flip_at);
	hid_dbg(hdev, "\t\t.bitmap_dial_byte = %u\n",
		frame->bitmap_dial_byte);
}

 
void uclogic_params_hid_dbg(const struct hid_device *hdev,
				const struct uclogic_params *params)
{
	size_t i;

	hid_dbg(hdev, ".invalid = %s\n",
		params->invalid ? "true" : "false");
	hid_dbg(hdev, ".desc_ptr = %p\n", params->desc_ptr);
	hid_dbg(hdev, ".desc_size = %u\n", params->desc_size);
	hid_dbg(hdev, ".pen = {\n");
	uclogic_params_pen_hid_dbg(hdev, &params->pen);
	hid_dbg(hdev, "\t}\n");
	hid_dbg(hdev, ".frame_list = {\n");
	for (i = 0; i < ARRAY_SIZE(params->frame_list); i++) {
		hid_dbg(hdev, "\t{\n");
		uclogic_params_frame_hid_dbg(hdev, &params->frame_list[i]);
		hid_dbg(hdev, "\t}%s\n",
			i < (ARRAY_SIZE(params->frame_list) - 1) ? "," : "");
	}
	hid_dbg(hdev, "}\n");
}

 
static int uclogic_params_get_str_desc(__u8 **pbuf, struct hid_device *hdev,
					__u8 idx, size_t len)
{
	int rc;
	struct usb_device *udev;
	__u8 *buf = NULL;

	 
	if (hdev == NULL) {
		rc = -EINVAL;
		goto cleanup;
	}

	udev = hid_to_usb_dev(hdev);

	buf = kmalloc(len, GFP_KERNEL);
	if (buf == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}

	rc = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
				USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
				(USB_DT_STRING << 8) + idx,
				0x0409, buf, len,
				USB_CTRL_GET_TIMEOUT);
	if (rc == -EPIPE) {
		hid_dbg(hdev, "string descriptor #%hhu not found\n", idx);
		goto cleanup;
	} else if (rc < 0) {
		hid_err(hdev,
			"failed retrieving string descriptor #%u: %d\n",
			idx, rc);
		goto cleanup;
	}

	if (pbuf != NULL) {
		*pbuf = buf;
		buf = NULL;
	}

cleanup:
	kfree(buf);
	return rc;
}

 
static void uclogic_params_pen_cleanup(struct uclogic_params_pen *pen)
{
	kfree(pen->desc_ptr);
	memset(pen, 0, sizeof(*pen));
}

 
static int uclogic_params_pen_init_v1(struct uclogic_params_pen *pen,
				      bool *pfound,
				      struct hid_device *hdev)
{
	int rc;
	bool found = false;
	 
	__u8 *buf = NULL;
	 
	const int len = 12;
	s32 resolution;
	 
	s32 desc_params[UCLOGIC_RDESC_PH_ID_NUM];
	__u8 *desc_ptr = NULL;

	 
	if (pen == NULL || pfound == NULL || hdev == NULL) {
		rc = -EINVAL;
		goto cleanup;
	}

	 
	rc = uclogic_params_get_str_desc(&buf, hdev, 100, len);
	if (rc == -EPIPE) {
		hid_dbg(hdev,
			"string descriptor with pen parameters not found, assuming not compatible\n");
		goto finish;
	} else if (rc < 0) {
		hid_err(hdev, "failed retrieving pen parameters: %d\n", rc);
		goto cleanup;
	} else if (rc != len) {
		hid_dbg(hdev,
			"string descriptor with pen parameters has invalid length (got %d, expected %d), assuming not compatible\n",
			rc, len);
		goto finish;
	}

	 
	desc_params[UCLOGIC_RDESC_PEN_PH_ID_X_LM] =
		get_unaligned_le16(buf + 2);
	desc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_LM] =
		get_unaligned_le16(buf + 4);
	desc_params[UCLOGIC_RDESC_PEN_PH_ID_PRESSURE_LM] =
		get_unaligned_le16(buf + 8);
	resolution = get_unaligned_le16(buf + 10);
	if (resolution == 0) {
		desc_params[UCLOGIC_RDESC_PEN_PH_ID_X_PM] = 0;
		desc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_PM] = 0;
	} else {
		desc_params[UCLOGIC_RDESC_PEN_PH_ID_X_PM] =
			desc_params[UCLOGIC_RDESC_PEN_PH_ID_X_LM] * 1000 /
			resolution;
		desc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_PM] =
			desc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_LM] * 1000 /
			resolution;
	}
	kfree(buf);
	buf = NULL;

	 
	desc_ptr = uclogic_rdesc_template_apply(
				uclogic_rdesc_v1_pen_template_arr,
				uclogic_rdesc_v1_pen_template_size,
				desc_params, ARRAY_SIZE(desc_params));
	if (desc_ptr == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}

	 
	memset(pen, 0, sizeof(*pen));
	pen->desc_ptr = desc_ptr;
	desc_ptr = NULL;
	pen->desc_size = uclogic_rdesc_v1_pen_template_size;
	pen->id = UCLOGIC_RDESC_V1_PEN_ID;
	pen->inrange = UCLOGIC_PARAMS_PEN_INRANGE_INVERTED;
	found = true;
finish:
	*pfound = found;
	rc = 0;
cleanup:
	kfree(desc_ptr);
	kfree(buf);
	return rc;
}

 
static s32 uclogic_params_get_le24(const void *p)
{
	const __u8 *b = p;
	return b[0] | (b[1] << 8UL) | (b[2] << 16UL);
}

 
static int uclogic_params_pen_init_v2(struct uclogic_params_pen *pen,
					bool *pfound,
					__u8 **pparams_ptr,
					size_t *pparams_len,
					struct hid_device *hdev)
{
	int rc;
	bool found = false;
	 
	__u8 *buf = NULL;
	 
	const int params_len_min = 18;
	 
	const int params_len_max = 32;
	 
	int params_len;
	size_t i;
	s32 resolution;
	 
	s32 desc_params[UCLOGIC_RDESC_PH_ID_NUM];
	__u8 *desc_ptr = NULL;

	 
	if (pen == NULL || pfound == NULL || hdev == NULL) {
		rc = -EINVAL;
		goto cleanup;
	}

	 
	rc = uclogic_params_get_str_desc(&buf, hdev, 200, params_len_max);
	if (rc == -EPIPE) {
		hid_dbg(hdev,
			"string descriptor with pen parameters not found, assuming not compatible\n");
		goto finish;
	} else if (rc < 0) {
		hid_err(hdev, "failed retrieving pen parameters: %d\n", rc);
		goto cleanup;
	} else if (rc < params_len_min) {
		hid_dbg(hdev,
			"string descriptor with pen parameters is too short (got %d, expected at least %d), assuming not compatible\n",
			rc, params_len_min);
		goto finish;
	}

	params_len = rc;

	 
	for (i = 2;
	     i < params_len &&
		(buf[i] >= 0x20 && buf[i] < 0x7f && buf[i + 1] == 0);
	     i += 2);
	if (i >= params_len) {
		hid_dbg(hdev,
			"string descriptor with pen parameters seems to contain only text, assuming not compatible\n");
		goto finish;
	}

	 
	desc_params[UCLOGIC_RDESC_PEN_PH_ID_X_LM] =
		uclogic_params_get_le24(buf + 2);
	desc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_LM] =
		uclogic_params_get_le24(buf + 5);
	desc_params[UCLOGIC_RDESC_PEN_PH_ID_PRESSURE_LM] =
		get_unaligned_le16(buf + 8);
	resolution = get_unaligned_le16(buf + 10);
	if (resolution == 0) {
		desc_params[UCLOGIC_RDESC_PEN_PH_ID_X_PM] = 0;
		desc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_PM] = 0;
	} else {
		desc_params[UCLOGIC_RDESC_PEN_PH_ID_X_PM] =
			desc_params[UCLOGIC_RDESC_PEN_PH_ID_X_LM] * 1000 /
			resolution;
		desc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_PM] =
			desc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_LM] * 1000 /
			resolution;
	}

	 
	desc_ptr = uclogic_rdesc_template_apply(
				uclogic_rdesc_v2_pen_template_arr,
				uclogic_rdesc_v2_pen_template_size,
				desc_params, ARRAY_SIZE(desc_params));
	if (desc_ptr == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}

	 
	memset(pen, 0, sizeof(*pen));
	pen->desc_ptr = desc_ptr;
	desc_ptr = NULL;
	pen->desc_size = uclogic_rdesc_v2_pen_template_size;
	pen->id = UCLOGIC_RDESC_V2_PEN_ID;
	pen->inrange = UCLOGIC_PARAMS_PEN_INRANGE_NONE;
	pen->fragmented_hires = true;
	pen->tilt_y_flipped = true;
	found = true;
	if (pparams_ptr != NULL) {
		*pparams_ptr = buf;
		buf = NULL;
	}
	if (pparams_len != NULL)
		*pparams_len = params_len;

finish:
	*pfound = found;
	rc = 0;
cleanup:
	kfree(desc_ptr);
	kfree(buf);
	return rc;
}

 
static void uclogic_params_frame_cleanup(struct uclogic_params_frame *frame)
{
	kfree(frame->desc_ptr);
	memset(frame, 0, sizeof(*frame));
}

 
static int uclogic_params_frame_init_with_desc(
					struct uclogic_params_frame *frame,
					const __u8 *desc_ptr,
					size_t desc_size,
					unsigned int id)
{
	__u8 *copy_desc_ptr;

	if (frame == NULL || (desc_ptr == NULL && desc_size != 0))
		return -EINVAL;

	copy_desc_ptr = kmemdup(desc_ptr, desc_size, GFP_KERNEL);
	if (copy_desc_ptr == NULL)
		return -ENOMEM;

	memset(frame, 0, sizeof(*frame));
	frame->desc_ptr = copy_desc_ptr;
	frame->desc_size = desc_size;
	frame->id = id;
	return 0;
}

 
static int uclogic_params_frame_init_v1(struct uclogic_params_frame *frame,
					bool *pfound,
					struct hid_device *hdev)
{
	int rc;
	bool found = false;
	struct usb_device *usb_dev;
	char *str_buf = NULL;
	const size_t str_len = 16;

	 
	if (frame == NULL || pfound == NULL || hdev == NULL) {
		rc = -EINVAL;
		goto cleanup;
	}

	usb_dev = hid_to_usb_dev(hdev);

	 
	str_buf = kzalloc(str_len, GFP_KERNEL);
	if (str_buf == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}

	rc = usb_string(usb_dev, 123, str_buf, str_len);
	if (rc == -EPIPE) {
		hid_dbg(hdev,
			"generic button -enabling string descriptor not found\n");
	} else if (rc < 0) {
		goto cleanup;
	} else if (strncmp(str_buf, "HK On", rc) != 0) {
		hid_dbg(hdev,
			"invalid response to enabling generic buttons: \"%s\"\n",
			str_buf);
	} else {
		hid_dbg(hdev, "generic buttons enabled\n");
		rc = uclogic_params_frame_init_with_desc(
				frame,
				uclogic_rdesc_v1_frame_arr,
				uclogic_rdesc_v1_frame_size,
				UCLOGIC_RDESC_V1_FRAME_ID);
		if (rc != 0)
			goto cleanup;
		found = true;
	}

	*pfound = found;
	rc = 0;
cleanup:
	kfree(str_buf);
	return rc;
}

 
static void uclogic_params_cleanup_event_hooks(struct uclogic_params *params)
{
	struct uclogic_raw_event_hook *curr, *n;

	if (!params || !params->event_hooks)
		return;

	list_for_each_entry_safe(curr, n, &params->event_hooks->list, list) {
		cancel_work_sync(&curr->work);
		list_del(&curr->list);
		kfree(curr->event);
		kfree(curr);
	}

	kfree(params->event_hooks);
	params->event_hooks = NULL;
}

 
void uclogic_params_cleanup(struct uclogic_params *params)
{
	if (!params->invalid) {
		size_t i;
		kfree(params->desc_ptr);
		uclogic_params_pen_cleanup(&params->pen);
		for (i = 0; i < ARRAY_SIZE(params->frame_list); i++)
			uclogic_params_frame_cleanup(&params->frame_list[i]);

		uclogic_params_cleanup_event_hooks(params);
		memset(params, 0, sizeof(*params));
	}
}

 
int uclogic_params_get_desc(const struct uclogic_params *params,
				__u8 **pdesc,
				unsigned int *psize)
{
	int rc = -ENOMEM;
	bool present = false;
	unsigned int size = 0;
	__u8 *desc = NULL;
	size_t i;

	 
	if (params == NULL || pdesc == NULL || psize == NULL)
		return -EINVAL;

	 
#define ADD_DESC(_desc_ptr, _desc_size) \
	do {                                                        \
		unsigned int new_size;                              \
		__u8 *new_desc;                                     \
		if ((_desc_ptr) == NULL) {                          \
			break;                                      \
		}                                                   \
		new_size = size + (_desc_size);                     \
		new_desc = krealloc(desc, new_size, GFP_KERNEL);    \
		if (new_desc == NULL) {                             \
			goto cleanup;                               \
		}                                                   \
		memcpy(new_desc + size, (_desc_ptr), (_desc_size)); \
		desc = new_desc;                                    \
		size = new_size;                                    \
		present = true;                                     \
	} while (0)

	ADD_DESC(params->desc_ptr, params->desc_size);
	ADD_DESC(params->pen.desc_ptr, params->pen.desc_size);
	for (i = 0; i < ARRAY_SIZE(params->frame_list); i++) {
		ADD_DESC(params->frame_list[i].desc_ptr,
				params->frame_list[i].desc_size);
	}

#undef ADD_DESC

	if (present) {
		*pdesc = desc;
		*psize = size;
		desc = NULL;
	}
	rc = 0;
cleanup:
	kfree(desc);
	return rc;
}

 
static void uclogic_params_init_invalid(struct uclogic_params *params)
{
	params->invalid = true;
}

 
static int uclogic_params_init_with_opt_desc(struct uclogic_params *params,
					     struct hid_device *hdev,
					     unsigned int orig_desc_size,
					     __u8 *desc_ptr,
					     unsigned int desc_size)
{
	__u8 *desc_copy_ptr = NULL;
	unsigned int desc_copy_size;
	int rc;

	 
	if (params == NULL || hdev == NULL ||
	    (desc_ptr == NULL && desc_size != 0)) {
		rc = -EINVAL;
		goto cleanup;
	}

	 
	if (hdev->dev_rsize == orig_desc_size) {
		hid_dbg(hdev,
			"device report descriptor matches the expected size, replacing\n");
		desc_copy_ptr = kmemdup(desc_ptr, desc_size, GFP_KERNEL);
		if (desc_copy_ptr == NULL) {
			rc = -ENOMEM;
			goto cleanup;
		}
		desc_copy_size = desc_size;
	} else {
		hid_dbg(hdev,
			"device report descriptor doesn't match the expected size (%u != %u), preserving\n",
			hdev->dev_rsize, orig_desc_size);
		desc_copy_ptr = NULL;
		desc_copy_size = 0;
	}

	 
	memset(params, 0, sizeof(*params));
	params->desc_ptr = desc_copy_ptr;
	desc_copy_ptr = NULL;
	params->desc_size = desc_copy_size;

	rc = 0;
cleanup:
	kfree(desc_copy_ptr);
	return rc;
}

 
static int uclogic_params_huion_init(struct uclogic_params *params,
				     struct hid_device *hdev)
{
	int rc;
	struct usb_device *udev;
	struct usb_interface *iface;
	__u8 bInterfaceNumber;
	bool found;
	 
	struct uclogic_params p = {0, };
	static const char transition_ver[] = "HUION_T153_160607";
	char *ver_ptr = NULL;
	const size_t ver_len = sizeof(transition_ver) + 1;
	__u8 *params_ptr = NULL;
	size_t params_len = 0;
	 
	const __u8 touch_ring_model_params_buf[] = {
		0x13, 0x03, 0x70, 0xC6, 0x00, 0x06, 0x7C, 0x00,
		0xFF, 0x1F, 0xD8, 0x13, 0x03, 0x0D, 0x10, 0x01,
		0x04, 0x3C, 0x3E
	};

	 
	if (params == NULL || hdev == NULL) {
		rc = -EINVAL;
		goto cleanup;
	}

	udev = hid_to_usb_dev(hdev);
	iface = to_usb_interface(hdev->dev.parent);
	bInterfaceNumber = iface->cur_altsetting->desc.bInterfaceNumber;

	 
	if (bInterfaceNumber == 1) {
		 
		p.pen.usage_invalid = true;
		goto output;
	 
	} else if (bInterfaceNumber != 0) {
		uclogic_params_init_invalid(&p);
		goto output;
	}

	 
	ver_ptr = kzalloc(ver_len, GFP_KERNEL);
	if (ver_ptr == NULL) {
		rc = -ENOMEM;
		goto cleanup;
	}
	rc = usb_string(udev, 201, ver_ptr, ver_len);
	if (rc == -EPIPE) {
		*ver_ptr = '\0';
	} else if (rc < 0) {
		hid_err(hdev,
			"failed retrieving Huion firmware version: %d\n", rc);
		goto cleanup;
	}

	 
	if (strcmp(ver_ptr, transition_ver) == 0) {
		hid_dbg(hdev,
			"transition firmware detected, not probing pen v2 parameters\n");
	} else {
		 
		rc = uclogic_params_pen_init_v2(&p.pen, &found,
						&params_ptr, &params_len,
						hdev);
		if (rc != 0) {
			hid_err(hdev,
				"failed probing pen v2 parameters: %d\n", rc);
			goto cleanup;
		} else if (found) {
			hid_dbg(hdev, "pen v2 parameters found\n");
			 
			rc = uclogic_params_frame_init_with_desc(
					&p.frame_list[0],
					uclogic_rdesc_v2_frame_buttons_arr,
					uclogic_rdesc_v2_frame_buttons_size,
					UCLOGIC_RDESC_V2_FRAME_BUTTONS_ID);
			if (rc != 0) {
				hid_err(hdev,
					"failed creating v2 frame button parameters: %d\n",
					rc);
				goto cleanup;
			}

			 
			p.pen.subreport_list[0].value = 0xe0;
			p.pen.subreport_list[0].id =
				UCLOGIC_RDESC_V2_FRAME_BUTTONS_ID;

			 
			if (params_ptr != NULL &&
			    params_len == sizeof(touch_ring_model_params_buf) &&
			    memcmp(params_ptr, touch_ring_model_params_buf,
				   params_len) == 0) {
				 
				rc = uclogic_params_frame_init_with_desc(
					&p.frame_list[1],
					uclogic_rdesc_v2_frame_touch_ring_arr,
					uclogic_rdesc_v2_frame_touch_ring_size,
					UCLOGIC_RDESC_V2_FRAME_TOUCH_ID);
				if (rc != 0) {
					hid_err(hdev,
						"failed creating v2 frame touch ring parameters: %d\n",
						rc);
					goto cleanup;
				}
				p.frame_list[1].suffix = "Touch Ring";
				p.frame_list[1].dev_id_byte =
					UCLOGIC_RDESC_V2_FRAME_TOUCH_DEV_ID_BYTE;
				p.frame_list[1].touch_byte = 5;
				p.frame_list[1].touch_max = 12;
				p.frame_list[1].touch_flip_at = 7;
			} else {
				 
				rc = uclogic_params_frame_init_with_desc(
					&p.frame_list[1],
					uclogic_rdesc_v2_frame_touch_strip_arr,
					uclogic_rdesc_v2_frame_touch_strip_size,
					UCLOGIC_RDESC_V2_FRAME_TOUCH_ID);
				if (rc != 0) {
					hid_err(hdev,
						"failed creating v2 frame touch strip parameters: %d\n",
						rc);
					goto cleanup;
				}
				p.frame_list[1].suffix = "Touch Strip";
				p.frame_list[1].dev_id_byte =
					UCLOGIC_RDESC_V2_FRAME_TOUCH_DEV_ID_BYTE;
				p.frame_list[1].touch_byte = 5;
				p.frame_list[1].touch_max = 8;
			}

			 
			p.pen.subreport_list[1].value = 0xf0;
			p.pen.subreport_list[1].id =
				UCLOGIC_RDESC_V2_FRAME_TOUCH_ID;

			 
			rc = uclogic_params_frame_init_with_desc(
					&p.frame_list[2],
					uclogic_rdesc_v2_frame_dial_arr,
					uclogic_rdesc_v2_frame_dial_size,
					UCLOGIC_RDESC_V2_FRAME_DIAL_ID);
			if (rc != 0) {
				hid_err(hdev,
					"failed creating v2 frame dial parameters: %d\n",
					rc);
				goto cleanup;
			}
			p.frame_list[2].suffix = "Dial";
			p.frame_list[2].dev_id_byte =
				UCLOGIC_RDESC_V2_FRAME_DIAL_DEV_ID_BYTE;
			p.frame_list[2].bitmap_dial_byte = 5;

			 
			p.pen.subreport_list[2].value = 0xf1;
			p.pen.subreport_list[2].id =
				UCLOGIC_RDESC_V2_FRAME_DIAL_ID;

			goto output;
		}
		hid_dbg(hdev, "pen v2 parameters not found\n");
	}

	 
	rc = uclogic_params_pen_init_v1(&p.pen, &found, hdev);
	if (rc != 0) {
		hid_err(hdev,
			"failed probing pen v1 parameters: %d\n", rc);
		goto cleanup;
	} else if (found) {
		hid_dbg(hdev, "pen v1 parameters found\n");
		 
		rc = uclogic_params_frame_init_v1(&p.frame_list[0],
						  &found, hdev);
		if (rc != 0) {
			hid_err(hdev, "v1 frame probing failed: %d\n", rc);
			goto cleanup;
		}
		hid_dbg(hdev, "frame v1 parameters%s found\n",
			(found ? "" : " not"));
		if (found) {
			 
			p.pen.subreport_list[0].value = 0xe0;
			p.pen.subreport_list[0].id =
				UCLOGIC_RDESC_V1_FRAME_ID;
		}
		goto output;
	}
	hid_dbg(hdev, "pen v1 parameters not found\n");

	uclogic_params_init_invalid(&p);

output:
	 
	memcpy(params, &p, sizeof(*params));
	memset(&p, 0, sizeof(p));
	rc = 0;
cleanup:
	kfree(params_ptr);
	kfree(ver_ptr);
	uclogic_params_cleanup(&p);
	return rc;
}

 
static int uclogic_probe_interface(struct hid_device *hdev, const u8 *magic_arr,
				   size_t magic_size, int endpoint)
{
	struct usb_device *udev;
	unsigned int pipe = 0;
	int sent;
	u8 *buf = NULL;
	int rc = 0;

	if (!hdev || !magic_arr) {
		rc = -EINVAL;
		goto cleanup;
	}

	buf = kmemdup(magic_arr, magic_size, GFP_KERNEL);
	if (!buf) {
		rc = -ENOMEM;
		goto cleanup;
	}

	udev = hid_to_usb_dev(hdev);
	pipe = usb_sndintpipe(udev, endpoint);

	rc = usb_interrupt_msg(udev, pipe, buf, magic_size, &sent, 1000);
	if (rc || sent != magic_size) {
		hid_err(hdev, "Interface probing failed: %d\n", rc);
		rc = -1;
		goto cleanup;
	}

	rc = 0;
cleanup:
	kfree(buf);
	return rc;
}

 
static int uclogic_params_parse_ugee_v2_desc(const __u8 *str_desc,
					     size_t str_desc_size,
					     s32 *desc_params,
					     size_t desc_params_size,
					     enum uclogic_params_frame_type *frame_type)
{
	s32 pen_x_lm, pen_y_lm;
	s32 pen_x_pm, pen_y_pm;
	s32 pen_pressure_lm;
	s32 frame_num_buttons;
	s32 resolution;

	 
	const int min_str_desc_size = 12;

	if (!str_desc || str_desc_size < min_str_desc_size)
		return -EINVAL;

	if (desc_params_size != UCLOGIC_RDESC_PH_ID_NUM)
		return -EINVAL;

	pen_x_lm = get_unaligned_le16(str_desc + 2);
	pen_y_lm = get_unaligned_le16(str_desc + 4);
	frame_num_buttons = str_desc[6];
	*frame_type = str_desc[7];
	pen_pressure_lm = get_unaligned_le16(str_desc + 8);

	resolution = get_unaligned_le16(str_desc + 10);
	if (resolution == 0) {
		pen_x_pm = 0;
		pen_y_pm = 0;
	} else {
		pen_x_pm = pen_x_lm * 1000 / resolution;
		pen_y_pm = pen_y_lm * 1000 / resolution;
	}

	desc_params[UCLOGIC_RDESC_PEN_PH_ID_X_LM] = pen_x_lm;
	desc_params[UCLOGIC_RDESC_PEN_PH_ID_X_PM] = pen_x_pm;
	desc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_LM] = pen_y_lm;
	desc_params[UCLOGIC_RDESC_PEN_PH_ID_Y_PM] = pen_y_pm;
	desc_params[UCLOGIC_RDESC_PEN_PH_ID_PRESSURE_LM] = pen_pressure_lm;
	desc_params[UCLOGIC_RDESC_FRAME_PH_ID_UM] = frame_num_buttons;

	return 0;
}

 
static int uclogic_params_ugee_v2_init_frame_buttons(struct uclogic_params *p,
						     const s32 *desc_params,
						     size_t desc_params_size)
{
	__u8 *rdesc_frame = NULL;
	int rc = 0;

	if (!p || desc_params_size != UCLOGIC_RDESC_PH_ID_NUM)
		return -EINVAL;

	rdesc_frame = uclogic_rdesc_template_apply(
				uclogic_rdesc_ugee_v2_frame_btn_template_arr,
				uclogic_rdesc_ugee_v2_frame_btn_template_size,
				desc_params, UCLOGIC_RDESC_PH_ID_NUM);
	if (!rdesc_frame)
		return -ENOMEM;

	rc = uclogic_params_frame_init_with_desc(&p->frame_list[0],
						 rdesc_frame,
						 uclogic_rdesc_ugee_v2_frame_btn_template_size,
						 UCLOGIC_RDESC_V1_FRAME_ID);
	kfree(rdesc_frame);
	return rc;
}

 
static int uclogic_params_ugee_v2_init_frame_dial(struct uclogic_params *p,
						  const s32 *desc_params,
						  size_t desc_params_size)
{
	__u8 *rdesc_frame = NULL;
	int rc = 0;

	if (!p || desc_params_size != UCLOGIC_RDESC_PH_ID_NUM)
		return -EINVAL;

	rdesc_frame = uclogic_rdesc_template_apply(
				uclogic_rdesc_ugee_v2_frame_dial_template_arr,
				uclogic_rdesc_ugee_v2_frame_dial_template_size,
				desc_params, UCLOGIC_RDESC_PH_ID_NUM);
	if (!rdesc_frame)
		return -ENOMEM;

	rc = uclogic_params_frame_init_with_desc(&p->frame_list[0],
						 rdesc_frame,
						 uclogic_rdesc_ugee_v2_frame_dial_template_size,
						 UCLOGIC_RDESC_V1_FRAME_ID);
	kfree(rdesc_frame);
	if (rc)
		return rc;

	p->frame_list[0].bitmap_dial_byte = 7;
	return 0;
}

 
static int uclogic_params_ugee_v2_init_frame_mouse(struct uclogic_params *p)
{
	int rc = 0;

	if (!p)
		return -EINVAL;

	rc = uclogic_params_frame_init_with_desc(&p->frame_list[1],
						 uclogic_rdesc_ugee_v2_frame_mouse_template_arr,
						 uclogic_rdesc_ugee_v2_frame_mouse_template_size,
						 UCLOGIC_RDESC_V1_FRAME_ID);
	return rc;
}

 
static bool uclogic_params_ugee_v2_has_battery(struct hid_device *hdev)
{
	struct uclogic_drvdata *drvdata = hid_get_drvdata(hdev);

	if (drvdata->quirks & UCLOGIC_BATTERY_QUIRK)
		return true;

	 
	if (hdev->vendor == USB_VENDOR_ID_UGEE &&
	    hdev->product == USB_DEVICE_ID_UGEE_XPPEN_TABLET_DECO_L) {
		struct usb_device *udev = hid_to_usb_dev(hdev);

		if (strstarts(udev->product, "Deco LW"))
			return true;
	}

	return false;
}

 
static int uclogic_params_ugee_v2_init_battery(struct hid_device *hdev,
					       struct uclogic_params *p)
{
	int rc = 0;

	if (!hdev || !p)
		return -EINVAL;

	 
	snprintf(hdev->uniq, sizeof(hdev->uniq), "%x-%x", hdev->vendor,
		 hdev->product);

	rc = uclogic_params_frame_init_with_desc(&p->frame_list[1],
						 uclogic_rdesc_ugee_v2_battery_template_arr,
						 uclogic_rdesc_ugee_v2_battery_template_size,
						 UCLOGIC_RDESC_UGEE_V2_BATTERY_ID);
	if (rc)
		return rc;

	p->frame_list[1].suffix = "Battery";
	p->pen.subreport_list[1].value = 0xf2;
	p->pen.subreport_list[1].id = UCLOGIC_RDESC_UGEE_V2_BATTERY_ID;

	return rc;
}

 
static void uclogic_params_ugee_v2_reconnect_work(struct work_struct *work)
{
	struct uclogic_raw_event_hook *event_hook;

	event_hook = container_of(work, struct uclogic_raw_event_hook, work);
	uclogic_probe_interface(event_hook->hdev, uclogic_ugee_v2_probe_arr,
				uclogic_ugee_v2_probe_size,
				uclogic_ugee_v2_probe_endpoint);
}

 
static int uclogic_params_ugee_v2_init_event_hooks(struct hid_device *hdev,
						   struct uclogic_params *p)
{
	struct uclogic_raw_event_hook *event_hook;
	__u8 reconnect_event[] = {
		 
		0x02, 0xF8, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	if (!p)
		return -EINVAL;

	 
	if (!uclogic_params_ugee_v2_has_battery(hdev))
		return 0;

	p->event_hooks = kzalloc(sizeof(*p->event_hooks), GFP_KERNEL);
	if (!p->event_hooks)
		return -ENOMEM;

	INIT_LIST_HEAD(&p->event_hooks->list);

	event_hook = kzalloc(sizeof(*event_hook), GFP_KERNEL);
	if (!event_hook)
		return -ENOMEM;

	INIT_WORK(&event_hook->work, uclogic_params_ugee_v2_reconnect_work);
	event_hook->hdev = hdev;
	event_hook->size = ARRAY_SIZE(reconnect_event);
	event_hook->event = kmemdup(reconnect_event, event_hook->size, GFP_KERNEL);
	if (!event_hook->event)
		return -ENOMEM;

	list_add_tail(&event_hook->list, &p->event_hooks->list);

	return 0;
}

 
static int uclogic_params_ugee_v2_init(struct uclogic_params *params,
				       struct hid_device *hdev)
{
	int rc = 0;
	struct uclogic_drvdata *drvdata;
	struct usb_interface *iface;
	__u8 bInterfaceNumber;
	const int str_desc_len = 12;
	__u8 *str_desc = NULL;
	__u8 *rdesc_pen = NULL;
	s32 desc_params[UCLOGIC_RDESC_PH_ID_NUM];
	enum uclogic_params_frame_type frame_type;
	 
	struct uclogic_params p = {0, };

	if (!params || !hdev) {
		rc = -EINVAL;
		goto cleanup;
	}

	drvdata = hid_get_drvdata(hdev);
	iface = to_usb_interface(hdev->dev.parent);
	bInterfaceNumber = iface->cur_altsetting->desc.bInterfaceNumber;

	if (bInterfaceNumber == 0) {
		rc = uclogic_params_ugee_v2_init_frame_mouse(&p);
		if (rc)
			goto cleanup;

		goto output;
	}

	if (bInterfaceNumber != 2) {
		uclogic_params_init_invalid(&p);
		goto output;
	}

	 
	rc = uclogic_probe_interface(hdev, uclogic_ugee_v2_probe_arr,
				     uclogic_ugee_v2_probe_size,
				     uclogic_ugee_v2_probe_endpoint);
	if (rc) {
		uclogic_params_init_invalid(&p);
		goto output;
	}

	 
	rc = uclogic_params_get_str_desc(&str_desc, hdev, 100, str_desc_len);
	if (rc != str_desc_len) {
		hid_err(hdev, "failed retrieving pen and frame parameters: %d\n", rc);
		uclogic_params_init_invalid(&p);
		goto output;
	}

	rc = uclogic_params_parse_ugee_v2_desc(str_desc, str_desc_len,
					       desc_params,
					       ARRAY_SIZE(desc_params),
					       &frame_type);
	if (rc)
		goto cleanup;

	kfree(str_desc);
	str_desc = NULL;

	 
	rdesc_pen = uclogic_rdesc_template_apply(
				uclogic_rdesc_ugee_v2_pen_template_arr,
				uclogic_rdesc_ugee_v2_pen_template_size,
				desc_params, ARRAY_SIZE(desc_params));
	if (!rdesc_pen) {
		rc = -ENOMEM;
		goto cleanup;
	}

	p.pen.desc_ptr = rdesc_pen;
	p.pen.desc_size = uclogic_rdesc_ugee_v2_pen_template_size;
	p.pen.id = 0x02;
	p.pen.subreport_list[0].value = 0xf0;
	p.pen.subreport_list[0].id = UCLOGIC_RDESC_V1_FRAME_ID;

	 
	if (drvdata->quirks & UCLOGIC_MOUSE_FRAME_QUIRK)
		frame_type = UCLOGIC_PARAMS_FRAME_MOUSE;

	switch (frame_type) {
	case UCLOGIC_PARAMS_FRAME_DIAL:
	case UCLOGIC_PARAMS_FRAME_MOUSE:
		rc = uclogic_params_ugee_v2_init_frame_dial(&p, desc_params,
							    ARRAY_SIZE(desc_params));
		break;
	case UCLOGIC_PARAMS_FRAME_BUTTONS:
	default:
		rc = uclogic_params_ugee_v2_init_frame_buttons(&p, desc_params,
							       ARRAY_SIZE(desc_params));
		break;
	}

	if (rc)
		goto cleanup;

	 
	if (uclogic_params_ugee_v2_has_battery(hdev)) {
		rc = uclogic_params_ugee_v2_init_battery(hdev, &p);
		if (rc) {
			hid_err(hdev, "error initializing battery: %d\n", rc);
			goto cleanup;
		}
	}

	 
	rc = uclogic_params_ugee_v2_init_event_hooks(hdev, &p);
	if (rc) {
		hid_err(hdev, "error initializing event hook list: %d\n", rc);
		goto cleanup;
	}

output:
	 
	memcpy(params, &p, sizeof(*params));
	memset(&p, 0, sizeof(p));
	rc = 0;
cleanup:
	kfree(str_desc);
	uclogic_params_cleanup(&p);
	return rc;
}

 
int uclogic_params_init(struct uclogic_params *params,
			struct hid_device *hdev)
{
	int rc;
	struct usb_device *udev;
	__u8  bNumInterfaces;
	struct usb_interface *iface;
	__u8 bInterfaceNumber;
	bool found;
	 
	struct uclogic_params p = {0, };

	 
	if (params == NULL || hdev == NULL || !hid_is_usb(hdev)) {
		rc = -EINVAL;
		goto cleanup;
	}

	udev = hid_to_usb_dev(hdev);
	bNumInterfaces = udev->config->desc.bNumInterfaces;
	iface = to_usb_interface(hdev->dev.parent);
	bInterfaceNumber = iface->cur_altsetting->desc.bInterfaceNumber;

	 
#define WITH_OPT_DESC(_orig_desc_token, _new_desc_token) \
	uclogic_params_init_with_opt_desc(                  \
		&p, hdev,                                   \
		UCLOGIC_RDESC_##_orig_desc_token##_SIZE,    \
		uclogic_rdesc_##_new_desc_token##_arr,      \
		uclogic_rdesc_##_new_desc_token##_size)

#define VID_PID(_vid, _pid) \
	(((__u32)(_vid) << 16) | ((__u32)(_pid) & U16_MAX))

	 

	switch (VID_PID(hdev->vendor, hdev->product)) {
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_TABLET_PF1209):
		rc = WITH_OPT_DESC(PF1209_ORIG, pf1209_fixed);
		if (rc != 0)
			goto cleanup;
		break;
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_TABLET_WP4030U):
		rc = WITH_OPT_DESC(WPXXXXU_ORIG, wp4030u_fixed);
		if (rc != 0)
			goto cleanup;
		break;
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_TABLET_WP5540U):
		if (hdev->dev_rsize == UCLOGIC_RDESC_WP5540U_V2_ORIG_SIZE) {
			if (bInterfaceNumber == 0) {
				 
				rc = uclogic_params_pen_init_v1(&p.pen,
								&found, hdev);
				if (rc != 0) {
					hid_err(hdev,
						"pen probing failed: %d\n",
						rc);
					goto cleanup;
				}
				if (!found) {
					hid_warn(hdev,
						 "pen parameters not found");
				}
			} else {
				uclogic_params_init_invalid(&p);
			}
		} else {
			rc = WITH_OPT_DESC(WPXXXXU_ORIG, wp5540u_fixed);
			if (rc != 0)
				goto cleanup;
		}
		break;
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_TABLET_WP8060U):
		rc = WITH_OPT_DESC(WPXXXXU_ORIG, wp8060u_fixed);
		if (rc != 0)
			goto cleanup;
		break;
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_TABLET_WP1062):
		rc = WITH_OPT_DESC(WP1062_ORIG, wp1062_fixed);
		if (rc != 0)
			goto cleanup;
		break;
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_WIRELESS_TABLET_TWHL850):
		switch (bInterfaceNumber) {
		case 0:
			rc = WITH_OPT_DESC(TWHL850_ORIG0, twhl850_fixed0);
			if (rc != 0)
				goto cleanup;
			break;
		case 1:
			rc = WITH_OPT_DESC(TWHL850_ORIG1, twhl850_fixed1);
			if (rc != 0)
				goto cleanup;
			break;
		case 2:
			rc = WITH_OPT_DESC(TWHL850_ORIG2, twhl850_fixed2);
			if (rc != 0)
				goto cleanup;
			break;
		}
		break;
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_TABLET_TWHA60):
		 
		if (bNumInterfaces != 3) {
			switch (bInterfaceNumber) {
			case 0:
				rc = WITH_OPT_DESC(TWHA60_ORIG0,
							twha60_fixed0);
				if (rc != 0)
					goto cleanup;
				break;
			case 1:
				rc = WITH_OPT_DESC(TWHA60_ORIG1,
							twha60_fixed1);
				if (rc != 0)
					goto cleanup;
				break;
			}
			break;
		}
		fallthrough;
	case VID_PID(USB_VENDOR_ID_HUION,
		     USB_DEVICE_ID_HUION_TABLET):
	case VID_PID(USB_VENDOR_ID_HUION,
		     USB_DEVICE_ID_HUION_TABLET2):
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_HUION_TABLET):
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_YIYNOVA_TABLET):
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_81):
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_DRAWIMAGE_G3):
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_45):
	case VID_PID(USB_VENDOR_ID_UCLOGIC,
		     USB_DEVICE_ID_UCLOGIC_UGEE_TABLET_47):
		rc = uclogic_params_huion_init(&p, hdev);
		if (rc != 0)
			goto cleanup;
		break;
	case VID_PID(USB_VENDOR_ID_UGTIZER,
		     USB_DEVICE_ID_UGTIZER_TABLET_GP0610):
	case VID_PID(USB_VENDOR_ID_UGTIZER,
		     USB_DEVICE_ID_UGTIZER_TABLET_GT5040):
	case VID_PID(USB_VENDOR_ID_UGEE,
		     USB_DEVICE_ID_UGEE_XPPEN_TABLET_G540):
	case VID_PID(USB_VENDOR_ID_UGEE,
		     USB_DEVICE_ID_UGEE_XPPEN_TABLET_G640):
	case VID_PID(USB_VENDOR_ID_UGEE,
		     USB_DEVICE_ID_UGEE_XPPEN_TABLET_STAR06):
	case VID_PID(USB_VENDOR_ID_UGEE,
		     USB_DEVICE_ID_UGEE_TABLET_RAINBOW_CV720):
		 
		if (bInterfaceNumber == 1) {
			 
			rc = uclogic_params_pen_init_v1(&p.pen, &found, hdev);
			if (rc != 0) {
				hid_err(hdev, "pen probing failed: %d\n", rc);
				goto cleanup;
			}
			if (!found) {
				hid_warn(hdev, "pen parameters not found");
				uclogic_params_init_invalid(&p);
			}
		} else {
			uclogic_params_init_invalid(&p);
		}
		break;
	case VID_PID(USB_VENDOR_ID_UGEE,
		     USB_DEVICE_ID_UGEE_XPPEN_TABLET_DECO01):
		 
		if (bInterfaceNumber == 1) {
			 
			rc = uclogic_params_pen_init_v1(&p.pen, &found, hdev);
			if (rc != 0) {
				hid_err(hdev, "pen probing failed: %d\n", rc);
				goto cleanup;
			}
			 
			rc = uclogic_params_frame_init_with_desc(
				&p.frame_list[0],
				uclogic_rdesc_xppen_deco01_frame_arr,
				uclogic_rdesc_xppen_deco01_frame_size,
				0);
			if (rc != 0)
				goto cleanup;
		} else {
			uclogic_params_init_invalid(&p);
		}
		break;
	case VID_PID(USB_VENDOR_ID_UGEE,
		     USB_DEVICE_ID_UGEE_PARBLO_A610_PRO):
	case VID_PID(USB_VENDOR_ID_UGEE,
		     USB_DEVICE_ID_UGEE_XPPEN_TABLET_DECO01_V2):
	case VID_PID(USB_VENDOR_ID_UGEE,
		     USB_DEVICE_ID_UGEE_XPPEN_TABLET_DECO_L):
	case VID_PID(USB_VENDOR_ID_UGEE,
		     USB_DEVICE_ID_UGEE_XPPEN_TABLET_DECO_PRO_MW):
	case VID_PID(USB_VENDOR_ID_UGEE,
		     USB_DEVICE_ID_UGEE_XPPEN_TABLET_DECO_PRO_S):
	case VID_PID(USB_VENDOR_ID_UGEE,
		     USB_DEVICE_ID_UGEE_XPPEN_TABLET_DECO_PRO_SW):
		rc = uclogic_params_ugee_v2_init(&p, hdev);
		if (rc != 0)
			goto cleanup;
		break;
	case VID_PID(USB_VENDOR_ID_TRUST,
		     USB_DEVICE_ID_TRUST_PANORA_TABLET):
	case VID_PID(USB_VENDOR_ID_UGEE,
		     USB_DEVICE_ID_UGEE_TABLET_G5):
		 
		if (bInterfaceNumber != 1) {
			uclogic_params_init_invalid(&p);
			break;
		}

		rc = uclogic_params_pen_init_v1(&p.pen, &found, hdev);
		if (rc != 0) {
			hid_err(hdev, "pen probing failed: %d\n", rc);
			goto cleanup;
		} else if (found) {
			rc = uclogic_params_frame_init_with_desc(
				&p.frame_list[0],
				uclogic_rdesc_ugee_g5_frame_arr,
				uclogic_rdesc_ugee_g5_frame_size,
				UCLOGIC_RDESC_UGEE_G5_FRAME_ID);
			if (rc != 0) {
				hid_err(hdev,
					"failed creating frame parameters: %d\n",
					rc);
				goto cleanup;
			}
			p.frame_list[0].re_lsb =
				UCLOGIC_RDESC_UGEE_G5_FRAME_RE_LSB;
			p.frame_list[0].dev_id_byte =
				UCLOGIC_RDESC_UGEE_G5_FRAME_DEV_ID_BYTE;
		} else {
			hid_warn(hdev, "pen parameters not found");
			uclogic_params_init_invalid(&p);
		}

		break;
	case VID_PID(USB_VENDOR_ID_UGEE,
		     USB_DEVICE_ID_UGEE_TABLET_EX07S):
		 
		if (bInterfaceNumber != 1) {
			uclogic_params_init_invalid(&p);
			break;
		}

		rc = uclogic_params_pen_init_v1(&p.pen, &found, hdev);
		if (rc != 0) {
			hid_err(hdev, "pen probing failed: %d\n", rc);
			goto cleanup;
		} else if (found) {
			rc = uclogic_params_frame_init_with_desc(
				&p.frame_list[0],
				uclogic_rdesc_ugee_ex07_frame_arr,
				uclogic_rdesc_ugee_ex07_frame_size,
				0);
			if (rc != 0) {
				hid_err(hdev,
					"failed creating frame parameters: %d\n",
					rc);
				goto cleanup;
			}
		} else {
			hid_warn(hdev, "pen parameters not found");
			uclogic_params_init_invalid(&p);
		}

		break;
	}

#undef VID_PID
#undef WITH_OPT_DESC

	 
	memcpy(params, &p, sizeof(*params));
	memset(&p, 0, sizeof(p));
	rc = 0;
cleanup:
	uclogic_params_cleanup(&p);
	return rc;
}

#ifdef CONFIG_HID_KUNIT_TEST
#include "hid-uclogic-params-test.c"
#endif
