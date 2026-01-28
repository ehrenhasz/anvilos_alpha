#ifndef CXD2880_INTEG_H
#define CXD2880_INTEG_H
#include "cxd2880_tnrdmd.h"
#define CXD2880_TNRDMD_WAIT_INIT_TIMEOUT	500
#define CXD2880_TNRDMD_WAIT_INIT_INTVL	10
#define CXD2880_TNRDMD_WAIT_AGC_STABLE		100
int cxd2880_integ_init(struct cxd2880_tnrdmd *tnr_dmd);
int cxd2880_integ_cancel(struct cxd2880_tnrdmd *tnr_dmd);
int cxd2880_integ_check_cancellation(struct cxd2880_tnrdmd
				     *tnr_dmd);
#endif
