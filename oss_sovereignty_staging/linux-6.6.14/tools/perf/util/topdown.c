
#include "topdown.h"
#include <linux/kernel.h>

__weak bool arch_topdown_sample_read(struct evsel *leader __maybe_unused)
{
	return false;
}
