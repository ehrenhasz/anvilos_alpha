 
 

#ifndef _LINEDISP_H
#define _LINEDISP_H

 
struct linedisp {
	struct device dev;
	struct timer_list timer;
	void (*update)(struct linedisp *linedisp);
	char *buf;
	char *message;
	unsigned int num_chars;
	unsigned int message_len;
	unsigned int scroll_pos;
	unsigned int scroll_rate;
};

int linedisp_register(struct linedisp *linedisp, struct device *parent,
		      unsigned int num_chars, char *buf,
		      void (*update)(struct linedisp *linedisp));
void linedisp_unregister(struct linedisp *linedisp);

#endif  
