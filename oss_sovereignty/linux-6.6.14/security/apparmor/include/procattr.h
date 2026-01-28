#ifndef __AA_PROCATTR_H
#define __AA_PROCATTR_H
int aa_getprocattr(struct aa_label *label, char **string);
int aa_setprocattr_changehat(char *args, size_t size, int flags);
#endif  
