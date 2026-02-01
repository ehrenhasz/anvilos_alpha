
#include <linux/err.h>
#include <linux/bug.h>
#include <linux/atomic.h>
#include <linux/errseq.h>
#include <linux/log2.h>

 

 
#define ERRSEQ_SHIFT		ilog2(MAX_ERRNO + 1)

 
#define ERRSEQ_SEEN		(1 << ERRSEQ_SHIFT)

 
#define ERRSEQ_CTR_INC		(1 << (ERRSEQ_SHIFT + 1))

 
errseq_t errseq_set(errseq_t *eseq, int err)
{
	errseq_t cur, old;

	 
	BUILD_BUG_ON_NOT_POWER_OF_2(MAX_ERRNO + 1);

	 
	old = READ_ONCE(*eseq);

	if (WARN(unlikely(err == 0 || (unsigned int)-err > MAX_ERRNO),
				"err = %d\n", err))
		return old;

	for (;;) {
		errseq_t new;

		 
		new = (old & ~(MAX_ERRNO|ERRSEQ_SEEN)) | -err;

		 
		if (old & ERRSEQ_SEEN)
			new += ERRSEQ_CTR_INC;

		 
		if (new == old) {
			cur = new;
			break;
		}

		 
		cur = cmpxchg(eseq, old, new);

		 
		if (likely(cur == old || cur == new))
			break;

		 
		old = cur;
	}
	return cur;
}
EXPORT_SYMBOL(errseq_set);

 
errseq_t errseq_sample(errseq_t *eseq)
{
	errseq_t old = READ_ONCE(*eseq);

	 
	if (!(old & ERRSEQ_SEEN))
		old = 0;
	return old;
}
EXPORT_SYMBOL(errseq_sample);

 
int errseq_check(errseq_t *eseq, errseq_t since)
{
	errseq_t cur = READ_ONCE(*eseq);

	if (likely(cur == since))
		return 0;
	return -(cur & MAX_ERRNO);
}
EXPORT_SYMBOL(errseq_check);

 
int errseq_check_and_advance(errseq_t *eseq, errseq_t *since)
{
	int err = 0;
	errseq_t old, new;

	 
	old = READ_ONCE(*eseq);
	if (old != *since) {
		 
		new = old | ERRSEQ_SEEN;
		if (new != old)
			cmpxchg(eseq, old, new);
		*since = new;
		err = -(new & MAX_ERRNO);
	}
	return err;
}
EXPORT_SYMBOL(errseq_check_and_advance);
