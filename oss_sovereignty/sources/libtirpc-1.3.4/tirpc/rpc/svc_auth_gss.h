

#ifndef _TIRPC_SVC_AUTH_GSS_H
#define _TIRPC_SVC_AUTH_GSS_H

#include <rpc/svc_auth.h>
#include <gssapi/gssapi.h>



#ifdef __cplusplus
extern "C" {
#endif

extern bool_t svcauth_gss_set_svc_name(gss_name_t name);
extern char *svcauth_gss_get_principal(SVCAUTH *);

#ifdef __cplusplus
}
#endif

#endif	
