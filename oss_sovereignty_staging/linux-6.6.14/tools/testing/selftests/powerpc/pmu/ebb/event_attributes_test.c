
 

#include <stdio.h>
#include <stdlib.h>

#include "ebb.h"


 
int event_attributes(void)
{
	struct event event, leader;

	SKIP_IF(!ebb_is_supported());

	event_init(&event, 0x1001e);
	event_leader_ebb_init(&event);
	 
	FAIL_IF(event_open(&event));
	event_close(&event);


	event_init(&event, 0x001e);  
	event_leader_ebb_init(&event);
	 
	FAIL_IF(event_open(&event) == 0);


	event_init(&event, 0x2001e);
	event_leader_ebb_init(&event);
	event.attr.exclusive = 0;
	 
	FAIL_IF(event_open(&event) == 0);


	event_init(&event, 0x3001e);
	event_leader_ebb_init(&event);
	event.attr.freq = 1;
	 
	FAIL_IF(event_open(&event) == 0);


	event_init(&event, 0x4001e);
	event_leader_ebb_init(&event);
	event.attr.sample_period = 1;
	 
	FAIL_IF(event_open(&event) == 0);


	event_init(&event, 0x1001e);
	event_leader_ebb_init(&event);
	event.attr.enable_on_exec = 1;
	 
	FAIL_IF(event_open(&event) == 0);


	event_init(&event, 0x1001e);
	event_leader_ebb_init(&event);
	event.attr.inherit = 1;
	 
	FAIL_IF(event_open(&event) == 0);


	event_init(&leader, 0x1001e);
	event_leader_ebb_init(&leader);
	FAIL_IF(event_open(&leader));

	event_init(&event, 0x20002);
	event_ebb_init(&event);

	 
	FAIL_IF(event_open_with_group(&event, leader.fd));
	event_close(&leader);
	event_close(&event);


	event_init(&leader, 0x1001e);
	event_leader_ebb_init(&leader);
	FAIL_IF(event_open(&leader));

	event_init(&event, 0x20002);

	 
	FAIL_IF(event_open_with_group(&event, leader.fd) == 0);
	event_close(&leader);


	event_init(&leader, 0x1001e);
	event_leader_ebb_init(&leader);
	 
	leader.attr.config &= ~(1ull << 63);

	FAIL_IF(event_open(&leader));

	event_init(&event, 0x20002);
	event_ebb_init(&event);

	 
	FAIL_IF(event_open_with_group(&event, leader.fd) == 0);
	event_close(&leader);


	event_init(&leader, 0x1001e);
	event_leader_ebb_init(&leader);
	leader.attr.exclusive = 0;
	 
	FAIL_IF(event_open(&leader) == 0);


	event_init(&leader, 0x1001e);
	event_leader_ebb_init(&leader);
	leader.attr.pinned = 0;
	 
	FAIL_IF(event_open(&leader) == 0);

	event_init(&event, 0x1001e);
	event_leader_ebb_init(&event);
	 
	SKIP_IF(require_paranoia_below(1));
	FAIL_IF(event_open_with_cpu(&event, 0) == 0);

	return 0;
}

int main(void)
{
	return test_harness(event_attributes, "event_attributes");
}
