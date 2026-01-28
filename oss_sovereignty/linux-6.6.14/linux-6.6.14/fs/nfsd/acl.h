#ifndef LINUX_NFS4_ACL_H
#define LINUX_NFS4_ACL_H
struct nfs4_acl;
struct svc_fh;
struct svc_rqst;
struct nfsd_attrs;
enum nfs_ftype4;
int nfs4_acl_bytes(int entries);
int nfs4_acl_get_whotype(char *, u32);
__be32 nfs4_acl_write_who(struct xdr_stream *xdr, int who);
int nfsd4_get_nfs4_acl(struct svc_rqst *rqstp, struct dentry *dentry,
		struct nfs4_acl **acl);
__be32 nfsd4_acl_to_attr(enum nfs_ftype4 type, struct nfs4_acl *acl,
			 struct nfsd_attrs *attr);
#endif  
