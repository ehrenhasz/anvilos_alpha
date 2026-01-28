



VIMPROG = ../../src/vim


VIMRUNTIME = ../..






RUN_VIMTEST = VIMRUNTIME=$(VIMRUNTIME) $(VALGRIND) $(ENVVARS) ../$(VIMPROG) -f $(GUI_FLAG)








test:
	@
	@echo "../$(VIMPROG)" > testdir/vimcmd
	@echo "$(RUN_VIMTEST)" >> testdir/vimcmd
	VIMRUNTIME=$(VIMRUNTIME) $(VIMPROG) --clean --not-a-term $(DEBUGLOG) -u testdir/runtest.vim
	@
	@
	@if [ -f testdir/messages ]; then cat testdir/messages; fi


clean testclean:
	rm -f testdir/failed/* testdir/done/* testdir/vimcmd testdir/messages
