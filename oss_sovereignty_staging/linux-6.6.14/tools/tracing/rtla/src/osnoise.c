
 

#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include "osnoise.h"
#include "utils.h"

 
char *osnoise_get_cpus(struct osnoise_context *context)
{
	if (context->curr_cpus)
		return context->curr_cpus;

	if (context->orig_cpus)
		return context->orig_cpus;

	context->orig_cpus = tracefs_instance_file_read(NULL, "osnoise/cpus", NULL);

	 
	return context->orig_cpus;
}

 
int osnoise_set_cpus(struct osnoise_context *context, char *cpus)
{
	char *orig_cpus = osnoise_get_cpus(context);
	char buffer[1024];
	int retval;

	if (!orig_cpus)
		return -1;

	context->curr_cpus = strdup(cpus);
	if (!context->curr_cpus)
		return -1;

	snprintf(buffer, 1024, "%s\n", cpus);

	debug_msg("setting cpus to %s from %s", cpus, context->orig_cpus);

	retval = tracefs_instance_file_write(NULL, "osnoise/cpus", buffer);
	if (retval < 0) {
		free(context->curr_cpus);
		context->curr_cpus = NULL;
		return -1;
	}

	return 0;
}

 
void osnoise_restore_cpus(struct osnoise_context *context)
{
	int retval;

	if (!context->orig_cpus)
		return;

	if (!context->curr_cpus)
		return;

	 
	if (!strcmp(context->orig_cpus, context->curr_cpus))
		goto out_done;

	debug_msg("restoring cpus to %s", context->orig_cpus);

	retval = tracefs_instance_file_write(NULL, "osnoise/cpus", context->orig_cpus);
	if (retval < 0)
		err_msg("could not restore original osnoise cpus\n");

out_done:
	free(context->curr_cpus);
	context->curr_cpus = NULL;
}

 
void osnoise_put_cpus(struct osnoise_context *context)
{
	osnoise_restore_cpus(context);

	if (!context->orig_cpus)
		return;

	free(context->orig_cpus);
	context->orig_cpus = NULL;
}

 
static long long osnoise_read_ll_config(char *rel_path)
{
	long long retval;
	char *buffer;

	buffer = tracefs_instance_file_read(NULL, rel_path, NULL);
	if (!buffer)
		return -1;

	 
	retval = get_llong_from_str(buffer);

	debug_msg("reading %s returned %lld\n", rel_path, retval);

	free(buffer);

	return retval;
}

 
static long long osnoise_write_ll_config(char *rel_path, long long value)
{
	char buffer[BUFF_U64_STR_SIZE];
	long long retval;

	snprintf(buffer, sizeof(buffer), "%lld\n", value);

	debug_msg("setting %s to %lld\n", rel_path, value);

	retval = tracefs_instance_file_write(NULL, rel_path, buffer);
	return retval;
}

 
unsigned long long osnoise_get_runtime(struct osnoise_context *context)
{
	long long runtime_us;

	if (context->runtime_us != OSNOISE_TIME_INIT_VAL)
		return context->runtime_us;

	if (context->orig_runtime_us != OSNOISE_TIME_INIT_VAL)
		return context->orig_runtime_us;

	runtime_us = osnoise_read_ll_config("osnoise/runtime_us");
	if (runtime_us < 0)
		goto out_err;

	context->orig_runtime_us = runtime_us;
	return runtime_us;

out_err:
	return OSNOISE_TIME_INIT_VAL;
}

 
unsigned long long osnoise_get_period(struct osnoise_context *context)
{
	long long period_us;

	if (context->period_us != OSNOISE_TIME_INIT_VAL)
		return context->period_us;

	if (context->orig_period_us != OSNOISE_TIME_INIT_VAL)
		return context->orig_period_us;

	period_us = osnoise_read_ll_config("osnoise/period_us");
	if (period_us < 0)
		goto out_err;

	context->orig_period_us = period_us;
	return period_us;

out_err:
	return OSNOISE_TIME_INIT_VAL;
}

static int __osnoise_write_runtime(struct osnoise_context *context,
				   unsigned long long runtime)
{
	int retval;

	if (context->orig_runtime_us == OSNOISE_TIME_INIT_VAL)
		return -1;

	retval = osnoise_write_ll_config("osnoise/runtime_us", runtime);
	if (retval < 0)
		return -1;

	context->runtime_us = runtime;
	return 0;
}

static int __osnoise_write_period(struct osnoise_context *context,
				  unsigned long long period)
{
	int retval;

	if (context->orig_period_us == OSNOISE_TIME_INIT_VAL)
		return -1;

	retval = osnoise_write_ll_config("osnoise/period_us", period);
	if (retval < 0)
		return -1;

	context->period_us = period;
	return 0;
}

 
int osnoise_set_runtime_period(struct osnoise_context *context,
			       unsigned long long runtime,
			       unsigned long long period)
{
	unsigned long long curr_runtime_us;
	unsigned long long curr_period_us;
	int retval;

	if (!period && !runtime)
		return 0;

	curr_runtime_us = osnoise_get_runtime(context);
	curr_period_us = osnoise_get_period(context);

	 
	if (curr_period_us == OSNOISE_TIME_INIT_VAL || curr_runtime_us == OSNOISE_TIME_INIT_VAL)
		return -1;

	if (!period) {
		if (runtime > curr_period_us)
			return -1;
		return __osnoise_write_runtime(context, runtime);
	} else if (!runtime) {
		if (period < curr_runtime_us)
			return -1;
		return __osnoise_write_period(context, period);
	}

	if (runtime > curr_period_us) {
		retval = __osnoise_write_period(context, period);
		if (retval)
			return -1;
		retval = __osnoise_write_runtime(context, runtime);
		if (retval)
			return -1;
	} else {
		retval = __osnoise_write_runtime(context, runtime);
		if (retval)
			return -1;
		retval = __osnoise_write_period(context, period);
		if (retval)
			return -1;
	}

	return 0;
}

 
void osnoise_restore_runtime_period(struct osnoise_context *context)
{
	unsigned long long orig_runtime = context->orig_runtime_us;
	unsigned long long orig_period = context->orig_period_us;
	unsigned long long curr_runtime = context->runtime_us;
	unsigned long long curr_period = context->period_us;
	int retval;

	if ((orig_runtime == OSNOISE_TIME_INIT_VAL) && (orig_period == OSNOISE_TIME_INIT_VAL))
		return;

	if ((orig_period == curr_period) && (orig_runtime == curr_runtime))
		goto out_done;

	retval = osnoise_set_runtime_period(context, orig_runtime, orig_period);
	if (retval)
		err_msg("Could not restore original osnoise runtime/period\n");

out_done:
	context->runtime_us = OSNOISE_TIME_INIT_VAL;
	context->period_us = OSNOISE_TIME_INIT_VAL;
}

 
void osnoise_put_runtime_period(struct osnoise_context *context)
{
	osnoise_restore_runtime_period(context);

	if (context->orig_runtime_us != OSNOISE_TIME_INIT_VAL)
		context->orig_runtime_us = OSNOISE_TIME_INIT_VAL;

	if (context->orig_period_us != OSNOISE_TIME_INIT_VAL)
		context->orig_period_us = OSNOISE_TIME_INIT_VAL;
}

 
static long long
osnoise_get_timerlat_period_us(struct osnoise_context *context)
{
	long long timerlat_period_us;

	if (context->timerlat_period_us != OSNOISE_TIME_INIT_VAL)
		return context->timerlat_period_us;

	if (context->orig_timerlat_period_us != OSNOISE_TIME_INIT_VAL)
		return context->orig_timerlat_period_us;

	timerlat_period_us = osnoise_read_ll_config("osnoise/timerlat_period_us");
	if (timerlat_period_us < 0)
		goto out_err;

	context->orig_timerlat_period_us = timerlat_period_us;
	return timerlat_period_us;

out_err:
	return OSNOISE_TIME_INIT_VAL;
}

 
int osnoise_set_timerlat_period_us(struct osnoise_context *context, long long timerlat_period_us)
{
	long long curr_timerlat_period_us = osnoise_get_timerlat_period_us(context);
	int retval;

	if (curr_timerlat_period_us == OSNOISE_TIME_INIT_VAL)
		return -1;

	retval = osnoise_write_ll_config("osnoise/timerlat_period_us", timerlat_period_us);
	if (retval < 0)
		return -1;

	context->timerlat_period_us = timerlat_period_us;

	return 0;
}

 
void osnoise_restore_timerlat_period_us(struct osnoise_context *context)
{
	int retval;

	if (context->orig_timerlat_period_us == OSNOISE_TIME_INIT_VAL)
		return;

	if (context->orig_timerlat_period_us == context->timerlat_period_us)
		goto out_done;

	retval = osnoise_write_ll_config("osnoise/timerlat_period_us", context->orig_timerlat_period_us);
	if (retval < 0)
		err_msg("Could not restore original osnoise timerlat_period_us\n");

out_done:
	context->timerlat_period_us = OSNOISE_TIME_INIT_VAL;
}

 
void osnoise_put_timerlat_period_us(struct osnoise_context *context)
{
	osnoise_restore_timerlat_period_us(context);

	if (context->orig_timerlat_period_us == OSNOISE_TIME_INIT_VAL)
		return;

	context->orig_timerlat_period_us = OSNOISE_TIME_INIT_VAL;
}

 
static long long
osnoise_get_stop_us(struct osnoise_context *context)
{
	long long stop_us;

	if (context->stop_us != OSNOISE_OPTION_INIT_VAL)
		return context->stop_us;

	if (context->orig_stop_us != OSNOISE_OPTION_INIT_VAL)
		return context->orig_stop_us;

	stop_us = osnoise_read_ll_config("osnoise/stop_tracing_us");
	if (stop_us < 0)
		goto out_err;

	context->orig_stop_us = stop_us;
	return stop_us;

out_err:
	return OSNOISE_OPTION_INIT_VAL;
}

 
int osnoise_set_stop_us(struct osnoise_context *context, long long stop_us)
{
	long long curr_stop_us = osnoise_get_stop_us(context);
	int retval;

	if (curr_stop_us == OSNOISE_OPTION_INIT_VAL)
		return -1;

	retval = osnoise_write_ll_config("osnoise/stop_tracing_us", stop_us);
	if (retval < 0)
		return -1;

	context->stop_us = stop_us;

	return 0;
}

 
void osnoise_restore_stop_us(struct osnoise_context *context)
{
	int retval;

	if (context->orig_stop_us == OSNOISE_OPTION_INIT_VAL)
		return;

	if (context->orig_stop_us == context->stop_us)
		goto out_done;

	retval = osnoise_write_ll_config("osnoise/stop_tracing_us", context->orig_stop_us);
	if (retval < 0)
		err_msg("Could not restore original osnoise stop_us\n");

out_done:
	context->stop_us = OSNOISE_OPTION_INIT_VAL;
}

 
void osnoise_put_stop_us(struct osnoise_context *context)
{
	osnoise_restore_stop_us(context);

	if (context->orig_stop_us == OSNOISE_OPTION_INIT_VAL)
		return;

	context->orig_stop_us = OSNOISE_OPTION_INIT_VAL;
}

 
static long long
osnoise_get_stop_total_us(struct osnoise_context *context)
{
	long long stop_total_us;

	if (context->stop_total_us != OSNOISE_OPTION_INIT_VAL)
		return context->stop_total_us;

	if (context->orig_stop_total_us != OSNOISE_OPTION_INIT_VAL)
		return context->orig_stop_total_us;

	stop_total_us = osnoise_read_ll_config("osnoise/stop_tracing_total_us");
	if (stop_total_us < 0)
		goto out_err;

	context->orig_stop_total_us = stop_total_us;
	return stop_total_us;

out_err:
	return OSNOISE_OPTION_INIT_VAL;
}

 
int osnoise_set_stop_total_us(struct osnoise_context *context, long long stop_total_us)
{
	long long curr_stop_total_us = osnoise_get_stop_total_us(context);
	int retval;

	if (curr_stop_total_us == OSNOISE_OPTION_INIT_VAL)
		return -1;

	retval = osnoise_write_ll_config("osnoise/stop_tracing_total_us", stop_total_us);
	if (retval < 0)
		return -1;

	context->stop_total_us = stop_total_us;

	return 0;
}

 
void osnoise_restore_stop_total_us(struct osnoise_context *context)
{
	int retval;

	if (context->orig_stop_total_us == OSNOISE_OPTION_INIT_VAL)
		return;

	if (context->orig_stop_total_us == context->stop_total_us)
		goto out_done;

	retval = osnoise_write_ll_config("osnoise/stop_tracing_total_us",
			context->orig_stop_total_us);
	if (retval < 0)
		err_msg("Could not restore original osnoise stop_total_us\n");

out_done:
	context->stop_total_us = OSNOISE_OPTION_INIT_VAL;
}

 
void osnoise_put_stop_total_us(struct osnoise_context *context)
{
	osnoise_restore_stop_total_us(context);

	if (context->orig_stop_total_us == OSNOISE_OPTION_INIT_VAL)
		return;

	context->orig_stop_total_us = OSNOISE_OPTION_INIT_VAL;
}

 
static long long
osnoise_get_print_stack(struct osnoise_context *context)
{
	long long print_stack;

	if (context->print_stack != OSNOISE_OPTION_INIT_VAL)
		return context->print_stack;

	if (context->orig_print_stack != OSNOISE_OPTION_INIT_VAL)
		return context->orig_print_stack;

	print_stack = osnoise_read_ll_config("osnoise/print_stack");
	if (print_stack < 0)
		goto out_err;

	context->orig_print_stack = print_stack;
	return print_stack;

out_err:
	return OSNOISE_OPTION_INIT_VAL;
}

 
int osnoise_set_print_stack(struct osnoise_context *context, long long print_stack)
{
	long long curr_print_stack = osnoise_get_print_stack(context);
	int retval;

	if (curr_print_stack == OSNOISE_OPTION_INIT_VAL)
		return -1;

	retval = osnoise_write_ll_config("osnoise/print_stack", print_stack);
	if (retval < 0)
		return -1;

	context->print_stack = print_stack;

	return 0;
}

 
void osnoise_restore_print_stack(struct osnoise_context *context)
{
	int retval;

	if (context->orig_print_stack == OSNOISE_OPTION_INIT_VAL)
		return;

	if (context->orig_print_stack == context->print_stack)
		goto out_done;

	retval = osnoise_write_ll_config("osnoise/print_stack", context->orig_print_stack);
	if (retval < 0)
		err_msg("Could not restore original osnoise print_stack\n");

out_done:
	context->print_stack = OSNOISE_OPTION_INIT_VAL;
}

 
void osnoise_put_print_stack(struct osnoise_context *context)
{
	osnoise_restore_print_stack(context);

	if (context->orig_print_stack == OSNOISE_OPTION_INIT_VAL)
		return;

	context->orig_print_stack = OSNOISE_OPTION_INIT_VAL;
}

 
static long long
osnoise_get_tracing_thresh(struct osnoise_context *context)
{
	long long tracing_thresh;

	if (context->tracing_thresh != OSNOISE_OPTION_INIT_VAL)
		return context->tracing_thresh;

	if (context->orig_tracing_thresh != OSNOISE_OPTION_INIT_VAL)
		return context->orig_tracing_thresh;

	tracing_thresh = osnoise_read_ll_config("tracing_thresh");
	if (tracing_thresh < 0)
		goto out_err;

	context->orig_tracing_thresh = tracing_thresh;
	return tracing_thresh;

out_err:
	return OSNOISE_OPTION_INIT_VAL;
}

 
int osnoise_set_tracing_thresh(struct osnoise_context *context, long long tracing_thresh)
{
	long long curr_tracing_thresh = osnoise_get_tracing_thresh(context);
	int retval;

	if (curr_tracing_thresh == OSNOISE_OPTION_INIT_VAL)
		return -1;

	retval = osnoise_write_ll_config("tracing_thresh", tracing_thresh);
	if (retval < 0)
		return -1;

	context->tracing_thresh = tracing_thresh;

	return 0;
}

 
void osnoise_restore_tracing_thresh(struct osnoise_context *context)
{
	int retval;

	if (context->orig_tracing_thresh == OSNOISE_OPTION_INIT_VAL)
		return;

	if (context->orig_tracing_thresh == context->tracing_thresh)
		goto out_done;

	retval = osnoise_write_ll_config("tracing_thresh", context->orig_tracing_thresh);
	if (retval < 0)
		err_msg("Could not restore original tracing_thresh\n");

out_done:
	context->tracing_thresh = OSNOISE_OPTION_INIT_VAL;
}

 
void osnoise_put_tracing_thresh(struct osnoise_context *context)
{
	osnoise_restore_tracing_thresh(context);

	if (context->orig_tracing_thresh == OSNOISE_OPTION_INIT_VAL)
		return;

	context->orig_tracing_thresh = OSNOISE_OPTION_INIT_VAL;
}

static int osnoise_options_get_option(char *option)
{
	char *options = tracefs_instance_file_read(NULL, "osnoise/options", NULL);
	char no_option[128];
	int retval = 0;
	char *opt;

	if (!options)
		return OSNOISE_OPTION_INIT_VAL;

	 
	snprintf(no_option, sizeof(no_option), "NO_%s", option);

	opt = strstr(options, no_option);
	if (opt)
		goto out_free;

	 
	opt = strstr(options, option);
	if (opt)
		retval = 1;
	else
		retval = OSNOISE_OPTION_INIT_VAL;

out_free:
	free(options);
	return retval;
}

static int osnoise_options_set_option(char *option, bool onoff)
{
	char no_option[128];

	if (onoff)
		return tracefs_instance_file_write(NULL, "osnoise/options", option);

	snprintf(no_option, sizeof(no_option), "NO_%s", option);

	return tracefs_instance_file_write(NULL, "osnoise/options", no_option);
}

static int osnoise_get_irq_disable(struct osnoise_context *context)
{
	if (context->opt_irq_disable != OSNOISE_OPTION_INIT_VAL)
		return context->opt_irq_disable;

	if (context->orig_opt_irq_disable != OSNOISE_OPTION_INIT_VAL)
		return context->orig_opt_irq_disable;

	context->orig_opt_irq_disable = osnoise_options_get_option("OSNOISE_IRQ_DISABLE");

	return context->orig_opt_irq_disable;
}

int osnoise_set_irq_disable(struct osnoise_context *context, bool onoff)
{
	int opt_irq_disable = osnoise_get_irq_disable(context);
	int retval;

	if (opt_irq_disable == OSNOISE_OPTION_INIT_VAL)
		return -1;

	if (opt_irq_disable == onoff)
		return 0;

	retval = osnoise_options_set_option("OSNOISE_IRQ_DISABLE", onoff);
	if (retval < 0)
		return -1;

	context->opt_irq_disable = onoff;

	return 0;
}

static void osnoise_restore_irq_disable(struct osnoise_context *context)
{
	int retval;

	if (context->orig_opt_irq_disable == OSNOISE_OPTION_INIT_VAL)
		return;

	if (context->orig_opt_irq_disable == context->opt_irq_disable)
		goto out_done;

	retval = osnoise_options_set_option("OSNOISE_IRQ_DISABLE", context->orig_opt_irq_disable);
	if (retval < 0)
		err_msg("Could not restore original OSNOISE_IRQ_DISABLE option\n");

out_done:
	context->orig_opt_irq_disable = OSNOISE_OPTION_INIT_VAL;
}

static void osnoise_put_irq_disable(struct osnoise_context *context)
{
	osnoise_restore_irq_disable(context);

	if (context->orig_opt_irq_disable == OSNOISE_OPTION_INIT_VAL)
		return;

	context->orig_opt_irq_disable = OSNOISE_OPTION_INIT_VAL;
}

static int osnoise_get_workload(struct osnoise_context *context)
{
	if (context->opt_workload != OSNOISE_OPTION_INIT_VAL)
		return context->opt_workload;

	if (context->orig_opt_workload != OSNOISE_OPTION_INIT_VAL)
		return context->orig_opt_workload;

	context->orig_opt_workload = osnoise_options_get_option("OSNOISE_WORKLOAD");

	return context->orig_opt_workload;
}

int osnoise_set_workload(struct osnoise_context *context, bool onoff)
{
	int opt_workload = osnoise_get_workload(context);
	int retval;

	if (opt_workload == OSNOISE_OPTION_INIT_VAL)
		return -1;

	if (opt_workload == onoff)
		return 0;

	retval = osnoise_options_set_option("OSNOISE_WORKLOAD", onoff);
	if (retval < 0)
		return -1;

	context->opt_workload = onoff;

	return 0;
}

static void osnoise_restore_workload(struct osnoise_context *context)
{
	int retval;

	if (context->orig_opt_workload == OSNOISE_OPTION_INIT_VAL)
		return;

	if (context->orig_opt_workload == context->opt_workload)
		goto out_done;

	retval = osnoise_options_set_option("OSNOISE_WORKLOAD", context->orig_opt_workload);
	if (retval < 0)
		err_msg("Could not restore original OSNOISE_WORKLOAD option\n");

out_done:
	context->orig_opt_workload = OSNOISE_OPTION_INIT_VAL;
}

static void osnoise_put_workload(struct osnoise_context *context)
{
	osnoise_restore_workload(context);

	if (context->orig_opt_workload == OSNOISE_OPTION_INIT_VAL)
		return;

	context->orig_opt_workload = OSNOISE_OPTION_INIT_VAL;
}

 
int enable_osnoise(struct trace_instance *trace)
{
	return enable_tracer_by_name(trace->inst, "osnoise");
}

 
int enable_timerlat(struct trace_instance *trace)
{
	return enable_tracer_by_name(trace->inst, "timerlat");
}

enum {
	FLAG_CONTEXT_NEWLY_CREATED	= (1 << 0),
	FLAG_CONTEXT_DELETED		= (1 << 1),
};

 
int osnoise_get_context(struct osnoise_context *context)
{
	int ret;

	if (context->flags & FLAG_CONTEXT_DELETED) {
		ret = -1;
	} else {
		context->ref++;
		ret = 0;
	}

	return ret;
}

 
struct osnoise_context *osnoise_context_alloc(void)
{
	struct osnoise_context *context;

	context = calloc(1, sizeof(*context));
	if (!context)
		return NULL;

	context->orig_stop_us		= OSNOISE_OPTION_INIT_VAL;
	context->stop_us		= OSNOISE_OPTION_INIT_VAL;

	context->orig_stop_total_us	= OSNOISE_OPTION_INIT_VAL;
	context->stop_total_us		= OSNOISE_OPTION_INIT_VAL;

	context->orig_print_stack	= OSNOISE_OPTION_INIT_VAL;
	context->print_stack		= OSNOISE_OPTION_INIT_VAL;

	context->orig_tracing_thresh	= OSNOISE_OPTION_INIT_VAL;
	context->tracing_thresh		= OSNOISE_OPTION_INIT_VAL;

	context->orig_opt_irq_disable	= OSNOISE_OPTION_INIT_VAL;
	context->opt_irq_disable	= OSNOISE_OPTION_INIT_VAL;

	context->orig_opt_workload	= OSNOISE_OPTION_INIT_VAL;
	context->opt_workload		= OSNOISE_OPTION_INIT_VAL;

	osnoise_get_context(context);

	return context;
}

 
void osnoise_put_context(struct osnoise_context *context)
{
	if (--context->ref < 1)
		context->flags |= FLAG_CONTEXT_DELETED;

	if (!(context->flags & FLAG_CONTEXT_DELETED))
		return;

	osnoise_put_cpus(context);
	osnoise_put_runtime_period(context);
	osnoise_put_stop_us(context);
	osnoise_put_stop_total_us(context);
	osnoise_put_timerlat_period_us(context);
	osnoise_put_print_stack(context);
	osnoise_put_tracing_thresh(context);
	osnoise_put_irq_disable(context);
	osnoise_put_workload(context);

	free(context);
}

 
void osnoise_destroy_tool(struct osnoise_tool *top)
{
	if (!top)
		return;

	trace_instance_destroy(&top->trace);

	if (top->context)
		osnoise_put_context(top->context);

	free(top);
}

 
struct osnoise_tool *osnoise_init_tool(char *tool_name)
{
	struct osnoise_tool *top;
	int retval;

	top = calloc(1, sizeof(*top));
	if (!top)
		return NULL;

	top->context = osnoise_context_alloc();
	if (!top->context)
		goto out_err;

	retval = trace_instance_init(&top->trace, tool_name);
	if (retval)
		goto out_err;

	return top;
out_err:
	osnoise_destroy_tool(top);
	return NULL;
}

 
struct osnoise_tool *osnoise_init_trace_tool(char *tracer)
{
	struct osnoise_tool *trace;
	int retval;

	trace = osnoise_init_tool("osnoise_trace");
	if (!trace)
		return NULL;

	retval = tracefs_event_enable(trace->trace.inst, "osnoise", NULL);
	if (retval < 0 && !errno) {
		err_msg("Could not find osnoise events\n");
		goto out_err;
	}

	retval = enable_tracer_by_name(trace->trace.inst, tracer);
	if (retval) {
		err_msg("Could not enable %s tracer for tracing\n", tracer);
		goto out_err;
	}

	return trace;
out_err:
	osnoise_destroy_tool(trace);
	return NULL;
}

static void osnoise_usage(int err)
{
	int i;

	static const char *msg[] = {
		"",
		"osnoise version " VERSION,
		"",
		"  usage: [rtla] osnoise [MODE] ...",
		"",
		"  modes:",
		"     top   - prints the summary from osnoise tracer",
		"     hist  - prints a histogram of osnoise samples",
		"",
		"if no MODE is given, the top mode is called, passing the arguments",
		NULL,
	};

	for (i = 0; msg[i]; i++)
		fprintf(stderr, "%s\n", msg[i]);
	exit(err);
}

int osnoise_main(int argc, char *argv[])
{
	if (argc == 0)
		goto usage;

	 
	if (argc == 1) {
		osnoise_top_main(argc, argv);
		exit(0);
	}

	if ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0)) {
		osnoise_usage(0);
	} else if (strncmp(argv[1], "-", 1) == 0) {
		 
		osnoise_top_main(argc, argv);
		exit(0);
	} else if (strcmp(argv[1], "top") == 0) {
		osnoise_top_main(argc-1, &argv[1]);
		exit(0);
	} else if (strcmp(argv[1], "hist") == 0) {
		osnoise_hist_main(argc-1, &argv[1]);
		exit(0);
	}

usage:
	osnoise_usage(1);
	exit(1);
}

int hwnoise_main(int argc, char *argv[])
{
	osnoise_top_main(argc, argv);
	exit(0);
}
