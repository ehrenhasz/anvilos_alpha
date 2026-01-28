#ifndef __MOCK_REQUEST__
#define __MOCK_REQUEST__
#include <linux/list.h>
#include "../i915_request.h"
struct i915_request *
mock_request(struct intel_context *ce, unsigned long delay);
bool mock_cancel_request(struct i915_request *request);
#endif  
