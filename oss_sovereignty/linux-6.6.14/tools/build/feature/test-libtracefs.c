
#include <tracefs/tracefs.h>

int main(void)
{
	struct tracefs_instance *inst = tracefs_instance_create("dummy");

	tracefs_instance_destroy(inst);
	return 0;
}
