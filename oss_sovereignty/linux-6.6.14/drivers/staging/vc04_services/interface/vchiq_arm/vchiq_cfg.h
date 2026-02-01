 
 

#ifndef VCHIQ_CFG_H
#define VCHIQ_CFG_H

#define VCHIQ_MAGIC              VCHIQ_MAKE_FOURCC('V', 'C', 'H', 'I')
 
#define VCHIQ_VERSION            8
 
#define VCHIQ_VERSION_MIN        3

 
#define VCHIQ_VERSION_LIB_VERSION 7

 
#define VCHIQ_VERSION_CLOSE_DELIVERED 7

 
#define VCHIQ_VERSION_SYNCHRONOUS_MODE 8

#define VCHIQ_MAX_STATES         1
#define VCHIQ_MAX_SERVICES       4096
#define VCHIQ_MAX_SLOTS          128
#define VCHIQ_MAX_SLOTS_PER_SIDE 64

#define VCHIQ_NUM_CURRENT_BULKS        32
#define VCHIQ_NUM_SERVICE_BULKS        4

#ifndef VCHIQ_ENABLE_DEBUG
#define VCHIQ_ENABLE_DEBUG             1
#endif

#ifndef VCHIQ_ENABLE_STATS
#define VCHIQ_ENABLE_STATS             1
#endif

#endif  
