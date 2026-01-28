
#ifndef MICROPY_INCLUDED_CC3200_SERVERSTASK_H
#define MICROPY_INCLUDED_CC3200_SERVERSTASK_H


#define SERVERS_PRIORITY                        2
#define SERVERS_STACK_SIZE                      1024 
#define SERVERS_STACK_LEN                       (SERVERS_STACK_SIZE / sizeof(StackType_t))

#define SERVERS_SSID_LEN_MAX                    16
#define SERVERS_KEY_LEN_MAX                     16

#define SERVERS_USER_PASS_LEN_MAX               32

#define SERVERS_CYCLE_TIME_MS                   2

#define SERVERS_DEF_USER                        "micro"
#define SERVERS_DEF_PASS                        "python"
#define SERVERS_DEF_TIMEOUT_MS                  300000        
#define SERVERS_MIN_TIMEOUT_MS                  5000          




extern StaticTask_t svTaskTCB;
extern StackType_t svTaskStack[];
extern char servers_user[];
extern char servers_pass[];


extern void TASK_Servers(void *pvParameters);
extern void servers_start(void);
extern void servers_stop(void);
extern void servers_reset(void);
extern void servers_wlan_cycle_power(void);
extern bool servers_are_enabled(void);
extern void servers_close_socket(int16_t *sd);
extern void servers_set_login(char *user, char *pass);
extern void server_sleep_sockets(void);
extern void servers_set_timeout(uint32_t timeout);
extern uint32_t servers_get_timeout(void);

#endif 
