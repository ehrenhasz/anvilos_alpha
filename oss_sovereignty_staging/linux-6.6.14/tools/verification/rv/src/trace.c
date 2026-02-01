
 

#include <sys/sendfile.h>
#include <tracefs.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <rv.h>
#include <trace.h>
#include <utils.h>

 
static struct tracefs_instance *create_instance(char *instance_name)
{
	return tracefs_instance_create(instance_name);
}

 
static void destroy_instance(struct tracefs_instance *inst)
{
	tracefs_instance_destroy(inst);
	tracefs_instance_free(inst);
}

 
int
collect_registered_events(struct tep_event *event, struct tep_record *record,
			  int cpu, void *context)
{
	struct trace_instance *trace = context;
	struct trace_seq *s = trace->seq;

	if (should_stop())
		return 1;

	if (!event->handler)
		return 0;

	event->handler(s, record, event, context);

	return 0;
}

 
void trace_instance_destroy(struct trace_instance *trace)
{
	if (trace->inst) {
		destroy_instance(trace->inst);
		trace->inst = NULL;
	}

	if (trace->seq) {
		free(trace->seq);
		trace->seq = NULL;
	}

	if (trace->tep) {
		tep_free(trace->tep);
		trace->tep = NULL;
	}
}

 
int trace_instance_init(struct trace_instance *trace, char *name)
{
	trace->seq = calloc(1, sizeof(*trace->seq));
	if (!trace->seq)
		goto out_err;

	trace_seq_init(trace->seq);

	trace->inst = create_instance(name);
	if (!trace->inst)
		goto out_err;

	trace->tep = tracefs_local_events(NULL);
	if (!trace->tep)
		goto out_err;

	 
	tracefs_trace_off(trace->inst);

	return 0;

out_err:
	trace_instance_destroy(trace);
	return 1;
}

 
int trace_instance_start(struct trace_instance *trace)
{
	return tracefs_trace_on(trace->inst);
}
