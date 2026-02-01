
 

#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include <trace.h>
#include <utils.h>
#include <in_kernel.h>

static int stop_session;

 
static void stop_rv(int sig)
{
	stop_session = 1;
}

 
int should_stop(void)
{
	return stop_session;
}

 
static void rv_list(int argc, char **argv)
{
	static const char *const usage[] = {
		"",
		"  usage: rv list [-h]",
		"",
		"	list all available monitors",
		"",
		"	-h/--help: print this menu",
		NULL,
	};
	int i;

	if (argc > 1) {
		fprintf(stderr, "rv version %s\n", VERSION);

		 
		for (i = 0; usage[i]; i++)
			fprintf(stderr, "%s\n", usage[i]);

		 
		if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))
			exit(0);
		else
			exit(1);
	}

	ikm_list_monitors();
	exit(0);
}

 
static void rv_mon(int argc, char **argv)
{
	char *monitor_name;
	int i, run = 0;

	static const char *const usage[] = {
		"",
		"  usage: rv mon [-h] monitor [monitor options]",
		"",
		"	run a monitor",
		"",
		"	-h/--help: print this menu",
		"",
		"	monitor [monitor options]: the monitor, passing",
		"	the arguments to the [monitor options]",
		NULL,
	};

	 
	if (argc == 1) {

		fprintf(stderr, "rv version %s\n", VERSION);

		for (i = 0; usage[i]; i++)
			fprintf(stderr, "%s\n", usage[i]);
		exit(1);
	} else if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {

		fprintf(stderr, "rv version %s\n", VERSION);

		for (i = 0; usage[i]; i++)
			fprintf(stderr, "%s\n", usage[i]);
		exit(0);
	}

	monitor_name = argv[1];
	 
	run += ikm_run_monitor(monitor_name, argc-1, &argv[1]);

	if (!run)
		err_msg("rv: monitor %s does not exist\n", monitor_name);
	exit(!run);
}

static void usage(int exit_val, const char *fmt, ...)
{
	char message[1024];
	va_list ap;
	int i;

	static const char *const usage[] = {
		"",
		"  usage: rv command [-h] [command_options]",
		"",
		"	-h/--help: print this menu",
		"",
		"	command: run one of the following command:",
		"	  list: list all available monitors",
		"	  mon:  run a monitor",
		"",
		"	[command options]: each command has its own set of options",
		"		           run rv command -h for further information",
		NULL,
	};

	va_start(ap, fmt);
	vsnprintf(message, sizeof(message), fmt, ap);
	va_end(ap);

	fprintf(stderr, "rv version %s: %s\n", VERSION, message);

	for (i = 0; usage[i]; i++)
		fprintf(stderr, "%s\n", usage[i]);

	exit(exit_val);
}

 
int main(int argc, char **argv)
{
	if (geteuid())
		usage(1, "%s needs root permission", argv[0]);

	if (argc <= 1)
		usage(1, "%s requires a command", argv[0]);

	if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))
		usage(0, "help");

	if (!strcmp(argv[1], "list"))
		rv_list(--argc, &argv[1]);

	if (!strcmp(argv[1], "mon")) {
		 
		signal(SIGINT, stop_rv);

		rv_mon(argc - 1, &argv[1]);
	}

	 
	usage(1, "%s does not know the %s command, old version?", argv[0], argv[1]);
}
