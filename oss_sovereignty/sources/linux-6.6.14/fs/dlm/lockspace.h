


#ifndef __LOCKSPACE_DOT_H__
#define __LOCKSPACE_DOT_H__


#define DLM_LSFL_FS	0x00000004

int dlm_lockspace_init(void);
void dlm_lockspace_exit(void);
struct dlm_ls *dlm_find_lockspace_global(uint32_t id);
struct dlm_ls *dlm_find_lockspace_local(void *id);
struct dlm_ls *dlm_find_lockspace_device(int minor);
void dlm_put_lockspace(struct dlm_ls *ls);
void dlm_stop_lockspaces(void);
int dlm_new_user_lockspace(const char *name, const char *cluster,
			   uint32_t flags, int lvblen,
			   const struct dlm_lockspace_ops *ops,
			   void *ops_arg, int *ops_result,
			   dlm_lockspace_t **lockspace);

#endif				

