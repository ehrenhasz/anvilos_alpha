 

 
#ifndef _NET_DSA_BRCM_H
#define _NET_DSA_BRCM_H

 
#define BRCM_TAG_SET_PORT_QUEUE(p, q)	((p) << 8 | q)
#define BRCM_TAG_GET_PORT(v)		((v) >> 8)
#define BRCM_TAG_GET_QUEUE(v)		((v) & 0xff)

#endif
