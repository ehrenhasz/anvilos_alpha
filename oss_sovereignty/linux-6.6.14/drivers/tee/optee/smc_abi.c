
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/arm-smccc.h>
#include <linux/cpuhotplug.h>
#include <linux/errno.h>
#include <linux/firmware.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/tee_drv.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include "optee_private.h"
#include "optee_smc.h"
#include "optee_rpc_cmd.h"
#include <linux/kmemleak.h>
#define CREATE_TRACE_POINTS
#include "optee_trace.h"

 

 
#define OPTEE_MIN_STATIC_POOL_ALIGN    9  

 
static unsigned int pcpu_irq_num;

static int optee_cpuhp_enable_pcpu_irq(unsigned int cpu)
{
	enable_percpu_irq(pcpu_irq_num, IRQ_TYPE_NONE);

	return 0;
}

static int optee_cpuhp_disable_pcpu_irq(unsigned int cpu)
{
	disable_percpu_irq(pcpu_irq_num);

	return 0;
}

 

static int from_msg_param_tmp_mem(struct tee_param *p, u32 attr,
				  const struct optee_msg_param *mp)
{
	struct tee_shm *shm;
	phys_addr_t pa;
	int rc;

	p->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT +
		  attr - OPTEE_MSG_ATTR_TYPE_TMEM_INPUT;
	p->u.memref.size = mp->u.tmem.size;
	shm = (struct tee_shm *)(unsigned long)mp->u.tmem.shm_ref;
	if (!shm) {
		p->u.memref.shm_offs = 0;
		p->u.memref.shm = NULL;
		return 0;
	}

	rc = tee_shm_get_pa(shm, 0, &pa);
	if (rc)
		return rc;

	p->u.memref.shm_offs = mp->u.tmem.buf_ptr - pa;
	p->u.memref.shm = shm;

	return 0;
}

static void from_msg_param_reg_mem(struct tee_param *p, u32 attr,
				   const struct optee_msg_param *mp)
{
	struct tee_shm *shm;

	p->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT +
		  attr - OPTEE_MSG_ATTR_TYPE_RMEM_INPUT;
	p->u.memref.size = mp->u.rmem.size;
	shm = (struct tee_shm *)(unsigned long)mp->u.rmem.shm_ref;

	if (shm) {
		p->u.memref.shm_offs = mp->u.rmem.offs;
		p->u.memref.shm = shm;
	} else {
		p->u.memref.shm_offs = 0;
		p->u.memref.shm = NULL;
	}
}

 
static int optee_from_msg_param(struct optee *optee, struct tee_param *params,
				size_t num_params,
				const struct optee_msg_param *msg_params)
{
	int rc;
	size_t n;

	for (n = 0; n < num_params; n++) {
		struct tee_param *p = params + n;
		const struct optee_msg_param *mp = msg_params + n;
		u32 attr = mp->attr & OPTEE_MSG_ATTR_TYPE_MASK;

		switch (attr) {
		case OPTEE_MSG_ATTR_TYPE_NONE:
			p->attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
			memset(&p->u, 0, sizeof(p->u));
			break;
		case OPTEE_MSG_ATTR_TYPE_VALUE_INPUT:
		case OPTEE_MSG_ATTR_TYPE_VALUE_OUTPUT:
		case OPTEE_MSG_ATTR_TYPE_VALUE_INOUT:
			optee_from_msg_param_value(p, attr, mp);
			break;
		case OPTEE_MSG_ATTR_TYPE_TMEM_INPUT:
		case OPTEE_MSG_ATTR_TYPE_TMEM_OUTPUT:
		case OPTEE_MSG_ATTR_TYPE_TMEM_INOUT:
			rc = from_msg_param_tmp_mem(p, attr, mp);
			if (rc)
				return rc;
			break;
		case OPTEE_MSG_ATTR_TYPE_RMEM_INPUT:
		case OPTEE_MSG_ATTR_TYPE_RMEM_OUTPUT:
		case OPTEE_MSG_ATTR_TYPE_RMEM_INOUT:
			from_msg_param_reg_mem(p, attr, mp);
			break;

		default:
			return -EINVAL;
		}
	}
	return 0;
}

static int to_msg_param_tmp_mem(struct optee_msg_param *mp,
				const struct tee_param *p)
{
	int rc;
	phys_addr_t pa;

	mp->attr = OPTEE_MSG_ATTR_TYPE_TMEM_INPUT + p->attr -
		   TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;

	mp->u.tmem.shm_ref = (unsigned long)p->u.memref.shm;
	mp->u.tmem.size = p->u.memref.size;

	if (!p->u.memref.shm) {
		mp->u.tmem.buf_ptr = 0;
		return 0;
	}

	rc = tee_shm_get_pa(p->u.memref.shm, p->u.memref.shm_offs, &pa);
	if (rc)
		return rc;

	mp->u.tmem.buf_ptr = pa;
	mp->attr |= OPTEE_MSG_ATTR_CACHE_PREDEFINED <<
		    OPTEE_MSG_ATTR_CACHE_SHIFT;

	return 0;
}

static int to_msg_param_reg_mem(struct optee_msg_param *mp,
				const struct tee_param *p)
{
	mp->attr = OPTEE_MSG_ATTR_TYPE_RMEM_INPUT + p->attr -
		   TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;

	mp->u.rmem.shm_ref = (unsigned long)p->u.memref.shm;
	mp->u.rmem.size = p->u.memref.size;
	mp->u.rmem.offs = p->u.memref.shm_offs;
	return 0;
}

 
static int optee_to_msg_param(struct optee *optee,
			      struct optee_msg_param *msg_params,
			      size_t num_params, const struct tee_param *params)
{
	int rc;
	size_t n;

	for (n = 0; n < num_params; n++) {
		const struct tee_param *p = params + n;
		struct optee_msg_param *mp = msg_params + n;

		switch (p->attr) {
		case TEE_IOCTL_PARAM_ATTR_TYPE_NONE:
			mp->attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
			memset(&mp->u, 0, sizeof(mp->u));
			break;
		case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INOUT:
			optee_to_msg_param_value(mp, p);
			break;
		case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT:
			if (tee_shm_is_dynamic(p->u.memref.shm))
				rc = to_msg_param_reg_mem(mp, p);
			else
				rc = to_msg_param_tmp_mem(mp, p);
			if (rc)
				return rc;
			break;
		default:
			return -EINVAL;
		}
	}
	return 0;
}

 

 
static void optee_enable_shm_cache(struct optee *optee)
{
	struct optee_call_waiter w;

	 
	optee_cq_wait_init(&optee->call_queue, &w);
	while (true) {
		struct arm_smccc_res res;

		optee->smc.invoke_fn(OPTEE_SMC_ENABLE_SHM_CACHE,
				     0, 0, 0, 0, 0, 0, 0, &res);
		if (res.a0 == OPTEE_SMC_RETURN_OK)
			break;
		optee_cq_wait_for_completion(&optee->call_queue, &w);
	}
	optee_cq_wait_final(&optee->call_queue, &w);
}

 
static void __optee_disable_shm_cache(struct optee *optee, bool is_mapped)
{
	struct optee_call_waiter w;

	 
	optee_cq_wait_init(&optee->call_queue, &w);
	while (true) {
		union {
			struct arm_smccc_res smccc;
			struct optee_smc_disable_shm_cache_result result;
		} res;

		optee->smc.invoke_fn(OPTEE_SMC_DISABLE_SHM_CACHE,
				     0, 0, 0, 0, 0, 0, 0, &res.smccc);
		if (res.result.status == OPTEE_SMC_RETURN_ENOTAVAIL)
			break;  
		if (res.result.status == OPTEE_SMC_RETURN_OK) {
			struct tee_shm *shm;

			 
			if (!is_mapped)
				continue;

			shm = reg_pair_to_ptr(res.result.shm_upper32,
					      res.result.shm_lower32);
			tee_shm_free(shm);
		} else {
			optee_cq_wait_for_completion(&optee->call_queue, &w);
		}
	}
	optee_cq_wait_final(&optee->call_queue, &w);
}

 
static void optee_disable_shm_cache(struct optee *optee)
{
	return __optee_disable_shm_cache(optee, true);
}

 
static void optee_disable_unmapped_shm_cache(struct optee *optee)
{
	return __optee_disable_shm_cache(optee, false);
}

#define PAGELIST_ENTRIES_PER_PAGE				\
	((OPTEE_MSG_NONCONTIG_PAGE_SIZE / sizeof(u64)) - 1)

 
static size_t get_pages_list_size(size_t num_entries)
{
	int pages = DIV_ROUND_UP(num_entries, PAGELIST_ENTRIES_PER_PAGE);

	return pages * OPTEE_MSG_NONCONTIG_PAGE_SIZE;
}

static u64 *optee_allocate_pages_list(size_t num_entries)
{
	return alloc_pages_exact(get_pages_list_size(num_entries), GFP_KERNEL);
}

static void optee_free_pages_list(void *list, size_t num_entries)
{
	free_pages_exact(list, get_pages_list_size(num_entries));
}

 
static void optee_fill_pages_list(u64 *dst, struct page **pages, int num_pages,
				  size_t page_offset)
{
	int n = 0;
	phys_addr_t optee_page;
	 
	struct {
		u64 pages_list[PAGELIST_ENTRIES_PER_PAGE];
		u64 next_page_data;
	} *pages_data;

	 
	BUILD_BUG_ON(PAGE_SIZE < OPTEE_MSG_NONCONTIG_PAGE_SIZE);

	pages_data = (void *)dst;
	 
	optee_page = page_to_phys(*pages) +
		round_down(page_offset, OPTEE_MSG_NONCONTIG_PAGE_SIZE);

	while (true) {
		pages_data->pages_list[n++] = optee_page;

		if (n == PAGELIST_ENTRIES_PER_PAGE) {
			pages_data->next_page_data =
				virt_to_phys(pages_data + 1);
			pages_data++;
			n = 0;
		}

		optee_page += OPTEE_MSG_NONCONTIG_PAGE_SIZE;
		if (!(optee_page & ~PAGE_MASK)) {
			if (!--num_pages)
				break;
			pages++;
			optee_page = page_to_phys(*pages);
		}
	}
}

static int optee_shm_register(struct tee_context *ctx, struct tee_shm *shm,
			      struct page **pages, size_t num_pages,
			      unsigned long start)
{
	struct optee *optee = tee_get_drvdata(ctx->teedev);
	struct optee_msg_arg *msg_arg;
	struct tee_shm *shm_arg;
	u64 *pages_list;
	size_t sz;
	int rc;

	if (!num_pages)
		return -EINVAL;

	rc = optee_check_mem_type(start, num_pages);
	if (rc)
		return rc;

	pages_list = optee_allocate_pages_list(num_pages);
	if (!pages_list)
		return -ENOMEM;

	 
	sz = optee_msg_arg_size(optee->rpc_param_count);
	shm_arg = tee_shm_alloc_priv_buf(ctx, sz);
	if (IS_ERR(shm_arg)) {
		rc = PTR_ERR(shm_arg);
		goto out;
	}
	msg_arg = tee_shm_get_va(shm_arg, 0);
	if (IS_ERR(msg_arg)) {
		rc = PTR_ERR(msg_arg);
		goto out;
	}

	optee_fill_pages_list(pages_list, pages, num_pages,
			      tee_shm_get_page_offset(shm));

	memset(msg_arg, 0, OPTEE_MSG_GET_ARG_SIZE(1));
	msg_arg->num_params = 1;
	msg_arg->cmd = OPTEE_MSG_CMD_REGISTER_SHM;
	msg_arg->params->attr = OPTEE_MSG_ATTR_TYPE_TMEM_OUTPUT |
				OPTEE_MSG_ATTR_NONCONTIG;
	msg_arg->params->u.tmem.shm_ref = (unsigned long)shm;
	msg_arg->params->u.tmem.size = tee_shm_get_size(shm);
	 
	msg_arg->params->u.tmem.buf_ptr = virt_to_phys(pages_list) |
	  (tee_shm_get_page_offset(shm) & (OPTEE_MSG_NONCONTIG_PAGE_SIZE - 1));

	if (optee->ops->do_call_with_arg(ctx, shm_arg, 0) ||
	    msg_arg->ret != TEEC_SUCCESS)
		rc = -EINVAL;

	tee_shm_free(shm_arg);
out:
	optee_free_pages_list(pages_list, num_pages);
	return rc;
}

static int optee_shm_unregister(struct tee_context *ctx, struct tee_shm *shm)
{
	struct optee *optee = tee_get_drvdata(ctx->teedev);
	struct optee_msg_arg *msg_arg;
	struct tee_shm *shm_arg;
	int rc = 0;
	size_t sz;

	 
	sz = optee_msg_arg_size(optee->rpc_param_count);
	shm_arg = tee_shm_alloc_priv_buf(ctx, sz);
	if (IS_ERR(shm_arg))
		return PTR_ERR(shm_arg);
	msg_arg = tee_shm_get_va(shm_arg, 0);
	if (IS_ERR(msg_arg)) {
		rc = PTR_ERR(msg_arg);
		goto out;
	}

	memset(msg_arg, 0, sz);
	msg_arg->num_params = 1;
	msg_arg->cmd = OPTEE_MSG_CMD_UNREGISTER_SHM;
	msg_arg->params[0].attr = OPTEE_MSG_ATTR_TYPE_RMEM_INPUT;
	msg_arg->params[0].u.rmem.shm_ref = (unsigned long)shm;

	if (optee->ops->do_call_with_arg(ctx, shm_arg, 0) ||
	    msg_arg->ret != TEEC_SUCCESS)
		rc = -EINVAL;
out:
	tee_shm_free(shm_arg);
	return rc;
}

static int optee_shm_register_supp(struct tee_context *ctx, struct tee_shm *shm,
				   struct page **pages, size_t num_pages,
				   unsigned long start)
{
	 
	return optee_check_mem_type(start, num_pages);
}

static int optee_shm_unregister_supp(struct tee_context *ctx,
				     struct tee_shm *shm)
{
	return 0;
}

 

static int pool_op_alloc(struct tee_shm_pool *pool,
			 struct tee_shm *shm, size_t size, size_t align)
{
	 
	if (shm->flags & TEE_SHM_PRIV)
		return optee_pool_op_alloc_helper(pool, shm, size, align, NULL);

	return optee_pool_op_alloc_helper(pool, shm, size, align,
					  optee_shm_register);
}

static void pool_op_free(struct tee_shm_pool *pool,
			 struct tee_shm *shm)
{
	if (!(shm->flags & TEE_SHM_PRIV))
		optee_pool_op_free_helper(pool, shm, optee_shm_unregister);
	else
		optee_pool_op_free_helper(pool, shm, NULL);
}

static void pool_op_destroy_pool(struct tee_shm_pool *pool)
{
	kfree(pool);
}

static const struct tee_shm_pool_ops pool_ops = {
	.alloc = pool_op_alloc,
	.free = pool_op_free,
	.destroy_pool = pool_op_destroy_pool,
};

 
static struct tee_shm_pool *optee_shm_pool_alloc_pages(void)
{
	struct tee_shm_pool *pool = kzalloc(sizeof(*pool), GFP_KERNEL);

	if (!pool)
		return ERR_PTR(-ENOMEM);

	pool->ops = &pool_ops;

	return pool;
}

 

static void handle_rpc_func_cmd_shm_free(struct tee_context *ctx,
					 struct optee_msg_arg *arg)
{
	struct tee_shm *shm;

	arg->ret_origin = TEEC_ORIGIN_COMMS;

	if (arg->num_params != 1 ||
	    arg->params[0].attr != OPTEE_MSG_ATTR_TYPE_VALUE_INPUT) {
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
		return;
	}

	shm = (struct tee_shm *)(unsigned long)arg->params[0].u.value.b;
	switch (arg->params[0].u.value.a) {
	case OPTEE_RPC_SHM_TYPE_APPL:
		optee_rpc_cmd_free_suppl(ctx, shm);
		break;
	case OPTEE_RPC_SHM_TYPE_KERNEL:
		tee_shm_free(shm);
		break;
	default:
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
	}
	arg->ret = TEEC_SUCCESS;
}

static void handle_rpc_func_cmd_shm_alloc(struct tee_context *ctx,
					  struct optee *optee,
					  struct optee_msg_arg *arg,
					  struct optee_call_ctx *call_ctx)
{
	phys_addr_t pa;
	struct tee_shm *shm;
	size_t sz;
	size_t n;

	arg->ret_origin = TEEC_ORIGIN_COMMS;

	if (!arg->num_params ||
	    arg->params[0].attr != OPTEE_MSG_ATTR_TYPE_VALUE_INPUT) {
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
		return;
	}

	for (n = 1; n < arg->num_params; n++) {
		if (arg->params[n].attr != OPTEE_MSG_ATTR_TYPE_NONE) {
			arg->ret = TEEC_ERROR_BAD_PARAMETERS;
			return;
		}
	}

	sz = arg->params[0].u.value.b;
	switch (arg->params[0].u.value.a) {
	case OPTEE_RPC_SHM_TYPE_APPL:
		shm = optee_rpc_cmd_alloc_suppl(ctx, sz);
		break;
	case OPTEE_RPC_SHM_TYPE_KERNEL:
		shm = tee_shm_alloc_priv_buf(optee->ctx, sz);
		break;
	default:
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
		return;
	}

	if (IS_ERR(shm)) {
		arg->ret = TEEC_ERROR_OUT_OF_MEMORY;
		return;
	}

	if (tee_shm_get_pa(shm, 0, &pa)) {
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
		goto bad;
	}

	sz = tee_shm_get_size(shm);

	if (tee_shm_is_dynamic(shm)) {
		struct page **pages;
		u64 *pages_list;
		size_t page_num;

		pages = tee_shm_get_pages(shm, &page_num);
		if (!pages || !page_num) {
			arg->ret = TEEC_ERROR_OUT_OF_MEMORY;
			goto bad;
		}

		pages_list = optee_allocate_pages_list(page_num);
		if (!pages_list) {
			arg->ret = TEEC_ERROR_OUT_OF_MEMORY;
			goto bad;
		}

		call_ctx->pages_list = pages_list;
		call_ctx->num_entries = page_num;

		arg->params[0].attr = OPTEE_MSG_ATTR_TYPE_TMEM_OUTPUT |
				      OPTEE_MSG_ATTR_NONCONTIG;
		 
		arg->params[0].u.tmem.buf_ptr = virt_to_phys(pages_list) |
			(tee_shm_get_page_offset(shm) &
			 (OPTEE_MSG_NONCONTIG_PAGE_SIZE - 1));
		arg->params[0].u.tmem.size = tee_shm_get_size(shm);
		arg->params[0].u.tmem.shm_ref = (unsigned long)shm;

		optee_fill_pages_list(pages_list, pages, page_num,
				      tee_shm_get_page_offset(shm));
	} else {
		arg->params[0].attr = OPTEE_MSG_ATTR_TYPE_TMEM_OUTPUT;
		arg->params[0].u.tmem.buf_ptr = pa;
		arg->params[0].u.tmem.size = sz;
		arg->params[0].u.tmem.shm_ref = (unsigned long)shm;
	}

	arg->ret = TEEC_SUCCESS;
	return;
bad:
	tee_shm_free(shm);
}

static void free_pages_list(struct optee_call_ctx *call_ctx)
{
	if (call_ctx->pages_list) {
		optee_free_pages_list(call_ctx->pages_list,
				      call_ctx->num_entries);
		call_ctx->pages_list = NULL;
		call_ctx->num_entries = 0;
	}
}

static void optee_rpc_finalize_call(struct optee_call_ctx *call_ctx)
{
	free_pages_list(call_ctx);
}

static void handle_rpc_func_cmd(struct tee_context *ctx, struct optee *optee,
				struct optee_msg_arg *arg,
				struct optee_call_ctx *call_ctx)
{

	switch (arg->cmd) {
	case OPTEE_RPC_CMD_SHM_ALLOC:
		free_pages_list(call_ctx);
		handle_rpc_func_cmd_shm_alloc(ctx, optee, arg, call_ctx);
		break;
	case OPTEE_RPC_CMD_SHM_FREE:
		handle_rpc_func_cmd_shm_free(ctx, arg);
		break;
	default:
		optee_rpc_cmd(ctx, optee, arg);
	}
}

 
static void optee_handle_rpc(struct tee_context *ctx,
			     struct optee_msg_arg *rpc_arg,
			     struct optee_rpc_param *param,
			     struct optee_call_ctx *call_ctx)
{
	struct tee_device *teedev = ctx->teedev;
	struct optee *optee = tee_get_drvdata(teedev);
	struct optee_msg_arg *arg;
	struct tee_shm *shm;
	phys_addr_t pa;

	switch (OPTEE_SMC_RETURN_GET_RPC_FUNC(param->a0)) {
	case OPTEE_SMC_RPC_FUNC_ALLOC:
		shm = tee_shm_alloc_priv_buf(optee->ctx, param->a1);
		if (!IS_ERR(shm) && !tee_shm_get_pa(shm, 0, &pa)) {
			reg_pair_from_64(&param->a1, &param->a2, pa);
			reg_pair_from_64(&param->a4, &param->a5,
					 (unsigned long)shm);
		} else {
			param->a1 = 0;
			param->a2 = 0;
			param->a4 = 0;
			param->a5 = 0;
		}
		kmemleak_not_leak(shm);
		break;
	case OPTEE_SMC_RPC_FUNC_FREE:
		shm = reg_pair_to_ptr(param->a1, param->a2);
		tee_shm_free(shm);
		break;
	case OPTEE_SMC_RPC_FUNC_FOREIGN_INTR:
		 
		break;
	case OPTEE_SMC_RPC_FUNC_CMD:
		if (rpc_arg) {
			arg = rpc_arg;
		} else {
			shm = reg_pair_to_ptr(param->a1, param->a2);
			arg = tee_shm_get_va(shm, 0);
			if (IS_ERR(arg)) {
				pr_err("%s: tee_shm_get_va %p failed\n",
				       __func__, shm);
				break;
			}
		}

		handle_rpc_func_cmd(ctx, optee, arg, call_ctx);
		break;
	default:
		pr_warn("Unknown RPC func 0x%x\n",
			(u32)OPTEE_SMC_RETURN_GET_RPC_FUNC(param->a0));
		break;
	}

	param->a0 = OPTEE_SMC_CALL_RETURN_FROM_RPC;
}

 
static int optee_smc_do_call_with_arg(struct tee_context *ctx,
				      struct tee_shm *shm, u_int offs)
{
	struct optee *optee = tee_get_drvdata(ctx->teedev);
	struct optee_call_waiter w;
	struct optee_rpc_param param = { };
	struct optee_call_ctx call_ctx = { };
	struct optee_msg_arg *rpc_arg = NULL;
	int rc;

	if (optee->rpc_param_count) {
		struct optee_msg_arg *arg;
		unsigned int rpc_arg_offs;

		arg = tee_shm_get_va(shm, offs);
		if (IS_ERR(arg))
			return PTR_ERR(arg);

		rpc_arg_offs = OPTEE_MSG_GET_ARG_SIZE(arg->num_params);
		rpc_arg = tee_shm_get_va(shm, offs + rpc_arg_offs);
		if (IS_ERR(rpc_arg))
			return PTR_ERR(rpc_arg);
	}

	if  (rpc_arg && tee_shm_is_dynamic(shm)) {
		param.a0 = OPTEE_SMC_CALL_WITH_REGD_ARG;
		reg_pair_from_64(&param.a1, &param.a2, (u_long)shm);
		param.a3 = offs;
	} else {
		phys_addr_t parg;

		rc = tee_shm_get_pa(shm, offs, &parg);
		if (rc)
			return rc;

		if (rpc_arg)
			param.a0 = OPTEE_SMC_CALL_WITH_RPC_ARG;
		else
			param.a0 = OPTEE_SMC_CALL_WITH_ARG;
		reg_pair_from_64(&param.a1, &param.a2, parg);
	}
	 
	optee_cq_wait_init(&optee->call_queue, &w);
	while (true) {
		struct arm_smccc_res res;

		trace_optee_invoke_fn_begin(&param);
		optee->smc.invoke_fn(param.a0, param.a1, param.a2, param.a3,
				     param.a4, param.a5, param.a6, param.a7,
				     &res);
		trace_optee_invoke_fn_end(&param, &res);

		if (res.a0 == OPTEE_SMC_RETURN_ETHREAD_LIMIT) {
			 
			optee_cq_wait_for_completion(&optee->call_queue, &w);
		} else if (OPTEE_SMC_RETURN_IS_RPC(res.a0)) {
			cond_resched();
			param.a0 = res.a0;
			param.a1 = res.a1;
			param.a2 = res.a2;
			param.a3 = res.a3;
			optee_handle_rpc(ctx, rpc_arg, &param, &call_ctx);
		} else {
			rc = res.a0;
			break;
		}
	}

	optee_rpc_finalize_call(&call_ctx);
	 
	optee_cq_wait_final(&optee->call_queue, &w);

	return rc;
}

static int simple_call_with_arg(struct tee_context *ctx, u32 cmd)
{
	struct optee_shm_arg_entry *entry;
	struct optee_msg_arg *msg_arg;
	struct tee_shm *shm;
	u_int offs;

	msg_arg = optee_get_msg_arg(ctx, 0, &entry, &shm, &offs);
	if (IS_ERR(msg_arg))
		return PTR_ERR(msg_arg);

	msg_arg->cmd = cmd;
	optee_smc_do_call_with_arg(ctx, shm, offs);

	optee_free_msg_arg(ctx, entry, offs);
	return 0;
}

static int optee_smc_do_bottom_half(struct tee_context *ctx)
{
	return simple_call_with_arg(ctx, OPTEE_MSG_CMD_DO_BOTTOM_HALF);
}

static int optee_smc_stop_async_notif(struct tee_context *ctx)
{
	return simple_call_with_arg(ctx, OPTEE_MSG_CMD_STOP_ASYNC_NOTIF);
}

 

static u32 get_async_notif_value(optee_invoke_fn *invoke_fn, bool *value_valid,
				 bool *value_pending)
{
	struct arm_smccc_res res;

	invoke_fn(OPTEE_SMC_GET_ASYNC_NOTIF_VALUE, 0, 0, 0, 0, 0, 0, 0, &res);

	if (res.a0) {
		*value_valid = false;
		return 0;
	}
	*value_valid = (res.a2 & OPTEE_SMC_ASYNC_NOTIF_VALUE_VALID);
	*value_pending = (res.a2 & OPTEE_SMC_ASYNC_NOTIF_VALUE_PENDING);
	return res.a1;
}

static irqreturn_t irq_handler(struct optee *optee)
{
	bool do_bottom_half = false;
	bool value_valid;
	bool value_pending;
	u32 value;

	do {
		value = get_async_notif_value(optee->smc.invoke_fn,
					      &value_valid, &value_pending);
		if (!value_valid)
			break;

		if (value == OPTEE_SMC_ASYNC_NOTIF_VALUE_DO_BOTTOM_HALF)
			do_bottom_half = true;
		else
			optee_notif_send(optee, value);
	} while (value_pending);

	if (do_bottom_half)
		return IRQ_WAKE_THREAD;
	return IRQ_HANDLED;
}

static irqreturn_t notif_irq_handler(int irq, void *dev_id)
{
	struct optee *optee = dev_id;

	return irq_handler(optee);
}

static irqreturn_t notif_irq_thread_fn(int irq, void *dev_id)
{
	struct optee *optee = dev_id;

	optee_smc_do_bottom_half(optee->ctx);

	return IRQ_HANDLED;
}

static int init_irq(struct optee *optee, u_int irq)
{
	int rc;

	rc = request_threaded_irq(irq, notif_irq_handler,
				  notif_irq_thread_fn,
				  0, "optee_notification", optee);
	if (rc)
		return rc;

	optee->smc.notif_irq = irq;

	return 0;
}

static irqreturn_t notif_pcpu_irq_handler(int irq, void *dev_id)
{
	struct optee_pcpu *pcpu = dev_id;
	struct optee *optee = pcpu->optee;

	if (irq_handler(optee) == IRQ_WAKE_THREAD)
		queue_work(optee->smc.notif_pcpu_wq,
			   &optee->smc.notif_pcpu_work);

	return IRQ_HANDLED;
}

static void notif_pcpu_irq_work_fn(struct work_struct *work)
{
	struct optee_smc *optee_smc = container_of(work, struct optee_smc,
						   notif_pcpu_work);
	struct optee *optee = container_of(optee_smc, struct optee, smc);

	optee_smc_do_bottom_half(optee->ctx);
}

static int init_pcpu_irq(struct optee *optee, u_int irq)
{
	struct optee_pcpu __percpu *optee_pcpu;
	int cpu, rc;

	optee_pcpu = alloc_percpu(struct optee_pcpu);
	if (!optee_pcpu)
		return -ENOMEM;

	for_each_present_cpu(cpu)
		per_cpu_ptr(optee_pcpu, cpu)->optee = optee;

	rc = request_percpu_irq(irq, notif_pcpu_irq_handler,
				"optee_pcpu_notification", optee_pcpu);
	if (rc)
		goto err_free_pcpu;

	INIT_WORK(&optee->smc.notif_pcpu_work, notif_pcpu_irq_work_fn);
	optee->smc.notif_pcpu_wq = create_workqueue("optee_pcpu_notification");
	if (!optee->smc.notif_pcpu_wq) {
		rc = -EINVAL;
		goto err_free_pcpu_irq;
	}

	optee->smc.optee_pcpu = optee_pcpu;
	optee->smc.notif_irq = irq;

	pcpu_irq_num = irq;
	rc = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "optee/pcpu-notif:starting",
			       optee_cpuhp_enable_pcpu_irq,
			       optee_cpuhp_disable_pcpu_irq);
	if (!rc)
		rc = -EINVAL;
	if (rc < 0)
		goto err_free_pcpu_irq;

	optee->smc.notif_cpuhp_state = rc;

	return 0;

err_free_pcpu_irq:
	free_percpu_irq(irq, optee_pcpu);
err_free_pcpu:
	free_percpu(optee_pcpu);

	return rc;
}

static int optee_smc_notif_init_irq(struct optee *optee, u_int irq)
{
	if (irq_is_percpu_devid(irq))
		return init_pcpu_irq(optee, irq);
	else
		return init_irq(optee, irq);
}

static void uninit_pcpu_irq(struct optee *optee)
{
	cpuhp_remove_state(optee->smc.notif_cpuhp_state);

	destroy_workqueue(optee->smc.notif_pcpu_wq);

	free_percpu_irq(optee->smc.notif_irq, optee->smc.optee_pcpu);
	free_percpu(optee->smc.optee_pcpu);
}

static void optee_smc_notif_uninit_irq(struct optee *optee)
{
	if (optee->smc.sec_caps & OPTEE_SMC_SEC_CAP_ASYNC_NOTIF) {
		optee_smc_stop_async_notif(optee->ctx);
		if (optee->smc.notif_irq) {
			if (irq_is_percpu_devid(optee->smc.notif_irq))
				uninit_pcpu_irq(optee);
			else
				free_irq(optee->smc.notif_irq, optee);

			irq_dispose_mapping(optee->smc.notif_irq);
		}
	}
}

 

static void optee_get_version(struct tee_device *teedev,
			      struct tee_ioctl_version_data *vers)
{
	struct tee_ioctl_version_data v = {
		.impl_id = TEE_IMPL_ID_OPTEE,
		.impl_caps = TEE_OPTEE_CAP_TZ,
		.gen_caps = TEE_GEN_CAP_GP,
	};
	struct optee *optee = tee_get_drvdata(teedev);

	if (optee->smc.sec_caps & OPTEE_SMC_SEC_CAP_DYNAMIC_SHM)
		v.gen_caps |= TEE_GEN_CAP_REG_MEM;
	if (optee->smc.sec_caps & OPTEE_SMC_SEC_CAP_MEMREF_NULL)
		v.gen_caps |= TEE_GEN_CAP_MEMREF_NULL;
	*vers = v;
}

static int optee_smc_open(struct tee_context *ctx)
{
	struct optee *optee = tee_get_drvdata(ctx->teedev);
	u32 sec_caps = optee->smc.sec_caps;

	return optee_open(ctx, sec_caps & OPTEE_SMC_SEC_CAP_MEMREF_NULL);
}

static const struct tee_driver_ops optee_clnt_ops = {
	.get_version = optee_get_version,
	.open = optee_smc_open,
	.release = optee_release,
	.open_session = optee_open_session,
	.close_session = optee_close_session,
	.invoke_func = optee_invoke_func,
	.cancel_req = optee_cancel_req,
	.shm_register = optee_shm_register,
	.shm_unregister = optee_shm_unregister,
};

static const struct tee_desc optee_clnt_desc = {
	.name = DRIVER_NAME "-clnt",
	.ops = &optee_clnt_ops,
	.owner = THIS_MODULE,
};

static const struct tee_driver_ops optee_supp_ops = {
	.get_version = optee_get_version,
	.open = optee_smc_open,
	.release = optee_release_supp,
	.supp_recv = optee_supp_recv,
	.supp_send = optee_supp_send,
	.shm_register = optee_shm_register_supp,
	.shm_unregister = optee_shm_unregister_supp,
};

static const struct tee_desc optee_supp_desc = {
	.name = DRIVER_NAME "-supp",
	.ops = &optee_supp_ops,
	.owner = THIS_MODULE,
	.flags = TEE_DESC_PRIVILEGED,
};

static const struct optee_ops optee_ops = {
	.do_call_with_arg = optee_smc_do_call_with_arg,
	.to_msg_param = optee_to_msg_param,
	.from_msg_param = optee_from_msg_param,
};

static int enable_async_notif(optee_invoke_fn *invoke_fn)
{
	struct arm_smccc_res res;

	invoke_fn(OPTEE_SMC_ENABLE_ASYNC_NOTIF, 0, 0, 0, 0, 0, 0, 0, &res);

	if (res.a0)
		return -EINVAL;
	return 0;
}

static bool optee_msg_api_uid_is_optee_api(optee_invoke_fn *invoke_fn)
{
	struct arm_smccc_res res;

	invoke_fn(OPTEE_SMC_CALLS_UID, 0, 0, 0, 0, 0, 0, 0, &res);

	if (res.a0 == OPTEE_MSG_UID_0 && res.a1 == OPTEE_MSG_UID_1 &&
	    res.a2 == OPTEE_MSG_UID_2 && res.a3 == OPTEE_MSG_UID_3)
		return true;
	return false;
}

#ifdef CONFIG_OPTEE_INSECURE_LOAD_IMAGE
static bool optee_msg_api_uid_is_optee_image_load(optee_invoke_fn *invoke_fn)
{
	struct arm_smccc_res res;

	invoke_fn(OPTEE_SMC_CALLS_UID, 0, 0, 0, 0, 0, 0, 0, &res);

	if (res.a0 == OPTEE_MSG_IMAGE_LOAD_UID_0 &&
	    res.a1 == OPTEE_MSG_IMAGE_LOAD_UID_1 &&
	    res.a2 == OPTEE_MSG_IMAGE_LOAD_UID_2 &&
	    res.a3 == OPTEE_MSG_IMAGE_LOAD_UID_3)
		return true;
	return false;
}
#endif

static void optee_msg_get_os_revision(optee_invoke_fn *invoke_fn)
{
	union {
		struct arm_smccc_res smccc;
		struct optee_smc_call_get_os_revision_result result;
	} res = {
		.result = {
			.build_id = 0
		}
	};

	invoke_fn(OPTEE_SMC_CALL_GET_OS_REVISION, 0, 0, 0, 0, 0, 0, 0,
		  &res.smccc);

	if (res.result.build_id)
		pr_info("revision %lu.%lu (%08lx)", res.result.major,
			res.result.minor, res.result.build_id);
	else
		pr_info("revision %lu.%lu", res.result.major, res.result.minor);
}

static bool optee_msg_api_revision_is_compatible(optee_invoke_fn *invoke_fn)
{
	union {
		struct arm_smccc_res smccc;
		struct optee_smc_calls_revision_result result;
	} res;

	invoke_fn(OPTEE_SMC_CALLS_REVISION, 0, 0, 0, 0, 0, 0, 0, &res.smccc);

	if (res.result.major == OPTEE_MSG_REVISION_MAJOR &&
	    (int)res.result.minor >= OPTEE_MSG_REVISION_MINOR)
		return true;
	return false;
}

static bool optee_msg_exchange_capabilities(optee_invoke_fn *invoke_fn,
					    u32 *sec_caps, u32 *max_notif_value,
					    unsigned int *rpc_param_count)
{
	union {
		struct arm_smccc_res smccc;
		struct optee_smc_exchange_capabilities_result result;
	} res;
	u32 a1 = 0;

	 
	if (!IS_ENABLED(CONFIG_SMP) || nr_cpu_ids == 1)
		a1 |= OPTEE_SMC_NSEC_CAP_UNIPROCESSOR;

	invoke_fn(OPTEE_SMC_EXCHANGE_CAPABILITIES, a1, 0, 0, 0, 0, 0, 0,
		  &res.smccc);

	if (res.result.status != OPTEE_SMC_RETURN_OK)
		return false;

	*sec_caps = res.result.capabilities;
	if (*sec_caps & OPTEE_SMC_SEC_CAP_ASYNC_NOTIF)
		*max_notif_value = res.result.max_notif_value;
	else
		*max_notif_value = OPTEE_DEFAULT_MAX_NOTIF_VALUE;
	if (*sec_caps & OPTEE_SMC_SEC_CAP_RPC_ARG)
		*rpc_param_count = (u8)res.result.data;
	else
		*rpc_param_count = 0;

	return true;
}

static struct tee_shm_pool *
optee_config_shm_memremap(optee_invoke_fn *invoke_fn, void **memremaped_shm)
{
	union {
		struct arm_smccc_res smccc;
		struct optee_smc_get_shm_config_result result;
	} res;
	unsigned long vaddr;
	phys_addr_t paddr;
	size_t size;
	phys_addr_t begin;
	phys_addr_t end;
	void *va;
	void *rc;

	invoke_fn(OPTEE_SMC_GET_SHM_CONFIG, 0, 0, 0, 0, 0, 0, 0, &res.smccc);
	if (res.result.status != OPTEE_SMC_RETURN_OK) {
		pr_err("static shm service not available\n");
		return ERR_PTR(-ENOENT);
	}

	if (res.result.settings != OPTEE_SMC_SHM_CACHED) {
		pr_err("only normal cached shared memory supported\n");
		return ERR_PTR(-EINVAL);
	}

	begin = roundup(res.result.start, PAGE_SIZE);
	end = rounddown(res.result.start + res.result.size, PAGE_SIZE);
	paddr = begin;
	size = end - begin;

	va = memremap(paddr, size, MEMREMAP_WB);
	if (!va) {
		pr_err("shared memory ioremap failed\n");
		return ERR_PTR(-EINVAL);
	}
	vaddr = (unsigned long)va;

	rc = tee_shm_pool_alloc_res_mem(vaddr, paddr, size,
					OPTEE_MIN_STATIC_POOL_ALIGN);
	if (IS_ERR(rc))
		memunmap(va);
	else
		*memremaped_shm = va;

	return rc;
}

 
static void optee_smccc_smc(unsigned long a0, unsigned long a1,
			    unsigned long a2, unsigned long a3,
			    unsigned long a4, unsigned long a5,
			    unsigned long a6, unsigned long a7,
			    struct arm_smccc_res *res)
{
	arm_smccc_smc(a0, a1, a2, a3, a4, a5, a6, a7, res);
}

static void optee_smccc_hvc(unsigned long a0, unsigned long a1,
			    unsigned long a2, unsigned long a3,
			    unsigned long a4, unsigned long a5,
			    unsigned long a6, unsigned long a7,
			    struct arm_smccc_res *res)
{
	arm_smccc_hvc(a0, a1, a2, a3, a4, a5, a6, a7, res);
}

static optee_invoke_fn *get_invoke_func(struct device *dev)
{
	const char *method;

	pr_info("probing for conduit method.\n");

	if (device_property_read_string(dev, "method", &method)) {
		pr_warn("missing \"method\" property\n");
		return ERR_PTR(-ENXIO);
	}

	if (!strcmp("hvc", method))
		return optee_smccc_hvc;
	else if (!strcmp("smc", method))
		return optee_smccc_smc;

	pr_warn("invalid \"method\" property: %s\n", method);
	return ERR_PTR(-EINVAL);
}

 
static int optee_smc_remove(struct platform_device *pdev)
{
	struct optee *optee = platform_get_drvdata(pdev);

	 
	if (!optee->rpc_param_count)
		optee_disable_shm_cache(optee);

	optee_smc_notif_uninit_irq(optee);

	optee_remove_common(optee);

	if (optee->smc.memremaped_shm)
		memunmap(optee->smc.memremaped_shm);

	kfree(optee);

	return 0;
}

 
static void optee_shutdown(struct platform_device *pdev)
{
	struct optee *optee = platform_get_drvdata(pdev);

	if (!optee->rpc_param_count)
		optee_disable_shm_cache(optee);
}

#ifdef CONFIG_OPTEE_INSECURE_LOAD_IMAGE

#define OPTEE_FW_IMAGE "optee/tee.bin"

static optee_invoke_fn *cpuhp_invoke_fn;

static int optee_cpuhp_probe(unsigned int cpu)
{
	 
	if (optee_msg_api_uid_is_optee_api(cpuhp_invoke_fn))
		return 0;
	else
		return -EINVAL;
}

static int optee_load_fw(struct platform_device *pdev,
			 optee_invoke_fn *invoke_fn)
{
	const struct firmware *fw = NULL;
	struct arm_smccc_res res;
	phys_addr_t data_pa;
	u8 *data_buf = NULL;
	u64 data_size;
	u32 data_pa_high, data_pa_low;
	u32 data_size_high, data_size_low;
	int rc;
	int hp_state;

	if (!optee_msg_api_uid_is_optee_image_load(invoke_fn))
		return 0;

	rc = request_firmware(&fw, OPTEE_FW_IMAGE, &pdev->dev);
	if (rc) {
		 
		if (system_state < SYSTEM_RUNNING)
			return -EPROBE_DEFER;
		goto fw_err;
	}

	data_size = fw->size;
	 
	data_buf = kmemdup(fw->data, fw->size, GFP_KERNEL | GFP_DMA);
	if (!data_buf) {
		rc = -ENOMEM;
		goto fw_err;
	}
	data_pa = virt_to_phys(data_buf);
	reg_pair_from_64(&data_pa_high, &data_pa_low, data_pa);
	reg_pair_from_64(&data_size_high, &data_size_low, data_size);
	goto fw_load;

fw_err:
	pr_warn("image loading failed\n");
	data_pa_high = 0;
	data_pa_low = 0;
	data_size_high = 0;
	data_size_low = 0;

fw_load:
	 
	pr_warn("OP-TEE image loaded from kernel, this can be insecure");
	invoke_fn(OPTEE_SMC_CALL_LOAD_IMAGE, data_size_high, data_size_low,
		  data_pa_high, data_pa_low, 0, 0, 0, &res);
	if (!rc)
		rc = res.a0;
	if (fw)
		release_firmware(fw);
	kfree(data_buf);

	if (!rc) {
		 
		cpuhp_invoke_fn = invoke_fn;
		hp_state = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "optee:probe",
					     optee_cpuhp_probe, NULL);
		if (hp_state < 0) {
			pr_warn("Failed with CPU hotplug setup for OP-TEE");
			return -EINVAL;
		}
		cpuhp_remove_state(hp_state);
		cpuhp_invoke_fn = NULL;
	}

	return rc;
}
#else
static inline int optee_load_fw(struct platform_device *pdev,
				optee_invoke_fn *invoke_fn)
{
	return 0;
}
#endif

static int optee_probe(struct platform_device *pdev)
{
	optee_invoke_fn *invoke_fn;
	struct tee_shm_pool *pool = ERR_PTR(-EINVAL);
	struct optee *optee = NULL;
	void *memremaped_shm = NULL;
	unsigned int rpc_param_count;
	struct tee_device *teedev;
	struct tee_context *ctx;
	u32 max_notif_value;
	u32 arg_cache_flags;
	u32 sec_caps;
	int rc;

	invoke_fn = get_invoke_func(&pdev->dev);
	if (IS_ERR(invoke_fn))
		return PTR_ERR(invoke_fn);

	rc = optee_load_fw(pdev, invoke_fn);
	if (rc)
		return rc;

	if (!optee_msg_api_uid_is_optee_api(invoke_fn)) {
		pr_warn("api uid mismatch\n");
		return -EINVAL;
	}

	optee_msg_get_os_revision(invoke_fn);

	if (!optee_msg_api_revision_is_compatible(invoke_fn)) {
		pr_warn("api revision mismatch\n");
		return -EINVAL;
	}

	if (!optee_msg_exchange_capabilities(invoke_fn, &sec_caps,
					     &max_notif_value,
					     &rpc_param_count)) {
		pr_warn("capabilities mismatch\n");
		return -EINVAL;
	}

	 
	if (sec_caps & OPTEE_SMC_SEC_CAP_DYNAMIC_SHM) {
		 
		if (sec_caps & OPTEE_SMC_SEC_CAP_RPC_ARG)
			arg_cache_flags = OPTEE_SHM_ARG_SHARED;
		else
			arg_cache_flags = OPTEE_SHM_ARG_ALLOC_PRIV;

		pool = optee_shm_pool_alloc_pages();
	}

	 
	if (IS_ERR(pool) && (sec_caps & OPTEE_SMC_SEC_CAP_HAVE_RESERVED_SHM)) {
		 
		arg_cache_flags = OPTEE_SHM_ARG_SHARED |
				  OPTEE_SHM_ARG_ALLOC_PRIV;
		pool = optee_config_shm_memremap(invoke_fn, &memremaped_shm);
	}

	if (IS_ERR(pool))
		return PTR_ERR(pool);

	optee = kzalloc(sizeof(*optee), GFP_KERNEL);
	if (!optee) {
		rc = -ENOMEM;
		goto err_free_pool;
	}

	optee->ops = &optee_ops;
	optee->smc.invoke_fn = invoke_fn;
	optee->smc.sec_caps = sec_caps;
	optee->rpc_param_count = rpc_param_count;

	teedev = tee_device_alloc(&optee_clnt_desc, NULL, pool, optee);
	if (IS_ERR(teedev)) {
		rc = PTR_ERR(teedev);
		goto err_free_optee;
	}
	optee->teedev = teedev;

	teedev = tee_device_alloc(&optee_supp_desc, NULL, pool, optee);
	if (IS_ERR(teedev)) {
		rc = PTR_ERR(teedev);
		goto err_unreg_teedev;
	}
	optee->supp_teedev = teedev;

	rc = tee_device_register(optee->teedev);
	if (rc)
		goto err_unreg_supp_teedev;

	rc = tee_device_register(optee->supp_teedev);
	if (rc)
		goto err_unreg_supp_teedev;

	mutex_init(&optee->call_queue.mutex);
	INIT_LIST_HEAD(&optee->call_queue.waiters);
	optee_supp_init(&optee->supp);
	optee->smc.memremaped_shm = memremaped_shm;
	optee->pool = pool;
	optee_shm_arg_cache_init(optee, arg_cache_flags);

	platform_set_drvdata(pdev, optee);
	ctx = teedev_open(optee->teedev);
	if (IS_ERR(ctx)) {
		rc = PTR_ERR(ctx);
		goto err_supp_uninit;
	}
	optee->ctx = ctx;
	rc = optee_notif_init(optee, max_notif_value);
	if (rc)
		goto err_close_ctx;

	if (sec_caps & OPTEE_SMC_SEC_CAP_ASYNC_NOTIF) {
		unsigned int irq;

		rc = platform_get_irq(pdev, 0);
		if (rc < 0) {
			pr_err("platform_get_irq: ret %d\n", rc);
			goto err_notif_uninit;
		}
		irq = rc;

		rc = optee_smc_notif_init_irq(optee, irq);
		if (rc) {
			irq_dispose_mapping(irq);
			goto err_notif_uninit;
		}
		enable_async_notif(optee->smc.invoke_fn);
		pr_info("Asynchronous notifications enabled\n");
	}

	 
	optee_disable_unmapped_shm_cache(optee);

	 
	if (!optee->rpc_param_count)
		optee_enable_shm_cache(optee);

	if (optee->smc.sec_caps & OPTEE_SMC_SEC_CAP_DYNAMIC_SHM)
		pr_info("dynamic shared memory is enabled\n");

	rc = optee_enumerate_devices(PTA_CMD_GET_DEVICES);
	if (rc)
		goto err_disable_shm_cache;

	pr_info("initialized driver\n");
	return 0;

err_disable_shm_cache:
	if (!optee->rpc_param_count)
		optee_disable_shm_cache(optee);
	optee_smc_notif_uninit_irq(optee);
	optee_unregister_devices();
err_notif_uninit:
	optee_notif_uninit(optee);
err_close_ctx:
	teedev_close_context(ctx);
err_supp_uninit:
	optee_shm_arg_cache_uninit(optee);
	optee_supp_uninit(&optee->supp);
	mutex_destroy(&optee->call_queue.mutex);
err_unreg_supp_teedev:
	tee_device_unregister(optee->supp_teedev);
err_unreg_teedev:
	tee_device_unregister(optee->teedev);
err_free_optee:
	kfree(optee);
err_free_pool:
	tee_shm_pool_free(pool);
	if (memremaped_shm)
		memunmap(memremaped_shm);
	return rc;
}

static const struct of_device_id optee_dt_match[] = {
	{ .compatible = "linaro,optee-tz" },
	{},
};
MODULE_DEVICE_TABLE(of, optee_dt_match);

static struct platform_driver optee_driver = {
	.probe  = optee_probe,
	.remove = optee_smc_remove,
	.shutdown = optee_shutdown,
	.driver = {
		.name = "optee",
		.of_match_table = optee_dt_match,
	},
};

int optee_smc_abi_register(void)
{
	return platform_driver_register(&optee_driver);
}

void optee_smc_abi_unregister(void)
{
	platform_driver_unregister(&optee_driver);
}
