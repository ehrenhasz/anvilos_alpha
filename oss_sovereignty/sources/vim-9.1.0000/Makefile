























first:
	@if test ! -f src/auto/config.mk; then \
		cp src/config.mk.dist src/auto/config.mk; \
	fi
	@echo "Starting make in the src directory."
	@echo "If there are problems, cd to the src directory and run make there"
	cd src && $(MAKE) $@



all install uninstall tools config configure reconfig proto depend lint tags types test scripttests testtiny test_libvterm unittests testclean clean distclean:
	@if test ! -f src/auto/config.mk; then \
		cp src/config.mk.dist src/auto/config.mk; \
	fi
	@echo "Starting make in the src directory."
	@echo "If there are problems, cd to the src directory and run make there"
	cd src && $(MAKE) $@
	@
	@if test "$@" = "test" -o "$@" = "testtiny"; then \
		$(MAKE) indenttest; \
		$(MAKE) syntaxtest; \
	fi
	@
	@if test "$@" = "clean" -o "$@" = "distclean" -o "$@" = "testclean"; then \
		(cd runtime/indent && $(MAKE) clean); \
		(cd runtime/syntax && $(MAKE) clean); \
	fi


VIM_FOR_INDENTTEST = ../../src/vim

indenttest:
	cd runtime/indent && \
		$(MAKE) clean && \
		$(MAKE) test VIM="$(VIM_FOR_INDENTTEST)"


VIM_FOR_SYNTAXTEST = ../../src/vim

syntaxtest:
	cd runtime/syntax && \
		$(MAKE) clean && \
		$(MAKE) test VIMPROG="$(VIM_FOR_SYNTAXTEST)"




























MAJOR = 9
MINOR = 1





















































































































































VIMVER	= vim-$(MAJOR).$(MINOR)
VERSION = $(MAJOR)$(MINOR)
VDOT	= $(MAJOR).$(MINOR)
VIMRTDIR = vim$(VERSION)


VIM	= vim




include Filelist




dist:
	mkdir dist



prepare:
	if test -f runtime/doc/uganda.nsis.txt; then \
		rm runtime/doc/uganda.nsis.txt; fi
	for name in $(IN_README_DIR); do \
	  cp READMEdir/"$$name" .; \
	  done


dist/comment:
	mkdir dist/comment

COMMENT_RT = comment/$(VERSION)-rt
COMMENT_W32 = comment/$(VERSION)-bin-w32
COMMENT_GVIM = comment/$(VERSION)-bin-gvim
COMMENT_OLE = comment/$(VERSION)-bin-ole
COMMENT_SRC = comment/$(VERSION)-src
COMMENT_HTML = comment/$(VERSION)-html
COMMENT_FARSI = comment/$(VERSION)-farsi

dist/$(COMMENT_RT): dist/comment
	echo "Vim - Vi IMproved - v$(VDOT) runtime files for MS-DOS and MS-Windows" > dist/$(COMMENT_RT)

dist/$(COMMENT_W32): dist/comment
	echo "Vim - Vi IMproved - v$(VDOT) binaries for MS-Windows NT/95" > dist/$(COMMENT_W32)

dist/$(COMMENT_GVIM): dist/comment
	echo "Vim - Vi IMproved - v$(VDOT) GUI binaries for MS-Windows NT/95" > dist/$(COMMENT_GVIM)

dist/$(COMMENT_OLE): dist/comment
	echo "Vim - Vi IMproved - v$(VDOT) MS-Windows GUI binaries with OLE support" > dist/$(COMMENT_OLE)

dist/$(COMMENT_SRC): dist/comment
	echo "Vim - Vi IMproved - v$(VDOT) sources for MS-DOS and MS-Windows" > dist/$(COMMENT_SRC)

dist/$(COMMENT_HTML): dist/comment
	echo "Vim - Vi IMproved - v$(VDOT) documentation in HTML" > dist/$(COMMENT_HTML)

dist/$(COMMENT_FARSI): dist/comment
	echo "Vim - Vi IMproved - v$(VDOT) Farsi language files" > dist/$(COMMENT_FARSI)

unixall: dist prepare
	-rm -f dist/$(VIMVER).tar.bz2
	-rm -rf dist/$(VIMRTDIR)
	mkdir dist/$(VIMRTDIR)
	tar cf - \
		$(RT_ALL) \
		$(RT_ALL_BIN) \
		$(RT_UNIX) \
		$(RT_UNIX_DOS_BIN) \
		$(RT_SCRIPTS) \
		$(LANG_GEN) \
		$(LANG_GEN_BIN) \
		$(SRC_ALL) \
		$(SRC_UNIX) \
		$(SRC_DOS_UNIX) \
		$(EXTRA) \
		$(LANG_SRC) \
		| (cd dist/$(VIMRTDIR); tar xf -)
	-rm $(IN_README_DIR)



	cp -f src/config.mk.dist dist/$(VIMRTDIR)/src/auto/config.mk

	touch dist/$(VIMRTDIR)/src/auto/config.h

	touch dist/$(VIMRTDIR)/src/configure


	touch dist/$(VIMRTDIR)/src/po/ja.sjis.po
	touch dist/$(VIMRTDIR)/src/po/cs.cp1250.po
	touch dist/$(VIMRTDIR)/src/po/pl.cp1250.po
	touch dist/$(VIMRTDIR)/src/po/sk.cp1250.po
	touch dist/$(VIMRTDIR)/src/po/zh_CN.cp936.po
	touch dist/$(VIMRTDIR)/src/po/ru.cp1251.po
	touch dist/$(VIMRTDIR)/src/po/uk.cp1251.po

	cd dist && tar cf $(VIMVER).tar $(VIMRTDIR)
	bzip2 dist/$(VIMVER).tar


amirt: dist prepare
	-rm -f dist/vim$(VERSION)rt.tar.gz
	-rm -rf dist/Vim
	mkdir dist/Vim
	mkdir dist/Vim/$(VIMRTDIR)
	tar cf - \
		$(ROOT_AMI) \
		$(RT_ALL) \
		$(RT_ALL_BIN) \
		$(RT_SCRIPTS) \
		$(RT_AMI) \
		$(RT_NO_UNIX) \
		$(RT_AMI_DOS) \
		| (cd dist/Vim/$(VIMRTDIR); tar xf -)
	-rm $(IN_README_DIR)
	mv dist/Vim/$(VIMRTDIR)/vimdir.info dist/Vim.info
	mv dist/Vim/$(VIMRTDIR)/runtime.info dist/Vim/$(VIMRTDIR).info
	mv dist/Vim/$(VIMRTDIR)/runtime/* dist/Vim/$(VIMRTDIR)
	rmdir dist/Vim/$(VIMRTDIR)/runtime
	cd dist && tar cf vim$(VERSION)rt.tar Vim Vim.info
	gzip -9 dist/vim$(VERSION)rt.tar
	mv dist/vim$(VERSION)rt.tar.gz dist/vim$(VERSION)rt.tgz


amibin: dist prepare
	-rm -f dist/vim$(VERSION)bin.tar.gz
	-rm -rf dist/Vim
	mkdir dist/Vim
	mkdir dist/Vim/$(VIMRTDIR)
	tar cf - \
		$(ROOT_AMI) \
		$(BIN_AMI) \
		Vim \
		Xxd \
		| (cd dist/Vim/$(VIMRTDIR); tar xf -)
	-rm $(IN_README_DIR)
	mv dist/Vim/$(VIMRTDIR)/vimdir.info dist/Vim.info
	mv dist/Vim/$(VIMRTDIR)/runtime.info dist/Vim/$(VIMRTDIR).info
	cd dist && tar cf vim$(VERSION)bin.tar Vim Vim.info
	gzip -9 dist/vim$(VERSION)bin.tar
	mv dist/vim$(VERSION)bin.tar.gz dist/vim$(VERSION)bin.tgz


amisrc: dist prepare
	-rm -f dist/vim$(VERSION)src.tar.gz
	-rm -rf dist/Vim
	mkdir dist/Vim
	mkdir dist/Vim/$(VIMRTDIR)
	tar cf - \
		$(ROOT_AMI) \
		$(SRC_ALL) \
		$(SRC_AMI) \
		$(SRC_AMI_DOS) \
		| (cd dist/Vim/$(VIMRTDIR); tar xf -)
	-rm $(IN_README_DIR)
	mv dist/Vim/$(VIMRTDIR)/vimdir.info dist/Vim.info
	mv dist/Vim/$(VIMRTDIR)/runtime.info dist/Vim/$(VIMRTDIR).info
	cd dist && tar cf vim$(VERSION)src.tar Vim Vim.info
	gzip -9 dist/vim$(VERSION)src.tar
	mv dist/vim$(VERSION)src.tar.gz dist/vim$(VERSION)src.tgz

no_title.vim: Makefile
	echo "set notitle noicon nocp nomodeline viminfo=" >no_title.vim


dossrc: dist no_title.vim dist/$(COMMENT_SRC) \
	runtime/doc/uganda.nsis.txt \
	nsis/gvim_version.nsh
	-rm -rf dist/vim$(VERSION)src.zip
	-rm -rf dist/vim
	mkdir dist/vim
	mkdir dist/vim/$(VIMRTDIR)
	tar cf - \
		$(SRC_ALL) \
		$(SRC_DOS) \
		$(SRC_DOS_BIN) \
		$(SRC_AMI_DOS) \
		$(SRC_DOS_UNIX) \
		runtime/doc/uganda.nsis.txt \
		nsis/gvim_version.nsh \
		| (cd dist/vim/$(VIMRTDIR); tar xf -)
	mv dist/vim/$(VIMRTDIR)/runtime/* dist/vim/$(VIMRTDIR)
	rmdir dist/vim/$(VIMRTDIR)/runtime
	
	$(VIM) -e -X -u no_title.vim -c ":set tx|wq" dist/vim/$(VIMRTDIR)/doc/uganda.nsis.txt
	cd dist && zip -9 -rD -z vim$(VERSION)src.zip vim <$(COMMENT_SRC)

runtime/doc/uganda.nsis.txt: runtime/doc/uganda.txt
	cd runtime/doc && $(MAKE) uganda.nsis.txt

nsis/gvim_version.nsh: Makefile
	echo "
	echo "!ifndef __GVIM_VER__NSH__"  >> $@
	echo "!define __GVIM_VER__NSH__"  >> $@
	echo "!define VER_MAJOR $(MAJOR)" >> $@
	echo "!define VER_MINOR $(MINOR)" >> $@
	echo "!endif" >> $@

dosrt: dist dist/$(COMMENT_RT) dosrt_files
	-rm -rf dist/vim$(VERSION)rt.zip
	cd dist && zip -9 -rD -z vim$(VERSION)rt.zip vim <$(COMMENT_RT)



dosrt_files: dist prepare no_title.vim
	-rm -rf dist/vim
	mkdir dist/vim
	mkdir dist/vim/$(VIMRTDIR)
	mkdir dist/vim/$(VIMRTDIR)/lang
	cd src && MAKEMO=yes $(MAKE) languages
	tar cf - \
		$(RT_ALL) \
		| (cd dist/vim/$(VIMRTDIR); tar xf -)
	tar cf - \
		$(RT_SCRIPTS) \
		$(RT_DOS) \
		$(RT_NO_UNIX) \
		$(RT_AMI_DOS) \
		$(LANG_GEN) \
		| (cd dist/vim/$(VIMRTDIR); tar xf -)
	tar cf - \
		$(RT_UNIX_DOS_BIN) \
		$(RT_ALL_BIN) \
		$(RT_DOS_BIN) \
		$(LANG_GEN_BIN) \
		| (cd dist/vim/$(VIMRTDIR); tar xf -)
	-rm $(IN_README_DIR)
	mv dist/vim/$(VIMRTDIR)/runtime/* dist/vim/$(VIMRTDIR)
	rmdir dist/vim/$(VIMRTDIR)/runtime




	for i in $(LANG_DOS); do \
	      if test "$$i" != "src/po/ja.mo" -a "$$i" != "src/po/ja.euc-jp.mo" -a "$$i" != "src/po/pl.mo" -a "$$i" != "src/po/cs.mo" -a "$$i" != "src/po/sk.mo" -a "$$i" != "src/po/zh_CN.mo" -a "$$i" != "src/po/ru.mo" -a "$$i" != "src/po/uk.mo"; then \
		n=`echo $$i | sed -e "s+src/po/\([-a-zA-Z0-9_]*\(.UTF-8\)*\)\(.sjis\)*\(.cp1250\)*\(.cp1251\)*\(.cp936\)*.mo+\1+"`; \
		mkdir dist/vim/$(VIMRTDIR)/lang/$$n; \
		mkdir dist/vim/$(VIMRTDIR)/lang/$$n/LC_MESSAGES; \
		cp $$i dist/vim/$(VIMRTDIR)/lang/$$n/LC_MESSAGES/vim.mo; \
	      fi \
	    done
	mkdir dist/vim/$(VIMRTDIR)/gettext32
	cp gettext32/libintl-8.dll dist/vim/$(VIMRTDIR)/gettext32/
	cp gettext32/libiconv-2.dll dist/vim/$(VIMRTDIR)/gettext32/
	cp gettext32/libgcc_s_sjlj-1.dll dist/vim/$(VIMRTDIR)/gettext32/
	mkdir dist/vim/$(VIMRTDIR)/gettext64
	cp gettext64/libintl-8.dll dist/vim/$(VIMRTDIR)/gettext64/
	cp gettext64/libiconv-2.dll dist/vim/$(VIMRTDIR)/gettext64/



runtime_unix2dos: dosrt_files
	-rm -rf `find runtime/dos -type f -print | sed -e /AAPDIR/d`
	cd dist/vim/$(VIMRTDIR); tar cf - * \
		| (cd ../../../runtime/dos; tar xf -)

dosbin: prepare dosbin_gvim dosbin_w32 dosbin_ole $(DOSBIN_S)
	-rm $(IN_README_DIR)


dosbin_gvim: dist no_title.vim dist/$(COMMENT_GVIM)
	-rm -rf dist/gvim$(VERSION).zip
	-rm -rf dist/vim
	mkdir dist/vim
	mkdir dist/vim/$(VIMRTDIR)
	tar cf - \
		$(BIN_DOS) \
		| (cd dist/vim/$(VIMRTDIR); tar xf -)
	cp gvim.exe dist/vim/$(VIMRTDIR)/gvim.exe
	cp teew32.exe dist/vim/$(VIMRTDIR)/tee.exe
	cp xxdw32.exe dist/vim/$(VIMRTDIR)/xxd.exe
	cp vimrun.exe dist/vim/$(VIMRTDIR)/vimrun.exe
	cp installw32.exe dist/vim/$(VIMRTDIR)/install.exe
	cp uninstallw32.exe dist/vim/$(VIMRTDIR)/uninstall.exe
	mkdir dist/vim/$(VIMRTDIR)/GvimExt32
	cp gvimext.dll dist/vim/$(VIMRTDIR)/GvimExt32/gvimext.dll
	mkdir dist/vim/$(VIMRTDIR)/GvimExt64
	cp gvimext64.dll dist/vim/$(VIMRTDIR)/GvimExt64/gvimext.dll
	cd dist && zip -9 -rD -z gvim$(VERSION).zip vim <$(COMMENT_GVIM)
	cp gvim.pdb dist/gvim$(VERSION).pdb


dosbin_w32: dist no_title.vim dist/$(COMMENT_W32)
	-rm -rf dist/vim$(VERSION)w32.zip
	-rm -rf dist/vim
	mkdir dist/vim
	mkdir dist/vim/$(VIMRTDIR)
	tar cf - \
		$(BIN_DOS) \
		| (cd dist/vim/$(VIMRTDIR); tar xf -)
	cp vimw32.exe dist/vim/$(VIMRTDIR)/vim.exe
	cp teew32.exe dist/vim/$(VIMRTDIR)/tee.exe
	cp xxdw32.exe dist/vim/$(VIMRTDIR)/xxd.exe
	cp installw32.exe dist/vim/$(VIMRTDIR)/install.exe
	cp uninstallw32.exe dist/vim/$(VIMRTDIR)/uninstall.exe
	cd dist && zip -9 -rD -z vim$(VERSION)w32.zip vim <$(COMMENT_W32)
	cp vimw32.pdb dist/vim$(VERSION)w32.pdb


dosbin_ole: dist no_title.vim dist/$(COMMENT_OLE)
	-rm -rf dist/gvim$(VERSION)ole.zip
	-rm -rf dist/vim
	mkdir dist/vim
	mkdir dist/vim/$(VIMRTDIR)
	tar cf - \
		$(BIN_DOS) \
		| (cd dist/vim/$(VIMRTDIR); tar xf -)
	cp gvim_ole.exe dist/vim/$(VIMRTDIR)/gvim.exe
	cp teew32.exe dist/vim/$(VIMRTDIR)/tee.exe
	cp xxdw32.exe dist/vim/$(VIMRTDIR)/xxd.exe
	cp vimrun.exe dist/vim/$(VIMRTDIR)/vimrun.exe
	cp installw32.exe dist/vim/$(VIMRTDIR)/install.exe
	cp uninstallw32.exe dist/vim/$(VIMRTDIR)/uninstall.exe
	cp gvimext.dll dist/vim/$(VIMRTDIR)/gvimext.dll
	cp README_ole.txt dist/vim/$(VIMRTDIR)
	cd dist && zip -9 -rD -z gvim$(VERSION)ole.zip vim <$(COMMENT_OLE)
	cp gvim_ole.pdb dist/gvim$(VERSION)ole.pdb

html: dist dist/$(COMMENT_HTML)
	-rm -rf dist/vim$(VERSION)html.zip
	cd runtime/doc && zip -9 -z ../../dist/vim$(VERSION)html.zip *.html <../../dist/$(COMMENT_HTML)

farsi: dist dist/$(COMMENT_FARSI)
	-rm -f dist/farsi$(VERSION).zip
	zip -9 -rD -z dist/farsi$(VERSION).zip farsi < dist/$(COMMENT_FARSI)
