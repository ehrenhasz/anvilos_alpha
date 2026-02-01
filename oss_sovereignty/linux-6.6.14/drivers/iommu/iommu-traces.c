
 

#include <linux/string.h>
#include <linux/types.h>

#define CREATE_TRACE_POINTS
#include <trace/events/iommu.h>

 
EXPORT_TRACEPOINT_SYMBOL_GPL(add_device_to_group);
EXPORT_TRACEPOINT_SYMBOL_GPL(remove_device_from_group);

 
EXPORT_TRACEPOINT_SYMBOL_GPL(attach_device_to_domain);

 
EXPORT_TRACEPOINT_SYMBOL_GPL(map);
EXPORT_TRACEPOINT_SYMBOL_GPL(unmap);

 
EXPORT_TRACEPOINT_SYMBOL_GPL(io_page_fault);
