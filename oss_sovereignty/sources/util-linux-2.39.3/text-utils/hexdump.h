
#ifndef UTIL_LINUX_HEXDUMP_H
#define UTIL_LINUX_HEXDUMP_H

#include "c.h"
#include "list.h"

struct hexdump_clr {
	struct list_head colorlist;	
	const char *fmt;		
	off_t offt;			
	int range;			
	int val;			
	char *str;			
	int invert;			
};

struct hexdump_pr {
	struct list_head prlist;		
#define	F_ADDRESS	0x001		
#define	F_BPAD		0x002		
#define	F_C		0x004		
#define	F_CHAR		0x008		
#define	F_DBL		0x010		
#define	F_INT		0x020		
#define	F_P		0x040		
#define	F_STR		0x080		
#define	F_U		0x100		
#define	F_UINT		0x200		
#define	F_TEXT		0x400		
	unsigned int flags;		
	int bcnt;			
	char *cchar;			
	struct list_head *colorlist;	
	char *fmt;			
	char *nospace;			
};

struct hexdump_fu {
	struct list_head fulist;		
	struct list_head prlist;		
#define	F_IGNORE	0x01		
#define	F_SETREP	0x02		
	unsigned int flags;		
	int reps;			
	int bcnt;			
	char *fmt;			
};

struct hexdump_fs {			
	struct list_head fslist;		
	struct list_head fulist;		
	int bcnt;
};

struct hexdump {
  struct list_head fshead;				
  ssize_t blocksize;			
  int exitval;				
  ssize_t length;			
  off_t skip;				
};

extern struct hexdump_fu *endfu;

enum _vflag { ALL, DUP, FIRST, WAIT };	
extern enum _vflag vflag;

int block_size(struct hexdump_fs *);
void add_fmt(const char *, struct hexdump *);
void rewrite_rules(struct hexdump_fs *, struct hexdump *);
void addfile(char *, struct hexdump *);
void display(struct hexdump *);
void __attribute__((__noreturn__)) usage(void);
void conv_c(struct hexdump_pr *, u_char *);
void conv_u(struct hexdump_pr *, u_char *);
int  next(char **, struct hexdump *);
int parse_args(int, char **, struct hexdump *);

#endif 
