
#ifndef MICROPY_INCLUDED_CC3200_MODS_MODSOCKET_H
#define MICROPY_INCLUDED_CC3200_MODS_MODSOCKET_H

#include "py/stream.h"

extern const mp_obj_dict_t socket_locals_dict;
extern const mp_stream_p_t socket_stream_p;

extern void modusocket_pre_init (void);
extern void modusocket_socket_add (int16_t sd, bool user);
extern void modusocket_socket_delete (int16_t sd);
extern void modusocket_enter_sleep (void);
extern void modusocket_close_all_user_sockets (void);

#endif 
