 
 

#ifndef __DIR_DOT_H__
#define __DIR_DOT_H__

int dlm_dir_nodeid(struct dlm_rsb *rsb);
int dlm_hash2nodeid(struct dlm_ls *ls, uint32_t hash);
void dlm_recover_dir_nodeid(struct dlm_ls *ls);
int dlm_recover_directory(struct dlm_ls *ls, uint64_t seq);
void dlm_copy_master_names(struct dlm_ls *ls, const char *inbuf, int inlen,
			   char *outbuf, int outlen, int nodeid);

#endif				 

