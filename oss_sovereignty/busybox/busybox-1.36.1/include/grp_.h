#ifndef BB_GRP_H
#define BB_GRP_H 1
PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN
#undef endgrent
#define endgrent     bb_internal_endgrent
#define getgrgid     bb_internal_getgrgid
#define getgrnam     bb_internal_getgrnam
#define getgrouplist bb_internal_getgrouplist
#define initgroups   bb_internal_initgroups
void FAST_FUNC endgrent(void);
struct group* FAST_FUNC getgrgid(gid_t __gid);
struct group* FAST_FUNC getgrnam(const char *__name);
int FAST_FUNC getgrouplist(const char *__user, gid_t __group,
		gid_t *__groups, int *__ngroups);
int FAST_FUNC initgroups(const char *__user, gid_t __group);
POP_SAVED_FUNCTION_VISIBILITY
#endif
