 
 

#ifndef CTIMAP_H
#define CTIMAP_H

#include <linux/list.h>

struct imapper {
	unsigned short slot;  
	unsigned short user;  
	unsigned short addr;  
	unsigned short next;  
	struct list_head	list;
};

int input_mapper_add(struct list_head *mappers, struct imapper *entry,
		     int (*map_op)(void *, struct imapper *), void *data);

int input_mapper_delete(struct list_head *mappers, struct imapper *entry,
		     int (*map_op)(void *, struct imapper *), void *data);

void free_input_mapper_list(struct list_head *mappers);

#endif  
