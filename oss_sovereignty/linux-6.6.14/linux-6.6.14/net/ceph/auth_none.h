#ifndef _FS_CEPH_AUTH_NONE_H
#define _FS_CEPH_AUTH_NONE_H
#include <linux/slab.h>
#include <linux/ceph/auth.h>
struct ceph_none_authorizer {
	struct ceph_authorizer base;
	char buf[128];
	int buf_len;
};
struct ceph_auth_none_info {
	bool starting;
};
int ceph_auth_none_init(struct ceph_auth_client *ac);
#endif
