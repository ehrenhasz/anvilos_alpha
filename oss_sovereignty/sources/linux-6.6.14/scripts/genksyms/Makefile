

hostprogs-always-y	+= genksyms

genksyms-objs	:= genksyms.o parse.tab.o lex.lex.o








ifeq ($(findstring 1,$(KBUILD_EXTRA_WARN)),)

quiet_cmd_bison_no_warn = $(quiet_cmd_bison)
      cmd_bison_no_warn = $(YACC) --version >/dev/null; \
			  $(cmd_bison) 2>/dev/null

$(obj)/pars%.tab.c $(obj)/pars%.tab.h: $(src)/pars%.y FORCE
	$(call if_changed,bison_no_warn)

endif


HOSTCFLAGS_parse.tab.o := -I $(srctree)/$(src)
HOSTCFLAGS_lex.lex.o := -I $(srctree)/$(src)


$(obj)/lex.lex.o: $(obj)/parse.tab.h
