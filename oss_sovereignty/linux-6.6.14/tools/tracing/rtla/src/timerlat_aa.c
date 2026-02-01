
 

#include <stdlib.h>
#include <errno.h>
#include "utils.h"
#include "osnoise.h"
#include "timerlat.h"
#include <unistd.h>

enum timelat_state {
	TIMERLAT_INIT = 0,
	TIMERLAT_WAITING_IRQ,
	TIMERLAT_WAITING_THREAD,
};

#define MAX_COMM		24

 
struct timerlat_aa_data {
	 
	int			curr_state;

	 
	unsigned long long	tlat_irq_seqnum;
	unsigned long long	tlat_irq_latency;
	unsigned long long	tlat_irq_timstamp;

	 
	unsigned long long	tlat_thread_seqnum;
	unsigned long long	tlat_thread_latency;
	unsigned long long	tlat_thread_timstamp;

	 
	unsigned long long	run_thread_pid;
	char			run_thread_comm[MAX_COMM];
	unsigned long long	thread_blocking_duration;
	unsigned long long	max_exit_idle_latency;

	 
	unsigned long long	timer_irq_start_time;
	unsigned long long	timer_irq_start_delay;
	unsigned long long	timer_irq_duration;
	unsigned long long	timer_exit_from_idle;

	 
	unsigned long long	prev_irq_duration;
	unsigned long long	prev_irq_timstamp;

	 
	unsigned long long	thread_nmi_sum;
	unsigned long long	thread_irq_sum;
	unsigned long long	thread_softirq_sum;
	unsigned long long	thread_thread_sum;

	 
	struct trace_seq	*prev_irqs_seq;
	struct trace_seq	*nmi_seq;
	struct trace_seq	*irqs_seq;
	struct trace_seq	*softirqs_seq;
	struct trace_seq	*threads_seq;
	struct trace_seq	*stack_seq;

	 
	char			current_comm[MAX_COMM];
	unsigned long long	current_pid;

	 
	unsigned long long	kworker;
	unsigned long long	kworker_func;
};

 
struct timerlat_aa_context {
	int nr_cpus;
	int dump_tasks;

	 
	struct timerlat_aa_data *taa_data;

	 
	struct osnoise_tool *tool;
};

 
static struct timerlat_aa_context *__timerlat_aa_ctx;

static struct timerlat_aa_context *timerlat_aa_get_ctx(void)
{
	return __timerlat_aa_ctx;
}

 
static struct timerlat_aa_data
*timerlat_aa_get_data(struct timerlat_aa_context *taa_ctx, int cpu)
{
	return &taa_ctx->taa_data[cpu];
}

 
static int timerlat_aa_irq_latency(struct timerlat_aa_data *taa_data,
				   struct trace_seq *s, struct tep_record *record,
				   struct tep_event *event)
{
	 
	taa_data->curr_state = TIMERLAT_WAITING_THREAD;
	taa_data->tlat_irq_timstamp = record->ts;

	 
	taa_data->thread_nmi_sum = 0;
	taa_data->thread_irq_sum = 0;
	taa_data->thread_softirq_sum = 0;
	taa_data->thread_thread_sum = 0;
	taa_data->thread_blocking_duration = 0;
	taa_data->timer_irq_start_time = 0;
	taa_data->timer_irq_duration = 0;
	taa_data->timer_exit_from_idle = 0;

	 
	trace_seq_reset(taa_data->nmi_seq);
	trace_seq_reset(taa_data->irqs_seq);
	trace_seq_reset(taa_data->softirqs_seq);
	trace_seq_reset(taa_data->threads_seq);

	 
	tep_get_field_val(s, event, "timer_latency", record, &taa_data->tlat_irq_latency, 1);
	tep_get_field_val(s, event, "seqnum", record, &taa_data->tlat_irq_seqnum, 1);

	 
	tep_get_common_field_val(s, event, "common_pid", record, &taa_data->run_thread_pid, 1);

	 
	if (taa_data->run_thread_pid)
		return 0;

	 
	if (taa_data->tlat_irq_latency < taa_data->max_exit_idle_latency)
		return 0;

	 
	if (taa_data->tlat_irq_timstamp - taa_data->tlat_irq_latency
	    < taa_data->prev_irq_timstamp + taa_data->prev_irq_duration)
		return 0;

	taa_data->max_exit_idle_latency = taa_data->tlat_irq_latency;

	return 0;
}

 
static int timerlat_aa_thread_latency(struct timerlat_aa_data *taa_data,
				      struct trace_seq *s, struct tep_record *record,
				      struct tep_event *event)
{
	 
	taa_data->curr_state = TIMERLAT_WAITING_IRQ;
	taa_data->tlat_thread_timstamp = record->ts;

	 
	tep_get_field_val(s, event, "timer_latency", record, &taa_data->tlat_thread_latency, 1);
	tep_get_field_val(s, event, "seqnum", record, &taa_data->tlat_thread_seqnum, 1);

	return 0;
}

 
static int timerlat_aa_handler(struct trace_seq *s, struct tep_record *record,
			struct tep_event *event, void *context)
{
	struct timerlat_aa_context *taa_ctx = timerlat_aa_get_ctx();
	struct timerlat_aa_data *taa_data = timerlat_aa_get_data(taa_ctx, record->cpu);
	unsigned long long thread;

	if (!taa_data)
		return -1;

	tep_get_field_val(s, event, "context", record, &thread, 1);
	if (!thread)
		return timerlat_aa_irq_latency(taa_data, s, record, event);
	else
		return timerlat_aa_thread_latency(taa_data, s, record, event);
}

 
static int timerlat_aa_nmi_handler(struct trace_seq *s, struct tep_record *record,
				   struct tep_event *event, void *context)
{
	struct timerlat_aa_context *taa_ctx = timerlat_aa_get_ctx();
	struct timerlat_aa_data *taa_data = timerlat_aa_get_data(taa_ctx, record->cpu);
	unsigned long long duration;
	unsigned long long start;

	tep_get_field_val(s, event, "duration", record, &duration, 1);
	tep_get_field_val(s, event, "start", record, &start, 1);

	if (taa_data->curr_state == TIMERLAT_WAITING_IRQ) {
		taa_data->prev_irq_duration = duration;
		taa_data->prev_irq_timstamp = start;

		trace_seq_reset(taa_data->prev_irqs_seq);
		trace_seq_printf(taa_data->prev_irqs_seq, "\t%24s	\t\t\t%9.2f us\n",
			 "nmi", ns_to_usf(duration));
		return 0;
	}

	taa_data->thread_nmi_sum += duration;
	trace_seq_printf(taa_data->nmi_seq, "	%24s	\t\t\t%9.2f us\n",
		 "nmi", ns_to_usf(duration));

	return 0;
}

 
static int timerlat_aa_irq_handler(struct trace_seq *s, struct tep_record *record,
				   struct tep_event *event, void *context)
{
	struct timerlat_aa_context *taa_ctx = timerlat_aa_get_ctx();
	struct timerlat_aa_data *taa_data = timerlat_aa_get_data(taa_ctx, record->cpu);
	unsigned long long expected_start;
	unsigned long long duration;
	unsigned long long vector;
	unsigned long long start;
	char *desc;
	int val;

	tep_get_field_val(s, event, "duration", record, &duration, 1);
	tep_get_field_val(s, event, "start", record, &start, 1);
	tep_get_field_val(s, event, "vector", record, &vector, 1);
	desc = tep_get_field_raw(s, event, "desc", record, &val, 1);

	 
	if (taa_data->curr_state == TIMERLAT_WAITING_IRQ) {
		taa_data->prev_irq_duration = duration;
		taa_data->prev_irq_timstamp = start;

		trace_seq_reset(taa_data->prev_irqs_seq);
		trace_seq_printf(taa_data->prev_irqs_seq, "\t%24s:%-3llu	\t\t%9.2f us\n",
				 desc, vector, ns_to_usf(duration));
		return 0;
	}

	 
	if (!taa_data->timer_irq_start_time) {
		expected_start = taa_data->tlat_irq_timstamp - taa_data->tlat_irq_latency;

		taa_data->timer_irq_start_time = start;
		taa_data->timer_irq_duration = duration;

		 
		if (expected_start < taa_data->timer_irq_start_time)
			taa_data->timer_irq_start_delay = taa_data->timer_irq_start_time - expected_start;
		else
			taa_data->timer_irq_start_delay = 0;

		 
		if (taa_data->run_thread_pid)
			return 0;

		if (expected_start > taa_data->prev_irq_timstamp + taa_data->prev_irq_duration)
			taa_data->timer_exit_from_idle = taa_data->timer_irq_start_delay;

		return 0;
	}

	 
	taa_data->thread_irq_sum += duration;
	trace_seq_printf(taa_data->irqs_seq, "	%24s:%-3llu	\t	%9.2f us\n",
			 desc, vector, ns_to_usf(duration));

	return 0;
}

static char *softirq_name[] = { "HI", "TIMER",	"NET_TX", "NET_RX", "BLOCK",
				"IRQ_POLL", "TASKLET", "SCHED", "HRTIMER", "RCU" };


 
static int timerlat_aa_softirq_handler(struct trace_seq *s, struct tep_record *record,
				       struct tep_event *event, void *context)
{
	struct timerlat_aa_context *taa_ctx = timerlat_aa_get_ctx();
	struct timerlat_aa_data *taa_data = timerlat_aa_get_data(taa_ctx, record->cpu);
	unsigned long long duration;
	unsigned long long vector;
	unsigned long long start;

	if (taa_data->curr_state == TIMERLAT_WAITING_IRQ)
		return 0;

	tep_get_field_val(s, event, "duration", record, &duration, 1);
	tep_get_field_val(s, event, "start", record, &start, 1);
	tep_get_field_val(s, event, "vector", record, &vector, 1);

	taa_data->thread_softirq_sum += duration;

	trace_seq_printf(taa_data->softirqs_seq, "\t%24s:%-3llu	\t	%9.2f us\n",
			 softirq_name[vector], vector, ns_to_usf(duration));
	return 0;
}

 
static int timerlat_aa_thread_handler(struct trace_seq *s, struct tep_record *record,
				      struct tep_event *event, void *context)
{
	struct timerlat_aa_context *taa_ctx = timerlat_aa_get_ctx();
	struct timerlat_aa_data *taa_data = timerlat_aa_get_data(taa_ctx, record->cpu);
	unsigned long long duration;
	unsigned long long start;
	unsigned long long pid;
	const char *comm;
	int val;

	if (taa_data->curr_state == TIMERLAT_WAITING_IRQ)
		return 0;

	tep_get_field_val(s, event, "duration", record, &duration, 1);
	tep_get_field_val(s, event, "start", record, &start, 1);

	tep_get_common_field_val(s, event, "common_pid", record, &pid, 1);
	comm = tep_get_field_raw(s, event, "comm", record, &val, 1);

	if (pid == taa_data->run_thread_pid && !taa_data->thread_blocking_duration) {
		taa_data->thread_blocking_duration = duration;

		if (comm)
			strncpy(taa_data->run_thread_comm, comm, MAX_COMM);
		else
			sprintf(taa_data->run_thread_comm, "<...>");

	} else {
		taa_data->thread_thread_sum += duration;

		trace_seq_printf(taa_data->threads_seq, "\t%24s:%-3llu	\t\t%9.2f us\n",
			 comm, pid, ns_to_usf(duration));
	}

	return 0;
}

 
static int timerlat_aa_stack_handler(struct trace_seq *s, struct tep_record *record,
			      struct tep_event *event, void *context)
{
	struct timerlat_aa_context *taa_ctx = timerlat_aa_get_ctx();
	struct timerlat_aa_data *taa_data = timerlat_aa_get_data(taa_ctx, record->cpu);
	unsigned long *caller;
	const char *function;
	int val, i;

	trace_seq_reset(taa_data->stack_seq);

	trace_seq_printf(taa_data->stack_seq, "    Blocking thread stack trace\n");
	caller = tep_get_field_raw(s, event, "caller", record, &val, 1);
	if (caller) {
		for (i = 0; ; i++) {
			function = tep_find_function(taa_ctx->tool->trace.tep, caller[i]);
			if (!function)
				break;
			trace_seq_printf(taa_data->stack_seq, "\t\t-> %s\n", function);
		}
	}
	return 0;
}

 
static int timerlat_aa_sched_switch_handler(struct trace_seq *s, struct tep_record *record,
					    struct tep_event *event, void *context)
{
	struct timerlat_aa_context *taa_ctx = timerlat_aa_get_ctx();
	struct timerlat_aa_data *taa_data = timerlat_aa_get_data(taa_ctx, record->cpu);
	const char *comm;
	int val;

	tep_get_field_val(s, event, "next_pid", record, &taa_data->current_pid, 1);
	comm = tep_get_field_raw(s, event, "next_comm", record, &val, 1);

	strncpy(taa_data->current_comm, comm, MAX_COMM);

	 
	taa_data->kworker = 0;
	taa_data->kworker_func = 0;

	return 0;
}

 
static int timerlat_aa_kworker_start_handler(struct trace_seq *s, struct tep_record *record,
					     struct tep_event *event, void *context)
{
	struct timerlat_aa_context *taa_ctx = timerlat_aa_get_ctx();
	struct timerlat_aa_data *taa_data = timerlat_aa_get_data(taa_ctx, record->cpu);

	tep_get_field_val(s, event, "work", record, &taa_data->kworker, 1);
	tep_get_field_val(s, event, "function", record, &taa_data->kworker_func, 1);
	return 0;
}

 
static void timerlat_thread_analysis(struct timerlat_aa_data *taa_data, int cpu,
				     int irq_thresh, int thread_thresh)
{
	long long exp_irq_ts;
	int total;
	int irq;

	 
	if (taa_data->tlat_irq_seqnum > taa_data->tlat_thread_seqnum) {
		irq = 1;
		total = taa_data->tlat_irq_latency;
	} else {
		irq = 0;
		total = taa_data->tlat_thread_latency;
	}

	 
	exp_irq_ts = taa_data->timer_irq_start_time - taa_data->timer_irq_start_delay;
	if (exp_irq_ts < taa_data->prev_irq_timstamp + taa_data->prev_irq_duration) {
		if (taa_data->prev_irq_timstamp < taa_data->timer_irq_start_time)
			printf("  Previous IRQ interference:	\t\t up to  %9.2f us\n",
				ns_to_usf(taa_data->prev_irq_duration));
	}

	 
	printf("  IRQ handler delay:		%16s	%9.2f us (%.2f %%)\n",
		(ns_to_usf(taa_data->timer_exit_from_idle) > 10) ? "(exit from idle)" : "",
		ns_to_usf(taa_data->timer_irq_start_delay),
		ns_to_per(total, taa_data->timer_irq_start_delay));

	 
	printf("  IRQ latency:	\t\t\t\t	%9.2f us\n",
		ns_to_usf(taa_data->tlat_irq_latency));

	if (irq) {
		 
		printf("  Blocking thread:\n");
		printf("	%24s:%-9llu\n",
			taa_data->run_thread_comm, taa_data->run_thread_pid);
	} else  {
		 
		printf("  Timerlat IRQ duration:	\t\t	%9.2f us (%.2f %%)\n",
			ns_to_usf(taa_data->timer_irq_duration),
			ns_to_per(total, taa_data->timer_irq_duration));

		 
		printf("  Blocking thread:	\t\t\t	%9.2f us (%.2f %%)\n",
			ns_to_usf(taa_data->thread_blocking_duration),
			ns_to_per(total, taa_data->thread_blocking_duration));

		printf("	%24s:%-9llu		%9.2f us\n",
			taa_data->run_thread_comm, taa_data->run_thread_pid,
			ns_to_usf(taa_data->thread_blocking_duration));
	}

	 
	trace_seq_do_printf(taa_data->stack_seq);

	 
	if (taa_data->thread_nmi_sum)
		printf("  NMI interference	\t\t\t	%9.2f us (%.2f %%)\n",
			ns_to_usf(taa_data->thread_nmi_sum),
			ns_to_per(total, taa_data->thread_nmi_sum));

	 
	if (irq)
		goto print_total;

	 
	if (taa_data->thread_irq_sum) {
		printf("  IRQ interference	\t\t\t	%9.2f us (%.2f %%)\n",
			ns_to_usf(taa_data->thread_irq_sum),
			ns_to_per(total, taa_data->thread_irq_sum));

		trace_seq_do_printf(taa_data->irqs_seq);
	}

	 
	if (taa_data->thread_softirq_sum) {
		printf("  Softirq interference	\t\t\t	%9.2f us (%.2f %%)\n",
			ns_to_usf(taa_data->thread_softirq_sum),
			ns_to_per(total, taa_data->thread_softirq_sum));

		trace_seq_do_printf(taa_data->softirqs_seq);
	}

	 
	if (taa_data->thread_thread_sum) {
		printf("  Thread interference	\t\t\t	%9.2f us (%.2f %%)\n",
			ns_to_usf(taa_data->thread_thread_sum),
			ns_to_per(total, taa_data->thread_thread_sum));

		trace_seq_do_printf(taa_data->threads_seq);
	}

	 
print_total:
	printf("------------------------------------------------------------------------\n");
	printf("  %s latency:	\t\t\t	%9.2f us (100%%)\n", irq ? "IRQ" : "Thread",
		ns_to_usf(total));
}

static int timerlat_auto_analysis_collect_trace(struct timerlat_aa_context *taa_ctx)
{
	struct trace_instance *trace = &taa_ctx->tool->trace;
	int retval;

	retval = tracefs_iterate_raw_events(trace->tep,
					    trace->inst,
					    NULL,
					    0,
					    collect_registered_events,
					    trace);
		if (retval < 0) {
			err_msg("Error iterating on events\n");
			return 0;
		}

	return 1;
}

 
void timerlat_auto_analysis(int irq_thresh, int thread_thresh)
{
	struct timerlat_aa_context *taa_ctx = timerlat_aa_get_ctx();
	unsigned long long max_exit_from_idle = 0;
	struct timerlat_aa_data *taa_data;
	int max_exit_from_idle_cpu;
	struct tep_handle *tep;
	int cpu;

	timerlat_auto_analysis_collect_trace(taa_ctx);

	 
	irq_thresh = irq_thresh * 1000;
	thread_thresh = thread_thresh * 1000;

	for (cpu = 0; cpu < taa_ctx->nr_cpus; cpu++) {
		taa_data = timerlat_aa_get_data(taa_ctx, cpu);

		if (irq_thresh && taa_data->tlat_irq_latency >= irq_thresh) {
			printf("## CPU %d hit stop tracing, analyzing it ##\n", cpu);
			timerlat_thread_analysis(taa_data, cpu, irq_thresh, thread_thresh);
		} else if (thread_thresh && (taa_data->tlat_thread_latency) >= thread_thresh) {
			printf("## CPU %d hit stop tracing, analyzing it ##\n", cpu);
			timerlat_thread_analysis(taa_data, cpu, irq_thresh, thread_thresh);
		}

		if (taa_data->max_exit_idle_latency > max_exit_from_idle) {
			max_exit_from_idle = taa_data->max_exit_idle_latency;
			max_exit_from_idle_cpu = cpu;
		}

	}

	if (max_exit_from_idle) {
		printf("\n");
		printf("Max timerlat IRQ latency from idle: %.2f us in cpu %d\n",
			ns_to_usf(max_exit_from_idle), max_exit_from_idle_cpu);
	}
	if (!taa_ctx->dump_tasks)
		return;

	printf("\n");
	printf("Printing CPU tasks:\n");
	for (cpu = 0; cpu < taa_ctx->nr_cpus; cpu++) {
		taa_data = timerlat_aa_get_data(taa_ctx, cpu);
		tep = taa_ctx->tool->trace.tep;

		printf("    [%.3d] %24s:%llu", cpu, taa_data->current_comm, taa_data->current_pid);

		if (taa_data->kworker_func)
			printf(" kworker:%s:%s",
				tep_find_function(tep, taa_data->kworker) ? : "<...>",
				tep_find_function(tep, taa_data->kworker_func));
		printf("\n");
	}

}

 
static void timerlat_aa_destroy_seqs(struct timerlat_aa_context *taa_ctx)
{
	struct timerlat_aa_data *taa_data;
	int i;

	if (!taa_ctx->taa_data)
		return;

	for (i = 0; i < taa_ctx->nr_cpus; i++) {
		taa_data = timerlat_aa_get_data(taa_ctx, i);

		if (taa_data->prev_irqs_seq) {
			trace_seq_destroy(taa_data->prev_irqs_seq);
			free(taa_data->prev_irqs_seq);
		}

		if (taa_data->nmi_seq) {
			trace_seq_destroy(taa_data->nmi_seq);
			free(taa_data->nmi_seq);
		}

		if (taa_data->irqs_seq) {
			trace_seq_destroy(taa_data->irqs_seq);
			free(taa_data->irqs_seq);
		}

		if (taa_data->softirqs_seq) {
			trace_seq_destroy(taa_data->softirqs_seq);
			free(taa_data->softirqs_seq);
		}

		if (taa_data->threads_seq) {
			trace_seq_destroy(taa_data->threads_seq);
			free(taa_data->threads_seq);
		}

		if (taa_data->stack_seq) {
			trace_seq_destroy(taa_data->stack_seq);
			free(taa_data->stack_seq);
		}
	}
}

 
static int timerlat_aa_init_seqs(struct timerlat_aa_context *taa_ctx)
{
	struct timerlat_aa_data *taa_data;
	int i;

	for (i = 0; i < taa_ctx->nr_cpus; i++) {

		taa_data = timerlat_aa_get_data(taa_ctx, i);

		taa_data->prev_irqs_seq = calloc(1, sizeof(*taa_data->prev_irqs_seq));
		if (!taa_data->prev_irqs_seq)
			goto out_err;

		trace_seq_init(taa_data->prev_irqs_seq);

		taa_data->nmi_seq = calloc(1, sizeof(*taa_data->nmi_seq));
		if (!taa_data->nmi_seq)
			goto out_err;

		trace_seq_init(taa_data->nmi_seq);

		taa_data->irqs_seq = calloc(1, sizeof(*taa_data->irqs_seq));
		if (!taa_data->irqs_seq)
			goto out_err;

		trace_seq_init(taa_data->irqs_seq);

		taa_data->softirqs_seq = calloc(1, sizeof(*taa_data->softirqs_seq));
		if (!taa_data->softirqs_seq)
			goto out_err;

		trace_seq_init(taa_data->softirqs_seq);

		taa_data->threads_seq = calloc(1, sizeof(*taa_data->threads_seq));
		if (!taa_data->threads_seq)
			goto out_err;

		trace_seq_init(taa_data->threads_seq);

		taa_data->stack_seq = calloc(1, sizeof(*taa_data->stack_seq));
		if (!taa_data->stack_seq)
			goto out_err;

		trace_seq_init(taa_data->stack_seq);
	}

	return 0;

out_err:
	timerlat_aa_destroy_seqs(taa_ctx);
	return -1;
}

 
static void timerlat_aa_unregister_events(struct osnoise_tool *tool, int dump_tasks)
{

	tep_unregister_event_handler(tool->trace.tep, -1, "ftrace", "timerlat",
				     timerlat_aa_handler, tool);

	tracefs_event_disable(tool->trace.inst, "osnoise", NULL);

	tep_unregister_event_handler(tool->trace.tep, -1, "osnoise", "nmi_noise",
				     timerlat_aa_nmi_handler, tool);

	tep_unregister_event_handler(tool->trace.tep, -1, "osnoise", "irq_noise",
				     timerlat_aa_irq_handler, tool);

	tep_unregister_event_handler(tool->trace.tep, -1, "osnoise", "softirq_noise",
				     timerlat_aa_softirq_handler, tool);

	tep_unregister_event_handler(tool->trace.tep, -1, "osnoise", "thread_noise",
				     timerlat_aa_thread_handler, tool);

	tep_unregister_event_handler(tool->trace.tep, -1, "ftrace", "kernel_stack",
				     timerlat_aa_stack_handler, tool);
	if (!dump_tasks)
		return;

	tracefs_event_disable(tool->trace.inst, "sched", "sched_switch");
	tep_unregister_event_handler(tool->trace.tep, -1, "sched", "sched_switch",
				     timerlat_aa_sched_switch_handler, tool);

	tracefs_event_disable(tool->trace.inst, "workqueue", "workqueue_execute_start");
	tep_unregister_event_handler(tool->trace.tep, -1, "workqueue", "workqueue_execute_start",
				     timerlat_aa_kworker_start_handler, tool);
}

 
static int timerlat_aa_register_events(struct osnoise_tool *tool, int dump_tasks)
{
	int retval;

	tep_register_event_handler(tool->trace.tep, -1, "ftrace", "timerlat",
				timerlat_aa_handler, tool);


	 
	retval = tracefs_event_enable(tool->trace.inst, "osnoise", NULL);
	if (retval < 0 && !errno) {
		err_msg("Could not find osnoise events\n");
		goto out_err;
	}

	tep_register_event_handler(tool->trace.tep, -1, "osnoise", "nmi_noise",
				   timerlat_aa_nmi_handler, tool);

	tep_register_event_handler(tool->trace.tep, -1, "osnoise", "irq_noise",
				   timerlat_aa_irq_handler, tool);

	tep_register_event_handler(tool->trace.tep, -1, "osnoise", "softirq_noise",
				   timerlat_aa_softirq_handler, tool);

	tep_register_event_handler(tool->trace.tep, -1, "osnoise", "thread_noise",
				   timerlat_aa_thread_handler, tool);

	tep_register_event_handler(tool->trace.tep, -1, "ftrace", "kernel_stack",
				   timerlat_aa_stack_handler, tool);

	if (!dump_tasks)
		return 0;

	 
	retval = tracefs_event_enable(tool->trace.inst, "sched", "sched_switch");
	if (retval < 0 && !errno) {
		err_msg("Could not find sched_switch\n");
		goto out_err;
	}

	tep_register_event_handler(tool->trace.tep, -1, "sched", "sched_switch",
				   timerlat_aa_sched_switch_handler, tool);

	retval = tracefs_event_enable(tool->trace.inst, "workqueue", "workqueue_execute_start");
	if (retval < 0 && !errno) {
		err_msg("Could not find workqueue_execute_start\n");
		goto out_err;
	}

	tep_register_event_handler(tool->trace.tep, -1, "workqueue", "workqueue_execute_start",
				   timerlat_aa_kworker_start_handler, tool);

	return 0;

out_err:
	timerlat_aa_unregister_events(tool, dump_tasks);
	return -1;
}

 
void timerlat_aa_destroy(void)
{
	struct timerlat_aa_context *taa_ctx = timerlat_aa_get_ctx();

	if (!taa_ctx)
		return;

	if (!taa_ctx->taa_data)
		goto out_ctx;

	timerlat_aa_unregister_events(taa_ctx->tool, taa_ctx->dump_tasks);
	timerlat_aa_destroy_seqs(taa_ctx);
	free(taa_ctx->taa_data);
out_ctx:
	free(taa_ctx);
}

 
int timerlat_aa_init(struct osnoise_tool *tool, int dump_tasks)
{
	int nr_cpus = sysconf(_SC_NPROCESSORS_CONF);
	struct timerlat_aa_context *taa_ctx;
	int retval;

	taa_ctx = calloc(1, sizeof(*taa_ctx));
	if (!taa_ctx)
		return -1;

	__timerlat_aa_ctx = taa_ctx;

	taa_ctx->nr_cpus = nr_cpus;
	taa_ctx->tool = tool;
	taa_ctx->dump_tasks = dump_tasks;

	taa_ctx->taa_data = calloc(nr_cpus, sizeof(*taa_ctx->taa_data));
	if (!taa_ctx->taa_data)
		goto out_err;

	retval = timerlat_aa_init_seqs(taa_ctx);
	if (retval)
		goto out_err;

	retval = timerlat_aa_register_events(tool, dump_tasks);
	if (retval)
		goto out_err;

	return 0;

out_err:
	timerlat_aa_destroy();
	return -1;
}
