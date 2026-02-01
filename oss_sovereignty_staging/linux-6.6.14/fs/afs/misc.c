
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include "internal.h"
#include "afs_fs.h"
#include "protocol_uae.h"

 
int afs_abort_to_error(u32 abort_code)
{
	switch (abort_code) {
		 
	case 13:		return -EACCES;
	case 27:		return -EFBIG;
	case 30:		return -EROFS;

		 
	case VSALVAGE:		return -EIO;
	case VNOVNODE:		return -ENOENT;
	case VNOVOL:		return -ENOMEDIUM;
	case VVOLEXISTS:	return -EEXIST;
	case VNOSERVICE:	return -EIO;
	case VOFFLINE:		return -ENOENT;
	case VONLINE:		return -EEXIST;
	case VDISKFULL:		return -ENOSPC;
	case VOVERQUOTA:	return -EDQUOT;
	case VBUSY:		return -EBUSY;
	case VMOVED:		return -ENXIO;

		 
	case AFSVL_IDEXIST:		return -EEXIST;
	case AFSVL_IO:			return -EREMOTEIO;
	case AFSVL_NAMEEXIST:		return -EEXIST;
	case AFSVL_CREATEFAIL:		return -EREMOTEIO;
	case AFSVL_NOENT:		return -ENOMEDIUM;
	case AFSVL_EMPTY:		return -ENOMEDIUM;
	case AFSVL_ENTDELETED:		return -ENOMEDIUM;
	case AFSVL_BADNAME:		return -EINVAL;
	case AFSVL_BADINDEX:		return -EINVAL;
	case AFSVL_BADVOLTYPE:		return -EINVAL;
	case AFSVL_BADSERVER:		return -EINVAL;
	case AFSVL_BADPARTITION:	return -EINVAL;
	case AFSVL_REPSFULL:		return -EFBIG;
	case AFSVL_NOREPSERVER:		return -ENOENT;
	case AFSVL_DUPREPSERVER:	return -EEXIST;
	case AFSVL_RWNOTFOUND:		return -ENOENT;
	case AFSVL_BADREFCOUNT:		return -EINVAL;
	case AFSVL_SIZEEXCEEDED:	return -EINVAL;
	case AFSVL_BADENTRY:		return -EINVAL;
	case AFSVL_BADVOLIDBUMP:	return -EINVAL;
	case AFSVL_IDALREADYHASHED:	return -EINVAL;
	case AFSVL_ENTRYLOCKED:		return -EBUSY;
	case AFSVL_BADVOLOPER:		return -EBADRQC;
	case AFSVL_BADRELLOCKTYPE:	return -EINVAL;
	case AFSVL_RERELEASE:		return -EREMOTEIO;
	case AFSVL_BADSERVERFLAG:	return -EINVAL;
	case AFSVL_PERM:		return -EACCES;
	case AFSVL_NOMEM:		return -EREMOTEIO;

		 
	case UAEPERM:			return -EPERM;
	case UAENOENT:			return -ENOENT;
	case UAEAGAIN:			return -EAGAIN;
	case UAEACCES:			return -EACCES;
	case UAEBUSY:			return -EBUSY;
	case UAEEXIST:			return -EEXIST;
	case UAENOTDIR:			return -ENOTDIR;
	case UAEISDIR:			return -EISDIR;
	case UAEFBIG:			return -EFBIG;
	case UAENOSPC:			return -ENOSPC;
	case UAEROFS:			return -EROFS;
	case UAEMLINK:			return -EMLINK;
	case UAEDEADLK:			return -EDEADLK;
	case UAENAMETOOLONG:		return -ENAMETOOLONG;
	case UAENOLCK:			return -ENOLCK;
	case UAENOTEMPTY:		return -ENOTEMPTY;
	case UAELOOP:			return -ELOOP;
	case UAEOVERFLOW:		return -EOVERFLOW;
	case UAENOMEDIUM:		return -ENOMEDIUM;
	case UAEDQUOT:			return -EDQUOT;

		 
	case RXKADINCONSISTENCY: return -EPROTO;
	case RXKADPACKETSHORT:	return -EPROTO;
	case RXKADLEVELFAIL:	return -EKEYREJECTED;
	case RXKADTICKETLEN:	return -EKEYREJECTED;
	case RXKADOUTOFSEQUENCE: return -EPROTO;
	case RXKADNOAUTH:	return -EKEYREJECTED;
	case RXKADBADKEY:	return -EKEYREJECTED;
	case RXKADBADTICKET:	return -EKEYREJECTED;
	case RXKADUNKNOWNKEY:	return -EKEYREJECTED;
	case RXKADEXPIRED:	return -EKEYEXPIRED;
	case RXKADSEALEDINCON:	return -EKEYREJECTED;
	case RXKADDATALEN:	return -EKEYREJECTED;
	case RXKADILLEGALLEVEL:	return -EKEYREJECTED;

	case RXGEN_OPCODE:	return -ENOTSUPP;

	default:		return -EREMOTEIO;
	}
}

 
void afs_prioritise_error(struct afs_error *e, int error, u32 abort_code)
{
	switch (error) {
	case 0:
		return;
	default:
		if (e->error == -ETIMEDOUT ||
		    e->error == -ETIME)
			return;
		fallthrough;
	case -ETIMEDOUT:
	case -ETIME:
		if (e->error == -ENOMEM ||
		    e->error == -ENONET)
			return;
		fallthrough;
	case -ENOMEM:
	case -ENONET:
		if (e->error == -ERFKILL)
			return;
		fallthrough;
	case -ERFKILL:
		if (e->error == -EADDRNOTAVAIL)
			return;
		fallthrough;
	case -EADDRNOTAVAIL:
		if (e->error == -ENETUNREACH)
			return;
		fallthrough;
	case -ENETUNREACH:
		if (e->error == -EHOSTUNREACH)
			return;
		fallthrough;
	case -EHOSTUNREACH:
		if (e->error == -EHOSTDOWN)
			return;
		fallthrough;
	case -EHOSTDOWN:
		if (e->error == -ECONNREFUSED)
			return;
		fallthrough;
	case -ECONNREFUSED:
		if (e->error == -ECONNRESET)
			return;
		fallthrough;
	case -ECONNRESET:  
		if (e->responded)
			return;
		e->error = error;
		return;

	case -ECONNABORTED:
		error = afs_abort_to_error(abort_code);
		fallthrough;
	case -ENETRESET:  
		e->responded = true;
		e->error = error;
		return;
	}
}
