 

 

#include "includes.h"

#ifdef GSSAPI

#include <sys/types.h>

#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "openbsd-compat/sys-queue.h"
#include "xmalloc.h"
#include "sshkey.h"
#include "hostfile.h"
#include "auth.h"
#include "log.h"
#include "channels.h"
#include "session.h"
#include "misc.h"
#include "servconf.h"

#include "ssh-gss.h"

extern ServerOptions options;

static ssh_gssapi_client gssapi_client =
    { GSS_C_EMPTY_BUFFER, GSS_C_EMPTY_BUFFER,
    GSS_C_NO_CREDENTIAL, NULL, {NULL, NULL, NULL, NULL}};

ssh_gssapi_mech gssapi_null_mech =
    { NULL, NULL, {0, NULL}, NULL, NULL, NULL, NULL};

#ifdef KRB5
extern ssh_gssapi_mech gssapi_kerberos_mech;
#endif

ssh_gssapi_mech* supported_mechs[]= {
#ifdef KRB5
	&gssapi_kerberos_mech,
#endif
	&gssapi_null_mech,
};

 
static gss_OID_set supported_oids;

void
ssh_gssapi_prepare_supported_oids(void)
{
	ssh_gssapi_supported_oids(&supported_oids);
}

OM_uint32
ssh_gssapi_test_oid_supported(OM_uint32 *ms, gss_OID member, int *present)
{
	if (supported_oids == NULL)
		ssh_gssapi_prepare_supported_oids();
	return gss_test_oid_set_member(ms, member, supported_oids, present);
}

 

 
 
static OM_uint32
ssh_gssapi_acquire_cred(Gssctxt *ctx)
{
	OM_uint32 status;
	char lname[NI_MAXHOST];
	gss_OID_set oidset;

	if (options.gss_strict_acceptor) {
		gss_create_empty_oid_set(&status, &oidset);
		gss_add_oid_set_member(&status, ctx->oid, &oidset);

		if (gethostname(lname, HOST_NAME_MAX)) {
			gss_release_oid_set(&status, &oidset);
			return (-1);
		}

		if (GSS_ERROR(ssh_gssapi_import_name(ctx, lname))) {
			gss_release_oid_set(&status, &oidset);
			return (ctx->major);
		}

		if ((ctx->major = gss_acquire_cred(&ctx->minor,
		    ctx->name, 0, oidset, GSS_C_ACCEPT, &ctx->creds,
		    NULL, NULL)))
			ssh_gssapi_error(ctx);

		gss_release_oid_set(&status, &oidset);
		return (ctx->major);
	} else {
		ctx->name = GSS_C_NO_NAME;
		ctx->creds = GSS_C_NO_CREDENTIAL;
	}
	return GSS_S_COMPLETE;
}

 
OM_uint32
ssh_gssapi_server_ctx(Gssctxt **ctx, gss_OID oid)
{
	if (*ctx)
		ssh_gssapi_delete_ctx(ctx);
	ssh_gssapi_build_ctx(ctx);
	ssh_gssapi_set_oid(*ctx, oid);
	return (ssh_gssapi_acquire_cred(*ctx));
}

 
void
ssh_gssapi_supported_oids(gss_OID_set *oidset)
{
	int i = 0;
	OM_uint32 min_status;
	int present;
	gss_OID_set supported;

	gss_create_empty_oid_set(&min_status, oidset);
	gss_indicate_mechs(&min_status, &supported);

	while (supported_mechs[i]->name != NULL) {
		if (GSS_ERROR(gss_test_oid_set_member(&min_status,
		    &supported_mechs[i]->oid, supported, &present)))
			present = 0;
		if (present)
			gss_add_oid_set_member(&min_status,
			    &supported_mechs[i]->oid, oidset);
		i++;
	}

	gss_release_oid_set(&min_status, &supported);
}


 
 
OM_uint32
ssh_gssapi_accept_ctx(Gssctxt *ctx, gss_buffer_desc *recv_tok,
    gss_buffer_desc *send_tok, OM_uint32 *flags)
{
	OM_uint32 status;
	gss_OID mech;

	ctx->major = gss_accept_sec_context(&ctx->minor,
	    &ctx->context, ctx->creds, recv_tok,
	    GSS_C_NO_CHANNEL_BINDINGS, &ctx->client, &mech,
	    send_tok, flags, NULL, &ctx->client_creds);

	if (GSS_ERROR(ctx->major))
		ssh_gssapi_error(ctx);

	if (ctx->client_creds)
		debug("Received some client credentials");
	else
		debug("Got no client credentials");

	status = ctx->major;

	 

	if (((flags == NULL) || ((*flags & GSS_C_MUTUAL_FLAG) &&
	    (*flags & GSS_C_INTEG_FLAG))) && (ctx->major == GSS_S_COMPLETE)) {
		if (ssh_gssapi_getclient(ctx, &gssapi_client))
			fatal("Couldn't convert client name");
	}

	return (status);
}

 
static OM_uint32
ssh_gssapi_parse_ename(Gssctxt *ctx, gss_buffer_t ename, gss_buffer_t name)
{
	u_char *tok;
	OM_uint32 offset;
	OM_uint32 oidl;

	tok = ename->value;

	 

	if (ename->length < 6 || memcmp(tok, "\x04\x01", 2) != 0)
		return GSS_S_FAILURE;

	 

	oidl = get_u16(tok+2);  
	oidl = oidl-2;  

	 
	if (tok[4] != 0x06 || tok[5] != oidl ||
	    ename->length < oidl+6 ||
	    !ssh_gssapi_check_oid(ctx, tok+6, oidl))
		return GSS_S_FAILURE;

	offset = oidl+6;

	if (ename->length < offset+4)
		return GSS_S_FAILURE;

	name->length = get_u32(tok+offset);
	offset += 4;

	if (UINT_MAX - offset < name->length)
		return GSS_S_FAILURE;
	if (ename->length < offset+name->length)
		return GSS_S_FAILURE;

	name->value = xmalloc(name->length+1);
	memcpy(name->value, tok+offset, name->length);
	((char *)name->value)[name->length] = 0;

	return GSS_S_COMPLETE;
}

 

 
OM_uint32
ssh_gssapi_getclient(Gssctxt *ctx, ssh_gssapi_client *client)
{
	int i = 0;

	gss_buffer_desc ename;

	client->mech = NULL;

	while (supported_mechs[i]->name != NULL) {
		if (supported_mechs[i]->oid.length == ctx->oid->length &&
		    (memcmp(supported_mechs[i]->oid.elements,
		    ctx->oid->elements, ctx->oid->length) == 0))
			client->mech = supported_mechs[i];
		i++;
	}

	if (client->mech == NULL)
		return GSS_S_FAILURE;

	if ((ctx->major = gss_display_name(&ctx->minor, ctx->client,
	    &client->displayname, NULL))) {
		ssh_gssapi_error(ctx);
		return (ctx->major);
	}

	if ((ctx->major = gss_export_name(&ctx->minor, ctx->client,
	    &ename))) {
		ssh_gssapi_error(ctx);
		return (ctx->major);
	}

	if ((ctx->major = ssh_gssapi_parse_ename(ctx,&ename,
	    &client->exportedname))) {
		return (ctx->major);
	}

	 
	client->creds = ctx->client_creds;
	ctx->client_creds = GSS_C_NO_CREDENTIAL;
	return (ctx->major);
}

 
void
ssh_gssapi_cleanup_creds(void)
{
	if (gssapi_client.store.filename != NULL) {
		 
		debug("removing gssapi cred file\"%s\"",
		    gssapi_client.store.filename);
		unlink(gssapi_client.store.filename);
	}
}

 
void
ssh_gssapi_storecreds(void)
{
	if (gssapi_client.mech && gssapi_client.mech->storecreds) {
		(*gssapi_client.mech->storecreds)(&gssapi_client);
	} else
		debug("ssh_gssapi_storecreds: Not a GSSAPI mechanism");
}

 
 
void
ssh_gssapi_do_child(char ***envp, u_int *envsizep)
{

	if (gssapi_client.store.envvar != NULL &&
	    gssapi_client.store.envval != NULL) {
		debug("Setting %s to %s", gssapi_client.store.envvar,
		    gssapi_client.store.envval);
		child_set_env(envp, envsizep, gssapi_client.store.envvar,
		    gssapi_client.store.envval);
	}
}

 
int
ssh_gssapi_userok(char *user)
{
	OM_uint32 lmin;

	if (gssapi_client.exportedname.length == 0 ||
	    gssapi_client.exportedname.value == NULL) {
		debug("No suitable client data");
		return 0;
	}
	if (gssapi_client.mech && gssapi_client.mech->userok)
		if ((*gssapi_client.mech->userok)(&gssapi_client, user))
			return 1;
		else {
			 
			gss_release_buffer(&lmin, &gssapi_client.displayname);
			gss_release_buffer(&lmin, &gssapi_client.exportedname);
			gss_release_cred(&lmin, &gssapi_client.creds);
			explicit_bzero(&gssapi_client,
			    sizeof(ssh_gssapi_client));
			return 0;
		}
	else
		debug("ssh_gssapi_userok: Unknown GSSAPI mechanism");
	return (0);
}

 
OM_uint32
ssh_gssapi_checkmic(Gssctxt *ctx, gss_buffer_t gssbuf, gss_buffer_t gssmic)
{
	ctx->major = gss_verify_mic(&ctx->minor, ctx->context,
	    gssbuf, gssmic, NULL);

	return (ctx->major);
}

 
const char *ssh_gssapi_displayname(void)
{
	if (gssapi_client.displayname.length == 0 ||
	    gssapi_client.displayname.value == NULL)
		return NULL;
	return (char *)gssapi_client.displayname.value;
}

#endif
