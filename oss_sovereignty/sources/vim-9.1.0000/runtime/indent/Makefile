

VIM = vim
VIMRUNTIME = ..




test:
	VIMRUNTIME=$(VIMRUNTIME) $(VIM) --clean --not-a-term -u testdir/runtest.vim


clean testclean:
	rm -f testdir/*.fail testdir/*.out
