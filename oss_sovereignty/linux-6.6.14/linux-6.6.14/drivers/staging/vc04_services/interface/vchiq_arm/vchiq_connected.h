#ifndef VCHIQ_CONNECTED_H
#define VCHIQ_CONNECTED_H
void vchiq_add_connected_callback(void (*callback)(void));
void vchiq_call_connected_callbacks(void);
#endif  
