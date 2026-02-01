
 

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fips.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/random.h>
#include "cifs_fs_sb.h"
#include "cifs_unicode.h"
#include "cifspdu.h"
#include "cifsglob.h"
#include "cifs_debug.h"
#include "cifsproto.h"
#include "../common/md4.h"

#ifndef false
#define false 0
#endif
#ifndef true
#define true 1
#endif

 
#define CVAL(buf,pos) (((unsigned char *)(buf))[pos])
#define SSVALX(buf,pos,val) (CVAL(buf,pos)=(val)&0xFF,CVAL(buf,pos+1)=(val)>>8)
#define SSVAL(buf,pos,val) SSVALX((buf),(pos),((__u16)(val)))

 
static int
mdfour(unsigned char *md4_hash, unsigned char *link_str, int link_len)
{
	int rc;
	struct md4_ctx mctx;

	rc = cifs_md4_init(&mctx);
	if (rc) {
		cifs_dbg(VFS, "%s: Could not init MD4\n", __func__);
		goto mdfour_err;
	}
	rc = cifs_md4_update(&mctx, link_str, link_len);
	if (rc) {
		cifs_dbg(VFS, "%s: Could not update MD4\n", __func__);
		goto mdfour_err;
	}
	rc = cifs_md4_final(&mctx, md4_hash);
	if (rc)
		cifs_dbg(VFS, "%s: Could not finalize MD4\n", __func__);


mdfour_err:
	return rc;
}

 

int
E_md4hash(const unsigned char *passwd, unsigned char *p16,
	const struct nls_table *codepage)
{
	int rc;
	int len;
	__le16 wpwd[129];

	 
	if (passwd)  
		len = cifs_strtoUTF16(wpwd, passwd, 128, codepage);
	else {
		len = 0;
		*wpwd = 0;  
	}

	rc = mdfour(p16, (unsigned char *) wpwd, len * sizeof(__le16));
	memzero_explicit(wpwd, sizeof(wpwd));

	return rc;
}
