 
#ifndef _WINDFARM_PID_H
#define _WINDFARM_PID_H

 

 

#define WF_PID_MAX_HISTORY	32

 
struct wf_pid_param {
	int	interval;	 
	int	history_len;	 
	int	additive;	 
	s32	gd, gp, gr;	 
	s32	itarget;	 
	s32	min,max;	 
};

struct wf_pid_state {
	int	first;				 
	int	index; 				 
	s32	target;				 
	s32	samples[WF_PID_MAX_HISTORY];	 
	s32	errors[WF_PID_MAX_HISTORY];	 

	struct wf_pid_param param;
};

extern void wf_pid_init(struct wf_pid_state *st, struct wf_pid_param *param);
extern s32 wf_pid_run(struct wf_pid_state *st, s32 sample);


 

#define WF_CPU_PID_MAX_HISTORY	32

 
struct wf_cpu_pid_param {
	int	interval;	 
	int	history_len;	 
	s32	gd, gp, gr;	 
	s32	pmaxadj;	 
	s32	ttarget;	 
	s32	tmax;		 
	s32	min,max;	 
};

struct wf_cpu_pid_state {
	int	first;				 
	int	index; 				 
	int	tindex; 			 
	s32	target;				 
	s32	last_delta;			 
	s32	powers[WF_PID_MAX_HISTORY];	 
	s32	errors[WF_PID_MAX_HISTORY];	 
	s32	temps[2];			 

	struct wf_cpu_pid_param param;
};

extern void wf_cpu_pid_init(struct wf_cpu_pid_state *st,
			    struct wf_cpu_pid_param *param);
extern s32 wf_cpu_pid_run(struct wf_cpu_pid_state *st, s32 power, s32 temp);

#endif  
