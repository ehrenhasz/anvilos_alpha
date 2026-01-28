









obj-$(CONFIG_SECURITY_SELINUX) := selinux.o

ccflags-y := -I$(srctree)/security/selinux -I$(srctree)/security/selinux/include

selinux-y := avc.o hooks.o selinuxfs.o netlink.o nlmsgtab.o netif.o \
	     netnode.o netport.o status.o \
	     ss/ebitmap.o ss/hashtab.o ss/symtab.o ss/sidtab.o ss/avtab.o \
	     ss/policydb.o ss/services.o ss/conditional.o ss/mls.o ss/context.o

selinux-$(CONFIG_SECURITY_NETWORK_XFRM) += xfrm.o
selinux-$(CONFIG_NETLABEL) += netlabel.o
selinux-$(CONFIG_SECURITY_INFINIBAND) += ibpkey.o
selinux-$(CONFIG_IMA) += ima.o

genhdrs := flask.h av_permissions.h



$(addprefix $(obj)/,$(selinux-y)): $(obj)/flask.h

quiet_cmd_genhdrs = GEN     $(addprefix $(obj)/,$(genhdrs))
      cmd_genhdrs = $< $(addprefix $(obj)/,$(genhdrs))





targets += flask.h
$(obj)/flask.h: scripts/selinux/genheaders/genheaders FORCE
	$(call if_changed,genhdrs)
