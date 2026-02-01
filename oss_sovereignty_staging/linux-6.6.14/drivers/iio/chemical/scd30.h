 
#ifndef _SCD30_H
#define _SCD30_H

#include <linux/completion.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/pm.h>
#include <linux/regulator/consumer.h>
#include <linux/types.h>

struct scd30_state;

enum scd30_cmd {
	 
	CMD_START_MEAS,
	 
	CMD_STOP_MEAS,
	 
	CMD_MEAS_INTERVAL,
	 
	CMD_MEAS_READY,
	 
	CMD_READ_MEAS,
	 
	CMD_ASC,
	 
	CMD_FRC,
	 
	CMD_TEMP_OFFSET,
	 
	CMD_FW_VERSION,
	 
	CMD_RESET,
	 
};

#define SCD30_MEAS_COUNT 3

typedef int (*scd30_command_t)(struct scd30_state *state, enum scd30_cmd cmd, u16 arg,
			       void *response, int size);

struct scd30_state {
	 
	struct mutex lock;
	struct device *dev;
	struct regulator *vdd;
	struct completion meas_ready;
	 
	void *priv;
	int irq;
	 
	u16 pressure_comp;
	u16 meas_interval;
	int meas[SCD30_MEAS_COUNT];

	scd30_command_t command;
};

extern const struct dev_pm_ops scd30_pm_ops;

int scd30_probe(struct device *dev, int irq, const char *name, void *priv, scd30_command_t command);

#endif
