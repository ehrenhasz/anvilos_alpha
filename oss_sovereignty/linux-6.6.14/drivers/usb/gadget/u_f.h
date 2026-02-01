
 

#ifndef __U_F_H__
#define __U_F_H__

#include <linux/usb/gadget.h>
#include <linux/overflow.h>

 
#define vla_group(groupname) size_t groupname##__next = 0
#define vla_group_size(groupname) groupname##__next

#define vla_item(groupname, type, name, n) \
	size_t groupname##_##name##__offset = ({			       \
		size_t offset = 0;					       \
		if (groupname##__next != SIZE_MAX) {			       \
			size_t align_mask = __alignof__(type) - 1;	       \
			size_t size = array_size(n, sizeof(type));	       \
			offset = (groupname##__next + align_mask) &	       \
				  ~align_mask;				       \
			if (check_add_overflow(offset, size,		       \
					       &groupname##__next)) {          \
				groupname##__next = SIZE_MAX;		       \
				offset = 0;				       \
			}						       \
		}							       \
		offset;							       \
	})

#define vla_item_with_sz(groupname, type, name, n) \
	size_t groupname##_##name##__sz = array_size(n, sizeof(type));	        \
	size_t groupname##_##name##__offset = ({			        \
		size_t offset = 0;						\
		if (groupname##__next != SIZE_MAX) {				\
			size_t align_mask = __alignof__(type) - 1;		\
			offset = (groupname##__next + align_mask) &		\
				  ~align_mask;					\
			if (check_add_overflow(offset, groupname##_##name##__sz,\
							&groupname##__next)) {	\
				groupname##__next = SIZE_MAX;			\
				offset = 0;					\
			}							\
		}								\
		offset;								\
	})

#define vla_ptr(ptr, groupname, name) \
	((void *) ((char *)ptr + groupname##_##name##__offset))

struct usb_ep;
struct usb_request;

 
struct usb_request *alloc_ep_req(struct usb_ep *ep, size_t len);

 
static inline void free_ep_req(struct usb_ep *ep, struct usb_request *req)
{
	WARN_ON(req->buf == NULL);
	kfree(req->buf);
	req->buf = NULL;
	usb_ep_free_request(ep, req);
}

#endif  
