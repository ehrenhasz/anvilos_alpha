




ccflags-y += -I$(src)			

obj-$(CONFIG_NFSD)	+= nfsd.o


nfsd-y			+= trace.o

nfsd-y 			+= nfssvc.o nfsctl.o nfsfh.o vfs.o \
			   export.o auth.o lockd.o nfscache.o \
			   stats.o filecache.o nfs3proc.o nfs3xdr.o
nfsd-$(CONFIG_NFSD_V2) += nfsproc.o nfsxdr.o
nfsd-$(CONFIG_NFSD_V2_ACL) += nfs2acl.o
nfsd-$(CONFIG_NFSD_V3_ACL) += nfs3acl.o
nfsd-$(CONFIG_NFSD_V4)	+= nfs4proc.o nfs4xdr.o nfs4state.o nfs4idmap.o \
			   nfs4acl.o nfs4callback.o nfs4recover.o
nfsd-$(CONFIG_NFSD_PNFS) += nfs4layouts.o
nfsd-$(CONFIG_NFSD_BLOCKLAYOUT) += blocklayout.o blocklayoutxdr.o
nfsd-$(CONFIG_NFSD_SCSILAYOUT) += blocklayout.o blocklayoutxdr.o
nfsd-$(CONFIG_NFSD_FLEXFILELAYOUT) += flexfilelayout.o flexfilelayoutxdr.o
