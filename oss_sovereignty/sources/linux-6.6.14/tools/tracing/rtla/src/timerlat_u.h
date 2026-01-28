


struct timerlat_u_params {
	
	int should_run;
	
	int stopped_running;

	
	cpu_set_t *set;
	char *cgroup_name;
	struct sched_attr *sched_param;
};

void *timerlat_u_dispatcher(void *data);
