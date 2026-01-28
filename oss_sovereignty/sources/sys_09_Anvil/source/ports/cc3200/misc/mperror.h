
#ifndef MICROPY_INCLUDED_CC3200_MISC_MPERROR_H
#define MICROPY_INCLUDED_CC3200_MISC_MPERROR_H

extern void NORETURN __fatal_error(const char *msg);

void mperror_init0 (void);
void mperror_bootloader_check_reset_cause (void);
void mperror_deinit_sfe_pin (void);
void mperror_signal_error (void);
void mperror_heartbeat_switch_off (void);
void mperror_heartbeat_signal (void);
void mperror_enable_heartbeat (bool enable);
bool mperror_is_heartbeat_enabled (void);

#endif 
