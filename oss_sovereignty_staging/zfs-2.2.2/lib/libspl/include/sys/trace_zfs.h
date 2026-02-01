 

#ifndef _LIBSPL_ZFS_TRACE_H
#define	_LIBSPL_ZFS_TRACE_H

 
#undef SET_ERROR
#define	SET_ERROR(err) \
	(__set_error(__FILE__, __func__, __LINE__, err), err)


#endif
