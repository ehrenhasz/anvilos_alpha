#ifndef _RPADLPAR_IO_H_
#define _RPADLPAR_IO_H_
int dlpar_sysfs_init(void);
void dlpar_sysfs_exit(void);
int dlpar_add_slot(char *drc_name);
int dlpar_remove_slot(char *drc_name);
#endif
