#if !defined(_FIR_H_)
#define _FIR_H_
struct fir16_state_t {
	int taps;
	int curr_pos;
	const int16_t *coeffs;
	int16_t *history;
};
struct fir32_state_t {
	int taps;
	int curr_pos;
	const int32_t *coeffs;
	int16_t *history;
};
struct fir_float_state_t {
	int taps;
	int curr_pos;
	const float *coeffs;
	float *history;
};
static inline const int16_t *fir16_create(struct fir16_state_t *fir,
					      const int16_t *coeffs, int taps)
{
	fir->taps = taps;
	fir->curr_pos = taps - 1;
	fir->coeffs = coeffs;
	fir->history = kcalloc(taps, sizeof(int16_t), GFP_KERNEL);
	return fir->history;
}
static inline void fir16_flush(struct fir16_state_t *fir)
{
	memset(fir->history, 0, fir->taps * sizeof(int16_t));
}
static inline void fir16_free(struct fir16_state_t *fir)
{
	kfree(fir->history);
}
static inline int16_t fir16(struct fir16_state_t *fir, int16_t sample)
{
	int32_t y;
	int i;
	int offset1;
	int offset2;
	fir->history[fir->curr_pos] = sample;
	offset2 = fir->curr_pos;
	offset1 = fir->taps - offset2;
	y = 0;
	for (i = fir->taps - 1; i >= offset1; i--)
		y += fir->coeffs[i] * fir->history[i - offset1];
	for (; i >= 0; i--)
		y += fir->coeffs[i] * fir->history[i + offset2];
	if (fir->curr_pos <= 0)
		fir->curr_pos = fir->taps;
	fir->curr_pos--;
	return (int16_t) (y >> 15);
}
static inline const int16_t *fir32_create(struct fir32_state_t *fir,
					      const int32_t *coeffs, int taps)
{
	fir->taps = taps;
	fir->curr_pos = taps - 1;
	fir->coeffs = coeffs;
	fir->history = kcalloc(taps, sizeof(int16_t), GFP_KERNEL);
	return fir->history;
}
static inline void fir32_flush(struct fir32_state_t *fir)
{
	memset(fir->history, 0, fir->taps * sizeof(int16_t));
}
static inline void fir32_free(struct fir32_state_t *fir)
{
	kfree(fir->history);
}
static inline int16_t fir32(struct fir32_state_t *fir, int16_t sample)
{
	int i;
	int32_t y;
	int offset1;
	int offset2;
	fir->history[fir->curr_pos] = sample;
	offset2 = fir->curr_pos;
	offset1 = fir->taps - offset2;
	y = 0;
	for (i = fir->taps - 1; i >= offset1; i--)
		y += fir->coeffs[i] * fir->history[i - offset1];
	for (; i >= 0; i--)
		y += fir->coeffs[i] * fir->history[i + offset2];
	if (fir->curr_pos <= 0)
		fir->curr_pos = fir->taps;
	fir->curr_pos--;
	return (int16_t) (y >> 15);
}
#endif
