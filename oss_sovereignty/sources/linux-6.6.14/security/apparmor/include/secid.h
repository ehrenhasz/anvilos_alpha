


#ifndef __AA_SECID_H
#define __AA_SECID_H

#include <linux/slab.h>
#include <linux/types.h>

struct aa_label;


#define AA_SECID_INVALID 0


#define AA_SECID_WILDCARD 1


extern int apparmor_display_secid_mode;

struct aa_label *aa_secid_to_label(u32 secid);
int apparmor_secid_to_secctx(u32 secid, char **secdata, u32 *seclen);
int apparmor_secctx_to_secid(const char *secdata, u32 seclen, u32 *secid);
void apparmor_release_secctx(char *secdata, u32 seclen);


int aa_alloc_secid(struct aa_label *label, gfp_t gfp);
void aa_free_secid(u32 secid);
void aa_secid_update(u32 secid, struct aa_label *label);

#endif 
