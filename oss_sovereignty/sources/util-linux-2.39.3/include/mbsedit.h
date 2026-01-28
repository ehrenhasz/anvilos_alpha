
#ifndef UTIL_LINUX_MBSEDIT_H
# define UTIL_LINUX_MBSEDIT_H

#include "mbsalign.h"
#include "widechar.h"

struct mbs_editor {
	char	*buf;		
	size_t	max_bytes;	
	size_t	max_cells;	
	size_t	cur_cells;	
	size_t  cur_bytes;	
	size_t  cursor;		
	size_t  cursor_cells;	
};

enum {
	MBS_EDIT_LEFT,
	MBS_EDIT_RIGHT,
	MBS_EDIT_END,
	MBS_EDIT_HOME
};

struct mbs_editor *mbs_new_edit(char *buf, size_t bufsz, size_t ncells);
char *mbs_free_edit(struct mbs_editor *edit);

int mbs_edit_goto(struct mbs_editor *edit, int where);
int mbs_edit_delete(struct mbs_editor *edit);
int mbs_edit_backspace(struct mbs_editor *edit);
int mbs_edit_insert(struct mbs_editor *edit, wint_t c);

#endif 
