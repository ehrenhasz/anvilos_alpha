#ifndef _TB_CFG
#define _TB_CFG
#include <linux/kref.h>
#include <linux/thunderbolt.h>
#include "nhi.h"
#include "tb_msgs.h"
struct tb_ctl;
typedef bool (*event_cb)(void *data, enum tb_cfg_pkg_type type,
			 const void *buf, size_t size);
struct tb_ctl *tb_ctl_alloc(struct tb_nhi *nhi, int timeout_msec, event_cb cb,
			    void *cb_data);
void tb_ctl_start(struct tb_ctl *ctl);
void tb_ctl_stop(struct tb_ctl *ctl);
void tb_ctl_free(struct tb_ctl *ctl);
struct tb_cfg_result {
	u64 response_route;
	u32 response_port;  
	int err;  
	enum tb_cfg_error tb_error;  
};
struct ctl_pkg {
	struct tb_ctl *ctl;
	void *buffer;
	struct ring_frame frame;
};
struct tb_cfg_request {
	struct kref kref;
	struct tb_ctl *ctl;
	const void *request;
	size_t request_size;
	enum tb_cfg_pkg_type request_type;
	void *response;
	size_t response_size;
	enum tb_cfg_pkg_type response_type;
	size_t npackets;
	bool (*match)(const struct tb_cfg_request *req,
		      const struct ctl_pkg *pkg);
	bool (*copy)(struct tb_cfg_request *req, const struct ctl_pkg *pkg);
	void (*callback)(void *callback_data);
	void *callback_data;
	unsigned long flags;
	struct work_struct work;
	struct tb_cfg_result result;
	struct list_head list;
};
#define TB_CFG_REQUEST_ACTIVE		0
#define TB_CFG_REQUEST_CANCELED		1
struct tb_cfg_request *tb_cfg_request_alloc(void);
void tb_cfg_request_get(struct tb_cfg_request *req);
void tb_cfg_request_put(struct tb_cfg_request *req);
int tb_cfg_request(struct tb_ctl *ctl, struct tb_cfg_request *req,
		   void (*callback)(void *), void *callback_data);
void tb_cfg_request_cancel(struct tb_cfg_request *req, int err);
struct tb_cfg_result tb_cfg_request_sync(struct tb_ctl *ctl,
			struct tb_cfg_request *req, int timeout_msec);
static inline u64 tb_cfg_get_route(const struct tb_cfg_header *header)
{
	return (u64) header->route_hi << 32 | header->route_lo;
}
static inline struct tb_cfg_header tb_cfg_make_header(u64 route)
{
	struct tb_cfg_header header = {
		.route_hi = route >> 32,
		.route_lo = route,
	};
	WARN_ON(tb_cfg_get_route(&header) != route);
	return header;
}
int tb_cfg_ack_notification(struct tb_ctl *ctl, u64 route,
			    const struct cfg_error_pkg *error);
int tb_cfg_ack_plug(struct tb_ctl *ctl, u64 route, u32 port, bool unplug);
struct tb_cfg_result tb_cfg_reset(struct tb_ctl *ctl, u64 route);
struct tb_cfg_result tb_cfg_read_raw(struct tb_ctl *ctl, void *buffer,
				     u64 route, u32 port,
				     enum tb_cfg_space space, u32 offset,
				     u32 length, int timeout_msec);
struct tb_cfg_result tb_cfg_write_raw(struct tb_ctl *ctl, const void *buffer,
				      u64 route, u32 port,
				      enum tb_cfg_space space, u32 offset,
				      u32 length, int timeout_msec);
int tb_cfg_read(struct tb_ctl *ctl, void *buffer, u64 route, u32 port,
		enum tb_cfg_space space, u32 offset, u32 length);
int tb_cfg_write(struct tb_ctl *ctl, const void *buffer, u64 route, u32 port,
		 enum tb_cfg_space space, u32 offset, u32 length);
int tb_cfg_get_upstream_port(struct tb_ctl *ctl, u64 route);
#endif
