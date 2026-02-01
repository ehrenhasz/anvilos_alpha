 
 

#include "dsp_biquad.h"

struct ec_disable_detector_state {
	struct biquad2_state notch;
	int notch_level;
	int channel_level;
	int tone_present;
	int tone_cycle_duration;
	int good_cycles;
	int hit;
};


#define FALSE 0
#define TRUE (!FALSE)

static inline void
echo_can_disable_detector_init(struct ec_disable_detector_state *det)
{
	 
	 
	biquad2_init(&det->notch,
		     (int32_t)(-0.7600000 * 32768.0),
		     (int32_t)(-0.1183852 * 32768.0),
		     (int32_t)(-0.5104039 * 32768.0),
		     (int32_t)(0.1567596 * 32768.0),
		     (int32_t)(1.0000000 * 32768.0));

	det->channel_level = 0;
	det->notch_level = 0;
	det->tone_present = FALSE;
	det->tone_cycle_duration = 0;
	det->good_cycles = 0;
	det->hit = 0;
}
 

static inline int
echo_can_disable_detector_update(struct ec_disable_detector_state *det,
				 int16_t amp)
{
	int16_t notched;

	notched = biquad2(&det->notch, amp);
	 
	det->channel_level += ((abs(amp) - det->channel_level) >> 5);
	det->notch_level += ((abs(notched) - det->notch_level) >> 4);
	if (det->channel_level > 280) {
		 
		if (det->notch_level * 6 < det->channel_level) {
			 
			if (!det->tone_present) {
				 
				if (det->tone_cycle_duration >= 425 * 8
				    && det->tone_cycle_duration <= 475 * 8) {
					det->good_cycles++;
					if (det->good_cycles > 2)
						det->hit = TRUE;
				}
				det->tone_cycle_duration = 0;
			}
			det->tone_present = TRUE;
		} else
			det->tone_present = FALSE;
		det->tone_cycle_duration++;
	} else {
		det->tone_present = FALSE;
		det->tone_cycle_duration = 0;
		det->good_cycles = 0;
	}
	return det->hit;
}
 
 
