 
 

#ifndef _IA_CSS_CIRCBUF_COMM_H
#define _IA_CSS_CIRCBUF_COMM_H

#include <type_support.h>   

#define IA_CSS_CIRCBUF_PADDING 1  

 
 
typedef struct ia_css_circbuf_desc_s ia_css_circbuf_desc_t;
struct ia_css_circbuf_desc_s {
	u8 size;	 
	u8 step;    
	u8 start;	 
	u8 end;	 
};

#define SIZE_OF_IA_CSS_CIRCBUF_DESC_S_STRUCT				\
	(4 * sizeof(uint8_t))

 
typedef struct ia_css_circbuf_elem_s ia_css_circbuf_elem_t;
struct ia_css_circbuf_elem_s {
	u32 val;	 
};

#define SIZE_OF_IA_CSS_CIRCBUF_ELEM_S_STRUCT				\
	(sizeof(uint32_t))

#endif  
