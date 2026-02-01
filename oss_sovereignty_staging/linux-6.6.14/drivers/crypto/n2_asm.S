 
 

#include <linux/linkage.h>
#include <asm/hypervisor.h>
#include "n2_core.h"

	 
ENTRY(sun4v_ncs_qconf)
	mov	HV_FAST_NCS_QCONF, %o5
	ta	HV_FAST_TRAP
	stx	%o1, [%o3]
	retl
	 nop
ENDPROC(sun4v_ncs_qconf)

	 
ENTRY(sun4v_ncs_qinfo)
	mov	%o1, %g1
	mov	%o2, %g2
	mov	%o3, %g3
	mov	HV_FAST_NCS_QINFO, %o5
	ta	HV_FAST_TRAP
	stx	%o1, [%g1]
	stx	%o2, [%g2]
	stx	%o3, [%g3]
	retl
	 nop
ENDPROC(sun4v_ncs_qinfo)

	 
ENTRY(sun4v_ncs_gethead)
	mov	%o1, %o2
	mov	HV_FAST_NCS_GETHEAD, %o5
	ta	HV_FAST_TRAP
	stx	%o1, [%o2]
	retl
	 nop
ENDPROC(sun4v_ncs_gethead)

	 
ENTRY(sun4v_ncs_gettail)
	mov	%o1, %o2
	mov	HV_FAST_NCS_GETTAIL, %o5
	ta	HV_FAST_TRAP
	stx	%o1, [%o2]
	retl
	 nop
ENDPROC(sun4v_ncs_gettail)

	 
ENTRY(sun4v_ncs_settail)
	mov	HV_FAST_NCS_SETTAIL, %o5
	ta	HV_FAST_TRAP
	retl
	 nop
ENDPROC(sun4v_ncs_settail)

	 
ENTRY(sun4v_ncs_qhandle_to_devino)
	mov	%o1, %o2
	mov	HV_FAST_NCS_QHANDLE_TO_DEVINO, %o5
	ta	HV_FAST_TRAP
	stx	%o1, [%o2]
	retl
	 nop
ENDPROC(sun4v_ncs_qhandle_to_devino)

	 
ENTRY(sun4v_ncs_sethead_marker)
	mov	HV_FAST_NCS_SETHEAD_MARKER, %o5
	ta	HV_FAST_TRAP
	retl
	 nop
ENDPROC(sun4v_ncs_sethead_marker)
