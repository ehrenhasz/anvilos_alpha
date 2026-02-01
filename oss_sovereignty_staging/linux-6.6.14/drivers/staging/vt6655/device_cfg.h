 
 

#ifndef __DEVICE_CONFIG_H
#define __DEVICE_CONFIG_H

#include <linux/types.h>

#define VID_TABLE_SIZE      64
#define MCAST_TABLE_SIZE    64
#define MCAM_SIZE           32
#define VCAM_SIZE           32
#define TX_QUEUE_NO         8

#define DEVICE_NAME         "vt6655"
#define DEVICE_FULL_DRV_NAM "VIA Networking Solomon-A/B/G Wireless LAN Adapter Driver"

#ifndef MAJOR_VERSION
#define MAJOR_VERSION       1
#endif

#ifndef MINOR_VERSION
#define MINOR_VERSION       17
#endif

#ifndef DEVICE_VERSION
#define DEVICE_VERSION       "1.19.12"
#endif

#include <linux/fs.h>
#include <linux/fcntl.h>

#define PKT_BUF_SZ          2390

#endif
