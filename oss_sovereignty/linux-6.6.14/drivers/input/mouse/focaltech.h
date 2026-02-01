 
 

#ifndef _FOCALTECH_H
#define _FOCALTECH_H

int focaltech_detect(struct psmouse *psmouse, bool set_properties);

#ifdef CONFIG_MOUSE_PS2_FOCALTECH
int focaltech_init(struct psmouse *psmouse);
#else
static inline int focaltech_init(struct psmouse *psmouse)
{
	return -ENOSYS;
}
#endif

#endif
