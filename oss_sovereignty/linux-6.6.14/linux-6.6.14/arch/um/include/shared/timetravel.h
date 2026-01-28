#ifndef _UM_TIME_TRAVEL_H_
#define _UM_TIME_TRAVEL_H_
enum time_travel_mode {
	TT_MODE_OFF,
	TT_MODE_BASIC,
	TT_MODE_INFCPU,
	TT_MODE_EXTERNAL,
};
#if defined(UML_CONFIG_UML_TIME_TRAVEL_SUPPORT) || \
    defined(CONFIG_UML_TIME_TRAVEL_SUPPORT)
extern enum time_travel_mode time_travel_mode;
#else
#define time_travel_mode TT_MODE_OFF
#endif  
#endif  
