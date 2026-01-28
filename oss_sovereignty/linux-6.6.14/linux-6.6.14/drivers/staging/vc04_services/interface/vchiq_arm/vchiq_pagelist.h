#ifndef VCHIQ_PAGELIST_H
#define VCHIQ_PAGELIST_H
#define PAGELIST_WRITE 0
#define PAGELIST_READ 1
#define PAGELIST_READ_WITH_FRAGMENTS 2
struct pagelist {
	u32 length;
	u16 type;
	u16 offset;
	u32 addrs[1];	 
};
#endif  
