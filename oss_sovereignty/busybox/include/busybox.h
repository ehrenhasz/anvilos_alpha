#ifndef BUSYBOX_H
#define BUSYBOX_H 1
#include "libbb.h"
#include "applet_metadata.h"
PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN
extern const char applet_names[] ALIGN1;
extern int (*const applet_main[])(int argc, char **argv);
extern const uint8_t applet_flags[] ALIGN1;
extern const uint8_t applet_suid[] ALIGN1;
extern const uint8_t applet_install_loc[] ALIGN1;
#if ENABLE_FEATURE_PREFER_APPLETS \
 || ENABLE_FEATURE_SH_STANDALONE \
 || ENABLE_FEATURE_SH_NOFORK
# define APPLET_IS_NOFORK(i) (applet_flags[(i)/4] & (1 << (2 * ((i)%4))))
# define APPLET_IS_NOEXEC(i) (applet_flags[(i)/4] & (1 << ((2 * ((i)%4))+1)))
#else
# define APPLET_IS_NOFORK(i) 0
# define APPLET_IS_NOEXEC(i) 0
#endif
#if ENABLE_FEATURE_SUID
# define APPLET_SUID(i) ((applet_suid[(i)/4] >> (2 * ((i)%4)) & 3))
#endif
#if ENABLE_FEATURE_INSTALLER
#define APPLET_INSTALL_LOC(i) ({ \
	unsigned v = (i); \
	if (v & 1) v = applet_install_loc[v/2] >> 4; \
	else v = applet_install_loc[v/2] & 0xf; \
	v; })
#endif
#if ENABLE_BUILD_LIBBUSYBOX
#if ENABLE_FEATURE_SHARED_BUSYBOX
int lbb_main(char **argv) EXTERNALLY_VISIBLE;
#else
int lbb_main(char **argv);
#endif
#endif
POP_SAVED_FUNCTION_VISIBILITY
#endif
