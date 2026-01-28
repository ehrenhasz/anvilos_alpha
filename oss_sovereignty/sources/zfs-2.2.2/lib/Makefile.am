




















































noinst_LTLIBRARIES =
lib_LTLIBRARIES =
pkgconfig_DATA =
include $(srcdir)/%D%/libavl/Makefile.am
include $(srcdir)/%D%/libicp/Makefile.am
include $(srcdir)/%D%/libnvpair/Makefile.am
include $(srcdir)/%D%/libshare/Makefile.am
include $(srcdir)/%D%/libspl/Makefile.am
include $(srcdir)/%D%/libtpool/Makefile.am
include $(srcdir)/%D%/libunicode/Makefile.am
include $(srcdir)/%D%/libuutil/Makefile.am
include $(srcdir)/%D%/libzfs_core/Makefile.am
include $(srcdir)/%D%/libzfs/Makefile.am
include $(srcdir)/%D%/libzfsbootenv/Makefile.am
include $(srcdir)/%D%/libzpool/Makefile.am
include $(srcdir)/%D%/libzstd/Makefile.am
include $(srcdir)/%D%/libzutil/Makefile.am
if BUILD_LINUX
include $(srcdir)/%D%/libefi/Makefile.am
endif


PHONY += lib
lib: $(noinst_LTLIBRARIES) $(lib_LTLIBRARIES)


PHONY += checkabi storeabi check_libabi_version allow_libabi_only_for_x86_64

check_libabi_version:
	if [ $$(abidw -v | $(SED) 's/[^0-9]//g') -lt 200 ]; then \
		printf '%s\n' "" \
		    "*** Please use libabigail 2.0.0 version or newer;" \
		    "*** otherwise results are not consistent!" \
		    "(or see https://github.com/openzfs/libabigail-docker)"; \
		exit 1; \
	fi

allow_libabi_only_for_x86_64:
	echo '*** ABI definitions provided apply only to x86_64:'
	echo '*** not checking or storing ABI and assuming success.'

if TARGET_CPU_X86_64

checkabi: check_libabi_version
	err=0; \
	for lib in $(lib_LTLIBRARIES); do \
		lib=$${lib%.la}; \
		[ -f $(srcdir)/lib/$$lib/$$lib.suppr ] || continue; \
		echo $$lib:; \
		abidiff --no-unreferenced-symbols \
		    --headers-dir1 include \
		    --suppressions $(srcdir)/lib/$$lib/$$lib.suppr \
		    $(srcdir)/lib/$$lib/$$lib.abi .libs/$$lib.so || err=$$((err + 1)); \
	done; \
	exit $$err

storeabi: check_libabi_version
	for lib in $(lib_LTLIBRARIES); do \
		lib=$${lib%.la}; \
		[ -f $(srcdir)/lib/$$lib/$$lib.suppr ] || continue; \
		abidw --no-show-locs \
		    --no-corpus-path \
		    --no-comp-dir-path \
		    --type-id-style hash \
		    .libs/$$lib.so > $(srcdir)/lib/$$lib/$$lib.abi; \
	done
else
checkabi: allow_libabi_only_for_x86_64
storeabi: allow_libabi_only_for_x86_64
endif
