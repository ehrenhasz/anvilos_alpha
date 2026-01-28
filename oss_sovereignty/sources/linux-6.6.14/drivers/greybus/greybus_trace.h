

#undef TRACE_SYSTEM
#define TRACE_SYSTEM greybus

#if !defined(_TRACE_GREYBUS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_GREYBUS_H

#include <linux/tracepoint.h>

struct gb_message;
struct gb_operation;
struct gb_connection;
struct gb_bundle;
struct gb_host_device;

DECLARE_EVENT_CLASS(gb_message,

	TP_PROTO(struct gb_message *message),

	TP_ARGS(message),

	TP_STRUCT__entry(
		__field(u16, size)
		__field(u16, operation_id)
		__field(u8, type)
		__field(u8, result)
	),

	TP_fast_assign(
		__entry->size = le16_to_cpu(message->header->size);
		__entry->operation_id =
			le16_to_cpu(message->header->operation_id);
		__entry->type = message->header->type;
		__entry->result = message->header->result;
	),

	TP_printk("size=%u operation_id=0x%04x type=0x%02x result=0x%02x",
		  __entry->size, __entry->operation_id,
		  __entry->type, __entry->result)
);

#define DEFINE_MESSAGE_EVENT(name)					\
		DEFINE_EVENT(gb_message, name,				\
				TP_PROTO(struct gb_message *message),	\
				TP_ARGS(message))


DEFINE_MESSAGE_EVENT(gb_message_send);


DEFINE_MESSAGE_EVENT(gb_message_recv_request);


DEFINE_MESSAGE_EVENT(gb_message_recv_response);


DEFINE_MESSAGE_EVENT(gb_message_cancel_outgoing);


DEFINE_MESSAGE_EVENT(gb_message_cancel_incoming);


DEFINE_MESSAGE_EVENT(gb_message_submit);

#undef DEFINE_MESSAGE_EVENT

DECLARE_EVENT_CLASS(gb_operation,

	TP_PROTO(struct gb_operation *operation),

	TP_ARGS(operation),

	TP_STRUCT__entry(
		__field(u16, cport_id)	
		__field(u16, id)	
		__field(u8, type)
		__field(unsigned long, flags)
		__field(int, active)
		__field(int, waiters)
		__field(int, errno)
	),

	TP_fast_assign(
		__entry->cport_id = operation->connection->hd_cport_id;
		__entry->id = operation->id;
		__entry->type = operation->type;
		__entry->flags = operation->flags;
		__entry->active = operation->active;
		__entry->waiters = atomic_read(&operation->waiters);
		__entry->errno = operation->errno;
	),

	TP_printk("id=%04x type=0x%02x cport_id=%04x flags=0x%lx active=%d waiters=%d errno=%d",
		  __entry->id, __entry->cport_id, __entry->type, __entry->flags,
		  __entry->active, __entry->waiters, __entry->errno)
);

#define DEFINE_OPERATION_EVENT(name)					\
		DEFINE_EVENT(gb_operation, name,			\
				TP_PROTO(struct gb_operation *operation), \
				TP_ARGS(operation))


DEFINE_OPERATION_EVENT(gb_operation_create);


DEFINE_OPERATION_EVENT(gb_operation_create_core);


DEFINE_OPERATION_EVENT(gb_operation_create_incoming);


DEFINE_OPERATION_EVENT(gb_operation_destroy);


DEFINE_OPERATION_EVENT(gb_operation_get_active);


DEFINE_OPERATION_EVENT(gb_operation_put_active);

#undef DEFINE_OPERATION_EVENT

DECLARE_EVENT_CLASS(gb_connection,

	TP_PROTO(struct gb_connection *connection),

	TP_ARGS(connection),

	TP_STRUCT__entry(
		__field(int, hd_bus_id)
		__field(u8, bundle_id)
		
		__dynamic_array(char, name, sizeof(connection->name))
		__field(enum gb_connection_state, state)
		__field(unsigned long, flags)
	),

	TP_fast_assign(
		__entry->hd_bus_id = connection->hd->bus_id;
		__entry->bundle_id = connection->bundle ?
				connection->bundle->id : BUNDLE_ID_NONE;
		memcpy(__get_str(name), connection->name,
					sizeof(connection->name));
		__entry->state = connection->state;
		__entry->flags = connection->flags;
	),

	TP_printk("hd_bus_id=%d bundle_id=0x%02x name=\"%s\" state=%u flags=0x%lx",
		  __entry->hd_bus_id, __entry->bundle_id, __get_str(name),
		  (unsigned int)__entry->state, __entry->flags)
);

#define DEFINE_CONNECTION_EVENT(name)					\
		DEFINE_EVENT(gb_connection, name,			\
				TP_PROTO(struct gb_connection *connection), \
				TP_ARGS(connection))


DEFINE_CONNECTION_EVENT(gb_connection_create);


DEFINE_CONNECTION_EVENT(gb_connection_release);


DEFINE_CONNECTION_EVENT(gb_connection_get);


DEFINE_CONNECTION_EVENT(gb_connection_put);


DEFINE_CONNECTION_EVENT(gb_connection_enable);


DEFINE_CONNECTION_EVENT(gb_connection_disable);

#undef DEFINE_CONNECTION_EVENT

DECLARE_EVENT_CLASS(gb_bundle,

	TP_PROTO(struct gb_bundle *bundle),

	TP_ARGS(bundle),

	TP_STRUCT__entry(
		__field(u8, intf_id)
		__field(u8, id)
		__field(u8, class)
		__field(size_t, num_cports)
	),

	TP_fast_assign(
		__entry->intf_id = bundle->intf->interface_id;
		__entry->id = bundle->id;
		__entry->class = bundle->class;
		__entry->num_cports = bundle->num_cports;
	),

	TP_printk("intf_id=0x%02x id=%02x class=0x%02x num_cports=%zu",
		  __entry->intf_id, __entry->id, __entry->class,
		  __entry->num_cports)
);

#define DEFINE_BUNDLE_EVENT(name)					\
		DEFINE_EVENT(gb_bundle, name,			\
				TP_PROTO(struct gb_bundle *bundle), \
				TP_ARGS(bundle))


DEFINE_BUNDLE_EVENT(gb_bundle_create);


DEFINE_BUNDLE_EVENT(gb_bundle_release);


DEFINE_BUNDLE_EVENT(gb_bundle_add);


DEFINE_BUNDLE_EVENT(gb_bundle_destroy);

#undef DEFINE_BUNDLE_EVENT

DECLARE_EVENT_CLASS(gb_interface,

	TP_PROTO(struct gb_interface *intf),

	TP_ARGS(intf),

	TP_STRUCT__entry(
		__field(u8, module_id)
		__field(u8, id)		
		__field(u8, device_id)
		__field(int, disconnected)	
		__field(int, ejected)		
		__field(int, active)		
		__field(int, enabled)		
		__field(int, mode_switch)	
	),

	TP_fast_assign(
		__entry->module_id = intf->module->module_id;
		__entry->id = intf->interface_id;
		__entry->device_id = intf->device_id;
		__entry->disconnected = intf->disconnected;
		__entry->ejected = intf->ejected;
		__entry->active = intf->active;
		__entry->enabled = intf->enabled;
		__entry->mode_switch = intf->mode_switch;
	),

	TP_printk("intf_id=%u device_id=%u module_id=%u D=%d J=%d A=%d E=%d M=%d",
		__entry->id, __entry->device_id, __entry->module_id,
		__entry->disconnected, __entry->ejected, __entry->active,
		__entry->enabled, __entry->mode_switch)
);

#define DEFINE_INTERFACE_EVENT(name)					\
		DEFINE_EVENT(gb_interface, name,			\
				TP_PROTO(struct gb_interface *intf),	\
				TP_ARGS(intf))


DEFINE_INTERFACE_EVENT(gb_interface_create);


DEFINE_INTERFACE_EVENT(gb_interface_release);


DEFINE_INTERFACE_EVENT(gb_interface_add);


DEFINE_INTERFACE_EVENT(gb_interface_del);


DEFINE_INTERFACE_EVENT(gb_interface_activate);


DEFINE_INTERFACE_EVENT(gb_interface_deactivate);


DEFINE_INTERFACE_EVENT(gb_interface_enable);


DEFINE_INTERFACE_EVENT(gb_interface_disable);

#undef DEFINE_INTERFACE_EVENT

DECLARE_EVENT_CLASS(gb_module,

	TP_PROTO(struct gb_module *module),

	TP_ARGS(module),

	TP_STRUCT__entry(
		__field(int, hd_bus_id)
		__field(u8, module_id)
		__field(size_t, num_interfaces)
		__field(int, disconnected)	
	),

	TP_fast_assign(
		__entry->hd_bus_id = module->hd->bus_id;
		__entry->module_id = module->module_id;
		__entry->num_interfaces = module->num_interfaces;
		__entry->disconnected = module->disconnected;
	),

	TP_printk("hd_bus_id=%d module_id=%u num_interfaces=%zu disconnected=%d",
		__entry->hd_bus_id, __entry->module_id,
		__entry->num_interfaces, __entry->disconnected)
);

#define DEFINE_MODULE_EVENT(name)					\
		DEFINE_EVENT(gb_module, name,				\
				TP_PROTO(struct gb_module *module),	\
				TP_ARGS(module))


DEFINE_MODULE_EVENT(gb_module_create);


DEFINE_MODULE_EVENT(gb_module_release);


DEFINE_MODULE_EVENT(gb_module_add);


DEFINE_MODULE_EVENT(gb_module_del);

#undef DEFINE_MODULE_EVENT

DECLARE_EVENT_CLASS(gb_host_device,

	TP_PROTO(struct gb_host_device *hd),

	TP_ARGS(hd),

	TP_STRUCT__entry(
		__field(int, bus_id)
		__field(size_t, num_cports)
		__field(size_t, buffer_size_max)
	),

	TP_fast_assign(
		__entry->bus_id = hd->bus_id;
		__entry->num_cports = hd->num_cports;
		__entry->buffer_size_max = hd->buffer_size_max;
	),

	TP_printk("bus_id=%d num_cports=%zu mtu=%zu",
		__entry->bus_id, __entry->num_cports,
		__entry->buffer_size_max)
);

#define DEFINE_HD_EVENT(name)						\
		DEFINE_EVENT(gb_host_device, name,			\
				TP_PROTO(struct gb_host_device *hd),	\
				TP_ARGS(hd))


DEFINE_HD_EVENT(gb_hd_create);


DEFINE_HD_EVENT(gb_hd_release);


DEFINE_HD_EVENT(gb_hd_add);


DEFINE_HD_EVENT(gb_hd_del);


DEFINE_HD_EVENT(gb_hd_in);

#undef DEFINE_HD_EVENT

#endif 


#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .


#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE greybus_trace
#include <trace/define_trace.h>

