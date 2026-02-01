
 

#ifndef _vendor_h_
#define _vendor_h_

#define BROADCOM_OUI	0x001018

enum brcmf_vndr_cmds {
	BRCMF_VNDR_CMDS_UNSPEC,
	BRCMF_VNDR_CMDS_DCMD,
	BRCMF_VNDR_CMDS_LAST
};

 
enum brcmf_nlattrs {
	BRCMF_NLATTR_UNSPEC,

	BRCMF_NLATTR_LEN,
	BRCMF_NLATTR_DATA,

	__BRCMF_NLATTR_AFTER_LAST,
	BRCMF_NLATTR_MAX = __BRCMF_NLATTR_AFTER_LAST - 1
};

 
struct brcmf_vndr_dcmd_hdr {
	uint cmd;
	int len;
	uint offset;
	uint set;
	uint magic;
};

extern const struct wiphy_vendor_command brcmf_vendor_cmds[];

#endif  
