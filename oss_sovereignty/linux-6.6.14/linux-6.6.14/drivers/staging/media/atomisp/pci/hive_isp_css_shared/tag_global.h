#ifndef __TAG_GLOBAL_H_INCLUDED__
#define __TAG_GLOBAL_H_INCLUDED__
#define TAG_CAP	1
#define TAG_EXP	2
#define TAG_NUM_CAPTURES_SIGN_SHIFT	 6
#define TAG_OFFSET_SIGN_SHIFT		 7
#define TAG_NUM_CAPTURES_SHIFT		 8
#define TAG_OFFSET_SHIFT		16
#define TAG_SKIP_SHIFT			24
#define TAG_EXP_ID_SHIFT		 8
struct sh_css_tag_descr {
	int num_captures;
	unsigned int skip;
	int offset;
	unsigned int exp_id;
};
#endif  
