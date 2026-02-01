


#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/livepatch.h>
#include <linux/slab.h>

 
static LIST_HEAD(ptr_list);
struct shadow_ptr {
	void *ptr;
	int id;
	struct list_head list;
};

static void free_ptr_list(void)
{
	struct shadow_ptr *sp, *tmp_sp;

	list_for_each_entry_safe(sp, tmp_sp, &ptr_list, list) {
		list_del(&sp->list);
		kfree(sp);
	}
}

static int ptr_id(void *ptr)
{
	struct shadow_ptr *sp;
	static int count;

	list_for_each_entry(sp, &ptr_list, list) {
		if (sp->ptr == ptr)
			return sp->id;
	}

	sp = kmalloc(sizeof(*sp), GFP_ATOMIC);
	if (!sp)
		return -ENOMEM;
	sp->ptr = ptr;
	sp->id = count++;

	list_add(&sp->list, &ptr_list);

	return sp->id;
}

 
static void *shadow_get(void *obj, unsigned long id)
{
	int **sv;

	sv = klp_shadow_get(obj, id);
	pr_info("klp_%s(obj=PTR%d, id=0x%lx) = PTR%d\n",
		__func__, ptr_id(obj), id, ptr_id(sv));

	return sv;
}

static void *shadow_alloc(void *obj, unsigned long id, size_t size,
			  gfp_t gfp_flags, klp_shadow_ctor_t ctor,
			  void *ctor_data)
{
	int **var = ctor_data;
	int **sv;

	sv = klp_shadow_alloc(obj, id, size, gfp_flags, ctor, var);
	pr_info("klp_%s(obj=PTR%d, id=0x%lx, size=%zx, gfp_flags=%pGg), ctor=PTR%d, ctor_data=PTR%d = PTR%d\n",
		__func__, ptr_id(obj), id, size, &gfp_flags, ptr_id(ctor),
		ptr_id(*var), ptr_id(sv));

	return sv;
}

static void *shadow_get_or_alloc(void *obj, unsigned long id, size_t size,
				 gfp_t gfp_flags, klp_shadow_ctor_t ctor,
				 void *ctor_data)
{
	int **var = ctor_data;
	int **sv;

	sv = klp_shadow_get_or_alloc(obj, id, size, gfp_flags, ctor, var);
	pr_info("klp_%s(obj=PTR%d, id=0x%lx, size=%zx, gfp_flags=%pGg), ctor=PTR%d, ctor_data=PTR%d = PTR%d\n",
		__func__, ptr_id(obj), id, size, &gfp_flags, ptr_id(ctor),
		ptr_id(*var), ptr_id(sv));

	return sv;
}

static void shadow_free(void *obj, unsigned long id, klp_shadow_dtor_t dtor)
{
	klp_shadow_free(obj, id, dtor);
	pr_info("klp_%s(obj=PTR%d, id=0x%lx, dtor=PTR%d)\n",
		__func__, ptr_id(obj), id, ptr_id(dtor));
}

static void shadow_free_all(unsigned long id, klp_shadow_dtor_t dtor)
{
	klp_shadow_free_all(id, dtor);
	pr_info("klp_%s(id=0x%lx, dtor=PTR%d)\n", __func__, id, ptr_id(dtor));
}


 
static int shadow_ctor(void *obj, void *shadow_data, void *ctor_data)
{
	int **sv = shadow_data;
	int **var = ctor_data;

	if (!var)
		return -EINVAL;

	*sv = *var;
	pr_info("%s: PTR%d -> PTR%d\n", __func__, ptr_id(sv), ptr_id(*var));

	return 0;
}

 
static void shadow_dtor(void *obj, void *shadow_data)
{
	int **sv = shadow_data;

	pr_info("%s(obj=PTR%d, shadow_data=PTR%d)\n",
		__func__, ptr_id(obj), ptr_id(sv));
}

 
#define NUM_OBJS 3

 
#define SV_ID1 0x1234
#define SV_ID2 0x1235

 
struct test_object {
	  
	struct shadow_ptr sp;

	 
	 
	 
};

static int test_klp_shadow_vars_init(void)
{
	struct test_object objs[NUM_OBJS];
	char nfields1[NUM_OBJS], *pnfields1[NUM_OBJS], **sv1[NUM_OBJS];
	char *pndup[NUM_OBJS];
	int nfields2[NUM_OBJS], *pnfields2[NUM_OBJS], **sv2[NUM_OBJS];
	void **sv;
	int ret;
	int i;

	ptr_id(NULL);

	 
	sv = shadow_get(&objs[0], SV_ID1);
	if (!sv)
		pr_info("  got expected NULL result\n");

	 
	for (i = 0; i < NUM_OBJS; i++) {
		pnfields1[i] = &nfields1[i];
		ptr_id(pnfields1[i]);

		if (i % 2) {
			sv1[i] = shadow_alloc(&objs[i], SV_ID1,
					sizeof(pnfields1[i]), GFP_KERNEL,
					shadow_ctor, &pnfields1[i]);
		} else {
			sv1[i] = shadow_get_or_alloc(&objs[i], SV_ID1,
					sizeof(pnfields1[i]), GFP_KERNEL,
					shadow_ctor, &pnfields1[i]);
		}
		if (!sv1[i]) {
			ret = -ENOMEM;
			goto out;
		}

		pnfields2[i] = &nfields2[i];
		ptr_id(pnfields2[i]);
		sv2[i] = shadow_alloc(&objs[i], SV_ID2, sizeof(pnfields2[i]),
					GFP_KERNEL, shadow_ctor, &pnfields2[i]);
		if (!sv2[i]) {
			ret = -ENOMEM;
			goto out;
		}
	}

	 
	for (i = 0; i < NUM_OBJS; i++) {
		 
		sv = shadow_get(&objs[i], SV_ID1);
		if (!sv) {
			ret = -EINVAL;
			goto out;
		}
		if ((char **)sv == sv1[i] && *sv1[i] == pnfields1[i])
			pr_info("  got expected PTR%d -> PTR%d result\n",
				ptr_id(sv1[i]), ptr_id(*sv1[i]));

		 
		sv = shadow_get(&objs[i], SV_ID2);
		if (!sv) {
			ret = -EINVAL;
			goto out;
		}
		if ((int **)sv == sv2[i] && *sv2[i] == pnfields2[i])
			pr_info("  got expected PTR%d -> PTR%d result\n",
				ptr_id(sv2[i]), ptr_id(*sv2[i]));
	}

	 
	for (i = 0; i < NUM_OBJS; i++) {
		pndup[i] = &nfields1[i];
		ptr_id(pndup[i]);

		sv = shadow_get_or_alloc(&objs[i], SV_ID1, sizeof(pndup[i]),
					GFP_KERNEL, shadow_ctor, &pndup[i]);
		if (!sv) {
			ret = -EINVAL;
			goto out;
		}
		if ((char **)sv == sv1[i] && *sv1[i] == pnfields1[i])
			pr_info("  got expected PTR%d -> PTR%d result\n",
					ptr_id(sv1[i]), ptr_id(*sv1[i]));
	}

	 
	for (i = 0; i < NUM_OBJS; i++) {
		shadow_free(&objs[i], SV_ID1, shadow_dtor);  
		sv = shadow_get(&objs[i], SV_ID1);
		if (!sv)
			pr_info("  got expected NULL result\n");
	}

	 
	for (i = 0; i < NUM_OBJS; i++) {
		sv = shadow_get(&objs[i], SV_ID2);	 
		if (!sv) {
			ret = -EINVAL;
			goto out;
		}
		if ((int **)sv == sv2[i] && *sv2[i] == pnfields2[i])
			pr_info("  got expected PTR%d -> PTR%d result\n",
					ptr_id(sv2[i]), ptr_id(*sv2[i]));
	}

	 
	shadow_free_all(SV_ID2, NULL);		 
	for (i = 0; i < NUM_OBJS; i++) {
		sv = shadow_get(&objs[i], SV_ID2);
		if (!sv)
			pr_info("  got expected NULL result\n");
	}

	free_ptr_list();

	return 0;
out:
	shadow_free_all(SV_ID1, NULL);		 
	shadow_free_all(SV_ID2, NULL);		 
	free_ptr_list();

	return ret;
}

static void test_klp_shadow_vars_exit(void)
{
}

module_init(test_klp_shadow_vars_init);
module_exit(test_klp_shadow_vars_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Joe Lawrence <joe.lawrence@redhat.com>");
MODULE_DESCRIPTION("Livepatch test: shadow variables");
