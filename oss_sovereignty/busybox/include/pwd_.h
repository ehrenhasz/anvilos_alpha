#ifndef BB_PWD_H
#define BB_PWD_H 1
PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN
#undef endpwent
#define setpwent    bb_internal_setpwent
#define endpwent    bb_internal_endpwent
#define getpwent    bb_internal_getpwent
#define getpwuid    bb_internal_getpwuid
#define getpwnam    bb_internal_getpwnam
#define getpwnam_r  bb_internal_getpwnam_r
void FAST_FUNC setpwent(void);
void FAST_FUNC endpwent(void);
struct passwd* FAST_FUNC getpwent(void);
struct passwd* FAST_FUNC getpwuid(uid_t __uid);
struct passwd* FAST_FUNC getpwnam(const char *__name);
int FAST_FUNC getpwnam_r(const char *__restrict __name,
		struct passwd *__restrict __resultbuf,
		char *__restrict __buffer, size_t __buflen,
		struct passwd **__restrict __result);
POP_SAVED_FUNCTION_VISIBILITY
#endif
