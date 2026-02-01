 
 

#ifndef PWC_DEC1_H
#define PWC_DEC1_H

#include <linux/mutex.h>

struct pwc_device;

struct pwc_dec1_private
{
	int version;
};

void pwc_dec1_init(struct pwc_device *pdev, const unsigned char *cmd);

#endif
