 
 

#ifndef __AA_TASK_H
#define __AA_TASK_H

static inline struct aa_task_ctx *task_ctx(struct task_struct *task)
{
	return task->security + apparmor_blob_sizes.lbs_task;
}

 
struct aa_task_ctx {
	struct aa_label *nnp;
	struct aa_label *onexec;
	struct aa_label *previous;
	u64 token;
};

int aa_replace_current_label(struct aa_label *label);
int aa_set_current_onexec(struct aa_label *label, bool stack);
int aa_set_current_hat(struct aa_label *label, u64 token);
int aa_restore_previous_label(u64 cookie);
struct aa_label *aa_get_task_label(struct task_struct *task);

 
static inline void aa_free_task_ctx(struct aa_task_ctx *ctx)
{
	if (ctx) {
		aa_put_label(ctx->nnp);
		aa_put_label(ctx->previous);
		aa_put_label(ctx->onexec);
	}
}

 
static inline void aa_dup_task_ctx(struct aa_task_ctx *new,
				   const struct aa_task_ctx *old)
{
	*new = *old;
	aa_get_label(new->nnp);
	aa_get_label(new->previous);
	aa_get_label(new->onexec);
}

 
static inline void aa_clear_task_ctx_trans(struct aa_task_ctx *ctx)
{
	AA_BUG(!ctx);

	aa_put_label(ctx->previous);
	aa_put_label(ctx->onexec);
	ctx->previous = NULL;
	ctx->onexec = NULL;
	ctx->token = 0;
}

#define AA_PTRACE_TRACE		MAY_WRITE
#define AA_PTRACE_READ		MAY_READ
#define AA_MAY_BE_TRACED	AA_MAY_APPEND
#define AA_MAY_BE_READ		AA_MAY_CREATE
#define PTRACE_PERM_SHIFT	2

#define AA_PTRACE_PERM_MASK (AA_PTRACE_READ | AA_PTRACE_TRACE | \
			     AA_MAY_BE_READ | AA_MAY_BE_TRACED)
#define AA_SIGNAL_PERM_MASK (MAY_READ | MAY_WRITE)

#define AA_SFS_SIG_MASK "hup int quit ill trap abrt bus fpe kill usr1 " \
	"segv usr2 pipe alrm term stkflt chld cont stop stp ttin ttou urg " \
	"xcpu xfsz vtalrm prof winch io pwr sys emt lost"

int aa_may_ptrace(const struct cred *tracer_cred, struct aa_label *tracer,
		  const struct cred *tracee_cred, struct aa_label *tracee,
		  u32 request);


#endif  
