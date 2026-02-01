 
 

#ifndef __SCLP_FTP_H__
#define __SCLP_FTP_H__

#include "hmcdrv_ftp.h"

int sclp_ftp_startup(void);
void sclp_ftp_shutdown(void);
ssize_t sclp_ftp_cmd(const struct hmcdrv_ftp_cmdspec *ftp, size_t *fsize);

#endif	  
