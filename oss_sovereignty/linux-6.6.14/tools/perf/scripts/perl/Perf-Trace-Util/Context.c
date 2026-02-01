
 
#include <stdbool.h>
#ifndef HAS_BOOL
# define HAS_BOOL 1
#endif
#line 1 "Context.xs"
 

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "../../../util/trace-event.h"

#ifndef PERL_UNUSED_VAR
#  define PERL_UNUSED_VAR(var) if (0) var = var
#endif

#line 42 "Context.c"

XS(XS_Perf__Trace__Context_common_pc);  
XS(XS_Perf__Trace__Context_common_pc)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Perf::Trace::Context::common_pc", "context");
    PERL_UNUSED_VAR(cv);  
    {
	struct scripting_context *	context = INT2PTR(struct scripting_context *,SvIV(ST(0)));
	int	RETVAL;
	dXSTARG;

	RETVAL = common_pc(context);
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}


XS(XS_Perf__Trace__Context_common_flags);  
XS(XS_Perf__Trace__Context_common_flags)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Perf::Trace::Context::common_flags", "context");
    PERL_UNUSED_VAR(cv);  
    {
	struct scripting_context *	context = INT2PTR(struct scripting_context *,SvIV(ST(0)));
	int	RETVAL;
	dXSTARG;

	RETVAL = common_flags(context);
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}


XS(XS_Perf__Trace__Context_common_lock_depth);  
XS(XS_Perf__Trace__Context_common_lock_depth)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    if (items != 1)
       Perl_croak(aTHX_ "Usage: %s(%s)", "Perf::Trace::Context::common_lock_depth", "context");
    PERL_UNUSED_VAR(cv);  
    {
	struct scripting_context *	context = INT2PTR(struct scripting_context *,SvIV(ST(0)));
	int	RETVAL;
	dXSTARG;

	RETVAL = common_lock_depth(context);
	XSprePUSH; PUSHi((IV)RETVAL);
    }
    XSRETURN(1);
}

#ifdef __cplusplus
extern "C"
#endif
XS(boot_Perf__Trace__Context);  
XS(boot_Perf__Trace__Context)
{
#ifdef dVAR
    dVAR; dXSARGS;
#else
    dXSARGS;
#endif
    const char* file = __FILE__;

    PERL_UNUSED_VAR(cv);  
    PERL_UNUSED_VAR(items);  
    XS_VERSION_BOOTCHECK ;

        newXSproto("Perf::Trace::Context::common_pc", XS_Perf__Trace__Context_common_pc, file, "$");
        newXSproto("Perf::Trace::Context::common_flags", XS_Perf__Trace__Context_common_flags, file, "$");
        newXSproto("Perf::Trace::Context::common_lock_depth", XS_Perf__Trace__Context_common_lock_depth, file, "$");
    if (PL_unitcheckav)
         call_list(PL_scopestack_ix, PL_unitcheckav);
    XSRETURN_YES;
}

