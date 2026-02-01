 

 

#include <stdio.h>
#include <libshare.h>
#include "libshare_impl.h"

 
static int
smb_enable_share(sa_share_impl_t impl_share)
{
	(void) impl_share;
	fputs("No SMB support in FreeBSD yet.\n", stderr);
	return (SA_NOT_SUPPORTED);
}
 
static int
smb_disable_share(sa_share_impl_t impl_share)
{
	(void) impl_share;
	fputs("No SMB support in FreeBSD yet.\n", stderr);
	return (SA_NOT_SUPPORTED);
}

 
static int
smb_validate_shareopts(const char *shareopts)
{
	(void) shareopts;
	fputs("No SMB support in FreeBSD yet.\n", stderr);
	return (SA_NOT_SUPPORTED);
}

 
static boolean_t
smb_is_share_active(sa_share_impl_t impl_share)
{
	(void) impl_share;
	return (B_FALSE);
}

static int
smb_update_shares(void)
{
	 
	return (0);
}

const sa_fstype_t libshare_smb_type = {
	.enable_share = smb_enable_share,
	.disable_share = smb_disable_share,
	.is_shared = smb_is_share_active,

	.validate_shareopts = smb_validate_shareopts,
	.commit_shares = smb_update_shares,
};
