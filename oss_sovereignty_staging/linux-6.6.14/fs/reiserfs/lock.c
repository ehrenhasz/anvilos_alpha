
#include "reiserfs.h"
#include <linux/mutex.h>

 
void reiserfs_write_lock(struct super_block *s)
{
	struct reiserfs_sb_info *sb_i = REISERFS_SB(s);

	if (sb_i->lock_owner != current) {
		mutex_lock(&sb_i->lock);
		sb_i->lock_owner = current;
	}

	 
	sb_i->lock_depth++;
}

void reiserfs_write_unlock(struct super_block *s)
{
	struct reiserfs_sb_info *sb_i = REISERFS_SB(s);

	 
	BUG_ON(sb_i->lock_owner != current);

	if (--sb_i->lock_depth == -1) {
		sb_i->lock_owner = NULL;
		mutex_unlock(&sb_i->lock);
	}
}

int __must_check reiserfs_write_unlock_nested(struct super_block *s)
{
	struct reiserfs_sb_info *sb_i = REISERFS_SB(s);
	int depth;

	 
	if (sb_i->lock_owner != current)
		return -1;

	depth = sb_i->lock_depth;

	sb_i->lock_depth = -1;
	sb_i->lock_owner = NULL;
	mutex_unlock(&sb_i->lock);

	return depth;
}

void reiserfs_write_lock_nested(struct super_block *s, int depth)
{
	struct reiserfs_sb_info *sb_i = REISERFS_SB(s);

	 
	if (depth == -1)
		return;

	mutex_lock(&sb_i->lock);
	sb_i->lock_owner = current;
	sb_i->lock_depth = depth;
}

 
void reiserfs_check_lock_depth(struct super_block *sb, char *caller)
{
	struct reiserfs_sb_info *sb_i = REISERFS_SB(sb);

	WARN_ON(sb_i->lock_depth < 0);
}

#ifdef CONFIG_REISERFS_CHECK
void reiserfs_lock_check_recursive(struct super_block *sb)
{
	struct reiserfs_sb_info *sb_i = REISERFS_SB(sb);

	WARN_ONCE((sb_i->lock_depth > 0), "Unwanted recursive reiserfs lock!\n");
}
#endif
