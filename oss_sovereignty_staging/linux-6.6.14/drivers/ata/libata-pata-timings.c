
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/libata.h>

 
 

static const struct ata_timing ata_timing[] = {
 
	{ XFER_PIO_0,     70, 290, 240, 600, 165, 150, 0,  600,   0 },
	{ XFER_PIO_1,     50, 290,  93, 383, 125, 100, 0,  383,   0 },
	{ XFER_PIO_2,     30, 290,  40, 330, 100,  90, 0,  240,   0 },
	{ XFER_PIO_3,     30,  80,  70, 180,  80,  70, 0,  180,   0 },
	{ XFER_PIO_4,     25,  70,  25, 120,  70,  25, 0,  120,   0 },
	{ XFER_PIO_5,     15,  65,  25, 100,  65,  25, 0,  100,   0 },
	{ XFER_PIO_6,     10,  55,  20,  80,  55,  20, 0,   80,   0 },

	{ XFER_SW_DMA_0, 120,   0,   0,   0, 480, 480, 50, 960,   0 },
	{ XFER_SW_DMA_1,  90,   0,   0,   0, 240, 240, 30, 480,   0 },
	{ XFER_SW_DMA_2,  60,   0,   0,   0, 120, 120, 20, 240,   0 },

	{ XFER_MW_DMA_0,  60,   0,   0,   0, 215, 215, 20, 480,   0 },
	{ XFER_MW_DMA_1,  45,   0,   0,   0,  80,  50, 5,  150,   0 },
	{ XFER_MW_DMA_2,  25,   0,   0,   0,  70,  25, 5,  120,   0 },
	{ XFER_MW_DMA_3,  25,   0,   0,   0,  65,  25, 5,  100,   0 },
	{ XFER_MW_DMA_4,  25,   0,   0,   0,  55,  20, 5,   80,   0 },

 
	{ XFER_UDMA_0,     0,   0,   0,   0,   0,   0, 0,    0, 120 },
	{ XFER_UDMA_1,     0,   0,   0,   0,   0,   0, 0,    0,  80 },
	{ XFER_UDMA_2,     0,   0,   0,   0,   0,   0, 0,    0,  60 },
	{ XFER_UDMA_3,     0,   0,   0,   0,   0,   0, 0,    0,  45 },
	{ XFER_UDMA_4,     0,   0,   0,   0,   0,   0, 0,    0,  30 },
	{ XFER_UDMA_5,     0,   0,   0,   0,   0,   0, 0,    0,  20 },
	{ XFER_UDMA_6,     0,   0,   0,   0,   0,   0, 0,    0,  15 },

	{ 0xFF }
};

#define ENOUGH(v, unit)		(((v)-1)/(unit)+1)
#define EZ(v, unit)		((v)?ENOUGH(((v) * 1000), unit):0)

static void ata_timing_quantize(const struct ata_timing *t,
				struct ata_timing *q, int T, int UT)
{
	q->setup	= EZ(t->setup,       T);
	q->act8b	= EZ(t->act8b,       T);
	q->rec8b	= EZ(t->rec8b,       T);
	q->cyc8b	= EZ(t->cyc8b,       T);
	q->active	= EZ(t->active,      T);
	q->recover	= EZ(t->recover,     T);
	q->dmack_hold	= EZ(t->dmack_hold,  T);
	q->cycle	= EZ(t->cycle,       T);
	q->udma		= EZ(t->udma,       UT);
}

void ata_timing_merge(const struct ata_timing *a, const struct ata_timing *b,
		      struct ata_timing *m, unsigned int what)
{
	if (what & ATA_TIMING_SETUP)
		m->setup = max(a->setup, b->setup);
	if (what & ATA_TIMING_ACT8B)
		m->act8b = max(a->act8b, b->act8b);
	if (what & ATA_TIMING_REC8B)
		m->rec8b = max(a->rec8b, b->rec8b);
	if (what & ATA_TIMING_CYC8B)
		m->cyc8b = max(a->cyc8b, b->cyc8b);
	if (what & ATA_TIMING_ACTIVE)
		m->active = max(a->active, b->active);
	if (what & ATA_TIMING_RECOVER)
		m->recover = max(a->recover, b->recover);
	if (what & ATA_TIMING_DMACK_HOLD)
		m->dmack_hold = max(a->dmack_hold, b->dmack_hold);
	if (what & ATA_TIMING_CYCLE)
		m->cycle = max(a->cycle, b->cycle);
	if (what & ATA_TIMING_UDMA)
		m->udma = max(a->udma, b->udma);
}
EXPORT_SYMBOL_GPL(ata_timing_merge);

const struct ata_timing *ata_timing_find_mode(u8 xfer_mode)
{
	const struct ata_timing *t = ata_timing;

	while (xfer_mode > t->mode)
		t++;

	if (xfer_mode == t->mode)
		return t;

	WARN_ONCE(true, "%s: unable to find timing for xfer_mode 0x%x\n",
			__func__, xfer_mode);

	return NULL;
}
EXPORT_SYMBOL_GPL(ata_timing_find_mode);

int ata_timing_compute(struct ata_device *adev, unsigned short speed,
		       struct ata_timing *t, int T, int UT)
{
	const u16 *id = adev->id;
	const struct ata_timing *s;
	struct ata_timing p;

	 
	s = ata_timing_find_mode(speed);
	if (!s)
		return -EINVAL;

	memcpy(t, s, sizeof(*s));

	 

	if (id[ATA_ID_FIELD_VALID] & 2) {	 
		memset(&p, 0, sizeof(p));

		if (speed >= XFER_PIO_0 && speed < XFER_SW_DMA_0) {
			if (speed <= XFER_PIO_2)
				p.cycle = p.cyc8b = id[ATA_ID_EIDE_PIO];
			else if ((speed <= XFER_PIO_4) ||
				 (speed == XFER_PIO_5 && !ata_id_is_cfa(id)))
				p.cycle = p.cyc8b = id[ATA_ID_EIDE_PIO_IORDY];
		} else if (speed >= XFER_MW_DMA_0 && speed <= XFER_MW_DMA_2)
			p.cycle = id[ATA_ID_EIDE_DMA_MIN];

		ata_timing_merge(&p, t, t, ATA_TIMING_CYCLE | ATA_TIMING_CYC8B);
	}

	 

	ata_timing_quantize(t, t, T, UT);

	 

	if (speed > XFER_PIO_6) {
		ata_timing_compute(adev, adev->pio_mode, &p, T, UT);
		ata_timing_merge(&p, t, t, ATA_TIMING_ALL);
	}

	 

	if (t->act8b + t->rec8b < t->cyc8b) {
		t->act8b += (t->cyc8b - (t->act8b + t->rec8b)) / 2;
		t->rec8b = t->cyc8b - t->act8b;
	}

	if (t->active + t->recover < t->cycle) {
		t->active += (t->cycle - (t->active + t->recover)) / 2;
		t->recover = t->cycle - t->active;
	}

	 
	if (t->active + t->recover > t->cycle)
		t->cycle = t->active + t->recover;

	return 0;
}
EXPORT_SYMBOL_GPL(ata_timing_compute);
