
 
#include <linux/bpf.h>
#include <linux/btf_ids.h>
#include <linux/cgroup.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>

#include "../cgroup/cgroup-internal.h"   

 

struct bpf_iter__cgroup {
	__bpf_md_ptr(struct bpf_iter_meta *, meta);
	__bpf_md_ptr(struct cgroup *, cgroup);
};

struct cgroup_iter_priv {
	struct cgroup_subsys_state *start_css;
	bool visited_all;
	bool terminate;
	int order;
};

static void *cgroup_iter_seq_start(struct seq_file *seq, loff_t *pos)
{
	struct cgroup_iter_priv *p = seq->private;

	cgroup_lock();

	 
	if (*pos > 0) {
		if (p->visited_all)
			return NULL;

		 
		return ERR_PTR(-EOPNOTSUPP);
	}

	++*pos;
	p->terminate = false;
	p->visited_all = false;
	if (p->order == BPF_CGROUP_ITER_DESCENDANTS_PRE)
		return css_next_descendant_pre(NULL, p->start_css);
	else if (p->order == BPF_CGROUP_ITER_DESCENDANTS_POST)
		return css_next_descendant_post(NULL, p->start_css);
	else  
		return p->start_css;
}

static int __cgroup_iter_seq_show(struct seq_file *seq,
				  struct cgroup_subsys_state *css, int in_stop);

static void cgroup_iter_seq_stop(struct seq_file *seq, void *v)
{
	struct cgroup_iter_priv *p = seq->private;

	cgroup_unlock();

	 
	if (!v) {
		__cgroup_iter_seq_show(seq, NULL, true);
		p->visited_all = true;
	}
}

static void *cgroup_iter_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct cgroup_subsys_state *curr = (struct cgroup_subsys_state *)v;
	struct cgroup_iter_priv *p = seq->private;

	++*pos;
	if (p->terminate)
		return NULL;

	if (p->order == BPF_CGROUP_ITER_DESCENDANTS_PRE)
		return css_next_descendant_pre(curr, p->start_css);
	else if (p->order == BPF_CGROUP_ITER_DESCENDANTS_POST)
		return css_next_descendant_post(curr, p->start_css);
	else if (p->order == BPF_CGROUP_ITER_ANCESTORS_UP)
		return curr->parent;
	else   
		return NULL;
}

static int __cgroup_iter_seq_show(struct seq_file *seq,
				  struct cgroup_subsys_state *css, int in_stop)
{
	struct cgroup_iter_priv *p = seq->private;
	struct bpf_iter__cgroup ctx;
	struct bpf_iter_meta meta;
	struct bpf_prog *prog;
	int ret = 0;

	 
	if (css && cgroup_is_dead(css->cgroup))
		return 0;

	ctx.meta = &meta;
	ctx.cgroup = css ? css->cgroup : NULL;
	meta.seq = seq;
	prog = bpf_iter_get_info(&meta, in_stop);
	if (prog)
		ret = bpf_iter_run_prog(prog, &ctx);

	 
	if (ret != 0)
		p->terminate = true;

	return 0;
}

static int cgroup_iter_seq_show(struct seq_file *seq, void *v)
{
	return __cgroup_iter_seq_show(seq, (struct cgroup_subsys_state *)v,
				      false);
}

static const struct seq_operations cgroup_iter_seq_ops = {
	.start  = cgroup_iter_seq_start,
	.next   = cgroup_iter_seq_next,
	.stop   = cgroup_iter_seq_stop,
	.show   = cgroup_iter_seq_show,
};

BTF_ID_LIST_GLOBAL_SINGLE(bpf_cgroup_btf_id, struct, cgroup)

static int cgroup_iter_seq_init(void *priv, struct bpf_iter_aux_info *aux)
{
	struct cgroup_iter_priv *p = (struct cgroup_iter_priv *)priv;
	struct cgroup *cgrp = aux->cgroup.start;

	 
	p->start_css = &cgrp->self;
	css_get(p->start_css);
	p->terminate = false;
	p->visited_all = false;
	p->order = aux->cgroup.order;
	return 0;
}

static void cgroup_iter_seq_fini(void *priv)
{
	struct cgroup_iter_priv *p = (struct cgroup_iter_priv *)priv;

	css_put(p->start_css);
}

static const struct bpf_iter_seq_info cgroup_iter_seq_info = {
	.seq_ops		= &cgroup_iter_seq_ops,
	.init_seq_private	= cgroup_iter_seq_init,
	.fini_seq_private	= cgroup_iter_seq_fini,
	.seq_priv_size		= sizeof(struct cgroup_iter_priv),
};

static int bpf_iter_attach_cgroup(struct bpf_prog *prog,
				  union bpf_iter_link_info *linfo,
				  struct bpf_iter_aux_info *aux)
{
	int fd = linfo->cgroup.cgroup_fd;
	u64 id = linfo->cgroup.cgroup_id;
	int order = linfo->cgroup.order;
	struct cgroup *cgrp;

	if (order != BPF_CGROUP_ITER_DESCENDANTS_PRE &&
	    order != BPF_CGROUP_ITER_DESCENDANTS_POST &&
	    order != BPF_CGROUP_ITER_ANCESTORS_UP &&
	    order != BPF_CGROUP_ITER_SELF_ONLY)
		return -EINVAL;

	if (fd && id)
		return -EINVAL;

	if (fd)
		cgrp = cgroup_v1v2_get_from_fd(fd);
	else if (id)
		cgrp = cgroup_get_from_id(id);
	else  
		cgrp = cgroup_get_from_path("/");

	if (IS_ERR(cgrp))
		return PTR_ERR(cgrp);

	aux->cgroup.start = cgrp;
	aux->cgroup.order = order;
	return 0;
}

static void bpf_iter_detach_cgroup(struct bpf_iter_aux_info *aux)
{
	cgroup_put(aux->cgroup.start);
}

static void bpf_iter_cgroup_show_fdinfo(const struct bpf_iter_aux_info *aux,
					struct seq_file *seq)
{
	char *buf;

	buf = kzalloc(PATH_MAX, GFP_KERNEL);
	if (!buf) {
		seq_puts(seq, "cgroup_path:\t<unknown>\n");
		goto show_order;
	}

	 
	cgroup_path_ns(aux->cgroup.start, buf, PATH_MAX,
		       current->nsproxy->cgroup_ns);
	seq_printf(seq, "cgroup_path:\t%s\n", buf);
	kfree(buf);

show_order:
	if (aux->cgroup.order == BPF_CGROUP_ITER_DESCENDANTS_PRE)
		seq_puts(seq, "order: descendants_pre\n");
	else if (aux->cgroup.order == BPF_CGROUP_ITER_DESCENDANTS_POST)
		seq_puts(seq, "order: descendants_post\n");
	else if (aux->cgroup.order == BPF_CGROUP_ITER_ANCESTORS_UP)
		seq_puts(seq, "order: ancestors_up\n");
	else  
		seq_puts(seq, "order: self_only\n");
}

static int bpf_iter_cgroup_fill_link_info(const struct bpf_iter_aux_info *aux,
					  struct bpf_link_info *info)
{
	info->iter.cgroup.order = aux->cgroup.order;
	info->iter.cgroup.cgroup_id = cgroup_id(aux->cgroup.start);
	return 0;
}

DEFINE_BPF_ITER_FUNC(cgroup, struct bpf_iter_meta *meta,
		     struct cgroup *cgroup)

static struct bpf_iter_reg bpf_cgroup_reg_info = {
	.target			= "cgroup",
	.feature		= BPF_ITER_RESCHED,
	.attach_target		= bpf_iter_attach_cgroup,
	.detach_target		= bpf_iter_detach_cgroup,
	.show_fdinfo		= bpf_iter_cgroup_show_fdinfo,
	.fill_link_info		= bpf_iter_cgroup_fill_link_info,
	.ctx_arg_info_size	= 1,
	.ctx_arg_info		= {
		{ offsetof(struct bpf_iter__cgroup, cgroup),
		  PTR_TO_BTF_ID_OR_NULL },
	},
	.seq_info		= &cgroup_iter_seq_info,
};

static int __init bpf_cgroup_iter_init(void)
{
	bpf_cgroup_reg_info.ctx_arg_info[0].btf_id = bpf_cgroup_btf_id[0];
	return bpf_iter_reg_target(&bpf_cgroup_reg_info);
}

late_initcall(bpf_cgroup_iter_init);
