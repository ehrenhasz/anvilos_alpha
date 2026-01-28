#ifndef U_UVC_H
#define U_UVC_H
#include <linux/mutex.h>
#include <linux/usb/composite.h>
#include <linux/usb/video.h>
#define fi_to_f_uvc_opts(f)	container_of(f, struct f_uvc_opts, func_inst)
struct f_uvc_opts {
	struct usb_function_instance			func_inst;
	unsigned int					streaming_interval;
	unsigned int					streaming_maxpacket;
	unsigned int					streaming_maxburst;
	unsigned int					control_interface;
	unsigned int					streaming_interface;
	char						function_name[32];
	unsigned int					last_unit_id;
	bool						enable_interrupt_ep;
	const struct uvc_descriptor_header * const	*fs_control;
	const struct uvc_descriptor_header * const	*ss_control;
	const struct uvc_descriptor_header * const	*fs_streaming;
	const struct uvc_descriptor_header * const	*hs_streaming;
	const struct uvc_descriptor_header * const	*ss_streaming;
	struct uvc_camera_terminal_descriptor		uvc_camera_terminal;
	struct uvc_processing_unit_descriptor		uvc_processing;
	struct uvc_output_terminal_descriptor		uvc_output_terminal;
	struct uvc_descriptor_header			*uvc_fs_control_cls[5];
	struct uvc_descriptor_header			*uvc_ss_control_cls[5];
	struct list_head				extension_units;
	struct uvc_descriptor_header			**uvc_fs_streaming_cls;
	struct uvc_descriptor_header			**uvc_hs_streaming_cls;
	struct uvc_descriptor_header			**uvc_ss_streaming_cls;
	u8						iad_index;
	u8						vs0_index;
	u8						vs1_index;
	struct mutex			lock;
	int				refcnt;
	struct uvcg_streaming_header	*header;
};
#endif  
