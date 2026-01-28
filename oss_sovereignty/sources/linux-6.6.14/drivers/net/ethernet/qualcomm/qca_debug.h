



#ifndef _QCA_DEBUG_H
#define _QCA_DEBUG_H

#include "qca_spi.h"

void qcaspi_init_device_debugfs(struct qcaspi *qca);

void qcaspi_remove_device_debugfs(struct qcaspi *qca);

void qcaspi_set_ethtool_ops(struct net_device *dev);

#endif 
