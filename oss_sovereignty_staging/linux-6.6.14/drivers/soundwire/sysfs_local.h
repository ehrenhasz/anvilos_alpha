 
 

#ifndef __SDW_SYSFS_LOCAL_H
#define __SDW_SYSFS_LOCAL_H

 

 
extern const struct attribute_group *sdw_slave_status_attr_groups[];

 
int sdw_slave_sysfs_init(struct sdw_slave *slave);
int sdw_slave_sysfs_dpn_init(struct sdw_slave *slave);

#endif  
