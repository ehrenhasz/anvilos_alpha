#ifndef __DIAG_FTP_H__
#define __DIAG_FTP_H__
#include "hmcdrv_ftp.h"
int diag_ftp_startup(void);
void diag_ftp_shutdown(void);
ssize_t diag_ftp_cmd(const struct hmcdrv_ftp_cmdspec *ftp, size_t *fsize);
#endif	  
