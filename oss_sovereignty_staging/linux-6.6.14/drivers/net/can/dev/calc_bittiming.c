
 

#include <linux/units.h>
#include <linux/can/dev.h>

#define CAN_CALC_MAX_ERROR 50  

 
static int
can_update_sample_point(const struct can_bittiming_const *btc,
			const unsigned int sample_point_nominal, const unsigned int tseg,
			unsigned int *tseg1_ptr, unsigned int *tseg2_ptr,
			unsigned int *sample_point_error_ptr)
{
	unsigned int sample_point_error, best_sample_point_error = UINT_MAX;
	unsigned int sample_point, best_sample_point = 0;
	unsigned int tseg1, tseg2;
	int i;

	for (i = 0; i <= 1; i++) {
		tseg2 = tseg + CAN_SYNC_SEG -
			(sample_point_nominal * (tseg + CAN_SYNC_SEG)) /
			1000 - i;
		tseg2 = clamp(tseg2, btc->tseg2_min, btc->tseg2_max);
		tseg1 = tseg - tseg2;
		if (tseg1 > btc->tseg1_max) {
			tseg1 = btc->tseg1_max;
			tseg2 = tseg - tseg1;
		}

		sample_point = 1000 * (tseg + CAN_SYNC_SEG - tseg2) /
			(tseg + CAN_SYNC_SEG);
		sample_point_error = abs(sample_point_nominal - sample_point);

		if (sample_point <= sample_point_nominal &&
		    sample_point_error < best_sample_point_error) {
			best_sample_point = sample_point;
			best_sample_point_error = sample_point_error;
			*tseg1_ptr = tseg1;
			*tseg2_ptr = tseg2;
		}
	}

	if (sample_point_error_ptr)
		*sample_point_error_ptr = best_sample_point_error;

	return best_sample_point;
}

int can_calc_bittiming(const struct net_device *dev, struct can_bittiming *bt,
		       const struct can_bittiming_const *btc, struct netlink_ext_ack *extack)
{
	struct can_priv *priv = netdev_priv(dev);
	unsigned int bitrate;			 
	unsigned int bitrate_error;		 
	unsigned int best_bitrate_error = UINT_MAX;
	unsigned int sample_point_error;	 
	unsigned int best_sample_point_error = UINT_MAX;
	unsigned int sample_point_nominal;	 
	unsigned int best_tseg = 0;		 
	unsigned int best_brp = 0;		 
	unsigned int brp, tsegall, tseg, tseg1 = 0, tseg2 = 0;
	u64 v64;
	int err;

	 
	if (bt->sample_point) {
		sample_point_nominal = bt->sample_point;
	} else {
		if (bt->bitrate > 800 * KILO  )
			sample_point_nominal = 750;
		else if (bt->bitrate > 500 * KILO  )
			sample_point_nominal = 800;
		else
			sample_point_nominal = 875;
	}

	 
	for (tseg = (btc->tseg1_max + btc->tseg2_max) * 2 + 1;
	     tseg >= (btc->tseg1_min + btc->tseg2_min) * 2; tseg--) {
		tsegall = CAN_SYNC_SEG + tseg / 2;

		 
		brp = priv->clock.freq / (tsegall * bt->bitrate) + tseg % 2;

		 
		brp = (brp / btc->brp_inc) * btc->brp_inc;
		if (brp < btc->brp_min || brp > btc->brp_max)
			continue;

		bitrate = priv->clock.freq / (brp * tsegall);
		bitrate_error = abs(bt->bitrate - bitrate);

		 
		if (bitrate_error > best_bitrate_error)
			continue;

		 
		if (bitrate_error < best_bitrate_error)
			best_sample_point_error = UINT_MAX;

		can_update_sample_point(btc, sample_point_nominal, tseg / 2,
					&tseg1, &tseg2, &sample_point_error);
		if (sample_point_error >= best_sample_point_error)
			continue;

		best_sample_point_error = sample_point_error;
		best_bitrate_error = bitrate_error;
		best_tseg = tseg / 2;
		best_brp = brp;

		if (bitrate_error == 0 && sample_point_error == 0)
			break;
	}

	if (best_bitrate_error) {
		 
		v64 = (u64)best_bitrate_error * 1000;
		do_div(v64, bt->bitrate);
		bitrate_error = (u32)v64;
		if (bitrate_error > CAN_CALC_MAX_ERROR) {
			NL_SET_ERR_MSG_FMT(extack,
					   "bitrate error: %u.%u%% too high",
					   bitrate_error / 10, bitrate_error % 10);
			return -EINVAL;
		}
		NL_SET_ERR_MSG_FMT(extack,
				   "bitrate error: %u.%u%%",
				   bitrate_error / 10, bitrate_error % 10);
	}

	 
	bt->sample_point = can_update_sample_point(btc, sample_point_nominal,
						   best_tseg, &tseg1, &tseg2,
						   NULL);

	v64 = (u64)best_brp * 1000 * 1000 * 1000;
	do_div(v64, priv->clock.freq);
	bt->tq = (u32)v64;
	bt->prop_seg = tseg1 / 2;
	bt->phase_seg1 = tseg1 - bt->prop_seg;
	bt->phase_seg2 = tseg2;

	can_sjw_set_default(bt);

	err = can_sjw_check(dev, bt, btc, extack);
	if (err)
		return err;

	bt->brp = best_brp;

	 
	bt->bitrate = priv->clock.freq /
		(bt->brp * can_bit_time(bt));

	return 0;
}

void can_calc_tdco(struct can_tdc *tdc, const struct can_tdc_const *tdc_const,
		   const struct can_bittiming *dbt,
		   u32 *ctrlmode, u32 ctrlmode_supported)

{
	if (!tdc_const || !(ctrlmode_supported & CAN_CTRLMODE_TDC_AUTO))
		return;

	*ctrlmode &= ~CAN_CTRLMODE_TDC_MASK;

	 
	if (dbt->brp == 1 || dbt->brp == 2) {
		 
		u32 sample_point_in_tc = (CAN_SYNC_SEG + dbt->prop_seg +
					  dbt->phase_seg1) * dbt->brp;

		if (sample_point_in_tc < tdc_const->tdco_min)
			return;
		tdc->tdco = min(sample_point_in_tc, tdc_const->tdco_max);
		*ctrlmode |= CAN_CTRLMODE_TDC_AUTO;
	}
}
