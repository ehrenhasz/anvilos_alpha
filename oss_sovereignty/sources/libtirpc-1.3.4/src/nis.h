

#ifndef _INTERNAL_NIS_H
#define _INTERNAL_NIS_H 1



#define NIS_PK_NONE 0

struct nis_attr {
	char *zattr_ndx;
	struct {
		u_int zattr_val_len;
		char *zattr_val_val;
	} zattr_val;
};
typedef struct nis_attr nis_attr;

typedef char *nis_name;

struct endpoint {
	char *uaddr;
	char *family;
	char *proto;
};
typedef struct endpoint endpoint;

struct nis_server {
	nis_name name;
	struct {
		u_int ep_len;
		endpoint *ep_val;
	} ep;
	uint32_t key_type;
	netobj pkey;
};
typedef struct nis_server nis_server;

#endif 
