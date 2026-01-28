


#ifndef _SSH_GSS_H
#define _SSH_GSS_H

#ifdef GSSAPI

#ifdef HAVE_GSSAPI_H
#include <gssapi.h>
#elif defined(HAVE_GSSAPI_GSSAPI_H)
#include <gssapi/gssapi.h>
#endif

#ifdef KRB5
# ifndef HEIMDAL
#  ifdef HAVE_GSSAPI_GENERIC_H
#   include <gssapi_generic.h>
#  elif defined(HAVE_GSSAPI_GSSAPI_GENERIC_H)
#   include <gssapi/gssapi_generic.h>
#  endif



#  if !HAVE_DECL_GSS_C_NT_HOSTBASED_SERVICE
#   define GSS_C_NT_HOSTBASED_SERVICE gss_nt_service_name
#  endif 

# endif 
#endif 


#define SSH2_MSG_USERAUTH_GSSAPI_RESPONSE		60
#define SSH2_MSG_USERAUTH_GSSAPI_TOKEN			61
#define SSH2_MSG_USERAUTH_GSSAPI_EXCHANGE_COMPLETE	63
#define SSH2_MSG_USERAUTH_GSSAPI_ERROR			64
#define SSH2_MSG_USERAUTH_GSSAPI_ERRTOK			65
#define SSH2_MSG_USERAUTH_GSSAPI_MIC			66

#define SSH_GSS_OIDTYPE 0x06

typedef struct {
	char *filename;
	char *envvar;
	char *envval;
	void *data;
} ssh_gssapi_ccache;

typedef struct {
	gss_buffer_desc displayname;
	gss_buffer_desc exportedname;
	gss_cred_id_t creds;
	struct ssh_gssapi_mech_struct *mech;
	ssh_gssapi_ccache store;
} ssh_gssapi_client;

typedef struct ssh_gssapi_mech_struct {
	char *enc_name;
	char *name;
	gss_OID_desc oid;
	int (*dochild) (ssh_gssapi_client *);
	int (*userok) (ssh_gssapi_client *, char *);
	int (*localname) (ssh_gssapi_client *, char **);
	void (*storecreds) (ssh_gssapi_client *);
} ssh_gssapi_mech;

typedef struct {
	OM_uint32	major; 
	OM_uint32	minor; 
	gss_ctx_id_t	context; 
	gss_name_t	name; 
	gss_OID		oid; 
	gss_cred_id_t	creds; 
	gss_name_t	client; 
	gss_cred_id_t	client_creds; 
} Gssctxt;

extern ssh_gssapi_mech *supported_mechs[];

int  ssh_gssapi_check_oid(Gssctxt *, void *, size_t);
void ssh_gssapi_set_oid_data(Gssctxt *, void *, size_t);
void ssh_gssapi_set_oid(Gssctxt *, gss_OID);
void ssh_gssapi_supported_oids(gss_OID_set *);
ssh_gssapi_mech *ssh_gssapi_get_ctype(Gssctxt *);
void ssh_gssapi_prepare_supported_oids(void);
OM_uint32 ssh_gssapi_test_oid_supported(OM_uint32 *, gss_OID, int *);

struct sshbuf;
int ssh_gssapi_get_buffer_desc(struct sshbuf *, gss_buffer_desc *);

OM_uint32 ssh_gssapi_import_name(Gssctxt *, const char *);
OM_uint32 ssh_gssapi_init_ctx(Gssctxt *, int,
    gss_buffer_desc *, gss_buffer_desc *, OM_uint32 *);
OM_uint32 ssh_gssapi_accept_ctx(Gssctxt *,
    gss_buffer_desc *, gss_buffer_desc *, OM_uint32 *);
OM_uint32 ssh_gssapi_getclient(Gssctxt *, ssh_gssapi_client *);
void ssh_gssapi_error(Gssctxt *);
char *ssh_gssapi_last_error(Gssctxt *, OM_uint32 *, OM_uint32 *);
void ssh_gssapi_build_ctx(Gssctxt **);
void ssh_gssapi_delete_ctx(Gssctxt **);
OM_uint32 ssh_gssapi_sign(Gssctxt *, gss_buffer_t, gss_buffer_t);
void ssh_gssapi_buildmic(struct sshbuf *, const char *,
    const char *, const char *, const struct sshbuf *);
int ssh_gssapi_check_mechanism(Gssctxt **, gss_OID, const char *);


OM_uint32 ssh_gssapi_server_ctx(Gssctxt **, gss_OID);
int ssh_gssapi_userok(char *name);
OM_uint32 ssh_gssapi_checkmic(Gssctxt *, gss_buffer_t, gss_buffer_t);
void ssh_gssapi_do_child(char ***, u_int *);
void ssh_gssapi_cleanup_creds(void);
void ssh_gssapi_storecreds(void);
const char *ssh_gssapi_displayname(void);

#endif 

#endif 
