 
 

#ifndef _LIFEBOOK_H
#define _LIFEBOOK_H

int lifebook_detect(struct psmouse *psmouse, bool set_properties);
int lifebook_init(struct psmouse *psmouse);

#ifdef CONFIG_MOUSE_PS2_LIFEBOOK
void lifebook_module_init(void);
#else
static inline void lifebook_module_init(void)
{
}
#endif

#endif
