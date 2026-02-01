 
#ifndef MICROPY_INCLUDED_MODOPENAMP_H
#define MICROPY_INCLUDED_MODOPENAMP_H

 
#ifdef MICROPY_PY_OPENAMP_CONFIG_FILE
#include MICROPY_PY_OPENAMP_CONFIG_FILE
#endif

 
 
 
#ifndef MICROPY_PY_OPENAMP_RSC_TABLE_ENABLE
#define MICROPY_PY_OPENAMP_RSC_TABLE_ENABLE   (1)
#endif

 
 
 
#ifndef MICROPY_PY_OPENAMP_TRACE_BUF_ENABLE
#define MICROPY_PY_OPENAMP_TRACE_BUF_ENABLE (1)
#endif



#ifndef MICROPY_PY_OPENAMP_REMOTEPROC_STORE_ENABLE
#define MICROPY_PY_OPENAMP_REMOTEPROC_STORE_ENABLE (1)
#endif



#ifndef MICROPY_PY_OPENAMP_REMOTEPROC_ELFLD_ENABLE
#define MICROPY_PY_OPENAMP_REMOTEPROC_ELFLD_ENABLE (1)
#endif





#if MICROPY_PY_OPENAMP_RSC_TABLE_ENABLE
typedef struct openamp_rsc_table {
    unsigned int version;
    unsigned int num;
    unsigned int reserved[2];
    #if MICROPY_PY_OPENAMP_TRACE_BUF_ENABLE
    unsigned int offset[2];
    #else
    unsigned int offset[1];
    #endif
    struct fw_rsc_vdev vdev;
    struct fw_rsc_vdev_vring vring0;
    struct fw_rsc_vdev_vring vring1;
    #if MICROPY_PY_OPENAMP_TRACE_BUF_ENABLE
    struct fw_rsc_trace trace;
    #endif
} openamp_rsc_table_t;
#endif 



void openamp_init(void);



void openamp_remoteproc_notified(mp_sched_node_t *node);

#endif 
