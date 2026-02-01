
 

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <dirent.h>
#include <libintl.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>
#include <limits.h>
#include <math.h>
#include <sys/stat.h>
#include <syslog.h>

#include "tmon.h"

 
struct pid_params p_param;
 
static double xk_1, xk_2;  

 
int init_thermal_controller(void)
{

	 
	p_param.ts = ticktime;
	 
	p_param.kp = .36;
	p_param.ki = 5.0;
	p_param.kd = 0.19;

	p_param.t_target = target_temp_user;

	return 0;
}

void controller_reset(void)
{
	 
	syslog(LOG_DEBUG, "TC inactive, relax p-state\n");
	p_param.y_k = 0.0;
	xk_1 = 0.0;
	xk_2 = 0.0;
	set_ctrl_state(0);
}

 
#define GUARD_BAND (2)
void controller_handler(const double xk, double *yk)
{
	double ek;
	double p_term, i_term, d_term;

	ek = p_param.t_target - xk;  
	if (ek >= 3.0) {
		syslog(LOG_DEBUG, "PID: %3.1f Below set point %3.1f, stop\n",
			xk, p_param.t_target);
		controller_reset();
		*yk = 0.0;
		return;
	}
	 
	p_term = -p_param.kp * (xk - xk_1);
	i_term = p_param.kp * p_param.ki * p_param.ts * ek;
	d_term = -p_param.kp * p_param.kd * (xk - 2 * xk_1 + xk_2) / p_param.ts;
	 
	*yk += p_term + i_term + d_term;
	 
	xk_1 = xk;
	xk_2 = xk_1;

	 
	if (*yk < -LIMIT_HIGH)
		*yk = -LIMIT_HIGH;
	else if (*yk > -LIMIT_LOW)
		*yk = -LIMIT_LOW;

	p_param.y_k = *yk;

	set_ctrl_state(lround(fabs(p_param.y_k)));

}
