
#ifndef _T3CDEV_H_
#define _T3CDEV_H_

#include <linux/list.h>
#include <linux/atomic.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <linux/skbuff.h>
#include <net/neighbour.h>

#define T3CNAMSIZ 16

struct cxgb3_client;

enum t3ctype {
	T3A = 0,
	T3B,
	T3C,
};

struct t3cdev {
	char name[T3CNAMSIZ];	
	enum t3ctype type;
	struct list_head ofld_dev_list;	
	struct net_device *lldev;	
	struct proc_dir_entry *proc_dir;	
	int (*send)(struct t3cdev *dev, struct sk_buff *skb);
	int (*recv)(struct t3cdev *dev, struct sk_buff **skb, int n);
	int (*ctl)(struct t3cdev *dev, unsigned int req, void *data);
	void (*neigh_update)(struct t3cdev *dev, struct neighbour *neigh);
	void *priv;		
	void __rcu *l2opt;	
	void *l3opt;		
	void *l4opt;		
	void *ulp;		
	void *ulp_iscsi;	
};

#endif				
