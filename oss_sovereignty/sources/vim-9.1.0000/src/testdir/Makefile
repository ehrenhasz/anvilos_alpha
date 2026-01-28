




VIMPROG = ../vim
XXDPROG = ../xxd/xxd


SCRIPTSOURCE = ../../runtime




REDIR_TEST_TO_NULL = --cmd 'au SwapExists * let v:swapchoice = "e"' | LC_ALL=C LANG=C LANGUAGE=C awk '/Executing Test_/{match($$0, "([0-9][0-9]:[0-9][0-9] *)?Executing Test_[^\\)]*\\)"); print substr($$0, RSTART, RLENGTH) "\r"; fflush()}'










default: nongui



include Make_all.mak


test_options.res test_alot.res: opt_test.vim

.SUFFIXES: .in .out .res .vim

nongui:	nolog tinytests newtests report

gui:	nolog tinytests newtests report

tiny:	nolog tinytests report

benchmark: $(SCRIPTS_BENCH)

report:
	@
	@/bin/sh -c "if test -f test.log; \
		then cp test.log test_result.log; \
		else echo No failures reported > test_result.log; \
		fi"
	$(VIMPROG) -u NONE $(NO_INITS) -S summarize.vim messages $(REDIR_TEST_TO_NULL)
	@rm -f starttime
	@echo
	@echo 'Test results:'
	@cat test_result.log
	@/bin/sh -c "if test -f test.log; \
		then echo TEST FAILURE; exit 1; \
		else echo ALL DONE; \
		fi"

$(SCRIPTS_TINY_OUT) $(NEW_TESTS_RES): $(VIMPROG)




$(NEW_TESTS):
	rm -f $@.res $(CLEANUP_FILES)
	@MAKEFLAGS=--no-print-directory $(MAKE) -f Makefile $@.res VIMPROG=$(VIMPROG) XXDPROG=$(XXDPROG) SCRIPTSOURCE=$(SCRIPTSOURCE)
	@cat messages
	@if test -f test.log; then \
		exit 1; \
	fi


test_vim9:
	rm -f test_vim9_*.res $(CLEANUP_FILES)
	@MAKEFLAGS=--no-print-directory $(MAKE) -f Makefile $(TEST_VIM9_RES) VIMPROG=$(VIMPROG) XXDPROG=$(XXDPROG) SCRIPTSOURCE=$(SCRIPTSOURCE)
	@cat messages
	@rm -f starttime
	@MAKEFLAGS=--no-print-directory $(MAKE) -f Makefile report VIMPROG=$(VIMPROG) XXDPROG=$(XXDPROG) SCRIPTSOURCE=$(SCRIPTSOURCE)
	@if test -f test.log; then \
		exit 1; \
	fi

RM_ON_RUN = test.out X* viminfo
RM_ON_START = test.ok benchmark.out
RUN_VIM = VIMRUNTIME=$(SCRIPTSOURCE) $(VALGRIND) $(VIMPROG) -f $(GUI_FLAG) -u unix.vim $(NO_INITS) -s dotest.in



clean:
	-rm -rf *.out *.failed *.res *.rej *.orig XfakeHOME Xdir1 Xfind
	-rm -f opt_test.vim test_result.log $(CLEANUP_FILES)
	-rm -rf $(RM_ON_RUN) $(RM_ON_START)
	-rm -f valgrind.*
	-rm -f asan.*
	-rm -f guidialog guidialogfile


benchmarkclean:
	rm -f $(SCRIPTS_BENCH)

nolog:
	-rm -f test_result.log $(CLEANUP_FILES)



tinytests: $(SCRIPTS_TINY_OUT)

.in.out:
	-rm -rf $*.failed test.ok $(RM_ON_RUN)
	cp $*.ok test.ok
	@
	@
	@
	@-/bin/sh -c "sleep .2 > /dev/null 2>&1 || sleep 1"
	$(RUN_VIM) $*.in $(REDIR_TEST_TO_NULL)

	@
	@/bin/sh -c "if test -f test.out; then \
		  if diff test.out $*.ok; \
		  then mv -f test.out $*.out; \
		  else echo $* FAILED >>test.log; mv -f test.out $*.failed; \
		  fi \
		else echo $* NO OUTPUT >>test.log; \
		fi"
	@/bin/sh -c "if test -f valgrind; then\
		  mv -f valgrind valgrind.$*; \
		fi"
	-rm -rf X* test.ok viminfo






RUN_VIMTEST = VIMRUNTIME=$(SCRIPTSOURCE) $(VALGRIND) $(VIMPROG) -f $(GUI_FLAG) -u unix.vim --gui-dialog-file guidialog

newtests: newtestssilent
	@/bin/sh -c "if test -f messages; then cat messages; fi"

newtestssilent: $(NEW_TESTS_RES)


.vim.res:
	@echo "$(VIMPROG)" > vimcmd
	@echo "$(RUN_VIMTEST)" >> vimcmd
	$(RUN_VIMTEST) $(NO_INITS) -S runtest.vim $*.vim $(REDIR_TEST_TO_NULL)
	@rm vimcmd

test_gui.res: test_gui.vim
	@echo "$(VIMPROG)" > vimcmd
	@echo "$(RUN_GVIMTEST)" >> vimcmd
	$(RUN_VIMTEST) -u NONE $(NO_INITS) -S runtest.vim $<
	@rm vimcmd

test_gui_init.res: test_gui_init.vim
	@echo "$(VIMPROG)" > vimcmd
	@echo "$(RUN_GVIMTEST_WITH_GVIMRC)" >> vimcmd
	$(RUN_VIMTEST) -u gui_preinit.vim -U gui_init.vim $(NO_PLUGINS) -S runtest.vim $<
	@rm vimcmd

opt_test.vim: ../optiondefs.h gen_opt_test.vim
	$(VIMPROG) -u NONE -S gen_opt_test.vim --noplugin --not-a-term ../optiondefs.h

test_xxd.res:
	XXD=$(XXDPROG); export XXD; $(RUN_VIMTEST) $(NO_INITS) -S runtest.vim test_xxd.vim

test_bench_regexp.res: test_bench_regexp.vim
	-rm -rf benchmark.out $(RM_ON_RUN)
	@
	@
	@
	@-/bin/sh -c "sleep .2 > /dev/null 2>&1 || sleep 1"
	$(RUN_VIMTEST) $(NO_INITS) -S runtest.vim $*.vim $(REDIR_TEST_TO_NULL)
	@/bin/sh -c "if test -f benchmark.out; then cat benchmark.out; fi"
