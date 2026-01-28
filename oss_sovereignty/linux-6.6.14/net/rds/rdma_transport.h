#ifndef _RDMA_TRANSPORT_H
#define _RDMA_TRANSPORT_H
#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>
#include "rds.h"
#define RDS_CM_PORT	16385
#define RDS_RDMA_RESOLVE_TIMEOUT_MS     5000
#define RDS_RDMA_REJ_INCOMPAT		1
int rds_rdma_cm_event_handler(struct rdma_cm_id *cm_id,
			      struct rdma_cm_event *event);
int rds6_rdma_cm_event_handler(struct rdma_cm_id *cm_id,
			       struct rdma_cm_event *event);
extern struct rds_transport rds_ib_transport;
int rds_ib_init(void);
void rds_ib_exit(void);
#endif
