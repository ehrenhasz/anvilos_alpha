 
#include "curs.h"

int
curs907a_new(struct nouveau_drm *drm, int head, s32 oclass,
	     struct nv50_wndw **pwndw)
{
	return curs507a_new_(&curs507a, drm, head, oclass,
			     0x00000001 << (head * 4), pwndw);
}
