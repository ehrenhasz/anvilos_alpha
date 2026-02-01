 

#include <crypto/padlock.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hw_random.h>
#include <linux/delay.h>
#include <asm/cpu_device_id.h>
#include <asm/io.h>
#include <asm/msr.h>
#include <asm/cpufeature.h>
#include <asm/fpu/api.h>




enum {
	VIA_STRFILT_CNT_SHIFT	= 16,
	VIA_STRFILT_FAIL	= (1 << 15),
	VIA_STRFILT_ENABLE	= (1 << 14),
	VIA_RAWBITS_ENABLE	= (1 << 13),
	VIA_RNG_ENABLE		= (1 << 6),
	VIA_NOISESRC1		= (1 << 8),
	VIA_NOISESRC2		= (1 << 9),
	VIA_XSTORE_CNT_MASK	= 0x0F,

	VIA_RNG_CHUNK_8		= 0x00,	 
	VIA_RNG_CHUNK_4		= 0x01,	 
	VIA_RNG_CHUNK_4_MASK	= 0xFFFFFFFF,
	VIA_RNG_CHUNK_2		= 0x02,	 
	VIA_RNG_CHUNK_2_MASK	= 0xFFFF,
	VIA_RNG_CHUNK_1		= 0x03,	 
	VIA_RNG_CHUNK_1_MASK	= 0xFF,
};

 

static inline u32 xstore(u32 *addr, u32 edx_in)
{
	u32 eax_out;

	asm(".byte 0x0F,0xA7,0xC0 /* xstore %%edi (addr=%0) */"
		: "=m" (*addr), "=a" (eax_out), "+d" (edx_in), "+D" (addr));

	return eax_out;
}

static int via_rng_data_present(struct hwrng *rng, int wait)
{
	char buf[16 + PADLOCK_ALIGNMENT - STACK_ALIGN] __attribute__
		((aligned(STACK_ALIGN)));
	u32 *via_rng_datum = (u32 *)PTR_ALIGN(&buf[0], PADLOCK_ALIGNMENT);
	u32 bytes_out;
	int i;

	 

	for (i = 0; i < 20; i++) {
		*via_rng_datum = 0;  
		bytes_out = xstore(via_rng_datum, VIA_RNG_CHUNK_1);
		bytes_out &= VIA_XSTORE_CNT_MASK;
		if (bytes_out || !wait)
			break;
		udelay(10);
	}
	rng->priv = *via_rng_datum;
	return bytes_out ? 1 : 0;
}

static int via_rng_data_read(struct hwrng *rng, u32 *data)
{
	u32 via_rng_datum = (u32)rng->priv;

	*data = via_rng_datum;

	return 1;
}

static int via_rng_init(struct hwrng *rng)
{
	struct cpuinfo_x86 *c = &cpu_data(0);
	u32 lo, hi, old_lo;

	 
	if (((c->x86 == 6) && (c->x86_model >= 0x0f))  || (c->x86 > 6)){
		if (!boot_cpu_has(X86_FEATURE_XSTORE_EN)) {
			pr_err(PFX "can't enable hardware RNG "
				"if XSTORE is not enabled\n");
			return -ENODEV;
		}
		return 0;
	}

	 
	rdmsr(MSR_VIA_RNG, lo, hi);

	old_lo = lo;
	lo &= ~(0x7f << VIA_STRFILT_CNT_SHIFT);
	lo &= ~VIA_XSTORE_CNT_MASK;
	lo &= ~(VIA_STRFILT_ENABLE | VIA_STRFILT_FAIL | VIA_RAWBITS_ENABLE);
	lo |= VIA_RNG_ENABLE;
	lo |= VIA_NOISESRC1;

	 

	 
	if ((c->x86_model == 9) && (c->x86_stepping > 7))
		lo |= VIA_NOISESRC2;

	 
	if (c->x86_model >= 10)
		lo |= VIA_NOISESRC2;

	if (lo != old_lo)
		wrmsr(MSR_VIA_RNG, lo, hi);

	 
	rdmsr(MSR_VIA_RNG, lo, hi);
	if ((lo & VIA_RNG_ENABLE) == 0) {
		pr_err(PFX "cannot enable VIA C3 RNG, aborting\n");
		return -ENODEV;
	}

	return 0;
}


static struct hwrng via_rng = {
	.name		= "via",
	.init		= via_rng_init,
	.data_present	= via_rng_data_present,
	.data_read	= via_rng_data_read,
};


static int __init via_rng_mod_init(void)
{
	int err;

	if (!boot_cpu_has(X86_FEATURE_XSTORE))
		return -ENODEV;

	pr_info("VIA RNG detected\n");
	err = hwrng_register(&via_rng);
	if (err) {
		pr_err(PFX "RNG registering failed (%d)\n",
		       err);
		goto out;
	}
out:
	return err;
}
module_init(via_rng_mod_init);

static void __exit via_rng_mod_exit(void)
{
	hwrng_unregister(&via_rng);
}
module_exit(via_rng_mod_exit);

static struct x86_cpu_id __maybe_unused via_rng_cpu_id[] = {
	X86_MATCH_FEATURE(X86_FEATURE_XSTORE, NULL),
	{}
};
MODULE_DEVICE_TABLE(x86cpu, via_rng_cpu_id);

MODULE_DESCRIPTION("H/W RNG driver for VIA CPU with PadLock");
MODULE_LICENSE("GPL");
