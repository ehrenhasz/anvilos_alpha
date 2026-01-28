struct config
{
	long sleep;		 
	long load;		 
	long sleep_step;	 
	long load_step;		 
	unsigned int cycles;	 
	unsigned int rounds;	 
	unsigned int cpu;	 
	char governor[15];	 
	enum sched_prio		 
	{
		SCHED_ERR = -1,
		SCHED_HIGH,
		SCHED_DEFAULT,
		SCHED_LOW
	} prio;
	unsigned int verbose;	 
	FILE *output;		 
	char *output_filename;	 
};
enum sched_prio string_to_prio(const char *str);
FILE *prepare_output(const char *dir);
int prepare_config(const char *path, struct config *config);
struct config *prepare_default_config();
