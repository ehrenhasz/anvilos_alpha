#ifndef AR9003_RTT_H
#define AR9003_RTT_H
#ifdef CONFIG_ATH9K_PCOEM
void ar9003_hw_rtt_enable(struct ath_hw *ah);
void ar9003_hw_rtt_disable(struct ath_hw *ah);
void ar9003_hw_rtt_set_mask(struct ath_hw *ah, u32 rtt_mask);
bool ar9003_hw_rtt_force_restore(struct ath_hw *ah);
void ar9003_hw_rtt_load_hist(struct ath_hw *ah);
void ar9003_hw_rtt_fill_hist(struct ath_hw *ah);
void ar9003_hw_rtt_clear_hist(struct ath_hw *ah);
bool ar9003_hw_rtt_restore(struct ath_hw *ah, struct ath9k_channel *chan);
#else
static inline void ar9003_hw_rtt_enable(struct ath_hw *ah)
{
}
static inline void ar9003_hw_rtt_disable(struct ath_hw *ah)
{
}
static inline void ar9003_hw_rtt_set_mask(struct ath_hw *ah, u32 rtt_mask)
{
}
static inline bool ar9003_hw_rtt_force_restore(struct ath_hw *ah)
{
	return false;
}
static inline void ar9003_hw_rtt_load_hist(struct ath_hw *ah)
{
}
static inline void ar9003_hw_rtt_fill_hist(struct ath_hw *ah)
{
}
static inline void ar9003_hw_rtt_clear_hist(struct ath_hw *ah)
{
}
static inline bool ar9003_hw_rtt_restore(struct ath_hw *ah, struct ath9k_channel *chan)
{
	return false;
}
#endif
#endif
