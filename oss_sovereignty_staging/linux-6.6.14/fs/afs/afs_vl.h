 
 

#ifndef AFS_VL_H
#define AFS_VL_H

#include "afs.h"

#define AFS_VL_PORT		7003	 
#define VL_SERVICE		52	 
#define YFS_VL_SERVICE		2503	 

enum AFSVL_Operations {
	VLGETENTRYBYID		= 503,	 
	VLGETENTRYBYNAME	= 504,	 
	VLPROBE			= 514,	 
	VLGETENTRYBYIDU		= 526,	 
	VLGETENTRYBYNAMEU	= 527,	 
	VLGETADDRSU		= 533,	 
	YVLGETENDPOINTS		= 64002,  
	YVLGETCELLNAME		= 64014,  
	VLGETCAPABILITIES	= 65537,  
};

enum AFSVL_Errors {
	AFSVL_IDEXIST 		= 363520,	 
	AFSVL_IO 		= 363521,	 
	AFSVL_NAMEEXIST 	= 363522,	 
	AFSVL_CREATEFAIL 	= 363523,	 
	AFSVL_NOENT 		= 363524,	 
	AFSVL_EMPTY 		= 363525,	 
	AFSVL_ENTDELETED 	= 363526,	 
	AFSVL_BADNAME 		= 363527,	 
	AFSVL_BADINDEX 		= 363528,	 
	AFSVL_BADVOLTYPE 	= 363529,	 
	AFSVL_BADSERVER 	= 363530,	 
	AFSVL_BADPARTITION 	= 363531,	 
	AFSVL_REPSFULL 		= 363532,	 
	AFSVL_NOREPSERVER 	= 363533,	 
	AFSVL_DUPREPSERVER 	= 363534,	 
	AFSVL_RWNOTFOUND 	= 363535,	 
	AFSVL_BADREFCOUNT 	= 363536,	 
	AFSVL_SIZEEXCEEDED 	= 363537,	 
	AFSVL_BADENTRY 		= 363538,	 
	AFSVL_BADVOLIDBUMP 	= 363539,	 
	AFSVL_IDALREADYHASHED 	= 363540,	 
	AFSVL_ENTRYLOCKED 	= 363541,	 
	AFSVL_BADVOLOPER 	= 363542,	 
	AFSVL_BADRELLOCKTYPE 	= 363543,	 
	AFSVL_RERELEASE 	= 363544,	 
	AFSVL_BADSERVERFLAG 	= 363545,	 
	AFSVL_PERM 		= 363546,	 
	AFSVL_NOMEM 		= 363547,	 
};

enum {
	YFS_SERVER_INDEX	= 0,
	YFS_SERVER_UUID		= 1,
	YFS_SERVER_ENDPOINT	= 2,
};

enum {
	YFS_ENDPOINT_IPV4	= 0,
	YFS_ENDPOINT_IPV6	= 1,
};

#define YFS_MAXENDPOINTS	16

 
struct afs_vldbentry {
	char		name[65];		 
	afs_voltype_t	type;			 
	unsigned	num_servers;		 
	unsigned	clone_id;		 

	unsigned	flags;
#define AFS_VLF_RWEXISTS	0x1000		 
#define AFS_VLF_ROEXISTS	0x2000		 
#define AFS_VLF_BACKEXISTS	0x4000		 

	afs_volid_t	volume_ids[3];		 

	struct {
		struct in_addr	addr;		 
		unsigned	partition;	 
		unsigned	flags;		 
#define AFS_VLSF_NEWREPSITE	0x0001	 
#define AFS_VLSF_ROVOL		0x0002	 
#define AFS_VLSF_RWVOL		0x0004	 
#define AFS_VLSF_BACKVOL	0x0008	 
#define AFS_VLSF_UUID		0x0010	 
#define AFS_VLSF_DONTUSE	0x0020	 
	} servers[8];
};

#define AFS_VLDB_MAXNAMELEN 65


struct afs_ListAddrByAttributes__xdr {
	__be32			Mask;
#define AFS_VLADDR_IPADDR	0x1	 
#define AFS_VLADDR_INDEX	0x2	 
#define AFS_VLADDR_UUID		0x4	 
	__be32			ipaddr;
	__be32			index;
	__be32			spare;
	struct afs_uuid__xdr	uuid;
};

struct afs_uvldbentry__xdr {
	__be32			name[AFS_VLDB_MAXNAMELEN];
	__be32			nServers;
	struct afs_uuid__xdr	serverNumber[AFS_NMAXNSERVERS];
	__be32			serverUnique[AFS_NMAXNSERVERS];
	__be32			serverPartition[AFS_NMAXNSERVERS];
	__be32			serverFlags[AFS_NMAXNSERVERS];
	__be32			volumeId[AFS_MAXTYPES];
	__be32			cloneId;
	__be32			flags;
	__be32			spares1;
	__be32			spares2;
	__be32			spares3;
	__be32			spares4;
	__be32			spares5;
	__be32			spares6;
	__be32			spares7;
	__be32			spares8;
	__be32			spares9;
};

struct afs_address_list {
	refcount_t		usage;
	unsigned int		version;
	unsigned int		nr_addrs;
	struct sockaddr_rxrpc	addrs[];
};

extern void afs_put_address_list(struct afs_address_list *alist);

#endif  
