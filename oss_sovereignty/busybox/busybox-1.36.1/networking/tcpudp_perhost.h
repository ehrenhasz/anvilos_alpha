PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN
struct hcc {
	char *ip;
	int pid;
};
struct hcc* FAST_FUNC ipsvd_perhost_init(unsigned);
unsigned FAST_FUNC ipsvd_perhost_add(struct hcc *cc, char *ip, unsigned maxconn, struct hcc **hccpp);
void FAST_FUNC ipsvd_perhost_remove(struct hcc *cc, int pid);
POP_SAVED_FUNCTION_VISIBILITY
