


obj-$(CONFIG_SECURITY_APPARMOR) += apparmor.o

apparmor-y := apparmorfs.o audit.o capability.o task.o ipc.o lib.o match.o \
              path.o domain.o policy.o policy_unpack.o procattr.o lsm.o \
              resource.o secid.o file.o policy_ns.o label.o mount.o net.o \
              policy_compat.o
apparmor-$(CONFIG_SECURITY_APPARMOR_HASH) += crypto.o

obj-$(CONFIG_SECURITY_APPARMOR_KUNIT_TEST) += apparmor_policy_unpack_test.o
apparmor_policy_unpack_test-objs += policy_unpack_test.o

clean-files := capability_names.h rlim_names.h net_names.h














quiet_cmd_make-af = GEN     $@
cmd_make-af = echo "static const char *address_family_names[] = {" > $@ ;\
	sed $< >>$@ -r -n -e "/AF_MAX/d" -e "/AF_LOCAL/d" -e "/AF_ROUTE/d" -e \
	 's/^\
	echo "};" >> $@ ;\
	printf '%s' '\
	sed -r -n -e "/AF_MAX/d" -e "/AF_LOCAL/d" -e "/AF_ROUTE/d" -e \
	 's/^\
	 $< | tr '\n' ' ' | sed -e 's/ $$/"\n/' >> $@






quiet_cmd_make-sock = GEN     $@
cmd_make-sock = echo "static const char *sock_type_names[] = {" >> $@ ;\
	sed $^ >>$@ -r -n \
	-e 's/^\tSOCK_([A-Z0-9_]+)[\t]+=[ \t]+([0-9]+)(.*)/[\2] = "\L\1",/p';\
	echo "};" >> $@






quiet_cmd_make-caps = GEN     $@
cmd_make-caps = echo "static const char *const capability_names[] = {" > $@ ;\
	sed $< >>$@ -r -n -e '/CAP_FS_MASK/d' \
	-e 's/^\
	echo "};" >> $@ ;\
	printf '%s' '\
	sed $< -r -n -e '/CAP_FS_MASK/d' \
	    -e 's/^\
	     tr '\n' ' ' | sed -e 's/ $$/"\n/' >> $@























quiet_cmd_make-rlim = GEN     $@
cmd_make-rlim = echo "static const char *const rlim_names[RLIM_NLIMITS] = {" \
	> $@ ;\
	sed $< >> $@ -r -n \
	    -e 's/^\
	echo "};" >> $@ ;\
	echo "static const int rlim_map[RLIM_NLIMITS] = {" >> $@ ;\
	sed -r -n "s/^\
	echo "};" >> $@ ; \
	printf '%s' '\
	sed -r -n 's/^\
	    tr '\n' ' ' | sed -e 's/ $$/"\n/' >> $@

$(obj)/capability.o : $(obj)/capability_names.h
$(obj)/net.o : $(obj)/net_names.h
$(obj)/resource.o : $(obj)/rlim_names.h
$(obj)/capability_names.h : $(srctree)/include/uapi/linux/capability.h \
			    $(src)/Makefile
	$(call cmd,make-caps)
$(obj)/rlim_names.h : $(srctree)/include/uapi/asm-generic/resource.h \
		      $(src)/Makefile
	$(call cmd,make-rlim)
$(obj)/net_names.h : $(srctree)/include/linux/socket.h \
		     $(srctree)/include/linux/net.h \
		     $(src)/Makefile
	$(call cmd,make-af)
	$(call cmd,make-sock)
