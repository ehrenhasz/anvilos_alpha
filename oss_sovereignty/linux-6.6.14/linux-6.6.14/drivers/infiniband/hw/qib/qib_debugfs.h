#ifndef _QIB_DEBUGFS_H
#define _QIB_DEBUGFS_H
#ifdef CONFIG_DEBUG_FS
struct qib_ibdev;
void qib_dbg_ibdev_init(struct qib_ibdev *ibd);
void qib_dbg_ibdev_exit(struct qib_ibdev *ibd);
void qib_dbg_init(void);
void qib_dbg_exit(void);
#endif
#endif                           
