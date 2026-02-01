 
 

#ifndef TMON_H
#define TMON_H

#define MAX_DISP_TEMP 125
#define MAX_CTRL_TEMP 105
#define MIN_CTRL_TEMP 40
#define MAX_NR_TZONE 16
#define MAX_NR_CDEV 32
#define MAX_NR_TRIP 16
#define MAX_NR_CDEV_TRIP 12  
#define MAX_TEMP_KC 140000
 
#define DATA_LEFT_ALIGN 10
#define NR_LINES_TZDATA 1
#define TMON_LOG_FILE "/var/tmp/tmon.log"

#include <sys/time.h>
#include <pthread.h>

extern unsigned long ticktime;
extern double time_elapsed;
extern unsigned long target_temp_user;
extern int dialogue_on;
extern char ctrl_cdev[];
extern pthread_mutex_t input_lock;
extern int tmon_exit;
extern int target_thermal_zone;
 
struct thermal_data_record {
	struct timeval tv;
	unsigned long temp[MAX_NR_TZONE];
	double pid_out_pct;
};

struct cdev_info {
	char type[64];
	int instance;
	unsigned long max_state;
	unsigned long cur_state;
	unsigned long flag;
};

enum trip_type {
	THERMAL_TRIP_CRITICAL,
	THERMAL_TRIP_HOT,
	THERMAL_TRIP_PASSIVE,
	THERMAL_TRIP_ACTIVE,
	NR_THERMAL_TRIP_TYPE,
};

struct trip_point {
	enum trip_type type;
	unsigned long temp;
	unsigned long hysteresis;
	int attribute;  
};

 
struct tz_info {
	char type[256];  
	int instance;
	int passive;  
	int nr_cdev;  
	int nr_trip_pts;
	struct trip_point tp[MAX_NR_TRIP];
	unsigned long cdev_binding;  
	 
	unsigned long trip_binding[MAX_NR_CDEV];
};

struct tmon_platform_data {
	int nr_tz_sensor;
	int nr_cooling_dev;
	 
	int max_tz_instance;
	int max_cdev_instance;
	struct tz_info *tzi;
	struct cdev_info *cdi;
};

struct control_ops {
	void (*set_ratio)(unsigned long ratio);
	unsigned long (*get_ratio)(unsigned long ratio);

};

enum cdev_types {
	CDEV_TYPE_PROC,
	CDEV_TYPE_FAN,
	CDEV_TYPE_MEM,
	CDEV_TYPE_NR,
};

 
enum tzone_types {
	TZONE_TYPE_ACPI,
	TZONE_TYPE_PCH,
	TZONE_TYPE_NR,
};

 
#define LIMIT_HIGH (95)
#define LIMIT_LOW  (2)

struct pid_params {
	double kp;   
	double ki;   
	double kd;   
	double ts;
	double k_lpf;

	double t_target;
	double y_k;
};

extern int init_thermal_controller(void);
extern void controller_handler(const double xk, double *yk);

extern struct tmon_platform_data ptdata;
extern struct pid_params p_param;

extern FILE *tmon_log;
extern int cur_thermal_record;  
extern struct thermal_data_record trec[];
extern const char *trip_type_name[];
extern unsigned long no_control;

extern void initialize_curses(void);
extern void show_controller_stats(char *line);
extern void show_title_bar(void);
extern void setup_windows(void);
extern void disable_tui(void);
extern void show_sensors_w(void);
extern void show_data_w(void);
extern void write_status_bar(int x, char *line);
extern void show_control_w();

extern void show_cooling_device(void);
extern void show_dialogue(void);
extern int update_thermal_data(void);

extern int probe_thermal_sysfs(void);
extern void free_thermal_data(void);
extern	void resize_handler(int sig);
extern void set_ctrl_state(unsigned long state);
extern void get_ctrl_state(unsigned long *state);
extern void *handle_tui_events(void *arg);
extern int sysfs_set_ulong(char *path, char *filename, unsigned long val);
extern int zone_instance_to_index(int zone_inst);
extern void close_windows(void);

#define PT_COLOR_DEFAULT    1
#define PT_COLOR_HEADER_BAR 2
#define PT_COLOR_ERROR      3
#define PT_COLOR_RED        4
#define PT_COLOR_YELLOW     5
#define PT_COLOR_GREEN      6
#define PT_COLOR_BRIGHT     7
#define PT_COLOR_BLUE	    8

 
#define TZONE_RECORD_SIZE 12
#define TZ_LEFT_ALIGN 32
#define CDEV_NAME_SIZE 20
#define CDEV_FLAG_IN_CONTROL (1 << 0)

 
#define DIAG_X 48
#define DIAG_Y 8
#define THERMAL_SYSFS "/sys/class/thermal"
#define CDEV "cooling_device"
#define TZONE "thermal_zone"
#define TDATA_LEFT 16
#endif  
