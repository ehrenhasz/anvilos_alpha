
 


 
#define __EXPORTED_HEADERS__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <linux/kconfig.h>

static void usage(char *name)
{
	printf("usage: %s [-m] policy_file context_file\n", name);
	exit(1);
}

 
struct security_class_mapping {
	const char *name;
	const char *perms[sizeof(unsigned) * 8 + 1];
};

#include "classmap.h"
#include "initial_sid_to_string.h"
#include "policycap_names.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int main(int argc, char *argv[])
{
	int i, j, mls = 0;
	int initial_sid_to_string_len;
	char **arg, *polout, *ctxout;

	FILE *fout;

	if (argc < 3)
		usage(argv[0]);
	arg = argv+1;
	if (argc==4 && strcmp(argv[1], "-m") == 0) {
		mls = 1;
		arg++;
	}
	polout = *arg++;
	ctxout = *arg;

	fout = fopen(polout, "w");
	if (!fout) {
		printf("Could not open %s for writing\n", polout);
		usage(argv[0]);
	}

	 
	for (i = 0; secclass_map[i].name; i++)
		fprintf(fout, "class %s\n", secclass_map[i].name);
	fprintf(fout, "\n");

	initial_sid_to_string_len = sizeof(initial_sid_to_string) / sizeof (char *);
	 
	for (i = 1; i < initial_sid_to_string_len; i++) {
		const char *name = initial_sid_to_string[i];

		if (name)
			fprintf(fout, "sid %s\n", name);
		else
			fprintf(fout, "sid unused%d\n", i);
	}
	fprintf(fout, "\n");

	 
	for (i = 0; secclass_map[i].name; i++) {
		const struct security_class_mapping *map = &secclass_map[i];
		fprintf(fout, "class %s\n", map->name);
		fprintf(fout, "{\n");
		for (j = 0; map->perms[j]; j++)
			fprintf(fout, "\t%s\n", map->perms[j]);
		fprintf(fout, "}\n\n");
	}
	fprintf(fout, "\n");

	 
	if (mls) {
		fprintf(fout, "sensitivity s0;\n");
		fprintf(fout, "sensitivity s1;\n");
		fprintf(fout, "dominance { s0 s1 }\n");
		fprintf(fout, "category c0;\n");
		fprintf(fout, "category c1;\n");
		fprintf(fout, "level s0:c0.c1;\n");
		fprintf(fout, "level s1:c0.c1;\n");
#define SYSTEMLOW "s0"
#define SYSTEMHIGH "s1:c0.c1"
		for (i = 0; secclass_map[i].name; i++) {
			const struct security_class_mapping *map = &secclass_map[i];

			fprintf(fout, "mlsconstrain %s {\n", map->name);
			for (j = 0; map->perms[j]; j++)
				fprintf(fout, "\t%s\n", map->perms[j]);
			 
			fprintf(fout, "} (l2 eq h2 and h1 dom h2);\n\n");
		}
	}

	 
	for (i = 0; i < ARRAY_SIZE(selinux_policycap_names); i++)
		fprintf(fout, "policycap %s;\n", selinux_policycap_names[i]);

	 
	fprintf(fout, "type base_t;\n");
	fprintf(fout, "role base_r;\n");
	fprintf(fout, "role base_r types { base_t };\n");
	for (i = 0; secclass_map[i].name; i++)
		fprintf(fout, "allow base_t base_t:%s *;\n",
			secclass_map[i].name);
	fprintf(fout, "user user_u roles { base_r }");
	if (mls)
		fprintf(fout, " level %s range %s - %s", SYSTEMLOW,
			SYSTEMLOW, SYSTEMHIGH);
	fprintf(fout, ";\n");

#define SUBJUSERROLETYPE "user_u:base_r:base_t"
#define OBJUSERROLETYPE "user_u:object_r:base_t"

	 
	for (i = 1; i < initial_sid_to_string_len; i++) {
		const char *name = initial_sid_to_string[i];

		if (name)
			fprintf(fout, "sid %s ", name);
		else
			fprintf(fout, "sid unused%d\n", i);
		fprintf(fout, SUBJUSERROLETYPE "%s\n",
			mls ? ":" SYSTEMLOW : "");
	}
	fprintf(fout, "\n");

#define FS_USE(behavior, fstype)			    \
	fprintf(fout, "fs_use_%s %s " OBJUSERROLETYPE "%s;\n", \
		behavior, fstype, mls ? ":" SYSTEMLOW : "")

	 
#ifdef CONFIG_EXT2_FS_SECURITY
	FS_USE("xattr", "ext2");
#endif
#ifdef CONFIG_EXT4_FS_SECURITY
#ifdef CONFIG_EXT4_USE_FOR_EXT2
	FS_USE("xattr", "ext2");
#endif
	FS_USE("xattr", "ext3");
	FS_USE("xattr", "ext4");
#endif
#ifdef CONFIG_JFS_SECURITY
	FS_USE("xattr", "jfs");
#endif
#ifdef CONFIG_REISERFS_FS_SECURITY
	FS_USE("xattr", "reiserfs");
#endif
#ifdef CONFIG_JFFS2_FS_SECURITY
	FS_USE("xattr", "jffs2");
#endif
#ifdef CONFIG_XFS_FS
	FS_USE("xattr", "xfs");
#endif
#ifdef CONFIG_GFS2_FS
	FS_USE("xattr", "gfs2");
#endif
#ifdef CONFIG_BTRFS_FS
	FS_USE("xattr", "btrfs");
#endif
#ifdef CONFIG_F2FS_FS_SECURITY
	FS_USE("xattr", "f2fs");
#endif
#ifdef CONFIG_OCFS2_FS
	FS_USE("xattr", "ocsfs2");
#endif
#ifdef CONFIG_OVERLAY_FS
	FS_USE("xattr", "overlay");
#endif
#ifdef CONFIG_SQUASHFS_XATTR
	FS_USE("xattr", "squashfs");
#endif

	 
	FS_USE("task", "pipefs");
	FS_USE("task", "sockfs");

	 
#ifdef CONFIG_UNIX98_PTYS
	FS_USE("trans", "devpts");
#endif
#ifdef CONFIG_HUGETLBFS
	FS_USE("trans", "hugetlbfs");
#endif
#ifdef CONFIG_TMPFS
	FS_USE("trans", "tmpfs");
#endif
#ifdef CONFIG_DEVTMPFS
	FS_USE("trans", "devtmpfs");
#endif
#ifdef CONFIG_POSIX_MQUEUE
	FS_USE("trans", "mqueue");
#endif

#define GENFSCON(fstype, prefix)			     \
	fprintf(fout, "genfscon %s %s " OBJUSERROLETYPE "%s\n", \
		fstype, prefix, mls ? ":" SYSTEMLOW : "")

	 
#ifdef CONFIG_PROC_FS
	GENFSCON("proc", "/");
#endif
#ifdef CONFIG_SECURITY_SELINUX
	GENFSCON("selinuxfs", "/");
#endif
#ifdef CONFIG_SYSFS
	GENFSCON("sysfs", "/");
#endif
#ifdef CONFIG_DEBUG_FS
	GENFSCON("debugfs", "/");
#endif
#ifdef CONFIG_TRACING
	GENFSCON("tracefs", "/");
#endif
#ifdef CONFIG_PSTORE
	GENFSCON("pstore", "/");
#endif
	GENFSCON("cgroup", "/");
	GENFSCON("cgroup2", "/");

	fclose(fout);

	fout = fopen(ctxout, "w");
	if (!fout) {
		printf("Wrote policy, but cannot open %s for writing\n", ctxout);
		usage(argv[0]);
	}
	fprintf(fout, "/ " OBJUSERROLETYPE "%s\n", mls ? ":" SYSTEMLOW : "");
	fprintf(fout, "/.* " OBJUSERROLETYPE "%s\n", mls ? ":" SYSTEMLOW : "");
	fclose(fout);

	return 0;
}
