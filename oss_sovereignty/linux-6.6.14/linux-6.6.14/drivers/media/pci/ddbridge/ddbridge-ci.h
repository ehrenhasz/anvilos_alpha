#ifndef __DDBRIDGE_CI_H__
#define __DDBRIDGE_CI_H__
#include "ddbridge.h"
int ddb_ci_attach(struct ddb_port *port, u32 bitrate);
void ddb_ci_detach(struct ddb_port *port);
#endif  
