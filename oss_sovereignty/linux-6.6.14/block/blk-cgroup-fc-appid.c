

#include "blk-cgroup.h"

 
int blkcg_set_fc_appid(char *app_id, u64 cgrp_id, size_t app_id_len)
{
	struct cgroup *cgrp;
	struct cgroup_subsys_state *css;
	struct blkcg *blkcg;
	int ret  = 0;

	if (app_id_len > FC_APPID_LEN)
		return -EINVAL;

	cgrp = cgroup_get_from_id(cgrp_id);
	if (IS_ERR(cgrp))
		return PTR_ERR(cgrp);
	css = cgroup_get_e_css(cgrp, &io_cgrp_subsys);
	if (!css) {
		ret = -ENOENT;
		goto out_cgrp_put;
	}
	blkcg = css_to_blkcg(css);
	 
	strscpy(blkcg->fc_app_id, app_id, app_id_len);
	css_put(css);
out_cgrp_put:
	cgroup_put(cgrp);
	return ret;
}
EXPORT_SYMBOL_GPL(blkcg_set_fc_appid);

 
char *blkcg_get_fc_appid(struct bio *bio)
{
	if (!bio->bi_blkg || bio->bi_blkg->blkcg->fc_app_id[0] == '\0')
		return NULL;
	return bio->bi_blkg->blkcg->fc_app_id;
}
EXPORT_SYMBOL_GPL(blkcg_get_fc_appid);
