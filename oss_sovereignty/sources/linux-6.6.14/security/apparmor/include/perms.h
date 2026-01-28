


#ifndef __AA_PERM_H
#define __AA_PERM_H

#include <linux/fs.h>
#include "label.h"

#define AA_MAY_EXEC		MAY_EXEC
#define AA_MAY_WRITE		MAY_WRITE
#define AA_MAY_READ		MAY_READ
#define AA_MAY_APPEND		MAY_APPEND

#define AA_MAY_CREATE		0x0010
#define AA_MAY_DELETE		0x0020
#define AA_MAY_OPEN		0x0040
#define AA_MAY_RENAME		0x0080		

#define AA_MAY_SETATTR		0x0100		
#define AA_MAY_GETATTR		0x0200		
#define AA_MAY_SETCRED		0x0400		
#define AA_MAY_GETCRED		0x0800

#define AA_MAY_CHMOD		0x1000		
#define AA_MAY_CHOWN		0x2000		
#define AA_MAY_CHGRP		0x4000		
#define AA_MAY_LOCK		0x8000		

#define AA_EXEC_MMAP		0x00010000
#define AA_MAY_MPROT		0x00020000	
#define AA_MAY_LINK		0x00040000	
#define AA_MAY_SNAPSHOT		0x00080000	

#define AA_MAY_DELEGATE
#define AA_CONT_MATCH		0x08000000

#define AA_MAY_STACK		0x10000000
#define AA_MAY_ONEXEC		0x20000000 
#define AA_MAY_CHANGE_PROFILE	0x40000000
#define AA_MAY_CHANGEHAT	0x80000000

#define AA_LINK_SUBSET		AA_MAY_LOCK	


#define PERMS_CHRS_MASK (MAY_READ | MAY_WRITE | AA_MAY_CREATE |		\
			 AA_MAY_DELETE | AA_MAY_LINK | AA_MAY_LOCK |	\
			 AA_MAY_EXEC | AA_EXEC_MMAP | AA_MAY_APPEND)

#define PERMS_NAMES_MASK (PERMS_CHRS_MASK | AA_MAY_OPEN | AA_MAY_RENAME |     \
			  AA_MAY_SETATTR | AA_MAY_GETATTR | AA_MAY_SETCRED | \
			  AA_MAY_GETCRED | AA_MAY_CHMOD | AA_MAY_CHOWN | \
			  AA_MAY_CHGRP | AA_MAY_MPROT | AA_MAY_SNAPSHOT | \
			  AA_MAY_STACK | AA_MAY_ONEXEC |		\
			  AA_MAY_CHANGE_PROFILE | AA_MAY_CHANGEHAT)

extern const char aa_file_perm_chrs[];
extern const char *aa_file_perm_names[];

struct aa_perms {
	u32 allow;
	u32 deny;	

	u32 subtree;	
	u32 cond;	

	u32 kill;	
	u32 complain;	
	u32 prompt;	

	u32 audit;	
	u32 quiet;	
	u32 hide;	


	u32 xindex;
	u32 tag;	
	u32 label;	
};


#define AA_INDEX_MASK			0x00ffffff
#define AA_INDEX_FLAG_MASK		0xff000000
#define AA_INDEX_NONE			0

#define ALL_PERMS_MASK 0xffffffff
extern struct aa_perms nullperms;
extern struct aa_perms allperms;


static inline void aa_perms_accum_raw(struct aa_perms *accum,
				      struct aa_perms *addend)
{
	accum->deny |= addend->deny;
	accum->allow &= addend->allow & ~addend->deny;
	accum->audit |= addend->audit & addend->allow;
	accum->quiet &= addend->quiet & ~addend->allow;
	accum->kill |= addend->kill & ~addend->allow;
	accum->complain |= addend->complain & ~addend->allow & ~addend->deny;
	accum->cond |= addend->cond & ~addend->allow & ~addend->deny;
	accum->hide &= addend->hide & ~addend->allow;
	accum->prompt |= addend->prompt & ~addend->allow & ~addend->deny;
	accum->subtree |= addend->subtree & ~addend->deny;

	if (!accum->xindex)
		accum->xindex = addend->xindex;
	if (!accum->tag)
		accum->tag = addend->tag;
	if (!accum->label)
		accum->label = addend->label;
}


static inline void aa_perms_accum(struct aa_perms *accum,
				  struct aa_perms *addend)
{
	accum->deny |= addend->deny;
	accum->allow &= addend->allow & ~accum->deny;
	accum->audit |= addend->audit & accum->allow;
	accum->quiet &= addend->quiet & ~accum->allow;
	accum->kill |= addend->kill & ~accum->allow;
	accum->complain |= addend->complain & ~accum->allow & ~accum->deny;
	accum->cond |= addend->cond & ~accum->allow & ~accum->deny;
	accum->hide &= addend->hide & ~accum->allow;
	accum->prompt |= addend->prompt & ~accum->allow & ~accum->deny;
	accum->subtree &= addend->subtree & ~accum->deny;

	if (!accum->xindex)
		accum->xindex = addend->xindex;
	if (!accum->tag)
		accum->tag = addend->tag;
	if (!accum->label)
		accum->label = addend->label;
}

#define xcheck(FN1, FN2)	\
({				\
	int e, error = FN1;	\
	e = FN2;		\
	if (e)			\
		error = e;	\
	error;			\
})



#define xcheck_ns_profile_profile(P1, P2, FN, args...)		\
({								\
	int ____e = 0;						\
	if (P1->ns == P2->ns)					\
		____e = FN((P1), (P2), args);			\
	(____e);						\
})

#define xcheck_ns_profile_label(P, L, FN, args...)		\
({								\
	struct aa_profile *__p2;				\
	fn_for_each((L), __p2,					\
		    xcheck_ns_profile_profile((P), __p2, (FN), args));	\
})

#define xcheck_ns_labels(L1, L2, FN, args...)			\
({								\
	struct aa_profile *__p1;				\
	fn_for_each((L1), __p1, FN(__p1, (L2), args));		\
})


#define xcheck_labels_profiles(L1, L2, FN, args...)		\
	xcheck_ns_labels((L1), (L2), xcheck_ns_profile_label, (FN), args)

#define xcheck_labels(L1, L2, P, FN1, FN2)			\
	xcheck(fn_for_each((L1), (P), (FN1)), fn_for_each((L2), (P), (FN2)))


extern struct aa_perms default_perms;


void aa_perm_mask_to_str(char *str, size_t str_size, const char *chrs,
			 u32 mask);
void aa_audit_perm_names(struct audit_buffer *ab, const char * const *names,
			 u32 mask);
void aa_audit_perm_mask(struct audit_buffer *ab, u32 mask, const char *chrs,
			u32 chrsmask, const char * const *names, u32 namesmask);
void aa_apply_modes_to_perms(struct aa_profile *profile,
			     struct aa_perms *perms);
void aa_perms_accum(struct aa_perms *accum, struct aa_perms *addend);
void aa_perms_accum_raw(struct aa_perms *accum, struct aa_perms *addend);
void aa_profile_match_label(struct aa_profile *profile,
			    struct aa_ruleset *rules, struct aa_label *label,
			    int type, u32 request, struct aa_perms *perms);
int aa_profile_label_perm(struct aa_profile *profile, struct aa_profile *target,
			  u32 request, int type, u32 *deny,
			  struct apparmor_audit_data *ad);
int aa_check_perms(struct aa_profile *profile, struct aa_perms *perms,
		   u32 request, struct apparmor_audit_data *ad,
		   void (*cb)(struct audit_buffer *, void *));
#endif 
