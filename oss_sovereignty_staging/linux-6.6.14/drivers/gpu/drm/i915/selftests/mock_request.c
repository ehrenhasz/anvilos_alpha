 

#include "gem/selftests/igt_gem_utils.h"
#include "gt/mock_engine.h"

#include "mock_request.h"

struct i915_request *
mock_request(struct intel_context *ce, unsigned long delay)
{
	struct i915_request *request;

	 
	request = intel_context_create_request(ce);
	if (IS_ERR(request))
		return NULL;

	request->mock.delay = delay;
	return request;
}

bool mock_cancel_request(struct i915_request *request)
{
	struct mock_engine *engine =
		container_of(request->engine, typeof(*engine), base);
	bool was_queued;

	spin_lock_irq(&engine->hw_lock);
	was_queued = !list_empty(&request->mock.link);
	list_del_init(&request->mock.link);
	spin_unlock_irq(&engine->hw_lock);

	if (was_queued)
		i915_request_unsubmit(request);

	return was_queued;
}
