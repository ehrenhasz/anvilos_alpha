#ifndef _VMMOUSE_H
#define _VMMOUSE_H
#define VMMOUSE_PSNAME  "VirtualPS/2"
int vmmouse_detect(struct psmouse *psmouse, bool set_properties);
int vmmouse_init(struct psmouse *psmouse);
#endif
