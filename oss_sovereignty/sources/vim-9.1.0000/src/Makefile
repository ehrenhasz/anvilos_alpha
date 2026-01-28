































































































































































































































































CC=















srcdir = .
VIMNAME = vim
EXNAME = ex
VIEWNAME = view














include auto/config.mk

CClink = $(CC)
















































































































































































































































































































































AUTOCONF ?= autoconf















LINT = lint
LINT_OPTIONS = -beprxzF





























































SANITIZER_LIBS = $(SANITIZER_CFLAGS)























































































































































































































VIMTARGET	= $(VIMNAME)$(EXEEXT)
EXTARGET	= $(EXNAME)$(LNKEXT)
VIEWTARGET	= $(VIEWNAME)$(LNKEXT)
GVIMNAME	= g$(VIMNAME)
GVIMTARGET	= $(GVIMNAME)$(LNKEXT)
GVIEWNAME	= g$(VIEWNAME)
GVIEWTARGET	= $(GVIEWNAME)$(LNKEXT)
RVIMNAME	= r$(VIMNAME)
RVIMTARGET	= $(RVIMNAME)$(LNKEXT)
RVIEWNAME	= r$(VIEWNAME)
RVIEWTARGET	= $(RVIEWNAME)$(LNKEXT)
RGVIMNAME	= r$(GVIMNAME)
RGVIMTARGET	= $(RGVIMNAME)$(LNKEXT)
RGVIEWNAME	= r$(GVIEWNAME)
RGVIEWTARGET	= $(RGVIEWNAME)$(LNKEXT)
VIMDIFFNAME	= $(VIMNAME)diff
GVIMDIFFNAME	= g$(VIMDIFFNAME)
VIMDIFFTARGET	= $(VIMDIFFNAME)$(LNKEXT)
GVIMDIFFTARGET	= $(GVIMDIFFNAME)$(LNKEXT)
EVIMNAME	= e$(VIMNAME)
EVIMTARGET	= $(EVIMNAME)$(LNKEXT)
EVIEWNAME	= e$(VIEWNAME)
EVIEWTARGET	= $(EVIEWNAME)$(LNKEXT)


TOOLS = xxd/xxd$(EXEEXT)
































MAN1DIR = /man1


VIMMAJOR = 9
VIMMINOR = 1



VIMDIR = /vim
VIMRTDIR = /vim$(VIMMAJOR)$(VIMMINOR)
HELPSUBDIR = /doc
COLSUBDIR = /colors
SYNSUBDIR = /syntax
INDSUBDIR = /indent
AUTOSUBDIR = /autoload
IMPORTSUBDIR = /import
PLUGSUBDIR = /plugin
FTPLUGSUBDIR = /ftplugin
LANGSUBDIR = /lang
COMPSUBDIR = /compiler
KMAPSUBDIR = /keymap
MACROSUBDIR = /macros
PACKSUBDIR = /pack
TOOLSSUBDIR = /tools
TUTORSUBDIR = /tutor
SPELLSUBDIR = /spell
PRINTSUBDIR = /print
PODIR = po

























VIMLOC		= $(DATADIR)$(VIMDIR)
VIMRTLOC	= $(DATADIR)$(VIMDIR)$(VIMRTDIR)
VIMRCLOC	= $(VIMLOC)
HELPSUBLOC	= $(VIMRTLOC)$(HELPSUBDIR)
COLSUBLOC	= $(VIMRTLOC)$(COLSUBDIR)
SYNSUBLOC	= $(VIMRTLOC)$(SYNSUBDIR)
INDSUBLOC	= $(VIMRTLOC)$(INDSUBDIR)
AUTOSUBLOC	= $(VIMRTLOC)$(AUTOSUBDIR)
IMPORTSUBLOC	= $(VIMRTLOC)$(IMPORTSUBDIR)
PLUGSUBLOC	= $(VIMRTLOC)$(PLUGSUBDIR)
FTPLUGSUBLOC	= $(VIMRTLOC)$(FTPLUGSUBDIR)
LANGSUBLOC	= $(VIMRTLOC)$(LANGSUBDIR)
COMPSUBLOC	= $(VIMRTLOC)$(COMPSUBDIR)
KMAPSUBLOC	= $(VIMRTLOC)$(KMAPSUBDIR)
MACROSUBLOC	= $(VIMRTLOC)$(MACROSUBDIR)
PACKSUBLOC	= $(VIMRTLOC)$(PACKSUBDIR)
TOOLSSUBLOC	= $(VIMRTLOC)$(TOOLSSUBDIR)
TUTORSUBLOC	= $(VIMRTLOC)$(TUTORSUBDIR)
SPELLSUBLOC	= $(VIMRTLOC)$(SPELLSUBDIR)
PRINTSUBLOC	= $(VIMRTLOC)$(PRINTSUBDIR)
SCRIPTLOC	= $(VIMRTLOC)






VIM_DEFAULTS_FILE = $(DESTDIR)$(SCRIPTLOC)/defaults.vim
EVIM_FILE	= $(DESTDIR)$(SCRIPTLOC)/evim.vim
MSWIN_FILE	= $(DESTDIR)$(SCRIPTLOC)/mswin.vim


SYS_MENU_FILE	= $(DESTDIR)$(SCRIPTLOC)/menu.vim
SYS_SYNMENU_FILE = $(DESTDIR)$(SCRIPTLOC)/synmenu.vim
SYS_DELMENU_FILE = $(DESTDIR)$(SCRIPTLOC)/delmenu.vim


SYS_BUGR_FILE	= $(DESTDIR)$(SCRIPTLOC)/bugreport.vim


SYS_FILETYPE_FILE = $(DESTDIR)$(SCRIPTLOC)/filetype.vim


SYS_FTOFF_FILE	= $(DESTDIR)$(SCRIPTLOC)/ftoff.vim


SYS_SCRIPTS_FILE = $(DESTDIR)$(SCRIPTLOC)/scripts.vim


SYS_FTPLUGIN_FILE = $(DESTDIR)$(SCRIPTLOC)/ftplugin.vim


SYS_FTPLUGOF_FILE = $(DESTDIR)$(SCRIPTLOC)/ftplugof.vim


SYS_INDENT_FILE = $(DESTDIR)$(SCRIPTLOC)/indent.vim


SYS_INDOFF_FILE = $(DESTDIR)$(SCRIPTLOC)/indoff.vim


SYS_OPTWIN_FILE = $(DESTDIR)$(SCRIPTLOC)/optwin.vim


INSTALL_PROG	= cp


INSTALL_DATA	= cp
INSTALL_DATA_R	= cp -r






BINMOD = 755


MANMOD = 644


HELPMOD = 644


SCRIPTMOD = 755


VIMSCRIPTMOD = 644


DIRMOD = 755


FILEMOD = 644


HELPSOURCE = ../runtime/doc


SCRIPTSOURCE = ../runtime


COLSOURCE = ../runtime/colors


SYNSOURCE = ../runtime/syntax


INDSOURCE = ../runtime/indent


AUTOSOURCE = ../runtime/autoload


IMPORTSOURCE = ../runtime/import


PLUGSOURCE = ../runtime/plugin


FTPLUGSOURCE = ../runtime/ftplugin


MACROSOURCE = ../runtime/macros


PACKSOURCE = ../runtime/pack


TOOLSSOURCE = ../runtime/tools


TUTORSOURCE = ../runtime/tutor


SPELLSOURCE = ../runtime/spell


LANGSOURCE = ../runtime/lang


COMPSOURCE = ../runtime/compiler


KMAPSOURCE = ../runtime/keymap


PRINTSOURCE = ../runtime/print






LINKIT = @echo >/dev/null






















NONE_INSTALL = install_normal


GTK_SRC		= gui.c gui_gtk.c gui_gtk_x11.c gui_gtk_f.c \
			gui_beval.c $(GRESOURCE_SRC)
GTK_OBJ		= objects/gui.o objects/gui_gtk.o objects/gui_gtk_x11.o \
			objects/gui_gtk_f.o \
			objects/gui_beval.o $(GRESOURCE_OBJ)
GTK_DEFS	= -DFEAT_GUI_GTK $(NARROW_PROTO)
GTK_IPATH	= $(GUI_INC_LOC)
GTK_LIBS_DIR	= $(GUI_LIB_LOC)
GTK_LIBS1	=
GTK_LIBS2	= $(GTK_LIBNAME)
GTK_INSTALL     = install_normal install_gui_extra
GTK_TARGETS	= installglinks
GTK_MAN_TARGETS = yes
GTK_TESTTARGET  = gui
GTK_BUNDLE	=


MOTIF_SRC	= gui.c gui_motif.c gui_x11.c gui_beval.c \
			gui_xmdlg.c gui_xmebw.c
MOTIF_OBJ	= objects/gui.o objects/gui_motif.o objects/gui_x11.o \
			objects/gui_beval.o \
			objects/gui_xmdlg.o objects/gui_xmebw.o
MOTIF_DEFS	= -DFEAT_GUI_MOTIF $(NARROW_PROTO)
MOTIF_IPATH	= $(GUI_INC_LOC)
MOTIF_LIBS_DIR	= $(GUI_LIB_LOC)
MOTIF_LIBS1	=
MOTIF_LIBS2	= $(MOTIF_LIBNAME) -lXt
MOTIF_INSTALL   = install_normal install_gui_extra
MOTIF_TARGETS	= installglinks
MOTIF_MAN_TARGETS = yes
MOTIF_TESTTARGET = gui
MOTIF_BUNDLE	=













PHOTONGUI_SRC	= gui.c gui_photon.c
PHOTONGUI_OBJ	= objects/gui.o objects/gui_photon.o
PHOTONGUI_DEFS	= -DFEAT_GUI_PHOTON
PHOTONGUI_IPATH	=
PHOTONGUI_LIBS_DIR =
PHOTONGUI_LIBS1	= -lph -lphexlib
PHOTONGUI_LIBS2	=
PHOTONGUI_INSTALL = install_normal install_gui_extra
PHOTONGUI_TARGETS = installglinks
PHOTONGUI_MAN_TARGETS = yes
PHOTONGUI_TESTTARGET = gui
PHOTONGUI_BUNDLE =


HAIKUGUI_SRC	= gui.c gui_haiku.cc
HAIKUGUI_OBJ	= objects/gui.o objects/gui_haiku.o
HAIKUGUI_DEFS	= -DFEAT_GUI_HAIKU
HAIKUGUI_IPATH	=
HAIKUGUI_LIBS_DIR =
HAIKUGUI_LIBS1	= -lbe -lroot -ltracker -ltranslation -lsupc++ -lstdc++
HAIKUGUI_LIBS2	=
HAIKUGUI_INSTALL = install_normal install_haiku_extra
HAIKUGUI_TARGETS	= installglinks_haiku
HAIKUGUI_MAN_TARGETS =
HAIKUGUI_TESTTARGET = gui
HAIKUGUI_BUNDLE =


ALL_GUI_SRC  = gui.c gui_gtk.c gui_gtk_f.c gui_motif.c gui_xmdlg.c gui_xmebw.c gui_gtk_x11.c gui_x11.c gui_haiku.cc
ALL_GUI_PRO  = gui.pro gui_gtk.pro gui_motif.pro gui_xmdlg.pro gui_gtk_x11.pro gui_x11.pro gui_w32.pro gui_photon.pro



TERM_DEPS = \
	libvterm/include/vterm.h \
	libvterm/include/vterm_keycodes.h \
	libvterm/src/rect.h \
	libvterm/src/utf8.h \
	libvterm/src/vterm_internal.h

TERM_SRC = libvterm/src/*.c

XDIFF_SRC = \
	xdiff/xdiffi.c \
	xdiff/xemit.c \
	xdiff/xprepare.c \
	xdiff/xutils.c \
	xdiff/xhistogram.c \
	xdiff/xpatience.c \

XDIFF_OBJS = \
	objects/xdiffi.o \
	objects/xemit.o \
	objects/xprepare.o \
	objects/xutils.o \
	objects/xhistogram.o \
	objects/xpatience.o \

XDIFF_INCL = \
	xdiff/xdiff.h \
	xdiff/xdiffi.h \
	xdiff/xemit.h \
	xdiff/xinclude.h \
	xdiff/xmacros.h \
	xdiff/xprepare.h \
	xdiff/xtypes.h \
	xdiff/xutils.h \







CPP_DEPEND = $(CC) -I$(srcdir) -M$(CPP_MM) \
		`echo "$(DEPEND_CFLAGS)" $(DEPEND_CFLAGS_FILTER)`














NO_ATTR = -D"__attribute\\(x\\)="





PROTO_FLAGS = -d -E"$(CPP)" $(NO_ATTR)






SHELL = /bin/sh




MKDIR_P = $(SHELL) install-sh -c -d

.SUFFIXES:
.SUFFIXES: .c .o .pro

VTERM_CFLAGS = -Ilibvterm/include

PRE_DEFS = -Iproto $(DEFS) $(GUI_DEFS) $(GUI_IPATH) $(CPPFLAGS) $(EXTRA_IPATHS)
POST_DEFS = $(X_CFLAGS) $(MZSCHEME_CFLAGS) $(EXTRA_DEFS)

ALL_CFLAGS = $(PRE_DEFS) $(CFLAGS) $(PROFILE_CFLAGS) $(SANITIZER_CFLAGS) $(LEAK_CFLAGS) $(ABORT_CFLAGS) $(POST_DEFS)

ALL_IF_CFLAGS = $(LUA_CFLAGS) $(PERL_CFLAGS) $(PYTHON_CFLAGS) $(PYTHON3_CFLAGS) $(RUBY_CFLAGS) $(TCL_CFLAGS)
ALL_IF_CFLAGS_EXTRA = $(LUA_CFLAGS_EXTRA) $(PERL_CFLAGS_EXTRA) $(PYTHON_CFLAGS_EXTRA) $(PYTHON3_CFLAGS_EXTRA) $(RUBY_CFLAGS_EXTRA) $(TCL_CFLAGS_EXTRA)



OSDEF_CFLAGS = $(PRE_DEFS) $(POST_DEFS)

LINT_CFLAGS = -DLINT -I. $(PRE_DEFS) $(POST_DEFS) $(ALL_IF_CFLAGS) $(VTERM_CFLAGS) \
	      -Dinline= -D__extension__= -Dalloca=alloca

LINT_EXTRA = -D"__attribute__(x)="

DEPEND_CFLAGS = -DPROTO -DDEPEND -DFEAT_GUI $(LINT_CFLAGS)



ALL_LIB_DIRS = $(GUI_LIBS_DIR) $(X_LIBS_DIR)
ALL_LIBS = \
	   $(GUI_LIBS1) \
	   $(GUI_X_LIBS) \
	   $(GUI_LIBS2) \
	   $(X_PRE_LIBS) \
	   $(X_LIBS) \
	   $(X_EXTRA_LIBS) \
	   $(MZSCHEME_LIBS) \
	   $(LIBS) \
	   $(EXTRA_LIBS) \
	   $(LUA_LIBS) \
	   $(PERL_LIBS) \
	   $(PYTHON_LIBS) \
	   $(PYTHON3_LIBS) \
	   $(TCL_LIBS) \
	   $(RUBY_LIBS) \
	   $(PROFILE_LIBS) \
	   $(SANITIZER_LIBS) \
	   $(LEAK_LIBS)


DEST_BIN = $(DESTDIR)$(BINDIR)
DEST_VIM = $(DESTDIR)$(VIMLOC)
DEST_RT = $(DESTDIR)$(VIMRTLOC)
DEST_HELP = $(DESTDIR)$(HELPSUBLOC)
DEST_COL = $(DESTDIR)$(COLSUBLOC)
DEST_SYN = $(DESTDIR)$(SYNSUBLOC)
DEST_IND = $(DESTDIR)$(INDSUBLOC)
DEST_AUTO = $(DESTDIR)$(AUTOSUBLOC)
DEST_IMPORT = $(DESTDIR)$(IMPORTSUBLOC)
DEST_PLUG = $(DESTDIR)$(PLUGSUBLOC)
DEST_FTP = $(DESTDIR)$(FTPLUGSUBLOC)
DEST_LANG = $(DESTDIR)$(LANGSUBLOC)
DEST_COMP = $(DESTDIR)$(COMPSUBLOC)
DEST_KMAP = $(DESTDIR)$(KMAPSUBLOC)
DEST_MACRO = $(DESTDIR)$(MACROSUBLOC)
DEST_PACK = $(DESTDIR)$(PACKSUBLOC)
DEST_TOOLS = $(DESTDIR)$(TOOLSSUBLOC)
DEST_TUTOR = $(DESTDIR)$(TUTORSUBLOC)
DEST_SPELL = $(DESTDIR)$(SPELLSUBLOC)
DEST_SCRIPT = $(DESTDIR)$(SCRIPTLOC)
DEST_PRINT = $(DESTDIR)$(PRINTSUBLOC)
DEST_MAN_TOP = $(DESTDIR)$(MANDIR)





DEST_MAN = $(DEST_MAN_TOP)$(MAN1DIR)
DEST_MAN_DA = $(DEST_MAN_TOP)/da$(MAN1DIR)
DEST_MAN_DA_I = $(DEST_MAN_TOP)/da.ISO8859-1$(MAN1DIR)
DEST_MAN_DA_U = $(DEST_MAN_TOP)/da.UTF-8$(MAN1DIR)
DEST_MAN_DE = $(DEST_MAN_TOP)/de$(MAN1DIR)
DEST_MAN_DE_I = $(DEST_MAN_TOP)/de.ISO8859-1$(MAN1DIR)
DEST_MAN_DE_U = $(DEST_MAN_TOP)/de.UTF-8$(MAN1DIR)
DEST_MAN_FR = $(DEST_MAN_TOP)/fr$(MAN1DIR)
DEST_MAN_FR_I = $(DEST_MAN_TOP)/fr.ISO8859-1$(MAN1DIR)
DEST_MAN_FR_U = $(DEST_MAN_TOP)/fr.UTF-8$(MAN1DIR)
DEST_MAN_IT = $(DEST_MAN_TOP)/it$(MAN1DIR)
DEST_MAN_IT_I = $(DEST_MAN_TOP)/it.ISO8859-1$(MAN1DIR)
DEST_MAN_IT_U = $(DEST_MAN_TOP)/it.UTF-8$(MAN1DIR)
DEST_MAN_JA_U = $(DEST_MAN_TOP)/ja$(MAN1DIR)
DEST_MAN_PL = $(DEST_MAN_TOP)/pl$(MAN1DIR)
DEST_MAN_PL_I = $(DEST_MAN_TOP)/pl.ISO8859-2$(MAN1DIR)
DEST_MAN_PL_U = $(DEST_MAN_TOP)/pl.UTF-8$(MAN1DIR)
DEST_MAN_RU = $(DEST_MAN_TOP)/ru.KOI8-R$(MAN1DIR)
DEST_MAN_RU_U = $(DEST_MAN_TOP)/ru.UTF-8$(MAN1DIR)
DEST_MAN_TR = $(DEST_MAN_TOP)/tr$(MAN1DIR)
DEST_MAN_TR_I = $(DEST_MAN_TOP)/tr.ISO8859-9$(MAN1DIR)
DEST_MAN_TR_U = $(DEST_MAN_TOP)/tr.UTF-8$(MAN1DIR)


include Make_all.mak


include testdir/Make_all.mak








BASIC_SRC = \
	alloc.c \
	arabic.c \
	arglist.c \
	autocmd.c \
	beval.c \
	blob.c \
	blowfish.c \
	buffer.c \
	change.c \
	charset.c \
	cindent.c \
	clientserver.c \
	clipboard.c \
	cmdexpand.c \
	cmdhist.c \
	crypt.c \
	crypt_zip.c \
	debugger.c \
	dict.c \
	diff.c \
	digraph.c \
	drawline.c \
	drawscreen.c \
	edit.c \
	eval.c \
	evalbuffer.c \
	evalfunc.c \
	evalvars.c \
	evalwindow.c \
	ex_cmds.c \
	ex_cmds2.c \
	ex_docmd.c \
	ex_eval.c \
	ex_getln.c \
	fileio.c \
	filepath.c \
	findfile.c \
	float.c \
	fold.c \
	getchar.c \
	gui_xim.c \
	hardcopy.c \
	hashtab.c \
	help.c \
	highlight.c \
	if_cscope.c \
	if_xcmdsrv.c \
	indent.c \
	insexpand.c \
	json.c \
	list.c \
	locale.c \
	logfile.c \
	main.c \
	map.c \
	mark.c \
	match.c \
	mbyte.c \
	memfile.c \
	memline.c \
	menu.c \
	message.c \
	misc1.c \
	misc2.c \
	mouse.c \
	move.c \
	normal.c \
	ops.c \
	option.c \
	optionstr.c \
	os_unix.c \
	auto/pathdef.c \
	popupmenu.c \
	popupwin.c \
	profiler.c \
	pty.c \
	quickfix.c \
	regexp.c \
	register.c \
	screen.c \
	scriptfile.c \
	search.c \
	session.c \
	sha256.c \
	sign.c \
	sound.c \
	spell.c \
	spellfile.c \
	spellsuggest.c \
	strings.c \
	syntax.c \
	tag.c \
	term.c \
	terminal.c \
	testing.c \
	textformat.c \
	textobject.c \
	textprop.c \
	time.c \
	typval.c \
	ui.c \
	undo.c \
	usercmd.c \
	userfunc.c \
	version.c \
	vim9class.c \
	vim9cmds.c \
	vim9compile.c \
	vim9execute.c \
	vim9expr.c \
	vim9instr.c \
	vim9script.c \
	vim9type.c \
	viminfo.c \
	window.c \
	bufwrite.c \
	$(OS_EXTRA_SRC)

SRC =	$(BASIC_SRC) \
	$(GUI_SRC) \
	$(TERM_SRC) \
	$(XDIFF_SRC) \
	$(LUA_SRC) \
	$(MZSCHEME_SRC) \
	$(PERL_SRC) \
	$(PYTHON_SRC) $(PYTHON3_SRC) \
	$(TCL_SRC) \
	$(RUBY_SRC)

EXTRA_SRC = if_lua.c if_mzsch.c auto/if_perl.c if_perlsfio.c \
	    if_python.c if_python3.c if_tcl.c if_ruby.c \
	    gui_beval.c netbeans.c job.c channel.c \
	    $(GRESOURCE_SRC)


JSON_TEST_SRC = json_test.c
JSON_TEST_TARGET = json_test$(EXEEXT)
KWORD_TEST_SRC = kword_test.c
KWORD_TEST_TARGET = kword_test$(EXEEXT)
MEMFILE_TEST_SRC = memfile_test.c
MEMFILE_TEST_TARGET = memfile_test$(EXEEXT)
MESSAGE_TEST_SRC = message_test.c
MESSAGE_TEST_TARGET = message_test$(EXEEXT)

UNITTEST_SRC = $(JSON_TEST_SRC) $(KWORD_TEST_SRC) $(MEMFILE_TEST_SRC) $(MESSAGE_TEST_SRC)
UNITTEST_TARGETS = $(JSON_TEST_TARGET) $(KWORD_TEST_TARGET) $(MEMFILE_TEST_TARGET) $(MESSAGE_TEST_TARGET)
RUN_UNITTESTS = run_json_test run_kword_test run_memfile_test run_message_test


ALL_LOCAL_SRC = $(BASIC_SRC) $(ALL_GUI_SRC) $(UNITTEST_SRC) $(EXTRA_SRC)
ALL_SRC = $(ALL_LOCAL_SRC) $(TERM_SRC) $(XDIFF_SRC)




LINT_SRC = $(BASIC_SRC) $(GUI_SRC) \
	   $(PYTHON_SRC) $(PYTHON3_SRC) $(TCL_SRC) \
	   $(NETBEANS_SRC) $(CHANNEL_SRC) $(TERM_SRC)




OBJ_COMMON = \
	objects/alloc.o \
	objects/arabic.o \
	objects/arglist.o \
	objects/autocmd.o \
	objects/beval.o \
	objects/buffer.o \
	objects/change.o \
	objects/blob.o \
	objects/blowfish.o \
	objects/cindent.o \
	objects/clientserver.o \
	objects/clipboard.o \
	objects/cmdexpand.o \
	objects/cmdhist.o \
	objects/crypt.o \
	objects/crypt_zip.o \
	objects/debugger.o \
	objects/dict.o \
	objects/diff.o \
	objects/digraph.o \
	objects/drawline.o \
	objects/drawscreen.o \
	objects/edit.o \
	objects/eval.o \
	objects/evalbuffer.o \
	objects/evalfunc.o \
	objects/evalvars.o \
	objects/evalwindow.o \
	objects/ex_cmds.o \
	objects/ex_cmds2.o \
	objects/ex_docmd.o \
	objects/ex_eval.o \
	objects/ex_getln.o \
	objects/fileio.o \
	objects/filepath.o \
	objects/findfile.o \
	objects/float.o \
	objects/fold.o \
	objects/getchar.o \
	objects/gui_xim.o \
	objects/hardcopy.o \
	objects/hashtab.o \
	objects/help.o \
	objects/highlight.o \
	objects/if_cscope.o \
	objects/if_xcmdsrv.o \
	objects/indent.o \
	objects/insexpand.o \
	objects/list.o \
	objects/locale.o \
	objects/logfile.o \
	objects/map.o \
	objects/mark.o \
	objects/match.o \
	objects/mbyte.o \
	objects/memline.o \
	objects/menu.o \
	objects/misc1.o \
	objects/misc2.o \
	objects/mouse.o \
	objects/move.o \
	objects/normal.o \
	objects/ops.o \
	objects/option.o \
	objects/optionstr.o \
	objects/os_unix.o \
	objects/pathdef.o \
	objects/popupmenu.o \
	objects/popupwin.o \
	objects/profiler.o \
	objects/pty.o \
	objects/quickfix.o \
	objects/regexp.o \
	objects/register.o \
	objects/screen.o \
	objects/scriptfile.o \
	objects/search.o \
	objects/session.o \
	objects/sha256.o \
	objects/sign.o \
	objects/sound.o \
	objects/spell.o \
	objects/spellfile.o \
	objects/spellsuggest.o \
	objects/strings.o \
	objects/syntax.o \
	objects/tag.o \
	objects/term.o \
	objects/terminal.o \
	objects/testing.o \
	objects/textformat.o \
	objects/textobject.o \
	objects/textprop.o \
	objects/time.o \
	objects/typval.o \
	objects/ui.o \
	objects/undo.o \
	objects/usercmd.o \
	objects/userfunc.o \
	objects/version.o \
	objects/vim9class.o \
	objects/vim9cmds.o \
	objects/vim9compile.o \
	objects/vim9execute.o \
	objects/vim9expr.o \
	objects/vim9instr.o \
	objects/vim9script.o \
	objects/vim9type.o \
	objects/viminfo.o \
	objects/window.o \
	objects/bufwrite.o \
	$(GUI_OBJ) \
	$(TERM_OBJ) \
	$(LUA_OBJ) \
	$(MZSCHEME_OBJ) \
	$(PERL_OBJ) \
	$(PYTHON_OBJ) \
	$(PYTHON3_OBJ) \
	$(TCL_OBJ) \
	$(RUBY_OBJ) \
	$(OS_EXTRA_OBJ) \
	$(NETBEANS_OBJ) \
	$(CHANNEL_OBJ) \
	$(XDIFF_OBJS_USED)


OBJ_MAIN = \
	objects/charset.o \
	objects/json.o \
	objects/main.o \
	objects/memfile.o \
	objects/message.o

OBJ = $(OBJ_COMMON) $(OBJ_MAIN)

OBJ_JSON_TEST = \
	objects/charset.o \
	objects/memfile.o \
	objects/message.o \
	objects/json_test.o

JSON_TEST_OBJ = $(OBJ_COMMON) $(OBJ_JSON_TEST)

OBJ_KWORD_TEST = \
	objects/json.o \
	objects/memfile.o \
	objects/message.o \
	objects/kword_test.o

KWORD_TEST_OBJ = $(OBJ_COMMON) $(OBJ_KWORD_TEST)

OBJ_MEMFILE_TEST = \
	objects/charset.o \
	objects/json.o \
	objects/message.o \
	objects/memfile_test.o

MEMFILE_TEST_OBJ = $(OBJ_COMMON) $(OBJ_MEMFILE_TEST)

OBJ_MESSAGE_TEST = \
	objects/charset.o \
	objects/json.o \
	objects/memfile.o \
	objects/message_test.o

MESSAGE_TEST_OBJ = $(OBJ_COMMON) $(OBJ_MESSAGE_TEST)

ALL_OBJ = $(OBJ_COMMON) \
	  $(OBJ_MAIN) \
	  $(OBJ_JSON_TEST) \
	  $(OBJ_KWORD_TEST) \
	  $(OBJ_MEMFILE_TEST) \
	  $(OBJ_MESSAGE_TEST)


PRO_AUTO = \
	alloc.pro \
	arabic.pro \
	arglist.pro \
	autocmd.pro \
	beval.pro \
	blowfish.pro \
	buffer.pro \
	bufwrite.pro \
	change.pro \
	channel.pro \
	charset.pro \
	cindent.pro \
	clientserver.pro \
	clipboard.pro \
	cmdexpand.pro \
	cmdhist.pro \
	crypt.pro \
	crypt_zip.pro \
	debugger.pro \
	dict.pro \
	diff.pro \
	digraph.pro \
	drawline.pro \
	drawscreen.pro \
	edit.pro \
	eval.pro \
	evalbuffer.pro \
	evalfunc.pro \
	evalvars.pro \
	evalwindow.pro \
	ex_cmds.pro \
	ex_cmds2.pro \
	ex_docmd.pro \
	ex_eval.pro \
	ex_getln.pro \
	fileio.pro \
	filepath.pro \
	findfile.pro \
	float.pro \
	fold.pro \
	getchar.pro \
	gui_xim.pro \
	gui_beval.pro \
	hardcopy.pro \
	hashtab.pro \
	help.pro \
	highlight.pro \
	if_cscope.pro \
	if_lua.pro \
	if_mzsch.pro \
	if_python.pro \
	if_python3.pro \
	if_ruby.pro \
	if_xcmdsrv.pro \
	indent.pro \
	insexpand.pro \
	job.pro \
	json.pro \
	list.pro \
	locale.pro \
	logfile.pro \
	main.pro \
	map.pro \
	mark.pro \
	match.pro \
	mbyte.pro \
	memfile.pro \
	memline.pro \
	menu.pro \
	message.pro \
	misc1.pro \
	misc2.pro \
	mouse.pro \
	move.pro \
	netbeans.pro \
	normal.pro \
	ops.pro \
	option.pro \
	optionstr.pro \
	os_mac_conv.pro \
	os_unix.pro \
	popupmenu.pro \
	popupwin.pro \
	profiler.pro \
	pty.pro \
	quickfix.pro \
	regexp.pro \
	register.pro \
	screen.pro \
	scriptfile.pro \
	search.pro \
	session.pro \
	sha256.pro \
	sign.pro \
	sound.pro \
	spell.pro \
	spellfile.pro \
	spellsuggest.pro \
	strings.pro \
	syntax.pro \
	tag.pro \
	term.pro \
	terminal.pro \
	termlib.pro \
	testing.pro \
	textformat.pro \
	textobject.pro \
	textprop.pro \
	time.pro \
	typval.pro \
	ui.pro \
	undo.pro \
	usercmd.pro \
	userfunc.pro \
	version.pro \
	vim9class.pro \
	vim9cmds.pro \
	vim9compile.pro \
	vim9execute.pro \
	vim9expr.pro \
	vim9instr.pro \
	vim9script.pro \
	vim9type.pro \
	viminfo.pro \
	window.pro \
	$(ALL_GUI_PRO) \
	$(TCL_PRO)

PRO_MANUAL = os_amiga.pro os_win32.pro \
	os_mswin.pro winclip.pro os_vms.pro $(PERL_PRO)


all: $(VIMTARGET) $(TOOLS) languages $(GUI_BUNDLE)

tools: $(TOOLS)













config auto/config.mk: auto/configure config.mk.in config.h.in
	-rm -f auto/config.cache
	if test "X$(MAKECMDGOALS)" != "Xclean" \
		-a "X$(MAKECMDGOALS)" != "Xdistclean" \
		-a "X$(MAKECMDGOALS)" != "Xautoconf" \
		-a "X$(MAKECMDGOALS)" != "Xreconfig"; then \
	    GUI_INC_LOC="$(GUI_INC_LOC)" GUI_LIB_LOC="$(GUI_LIB_LOC)" \
		CC="$(CC)" CPPFLAGS="$(CPPFLAGS)" CFLAGS="$(CFLAGS)" \
		LDFLAGS="$(LDFLAGS)" $(CONF_SHELL) srcdir="$(srcdir)" \
		./configure $(CONF_OPT_GUI) $(CONF_OPT_X) $(CONF_OPT_XSMP) \
		$(CONF_OPT_AUTOSERVE) $(CONF_OPT_DARWIN) $(CONF_OPT_FAIL) \
		$(CONF_OPT_PERL) $(CONF_OPT_PYTHON) $(CONF_OPT_PYTHON3) \
		$(CONF_OPT_TCL) $(CONF_OPT_RUBY) $(CONF_OPT_NLS) \
		$(CONF_OPT_CSCOPE) $(CONF_OPT_MULTIBYTE) $(CONF_OPT_INPUT) \
		$(CONF_OPT_OUTPUT) $(CONF_OPT_GPM) \
		$(CONF_OPT_FEAT) $(CONF_TERM_LIB) \
		$(CONF_OPT_COMPBY) $(CONF_OPT_ACL) $(CONF_OPT_NETBEANS) \
		$(CONF_OPT_CHANNEL) $(CONF_OPT_TERMINAL) \
		$(CONF_ARGS) $(CONF_ARGS1) $(CONF_ARGS2) $(CONF_ARGS3) \
		$(CONF_ARGS4) $(CONF_ARGS5) $(CONF_ARGS6) \
		$(CONF_OPT_MZSCHEME) $(CONF_OPT_PLTHOME) \
		$(CONF_OPT_LUA) $(CONF_OPT_LUA_PREFIX) \
		$(CONF_OPT_SYSMOUSE) $(CONF_OPT_CANBERRA) $(CONF_OPT_SODIUM); \
	fi





reconfig: scratch clean
	$(MAKE) -f Makefile config
	$(MAKE) -f Makefile all

















autoconf:
	if test ! -f configure.save; then mv configure configure.save; fi
	$(AUTOCONF)
	sed -e 's+>config.log+>auto/config.log+' -e 's+\./config.log+auto/config.log+' configure > auto/configure
	chmod 755 auto/configure
	mv -f configure.save configure
	-rm -rf autom4te.cache
	-rm -f auto/config.status auto/config.cache






cmdidxs: ex_cmds.h
	vim --clean -X --not-a-term -S create_cmdidxs.vim -c quit








nvcmdidxs: auto/config.mk nv_cmds.h
	$(CC) -I$(srcdir) $(ALL_CFLAGS) create_nvcmdidxs.c -o create_nvcmdidxs
	vim --clean -X --not-a-term -S create_nvcmdidxs.vim -c quit
	-rm -f create_nvcmdidxs



CCC_NF = $(CC) -c -I$(srcdir)
CCC = $(CCC_NF) $(ALL_CFLAGS)




$(VIMTARGET): auto/config.mk $(OBJ) objects/version.o
	@$(BUILD_DATE_MSG)
	@LINK="$(PURIFY) $(SHRPENV) $(CClink) $(ALL_LIB_DIRS) $(LDFLAGS) \
		-o $(VIMTARGET) $(OBJ) $(ALL_LIBS)" \
		MAKE="$(MAKE)" LINK_AS_NEEDED=$(LINK_AS_NEEDED) \
		PROG="vim" \
		sh $(srcdir)/link.sh

xxd/xxd$(EXEEXT): xxd/xxd.c
	cd xxd; CC="$(CC)" CFLAGS="$(CPPFLAGS) $(CFLAGS)" LDFLAGS="$(LDFLAGS)" \
		$(MAKE) -f Makefile



languages:
	@if test -n "$(MAKEMO)" -a -f $(PODIR)/Makefile; then \
		cd $(PODIR); \
		  CC="$(CC)" $(MAKE) prefix=$(DESTDIR)$(prefix); \
	fi
	-@if test -n "$(MAKEMO)" -a -f $(PODIR)/Makefile; then \
		cd $(PODIR); \
		  CC="$(CC)" $(MAKE) prefix=$(DESTDIR)$(prefix) converted; \
	fi


update-po:
	cd $(PODIR); CC="$(CC)" $(MAKE) prefix=$(DESTDIR)$(prefix) update-po







proto: $(PRO_AUTO) $(PRO_MANUAL)







CPROTO = cproto $(PROTO_FLAGS) -DPROTO \
	 `echo '$(LINT_CFLAGS)' | sed -e 's/ -[a-z-]\+//g' -e 's/ -O[^ ]\+//g'`








.c.pro:
	$(CPROTO) -DFEAT_GUI $< > proto/$@
	echo "/* vim: set ft=c : */" >> proto/$@

os_amiga.pro: os_amiga.c
	$(CPROTO) -DAMIGA -UHAVE_CONFIG_H -DBPTR=char* $< > proto/$@
	echo "/* vim: set ft=c : */" >> proto/$@

os_win32.pro: os_win32.c
	$(CPROTO) -DWIN32 -UHAVE_CONFIG_H $< > proto/$@
	echo "/* vim: set ft=c : */" >> proto/$@

os_mswin.pro: os_mswin.c
	$(CPROTO) -DWIN32 -UHAVE_CONFIG_H $< > proto/$@
	echo "/* vim: set ft=c : */" >> proto/$@

winclip.pro: winclip.c
	$(CPROTO) -DWIN32 -UHAVE_CONFIG_H $< > proto/$@
	echo "/* vim: set ft=c : */" >> proto/$@

os_vms.pro: os_vms.c

	mv auto/config.h auto/config.h.save
	cp os_vms_conf.h auto/config.h
	$(CPROTO) -DVMS -UFEAT_GUI_MOTIF -UFEAT_GUI_GTK $< > proto/$@
	echo "/* vim: set ft=c : */" >> proto/$@
	rm auto/config.h
	mv auto/config.h.save auto/config.h



if_perl.pro: auto/if_perl.c
	$(CPROTO) -DFEAT_GUI auto/if_perl.c | sed "/_VI/d" > proto/$@

gui_gtk_gresources.pro: auto/gui_gtk_gresources.c
	$(CPROTO) -DFEAT_GUI $< > proto/$@
	echo "/* vim: set ft=c : */" >> proto/$@

notags:
	-rm -f tags



tags TAGS: notags
	$(TAGPRG) $(TAGS_FILES)



.PHONY: cscope csclean
csclean:
	-rm -vf cscope.out
cscope.out:
	cscope -bv ./*.[ch] ./*.cpp ./if_perl.xs auto/*.h auto/pathdef.c proto/*.pro xxd/xxd.c
cscope: csclean cscope.out  ;


types: types.vim
types.vim: $(TAGS_FILES)
	ctags --c-kinds=gstu -o- $(TAGS_FILES) |\
		awk 'BEGIN{printf("syntax keyword Type\t")}\
			{printf("%s ", $$1)}END{print ""}' > $@
	echo "syn keyword Constant OK FAIL TRUE FALSE MAYBE" >> $@





test check: unittests $(TERM_TEST) scripttests










scripttests:
	$(MAKE) -f Makefile $(VIMTARGET)
	if test -n "$(MAKEMO)" -a -f $(PODIR)/Makefile; then \
		cd $(PODIR); $(MAKE) -f Makefile check VIM=../$(VIMTARGET); \
	fi
	-if test $(VIMTARGET) != vim -a ! -r vim; then \
		ln -s $(VIMTARGET) vim; \
	fi
	cd testdir; $(MAKE) -f Makefile $(GUI_TESTTARGET) VIMPROG=../$(VIMTARGET) $(GUI_TESTARG) SCRIPTSOURCE=../$(SCRIPTSOURCE)


testgui:
	cd testdir; $(MAKE) -f Makefile $(GUI_TESTTARGET) VIMPROG=../$(VIMTARGET) GUI_FLAG=-g $(GUI_TESTARG) SCRIPTSOURCE=../$(SCRIPTSOURCE)

testtiny:
	cd testdir; $(MAKE) -f Makefile tiny VIMPROG=../$(VIMTARGET) SCRIPTSOURCE=../$(SCRIPTSOURCE)


benchmark:
	cd testdir; \
		$(MAKE) -f Makefile benchmarkclean; \
		$(MAKE) -f Makefile benchmark VIMPROG=../$(VIMTARGET) SCRIPTSOURCE=../$(SCRIPTSOURCE)

unittesttargets:
	$(MAKE) -f Makefile $(UNITTEST_TARGETS)


VIMTESTTARGET = $(VIMTARGET)



unittest unittests: $(RUN_UNITTESTS)

run_json_test: $(JSON_TEST_TARGET)
	$(VALGRIND) ./$(JSON_TEST_TARGET) || exit 1; echo $* passed;

run_kword_test: $(KWORD_TEST_TARGET)
	$(VALGRIND) ./$(KWORD_TEST_TARGET) || exit 1; echo $* passed;

run_memfile_test: $(MEMFILE_TEST_TARGET)
	$(VALGRIND) ./$(MEMFILE_TEST_TARGET) || exit 1; echo $* passed;

run_message_test: $(MESSAGE_TEST_TARGET)
	$(VALGRIND) ./$(MESSAGE_TEST_TARGET) || exit 1; echo $* passed;




test_libvterm:
	@if $(MAKE) --version 2>/dev/null | grep -qs "GNU Make"; then \
	  if test -x "/usr/bin/gcc"; then \
	    cd libvterm; $(MAKE) -f Makefile test CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"; \
	  fi \
	fi



$(SCRIPTS_TINY):
	cd testdir; rm -f $@.out; $(MAKE) -f Makefile $@.out VIMPROG=../$(VIMTESTTARGET) $(GUI_TESTARG) SCRIPTSOURCE=../$(SCRIPTSOURCE)







$(NEW_TESTS) test_vim9:
	cd testdir; $(MAKE) $@ VIMPROG=../$(VIMTESTTARGET) $(GUI_TESTARG) SCRIPTSOURCE=../$(SCRIPTSOURCE)

newtests:
	cd testdir; rm -f $@.res test.log messages; $(MAKE) -f Makefile newtestssilent VIMPROG=../$(VIMTESTTARGET) $(GUI_TESTARG) SCRIPTSOURCE=../$(SCRIPTSOURCE)
	@if test -f testdir/test.log; then \
		cat testdir/test.log; \
	fi
	cat testdir/messages

testclean:
	cd testdir; $(MAKE) -f Makefile clean
	if test -d $(PODIR); then \
		cd $(PODIR); $(MAKE) checkclean; \
	fi



$(JSON_TEST_TARGET): auto/config.mk $(JSON_TEST_OBJ) objects/version.o
	@LINK="$(PURIFY) $(SHRPENV) $(CClink) $(ALL_LIB_DIRS) $(LDFLAGS) \
		-o $(JSON_TEST_TARGET) $(JSON_TEST_OBJ) $(ALL_LIBS)" \
		MAKE="$(MAKE)" LINK_AS_NEEDED=$(LINK_AS_NEEDED) \
		PROG="json_test" \
		sh $(srcdir)/link.sh

$(KWORD_TEST_TARGET): auto/config.mk $(KWORD_TEST_OBJ) objects/version.o
	@LINK="$(PURIFY) $(SHRPENV) $(CClink) $(ALL_LIB_DIRS) $(LDFLAGS) \
		-o $(KWORD_TEST_TARGET) $(KWORD_TEST_OBJ) $(ALL_LIBS)" \
		MAKE="$(MAKE)" LINK_AS_NEEDED=$(LINK_AS_NEEDED) \
		PROG="kword_test" \
		sh $(srcdir)/link.sh

$(MEMFILE_TEST_TARGET): auto/config.mk $(MEMFILE_TEST_OBJ) objects/version.o
	@LINK="$(PURIFY) $(SHRPENV) $(CClink) $(ALL_LIB_DIRS) $(LDFLAGS) \
		-o $(MEMFILE_TEST_TARGET) $(MEMFILE_TEST_OBJ) $(ALL_LIBS)" \
		MAKE="$(MAKE)" LINK_AS_NEEDED=$(LINK_AS_NEEDED) \
		PROG="memfile_test" \
		sh $(srcdir)/link.sh

$(MESSAGE_TEST_TARGET): auto/config.mk $(MESSAGE_TEST_OBJ) objects/version.o
	@LINK="$(PURIFY) $(SHRPENV) $(CClink) $(ALL_LIB_DIRS) $(LDFLAGS) \
		-o $(MESSAGE_TEST_TARGET) $(MESSAGE_TEST_OBJ) $(ALL_LIBS)" \
		MAKE="$(MAKE)" LINK_AS_NEEDED=$(LINK_AS_NEEDED) \
		PROG="message_test" \
		sh $(srcdir)/link.sh



install: $(GUI_INSTALL)

install_normal: installvim installtools $(INSTALL_LANGS) install-icons

install_gui_extra: installgtutorbin

installvim: installvimbin installtutorbin \
		installruntime installlinks installmanlinks










installvimbin: $(VIMTARGET) $(DESTDIR)$(exec_prefix) $(DEST_BIN)
	-if test -f $(DEST_BIN)/$(VIMTARGET); then \
	  mv -f $(DEST_BIN)/$(VIMTARGET) $(DEST_BIN)/$(VIMNAME).rm; \
	  rm -f $(DEST_BIN)/$(VIMNAME).rm; \
	fi
	$(INSTALL_PROG) $(VIMTARGET) $(DEST_BIN)
	$(STRIP) $(DEST_BIN)/$(VIMTARGET)
	chmod $(BINMOD) $(DEST_BIN)/$(VIMTARGET)

	-$(LINKIT)



INSTALLMANARGS = $(VIMLOC) $(SCRIPTLOC) $(VIMRCLOC) $(HELPSOURCE) $(MANMOD) \
		$(VIMNAME) $(VIMDIFFNAME) $(EVIMNAME)


installruntime: installrtbase installmacros installpack installtutor installspell



installrtbase: $(HELPSOURCE)/vim.1 $(DEST_VIM) $(VIMTARGET) $(DEST_RT) \
		$(DEST_HELP) $(DEST_PRINT) $(DEST_COL) \
		$(DEST_SYN) $(DEST_SYN)/shared $(DEST_IND) $(DEST_FTP) \
		$(DEST_AUTO) $(DEST_AUTO)/dist $(DEST_AUTO)/xml $(DEST_AUTO)/zig \
		$(DEST_IMPORT) $(DEST_IMPORT)/dist \
		$(DEST_PLUG) $(DEST_TUTOR) $(DEST_SPELL) $(DEST_COMP)
	-$(SHELL) ./installman.sh install $(DEST_MAN) "" $(INSTALLMANARGS)



	cd $(HELPSOURCE); if test -z "$(CROSS_COMPILING)" -a -f tags; then \
		mv -f tags tags.dist; fi
	@echo generating help tags
	
	
	-@BUILD_DIR="`pwd`"; cd $(HELPSOURCE); if test -z "$(CROSS_COMPILING)"; then \
		$(MAKE) VIMEXE="$$BUILD_DIR/$(VIMTARGET)" vimtags; fi
	cd $(HELPSOURCE); \
		files=`ls *.txt tags`; \
		files="$$files `ls *.??x tags-?? 2>/dev/null || true`"; \
		$(INSTALL_DATA) $$files  $(DEST_HELP); \
		cd $(DEST_HELP); \
		chmod $(HELPMOD) $$files
	$(INSTALL_DATA)  $(HELPSOURCE)/*.pl $(DEST_HELP)
	chmod $(SCRIPTMOD) $(DEST_HELP)/*.pl
	cd $(HELPSOURCE); if test -f tags.dist; then mv -f tags.dist tags; fi

	$(INSTALL_DATA) $(SCRIPTSOURCE)/menu.vim $(SYS_MENU_FILE)
	chmod $(VIMSCRIPTMOD) $(SYS_MENU_FILE)
	$(INSTALL_DATA) $(SCRIPTSOURCE)/synmenu.vim $(SYS_SYNMENU_FILE)
	chmod $(VIMSCRIPTMOD) $(SYS_SYNMENU_FILE)
	$(INSTALL_DATA) $(SCRIPTSOURCE)/delmenu.vim $(SYS_DELMENU_FILE)
	chmod $(VIMSCRIPTMOD) $(SYS_DELMENU_FILE)

	$(INSTALL_DATA) $(SCRIPTSOURCE)/defaults.vim $(VIM_DEFAULTS_FILE)
	chmod $(VIMSCRIPTMOD) $(VIM_DEFAULTS_FILE)
	$(INSTALL_DATA) $(SCRIPTSOURCE)/evim.vim $(EVIM_FILE)
	chmod $(VIMSCRIPTMOD) $(EVIM_FILE)
	$(INSTALL_DATA) $(SCRIPTSOURCE)/mswin.vim $(MSWIN_FILE)
	chmod $(VIMSCRIPTMOD) $(MSWIN_FILE)

	$(INSTALL_DATA) $(SCRIPTSOURCE)/bugreport.vim $(SYS_BUGR_FILE)
	chmod $(VIMSCRIPTMOD) $(SYS_BUGR_FILE)

	$(INSTALL_DATA) $(SCRIPTSOURCE)/vimrc_example.vim $(DEST_SCRIPT)
	chmod $(VIMSCRIPTMOD) $(DEST_SCRIPT)/vimrc_example.vim
	$(INSTALL_DATA) $(SCRIPTSOURCE)/gvimrc_example.vim $(DEST_SCRIPT)
	chmod $(VIMSCRIPTMOD) $(DEST_SCRIPT)/gvimrc_example.vim

	$(INSTALL_DATA) $(SCRIPTSOURCE)/filetype.vim $(SYS_FILETYPE_FILE)
	chmod $(VIMSCRIPTMOD) $(SYS_FILETYPE_FILE)
	$(INSTALL_DATA) $(SCRIPTSOURCE)/ftoff.vim $(SYS_FTOFF_FILE)
	chmod $(VIMSCRIPTMOD) $(SYS_FTOFF_FILE)
	$(INSTALL_DATA) $(SCRIPTSOURCE)/scripts.vim $(SYS_SCRIPTS_FILE)
	chmod $(VIMSCRIPTMOD) $(SYS_SCRIPTS_FILE)
	$(INSTALL_DATA) $(SCRIPTSOURCE)/ftplugin.vim $(SYS_FTPLUGIN_FILE)
	chmod $(VIMSCRIPTMOD) $(SYS_FTPLUGIN_FILE)
	$(INSTALL_DATA) $(SCRIPTSOURCE)/ftplugof.vim $(SYS_FTPLUGOF_FILE)
	chmod $(VIMSCRIPTMOD) $(SYS_FTPLUGOF_FILE)
	$(INSTALL_DATA) $(SCRIPTSOURCE)/indent.vim $(SYS_INDENT_FILE)
	chmod $(VIMSCRIPTMOD) $(SYS_INDENT_FILE)
	$(INSTALL_DATA) $(SCRIPTSOURCE)/indoff.vim $(SYS_INDOFF_FILE)
	chmod $(VIMSCRIPTMOD) $(SYS_INDOFF_FILE)
	$(INSTALL_DATA) $(SCRIPTSOURCE)/optwin.vim $(SYS_OPTWIN_FILE)
	chmod $(VIMSCRIPTMOD) $(SYS_OPTWIN_FILE)

	cd $(PRINTSOURCE); $(INSTALL_DATA) *.ps $(DEST_PRINT)
	cd $(DEST_PRINT); chmod $(FILEMOD) *.ps

	cd $(COLSOURCE); $(INSTALL_DATA_R) *.vim lists tools README.txt $(DEST_COL)
	cd $(DEST_COL); chmod $(DIRMOD) lists tools
	cd $(DEST_COL); chmod $(HELPMOD) *.vim README.txt lists/*.vim tools/*.vim

	cd $(SYNSOURCE); $(INSTALL_DATA) *.vim README.txt $(DEST_SYN)
	cd $(DEST_SYN); chmod $(HELPMOD) *.vim README.txt
	cd $(SYNSOURCE)/shared; $(INSTALL_DATA) *.vim README.txt $(DEST_SYN)/shared
	cd $(DEST_SYN)/shared; chmod $(HELPMOD) *.vim README.txt

	cd $(INDSOURCE); $(INSTALL_DATA) *.vim README.txt $(DEST_IND)
	cd $(DEST_IND); chmod $(HELPMOD) *.vim README.txt

	cd $(AUTOSOURCE); $(INSTALL_DATA) *.vim README.txt $(DEST_AUTO)
	cd $(DEST_AUTO); chmod $(HELPMOD) *.vim README.txt
	cd $(AUTOSOURCE)/dist; $(INSTALL_DATA) *.vim $(DEST_AUTO)/dist
	cd $(DEST_AUTO)/dist; chmod $(HELPMOD) *.vim
	cd $(AUTOSOURCE)/xml; $(INSTALL_DATA) *.vim $(DEST_AUTO)/xml
	cd $(DEST_AUTO)/xml; chmod $(HELPMOD) *.vim
	cd $(AUTOSOURCE)/zig; $(INSTALL_DATA) *.vim $(DEST_AUTO)/zig
	cd $(DEST_AUTO)/zig; chmod $(HELPMOD) *.vim

	cd $(IMPORTSOURCE)/dist; $(INSTALL_DATA) *.vim $(DEST_IMPORT)/dist
	cd $(DEST_IMPORT)/dist; chmod $(HELPMOD) *.vim

	cd $(PLUGSOURCE); $(INSTALL_DATA) *.vim README.txt $(DEST_PLUG)
	cd $(DEST_PLUG); chmod $(HELPMOD) *.vim README.txt

	cd $(FTPLUGSOURCE); $(INSTALL_DATA) *.vim README.txt logtalk.dict $(DEST_FTP)
	cd $(DEST_FTP); chmod $(HELPMOD) *.vim README.txt logtalk.dict

	cd $(COMPSOURCE); $(INSTALL_DATA) *.vim README.txt $(DEST_COMP)
	cd $(DEST_COMP); chmod $(HELPMOD) *.vim README.txt

installmacros: $(DEST_VIM) $(DEST_RT) $(DEST_MACRO)
	$(INSTALL_DATA_R) $(MACROSOURCE)/* $(DEST_MACRO)
	chmod $(DIRMOD) `find $(DEST_MACRO) -type d -print`
	chmod $(FILEMOD) `find $(DEST_MACRO) -type f -print`
	chmod $(SCRIPTMOD) $(DEST_MACRO)/less.sh


	cvs=`find $(DEST_MACRO) \( -name CVS -o -name AAPDIR -o -name "*.info" \) -print`; \
	      if test -n "$$cvs"; then \
		 rm -rf $$cvs; \
	      fi

installpack: $(DEST_VIM) $(DEST_RT) $(DEST_PACK)
	$(INSTALL_DATA_R) $(PACKSOURCE)/* $(DEST_PACK)
	chmod $(DIRMOD) `find $(DEST_PACK) -type d -print`
	chmod $(FILEMOD) `find $(DEST_PACK) -type f -print`


installtutorbin: $(DEST_BIN)
	$(INSTALL_DATA) vimtutor $(DEST_BIN)/$(VIMNAME)tutor
	chmod $(SCRIPTMOD) $(DEST_BIN)/$(VIMNAME)tutor

installgtutorbin: $(DEST_BIN)
	$(INSTALL_DATA) gvimtutor $(DEST_BIN)/$(GVIMNAME)tutor
	chmod $(SCRIPTMOD) $(DEST_BIN)/$(GVIMNAME)tutor

installtutor: $(DEST_RT) $(DEST_TUTOR)
	-$(INSTALL_DATA) $(TUTORSOURCE)/README* $(TUTORSOURCE)/tutor* $(DEST_TUTOR)
	-rm -f $(DEST_TUTOR)/*.info
	chmod $(HELPMOD) $(DEST_TUTOR)/*



installspell: $(DEST_VIM) $(DEST_RT) $(DEST_SPELL)
	if test -f $(SPELLSOURCE)/en.latin1.spl; then \
	  $(INSTALL_DATA) $(SPELLSOURCE)/*.spl $(SPELLSOURCE)/*.sug $(SPELLSOURCE)/*.vim $(DEST_SPELL); \
	  chmod $(HELPMOD) $(DEST_SPELL)/*.spl $(DEST_SPELL)/*.sug $(DEST_SPELL)/*.vim; \
	fi


installtools: $(TOOLS) $(DESTDIR)$(exec_prefix) $(DEST_BIN) \
		$(TOOLSSOURCE) $(DEST_VIM) $(DEST_RT) $(DEST_TOOLS) \
		$(INSTALL_TOOL_LANGS)
	if test -f $(DEST_BIN)/xxd$(EXEEXT); then \
	  mv -f $(DEST_BIN)/xxd$(EXEEXT) $(DEST_BIN)/xxd.rm; \
	  rm -f $(DEST_BIN)/xxd.rm; \
	fi
	$(INSTALL_PROG) xxd/xxd$(EXEEXT) $(DEST_BIN)
	$(STRIP) $(DEST_BIN)/xxd$(EXEEXT)
	chmod $(BINMOD) $(DEST_BIN)/xxd$(EXEEXT)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN) "" $(INSTALLMANARGS)


	$(INSTALL_DATA_R) $(TOOLSSOURCE)/* $(DEST_TOOLS)

	cvs=`find $(DEST_TOOLS) \( -name CVS -o -name AAPDIR \) -print`; \
	      if test -n "$$cvs"; then \
		 rm -rf $$cvs; \
	      fi
	-chmod $(FILEMOD) $(DEST_TOOLS)/*

	perlpath=`./which.sh perl` && sed -e "s+/usr/bin/perl+$$perlpath+" $(TOOLSSOURCE)/efm_perl.pl >$(DEST_TOOLS)/efm_perl.pl
	awkpath=`./which.sh nawk` && sed -e "s+/usr/bin/nawk+$$awkpath+" $(TOOLSSOURCE)/mve.awk >$(DEST_TOOLS)/mve.awk; if test -z "$$awkpath"; then \
		awkpath=`./which.sh gawk` && sed -e "s+/usr/bin/nawk+$$awkpath+" $(TOOLSSOURCE)/mve.awk >$(DEST_TOOLS)/mve.awk; if test -z "$$awkpath"; then \
		awkpath=`./which.sh awk` && sed -e "s+/usr/bin/nawk+$$awkpath+" $(TOOLSSOURCE)/mve.awk >$(DEST_TOOLS)/mve.awk; fi; fi
	-chmod $(SCRIPTMOD) `grep -l "^


install-tool-languages:
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_DA) "-da" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_DA_I) "-da" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_DA_U) "-da.UTF-8" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_DE) "-de" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_DE_I) "-de" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_DE_U) "-de.UTF-8" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_FR) "-fr" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_FR_I) "-fr" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_FR_U) "-fr.UTF-8" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_IT) "-it" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_IT_I) "-it" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_IT_U) "-it.UTF-8" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_JA_U) "-ja.UTF-8" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_PL) "-pl" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_PL_I) "-pl" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_PL_U) "-pl.UTF-8" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_RU) "-ru" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_RU_U) "-ru.UTF-8" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_TR) "-tr" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_TR_I) "-tr" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh xxd $(DEST_MAN_TR_U) "-tr.UTF-8" $(INSTALLMANARGS)



install-languages: languages $(DEST_LANG) $(DEST_KMAP)
	-$(SHELL) ./installman.sh install $(DEST_MAN_DA) "-da" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh install $(DEST_MAN_DA_I) "-da" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh install $(DEST_MAN_DA_U) "-da.UTF-8" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh install $(DEST_MAN_DE) "-de" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh install $(DEST_MAN_DE_I) "-de" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh install $(DEST_MAN_DE_U) "-de.UTF-8" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh install $(DEST_MAN_FR) "-fr" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh install $(DEST_MAN_FR_I) "-fr" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh install $(DEST_MAN_FR_U) "-fr.UTF-8" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh install $(DEST_MAN_IT) "-it" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh install $(DEST_MAN_IT_I) "-it" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh install $(DEST_MAN_IT_U) "-it.UTF-8" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh install $(DEST_MAN_JA_U) "-ja.UTF-8" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh install $(DEST_MAN_PL) "-pl" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh install $(DEST_MAN_PL_I) "-pl" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh install $(DEST_MAN_PL_U) "-pl.UTF-8" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh install $(DEST_MAN_RU) "-ru" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh install $(DEST_MAN_RU_U) "-ru.UTF-8" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh install $(DEST_MAN_TR) "-tr" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh install $(DEST_MAN_TR_I) "-tr" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh install $(DEST_MAN_TR_U) "-tr.UTF-8" $(INSTALLMANARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_DA) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_DA_I) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_DA_U) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_DE) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_DE_I) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_DE_U) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_FR) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_FR_I) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_FR_U) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_IT) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_IT_I) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_IT_U) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_JA_U) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_PL) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_PL_I) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_PL_U) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_RU) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_RU_U) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_TR) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_TR_I) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_TR_U) $(INSTALLMLARGS)
	if test -n "$(MAKEMO)" -a -f $(PODIR)/Makefile; then \
	   cd $(PODIR); $(MAKE) prefix=$(DESTDIR)$(prefix) LOCALEDIR=$(DEST_LANG) \
	   INSTALL_DATA=$(INSTALL_DATA) FILEMOD=$(FILEMOD) install; \
	fi
	if test -d $(LANGSOURCE); then \
	   $(INSTALL_DATA) $(LANGSOURCE)/README.txt $(LANGSOURCE)/*.vim $(DEST_LANG); \
	   chmod $(FILEMOD) $(DEST_LANG)/README.txt $(DEST_LANG)/*.vim; \
	fi
	if test -d $(KMAPSOURCE); then \
	   $(INSTALL_DATA) $(KMAPSOURCE)/README.txt $(KMAPSOURCE)/*.vim $(DEST_KMAP); \
	   chmod $(FILEMOD) $(DEST_KMAP)/README.txt $(DEST_KMAP)/*.vim; \
	fi



ICON48PATH = $(DESTDIR)$(DATADIR)/icons/hicolor/48x48/apps
ICON32PATH = $(DESTDIR)$(DATADIR)/icons/locolor/32x32/apps
ICON16PATH = $(DESTDIR)$(DATADIR)/icons/locolor/16x16/apps
ICONTHEMEPATH = $(DATADIR)/icons/hicolor
DESKTOPPATH = $(DESTDIR)$(DATADIR)/applications
KDEPATH = $(HOME)/.kde/share/icons
install-icons:
	if test -n "$(DESTDIR)"; then \
		$(MKDIR_P) $(ICON48PATH) $(ICON32PATH) \
		$(ICON16PATH) $(DESKTOPPATH); \
	fi

	if test -d $(ICON48PATH) -a -w $(ICON48PATH) \
		-a ! -f $(ICON48PATH)/gvim.png; then \
	   $(INSTALL_DATA) $(SCRIPTSOURCE)/vim48x48.png $(ICON48PATH)/gvim.png; \
	   if test -z "$(DESTDIR)" -a -x "$(GTK_UPDATE_ICON_CACHE)" \
		   -a -w $(ICONTHEMEPATH) \
		   -a -f $(ICONTHEMEPATH)/index.theme; then \
		$(GTK_UPDATE_ICON_CACHE) -q $(ICONTHEMEPATH); \
	   fi \
	fi
	if test -d $(ICON32PATH) -a -w $(ICON32PATH) \
		-a ! -f $(ICON32PATH)/gvim.png; then \
	   $(INSTALL_DATA) $(SCRIPTSOURCE)/vim32x32.png $(ICON32PATH)/gvim.png; \
	fi
	if test -d $(ICON16PATH) -a -w $(ICON16PATH) \
		-a ! -f $(ICON16PATH)/gvim.png; then \
	   $(INSTALL_DATA) $(SCRIPTSOURCE)/vim16x16.png $(ICON16PATH)/gvim.png; \
	fi
	if test -d $(DESKTOPPATH) -a -w $(DESKTOPPATH); then \
	   if test -f po/vim.desktop -a -f po/gvim.desktop; then \
		$(INSTALL_DATA) po/vim.desktop po/gvim.desktop \
			$(DESKTOPPATH); \
	   else \
		$(INSTALL_DATA) $(SCRIPTSOURCE)/vim.desktop \
			$(SCRIPTSOURCE)/gvim.desktop \
			$(DESKTOPPATH); \
	   fi; \
	   if test -z "$(DESTDIR)" -a -x "$(UPDATE_DESKTOP_DATABASE)"; then \
	      $(UPDATE_DESKTOP_DATABASE) -q $(DESKTOPPATH); \
	   fi \
	fi

$(HELPSOURCE)/vim.1 $(MACROSOURCE) $(TOOLSSOURCE):
	@echo Runtime files not found.
	@echo You need to unpack the runtime archive before running "make install".
	test -f error

$(DESTDIR)$(exec_prefix) $(DEST_BIN) \
		$(DEST_VIM) $(DEST_RT) $(DEST_HELP) \
		$(DEST_PRINT) $(DEST_COL) $(DEST_SYN) $(DEST_SYN)/shared \
		$(DEST_IND) $(DEST_FTP) \
		$(DEST_LANG) $(DEST_KMAP) $(DEST_COMP) $(DEST_MACRO) \
		$(DEST_PACK) $(DEST_TOOLS) $(DEST_TUTOR) $(DEST_SPELL) \
		$(DEST_AUTO) $(DEST_AUTO)/dist $(DEST_AUTO)/xml $(DEST_AUTO)/zig \
		$(DEST_IMPORT) $(DEST_IMPORT)/dist $(DEST_PLUG):
	$(MKDIR_P) $@
	-chmod $(DIRMOD) $@



installlinks: $(GUI_TARGETS) \
			$(DEST_BIN)/$(EXTARGET) \
			$(DEST_BIN)/$(VIEWTARGET) \
			$(DEST_BIN)/$(RVIMTARGET) \
			$(DEST_BIN)/$(RVIEWTARGET) \
			$(INSTALLVIMDIFF)

installglinks: $(DEST_BIN)/$(GVIMTARGET) \
			$(DEST_BIN)/$(GVIEWTARGET) \
			$(DEST_BIN)/$(RGVIMTARGET) \
			$(DEST_BIN)/$(RGVIEWTARGET) \
			$(DEST_BIN)/$(EVIMTARGET) \
			$(DEST_BIN)/$(EVIEWTARGET) \
			$(INSTALLGVIMDIFF)

installvimdiff: $(DEST_BIN)/$(VIMDIFFTARGET)
installgvimdiff: $(DEST_BIN)/$(GVIMDIFFTARGET)

$(DEST_BIN)/$(EXTARGET): $(DEST_BIN)
	cd $(DEST_BIN); ln -s $(VIMTARGET) $(EXTARGET)

$(DEST_BIN)/$(VIEWTARGET): $(DEST_BIN)
	cd $(DEST_BIN); ln -s $(VIMTARGET) $(VIEWTARGET)

$(DEST_BIN)/$(GVIMTARGET): $(DEST_BIN)
	cd $(DEST_BIN); ln -s $(VIMTARGET) $(GVIMTARGET)

$(DEST_BIN)/$(GVIEWTARGET): $(DEST_BIN)
	cd $(DEST_BIN); ln -s $(VIMTARGET) $(GVIEWTARGET)

$(DEST_BIN)/$(RVIMTARGET): $(DEST_BIN)
	cd $(DEST_BIN); ln -s $(VIMTARGET) $(RVIMTARGET)

$(DEST_BIN)/$(RVIEWTARGET): $(DEST_BIN)
	cd $(DEST_BIN); ln -s $(VIMTARGET) $(RVIEWTARGET)

$(DEST_BIN)/$(RGVIMTARGET): $(DEST_BIN)
	cd $(DEST_BIN); ln -s $(VIMTARGET) $(RGVIMTARGET)

$(DEST_BIN)/$(RGVIEWTARGET): $(DEST_BIN)
	cd $(DEST_BIN); ln -s $(VIMTARGET) $(RGVIEWTARGET)

$(DEST_BIN)/$(VIMDIFFTARGET): $(DEST_BIN)
	cd $(DEST_BIN); ln -s $(VIMTARGET) $(VIMDIFFTARGET)

$(DEST_BIN)/$(GVIMDIFFTARGET): $(DEST_BIN)
	cd $(DEST_BIN); ln -s $(VIMTARGET) $(GVIMDIFFTARGET)

$(DEST_BIN)/$(EVIMTARGET): $(DEST_BIN)
	cd $(DEST_BIN); ln -s $(VIMTARGET) $(EVIMTARGET)

$(DEST_BIN)/$(EVIEWTARGET): $(DEST_BIN)
	cd $(DEST_BIN); ln -s $(VIMTARGET) $(EVIEWTARGET)




INSTALLMLARGS = $(VIMNAME) $(VIMDIFFNAME) $(EVIMNAME) \
		$(EXNAME) $(VIEWNAME) $(RVIMNAME) $(RVIEWNAME) \
		$(GVIMNAME) $(GVIEWNAME) $(RGVIMNAME) $(RGVIEWNAME) \
		$(GVIMDIFFNAME) $(EVIEWNAME)

installmanlinks:
	-$(SHELL) ./installml.sh install "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN) $(INSTALLMLARGS)

uninstall: uninstall_runtime
	-rm -f $(DEST_BIN)/$(VIMTARGET)
	-rm -f $(DEST_BIN)/vimtutor
	-rm -f $(DEST_BIN)/gvimtutor
	-rm -f $(DEST_BIN)/$(EXTARGET) $(DEST_BIN)/$(VIEWTARGET)
	-rm -f $(DEST_BIN)/$(GVIMTARGET) $(DEST_BIN)/$(GVIEWTARGET)
	-rm -f $(DEST_BIN)/$(RVIMTARGET) $(DEST_BIN)/$(RVIEWTARGET)
	-rm -f $(DEST_BIN)/$(RGVIMTARGET) $(DEST_BIN)/$(RGVIEWTARGET)
	-rm -f $(DEST_BIN)/$(VIMDIFFTARGET) $(DEST_BIN)/$(GVIMDIFFTARGET)
	-rm -f $(DEST_BIN)/$(EVIMTARGET) $(DEST_BIN)/$(EVIEWTARGET)
	-rm -f $(DEST_BIN)/xxd$(EXEEXT)


uninstall_runtime:
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_DA) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_DA_I) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_DA_U) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_DE) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_DE_I) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_DE_U) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_FR) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_FR_I) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_FR_U) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_IT) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_IT_I) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_IT_U) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_JA_U) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_PL) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_PL_I) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_PL_U) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_RU) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_RU_U) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_TR) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_TR_I) "" $(INSTALLMANARGS)
	-$(SHELL) ./installman.sh uninstall $(DEST_MAN_TR_U) "" $(INSTALLMANARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_DA) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_DA_I) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_DA_U) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_DE) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_DE_I) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_DE_U) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_FR) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_FR_I) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_FR_U) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_IT) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_IT_I) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_IT_U) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_JA_U) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_PL) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_PL_I) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_PL_U) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_RU) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_RU_U) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_TR) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_TR_I) $(INSTALLMLARGS)
	-$(SHELL) ./installml.sh uninstall "$(GUI_MAN_TARGETS)" \
		$(DEST_MAN_TR_U) $(INSTALLMLARGS)
	-rm -f $(DEST_MAN)/xxd.1
	-rm -f $(DEST_MAN_DA)/xxd.1 $(DEST_MAN_DA_I)/xxd.1 $(DEST_MAN_DA_U)/xxd.1
	-rm -f $(DEST_MAN_DE)/xxd.1 $(DEST_MAN_DE_I)/xxd.1 $(DEST_MAN_DE_U)/xxd.1
	-rm -f $(DEST_MAN_FR)/xxd.1 $(DEST_MAN_FR_I)/xxd.1 $(DEST_MAN_FR_U)/xxd.1
	-rm -f $(DEST_MAN_IT)/xxd.1 $(DEST_MAN_IT_I)/xxd.1 $(DEST_MAN_IT_U)/xxd.1
	-rm -f $(DEST_MAN_JA_U)/xxd.1
	-rm -f $(DEST_MAN_PL)/xxd.1 $(DEST_MAN_PL_I)/xxd.1 $(DEST_MAN_PL_U)/xxd.1
	-rm -f $(DEST_MAN_RU)/xxd.1 $(DEST_MAN_RU_U)/xxd.1
	-rm -f $(DEST_HELP)/*.txt $(DEST_HELP)/tags $(DEST_HELP)/*.pl
	-rm -f $(DEST_HELP)/*.??x $(DEST_HELP)/tags-??
	-rm -f $(SYS_MENU_FILE) $(SYS_SYNMENU_FILE) $(SYS_DELMENU_FILE)
	-rm -f $(SYS_BUGR_FILE) $(VIM_DEFAULTS_FILE) $(EVIM_FILE) $(MSWIN_FILE)
	-rm -f $(DEST_SCRIPT)/gvimrc_example.vim $(DEST_SCRIPT)/vimrc_example.vim
	-rm -f $(SYS_FILETYPE_FILE) $(SYS_FTOFF_FILE) $(SYS_SCRIPTS_FILE)
	-rm -f $(SYS_INDOFF_FILE) $(SYS_INDENT_FILE)
	-rm -f $(SYS_FTPLUGOF_FILE) $(SYS_FTPLUGIN_FILE)
	-rm -f $(SYS_OPTWIN_FILE)
	-rm -f $(DEST_COL)/*.vim $(DEST_COL)/README.txt
	-rm -rf $(DEST_COL)/tools
	-rm -rf $(DEST_COL)/lists
	-rm -f $(DEST_SYN)/shared/*.vim $(DEST_SYN)/shared/README.txt
	-rm -f $(DEST_SYN)/*.vim $(DEST_SYN)/README.txt
	-rm -f $(DEST_IND)/*.vim $(DEST_IND)/README.txt
	-rm -rf $(DEST_MACRO)
	-rm -rf $(DEST_PACK)
	-rm -rf $(DEST_TUTOR)
	-rm -rf $(DEST_SPELL)
	-rm -rf $(DEST_TOOLS)
	-rm -rf $(DEST_LANG)
	-rm -rf $(DEST_KMAP)
	-rm -rf $(DEST_COMP)
	-rm -f $(DEST_PRINT)/*.ps
	-rmdir $(DEST_HELP) $(DEST_PRINT) $(DEST_COL) $(DEST_SYN)/shared
	-rmdir $(DEST_SYN) $(DEST_IND)
	-rm -rf $(DEST_FTP)/*.vim $(DEST_FTP)/README.txt $(DEST_FTP)/logtalk.dict
	-rm -f $(DEST_AUTO)/*.vim $(DEST_AUTO)/README.txt
	-rm -f $(DEST_AUTO)/dist/*.vim $(DEST_AUTO)/xml/*.vim $(DEST_AUTO)/zig/*.vim
	-rm -f $(DEST_IMPORT)/dist/*.vim
	-rm -f $(DEST_PLUG)/*.vim $(DEST_PLUG)/README.txt
	-rmdir $(DEST_FTP) $(DEST_AUTO)/dist $(DEST_AUTO)/xml $(DEST_AUTO)/zig $(DEST_AUTO)
	-rmdir $(DEST_IMPORT)/dist $(DEST_IMPORT)
	-rmdir $(DEST_PLUG) $(DEST_RT)

	-rmdir $(DEST_VIM)



clean celan: testclean
	-rm -f *.o core $(VIMTARGET).core $(VIMTARGET) vim xxd/*.o
	-rm -rf objects
	-rm -f $(TOOLS) auto/osdef.h auto/pathdef.c auto/if_perl.c auto/gui_gtk_gresources.c auto/gui_gtk_gresources.h auto/os_haiku.rdef
	-rm -f conftest* *~ auto/link.sed
	-rm -f testdir/opt_test.vim
	-rm -f $(UNITTEST_TARGETS)
	-rm -f runtime pixmaps
	-rm -f mzscheme_base.c
	-rm -rf libvterm/.libs libvterm/src/.libs libvterm/t/.libs libvterm/src/*.o libvterm/src/*.lo libvterm/t/*.o libvterm/t/*.lo libvterm/t/harness libvterm/libvterm.la
	if test -d $(PODIR); then \
		cd $(PODIR); $(MAKE) prefix=$(DESTDIR)$(prefix) clean; \
	fi




















SHADOWDIR = shadow

LINKEDFILES = ../*.[chm] ../*.cc ../*.in ../*.sh ../*.xs ../*.xbm ../gui_gtk_res.xml ../toolcheck ../proto ../libvterm ../vimtutor ../gvimtutor ../install-sh ../Make_all.mak

shadow:	runtime pixmaps
	$(MKDIR_P) $(SHADOWDIR)
	cd $(SHADOWDIR); ln -s $(LINKEDFILES) .
	mkdir $(SHADOWDIR)/auto
	cd $(SHADOWDIR)/auto; ln -s ../../auto/configure .
	$(MKDIR_P) $(SHADOWDIR)/po
	cd $(SHADOWDIR)/po; ln -s ../../po/*.po ../../po/*.mak ../../po/*.vim ../../po/*.in ../../po/Makefile ../../po/*.c .
	cd $(SHADOWDIR); rm -f auto/link.sed
	cp Makefile configure $(SHADOWDIR)
	rm -f $(SHADOWDIR)/auto/config.mk $(SHADOWDIR)/config.mk.dist
	cp config.mk.dist $(SHADOWDIR)/auto/config.mk
	cp config.mk.dist $(SHADOWDIR)
	$(MKDIR_P) $(SHADOWDIR)/xxd
	cd $(SHADOWDIR)/xxd; ln -s ../../xxd/*.[ch] ../../xxd/Make* .
	$(MKDIR_P) $(SHADOWDIR)/xdiff
	cd $(SHADOWDIR)/xdiff; ln -s ../../xdiff/*.[ch] .
	$(MKDIR_P) $(SHADOWDIR)/testdir
	cd $(SHADOWDIR)/testdir; ln -s ../../testdir/Makefile \
				 ../../testdir/Make_all.mak \
				 ../../testdir/README.txt \
				 ../../testdir/*.in \
				 ../../testdir/*.vim \
				 ../../testdir/*.py \
				 ../../testdir/python* \
				 ../../testdir/pyxfile \
				 ../../testdir/sautest \
				 ../../testdir/samples \
				 ../../testdir/dumps \
				 ../../testdir/*.ok \
				 ../../testdir/testluaplugin \
				 .




shadowupdate:
	ln -sf $(LINKEDFILES) .


runtime:
	-ln -s ../runtime .


pixmaps:
	-ln -s ../pixmaps .



menu: ./vim ../runtime/makemenu.vim
	./vim --clean -X --not-a-term -S ../runtime/makemenu.vim


scrub scratch:
	-rm -f auto/config.status auto/config.cache config.log auto/config.log
	-rm -f auto/config.h auto/link.log auto/link.sed auto/config.mk
	touch auto/config.h
	cp config.mk.dist auto/config.mk

distclean: clean scratch
	-rm -f tags

dist: distclean
	@echo
	@echo Making the distribution has to be done in the top directory

mdepend:
	-@rm -f Makefile~
	cp Makefile Makefile~
	sed -e '/\
	@for i in $(ALL_SRC) ; do \
	  echo "$$i" ; \
	  echo `echo "$$i" | sed -e 's/[^ ]*\.c$$/objects\/\1.o/'`": $$i" `\
	    $(CPP) $$i |\
	    grep '^
	    sort -t'"' -u +1 -2 |\
	    sed -e 's/.*"\.\/\(.*\)".*/\1/'\
	    ` >> tmp_make ; \
	done
	mv tmp_make Makefile

depend:
	-@rm -f Makefile~
	cp Makefile Makefile~
	sed -e '/\
	-for i in $(ALL_LOCAL_SRC); do echo $$i; \
		$(CPP_DEPEND) $$i | \
		sed -e 's+^\([^ ]*\.o\)+objects/\1+' >> tmp_make; done
	-for i in $(TERM_SRC); do echo $$i; \
		$(CPP_DEPEND) $$i | \
		sed -e 's+^\([^ ]*\.o\)+objects/vterm_\1+' >> tmp_make; done
	-for i in $(XDIFF_SRC); do echo $$i; \
		$(CPP_DEPEND) $$i | \
		sed -e 's+^\([^ ]*\.o\)+objects/\1+' -e 's+xdiff/\.\./++g' >> tmp_make; done
	mv tmp_make Makefile


lint:
	$(LINT) $(LINT_OPTIONS) $(LINT_CFLAGS) $(LINT_EXTRA) $(LINT_SRC)
	-rm -f *.ln


lintinstall:
	$(LINT) $(LINT_OPTIONS) -DWIN32 -DUNIX_LINT dosinst.c
	-rm -f dosinst.ln



.c.o:
	$(CCC) $<

auto/if_perl.c: if_perl.xs
	$(PERL) -e 'unless ( $$] >= 5.005 ) { for (qw(na defgv errgv)) { print "
	$(PERL) $(PERL_XSUBPP) -prototypes -typemap \
	    $(PERLLIB)/ExtUtils/typemap if_perl.xs >> $@

auto/osdef.h: auto/config.h osdef.sh osdef1.h.in osdef2.h.in
	CC="$(CC) $(OSDEF_CFLAGS)" srcdir=$(srcdir) sh $(srcdir)/osdef.sh

auto/os_haiku.rdef: os_haiku.rdef.in
	@echo creating $@
	@echo '/* This file is automatically created by Makefile */' >> $@
	@echo '/* DO NOT EDIT!  Change Makefile only. */' >> $@
	@cat $(srcdir)/os_haiku.rdef.in >> auto/os_haiku.rdef
	sed -i "s|@MAJOR@|$(VIMMAJOR)|" auto/os_haiku.rdef
	sed -i "s|@MINOR@|$(VIMMINOR)|" auto/os_haiku.rdef

auto/pathdef.c: Makefile auto/config.mk
	-@echo creating $@
	-@echo '/* pathdef.c */' > $@
	-@echo '/* This file is automatically created by Makefile' >> $@
	-@echo ' * DO NOT EDIT!  Change Makefile only. */' >> $@
	-@echo '
	-@echo 'char_u *default_vim_dir = (char_u *)"$(VIMRCLOC)";' | $(QUOTESED) >> $@
	-@echo 'char_u *default_vimruntime_dir = (char_u *)"$(VIMRUNTIMEDIR)";' | $(QUOTESED) >> $@
	-@echo 'char_u *all_cflags = (char_u *)"$(CC) -c -I$(srcdir) $(ALL_CFLAGS)";' | $(QUOTESED) >>  $@
	-@echo 'char_u *all_lflags = (char_u *)"$(CC) $(ALL_LIB_DIRS) $(LDFLAGS) -o $(VIMTARGET) $(ALL_LIBS) ";' | $(QUOTESED) >>  $@
	-@echo 'char_u *compiled_user = (char_u *)"' | tr -d $(NL) >> $@
	-@if test -n "$(COMPILEDBY)"; then \
		echo "$(COMPILEDBY)" | tr -d $(NL) >> $@; \
		else ((logname) 2>/dev/null || whoami) | tr -d $(NL) >> $@; fi
	-@echo '";' >> $@
	-@echo 'char_u *compiled_sys = (char_u *)"' | tr -d $(NL) >> $@
	-@if test -z "$(COMPILEDBY)"; then hostname | tr -d $(NL) >> $@; fi
	-@echo '";' >> $@
	-@sh $(srcdir)/pathdef.sh

GUI_GTK_RES_INPUTS = \
	../pixmaps/stock_vim_build_tags.png \
	../pixmaps/stock_vim_find_help.png \
	../pixmaps/stock_vim_save_all.png \
	../pixmaps/stock_vim_session_load.png \
	../pixmaps/stock_vim_session_new.png \
	../pixmaps/stock_vim_session_save.png \
	../pixmaps/stock_vim_shell.png \
	../pixmaps/stock_vim_window_maximize.png \
	../pixmaps/stock_vim_window_maximize_width.png \
	../pixmaps/stock_vim_window_minimize.png \
	../pixmaps/stock_vim_window_minimize_width.png \
	../pixmaps/stock_vim_window_split.png \
	../pixmaps/stock_vim_window_split_vertical.png

auto/gui_gtk_gresources.c: gui_gtk_res.xml $(GUI_GTK_RES_INPUTS)
	$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=../pixmaps --generate --c-name=gui_gtk --manual-register gui_gtk_res.xml
auto/gui_gtk_gresources.h: gui_gtk_res.xml $(GUI_GTK_RES_INPUTS)
	if test -z "$(GLIB_COMPILE_RESOURCES)"; then touch $@; else \
		$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=../pixmaps --generate --c-name=gui_gtk --manual-register gui_gtk_res.xml; \
	fi



VIM_H_DEPENDENCIES = \
	vim.h protodef.h auto/config.h feature.h os_unix.h \
	auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
	structs.h regexp.h gui.h alloc.h ex_cmds.h spell.h \
	proto.h globals.h errors.h





objects: objects/.dirstamp

objects/.dirstamp:
	$(MKDIR_P) objects
	touch objects/.dirstamp




$(ALL_OBJ): objects/.dirstamp

objects/alloc.o: alloc.c
	$(CCC) -o $@ alloc.c

objects/arabic.o: arabic.c
	$(CCC) -o $@ arabic.c

objects/arglist.o: arglist.c
	$(CCC) -o $@ arglist.c

objects/autocmd.o: autocmd.c
	$(CCC) -o $@ autocmd.c

objects/blob.o: blob.c
	$(CCC) -o $@ blob.c

objects/blowfish.o: blowfish.c
	$(CCC) -o $@ blowfish.c

objects/buffer.o: buffer.c
	$(CCC) -o $@ buffer.c

objects/bufwrite.o: bufwrite.c
	$(CCC) -o $@ bufwrite.c

objects/change.o: change.c
	$(CCC) -o $@ change.c

objects/charset.o: charset.c
	$(CCC) -o $@ charset.c

objects/cindent.o: cindent.c
	$(CCC) -o $@ cindent.c

objects/clientserver.o: clientserver.c
	$(CCC) -o $@ clientserver.c

objects/clipboard.o: clipboard.c
	$(CCC) -o $@ clipboard.c

objects/cmdexpand.o: cmdexpand.c
	$(CCC) -o $@ cmdexpand.c

objects/cmdhist.o: cmdhist.c
	$(CCC) -o $@ cmdhist.c

objects/crypt.o: crypt.c
	$(CCC) -o $@ crypt.c

objects/crypt_zip.o: crypt_zip.c
	$(CCC) -o $@ crypt_zip.c

objects/debugger.o: debugger.c
	$(CCC) -o $@ debugger.c

objects/dict.o: dict.c
	$(CCC) -o $@ dict.c

objects/diff.o: diff.c $(XDIFF_INCL)
	$(CCC) -o $@ diff.c

objects/digraph.o: digraph.c
	$(CCC) -o $@ digraph.c

objects/drawline.o: drawline.c
	$(CCC) -o $@ drawline.c

objects/drawscreen.o: drawscreen.c
	$(CCC) -o $@ drawscreen.c

objects/edit.o: edit.c
	$(CCC) -o $@ edit.c

objects/eval.o: eval.c
	$(CCC) -o $@ eval.c

objects/evalbuffer.o: evalbuffer.c
	$(CCC) -o $@ evalbuffer.c

objects/evalfunc.o: evalfunc.c
	$(CCC) -o $@ evalfunc.c

objects/evalvars.o: evalvars.c
	$(CCC) -o $@ evalvars.c

objects/evalwindow.o: evalwindow.c
	$(CCC) -o $@ evalwindow.c

objects/ex_cmds.o: ex_cmds.c
	$(CCC) -o $@ ex_cmds.c

objects/ex_cmds2.o: ex_cmds2.c
	$(CCC) -o $@ ex_cmds2.c

objects/ex_docmd.o: ex_docmd.c
	$(CCC) -o $@ ex_docmd.c

objects/ex_eval.o: ex_eval.c
	$(CCC) -o $@ ex_eval.c

objects/ex_getln.o: ex_getln.c
	$(CCC) -o $@ ex_getln.c

objects/fileio.o: fileio.c
	$(CCC) -o $@ fileio.c

objects/filepath.o: filepath.c
	$(CCC) -o $@ filepath.c

objects/findfile.o: findfile.c
	$(CCC) -o $@ findfile.c

objects/float.o: float.c
	$(CCC) -o $@ float.c

objects/fold.o: fold.c
	$(CCC) -o $@ fold.c

objects/getchar.o: getchar.c
	$(CCC) -o $@ getchar.c

objects/hardcopy.o: hardcopy.c
	$(CCC) -o $@ hardcopy.c

objects/hashtab.o: hashtab.c
	$(CCC) -o $@ hashtab.c

objects/help.o: help.c
	$(CCC) -o $@ help.c

objects/gui.o: gui.c
	$(CCC) -o $@ gui.c

objects/beval.o: beval.c
	$(CCC) -o $@ beval.c

objects/gui_beval.o: gui_beval.c
	$(CCC) -o $@ gui_beval.c

objects/gui_gtk.o: gui_gtk.c
	$(CCC) -o $@ gui_gtk.c

objects/gui_gtk_f.o: gui_gtk_f.c
	$(CCC) -o $@ gui_gtk_f.c

objects/gui_gtk_gresources.o: auto/gui_gtk_gresources.c
	$(CCC_NF) $(PERL_CFLAGS) $(ALL_CFLAGS) -o $@ auto/gui_gtk_gresources.c

objects/gui_gtk_x11.o: gui_gtk_x11.c
	$(CCC) -o $@ gui_gtk_x11.c

objects/gui_haiku.o: gui_haiku.cc
	$(CCC) -o $@ gui_haiku.cc

objects/gui_motif.o: gui_motif.c
	$(CCC) -o $@ gui_motif.c

objects/gui_xmdlg.o: gui_xmdlg.c
	$(CCC) -o $@ gui_xmdlg.c

objects/gui_xmebw.o: gui_xmebw.c
	$(CCC) -o $@ gui_xmebw.c

objects/gui_x11.o: gui_x11.c
	$(CCC) -o $@ gui_x11.c

objects/gui_xim.o: gui_xim.c
	$(CCC) -o $@ gui_xim.c

objects/gui_photon.o: gui_photon.c
	$(CCC) -o $@ gui_photon.c

objects/highlight.o: highlight.c
	$(CCC) -o $@ highlight.c

objects/if_cscope.o: if_cscope.c
	$(CCC) -o $@ if_cscope.c

objects/if_xcmdsrv.o: if_xcmdsrv.c
	$(CCC) -o $@ if_xcmdsrv.c

objects/if_lua.o: if_lua.c
	$(CCC_NF) $(LUA_CFLAGS) $(ALL_CFLAGS) $(LUA_CFLAGS_EXTRA) -o $@ if_lua.c

objects/if_mzsch.o: if_mzsch.c $(MZSCHEME_EXTRA)
	$(CCC) -o $@ $(MZSCHEME_CFLAGS_EXTRA) if_mzsch.c

mzscheme_base.c:
	$(MZSCHEME_MZC) --c-mods mzscheme_base.c ++lib scheme/base

objects/if_perl.o: auto/if_perl.c
	$(CCC_NF) $(PERL_CFLAGS) $(ALL_CFLAGS) $(PERL_CFLAGS_EXTRA) -o $@ auto/if_perl.c

objects/if_perlsfio.o: if_perlsfio.c
	$(CCC_NF) $(PERL_CFLAGS) $(ALL_CFLAGS) $(PERL_CFLAGS_EXTRA) -o $@ if_perlsfio.c

objects/if_python.o: if_python.c if_py_both.h
	$(CCC_NF) $(PYTHON_CFLAGS) $(ALL_CFLAGS) $(PYTHON_CFLAGS_EXTRA) -o $@ if_python.c

objects/if_python3.o: if_python3.c if_py_both.h
	$(CCC_NF) $(PYTHON3_CFLAGS) $(ALL_CFLAGS) $(PYTHON3_CFLAGS_EXTRA) -o $@ if_python3.c

objects/if_ruby.o: if_ruby.c
	$(CCC_NF) $(RUBY_CFLAGS) $(ALL_CFLAGS) $(RUBY_CFLAGS_EXTRA) -o $@ if_ruby.c

objects/if_tcl.o: if_tcl.c
	$(CCC_NF) $(TCL_CFLAGS) $(ALL_CFLAGS) $(TCL_CFLAGS_EXTRA) -o $@ if_tcl.c

objects/indent.o: indent.c
	$(CCC) -o $@ indent.c

objects/insexpand.o: insexpand.c
	$(CCC) -o $@ insexpand.c

objects/job.o: job.c
	$(CCC) -o $@ job.c

objects/json.o: json.c
	$(CCC) -o $@ json.c

objects/json_test.o: json_test.c
	$(CCC) -o $@ json_test.c

objects/kword_test.o: kword_test.c
	$(CCC) -o $@ kword_test.c

objects/list.o: list.c
	$(CCC) -o $@ list.c

objects/locale.o: locale.c
	$(CCC) -o $@ locale.c

objects/logfile.o: logfile.c
	$(CCC) -o $@ logfile.c

objects/main.o: main.c
	$(CCC) -o $@ main.c

objects/map.o: map.c
	$(CCC) -o $@ map.c

objects/mark.o: mark.c
	$(CCC) -o $@ mark.c

objects/match.o: match.c
	$(CCC) -o $@ match.c

objects/memfile.o: memfile.c
	$(CCC) -o $@ memfile.c

objects/memfile_test.o: memfile_test.c
	$(CCC) -o $@ memfile_test.c

objects/memline.o: memline.c
	$(CCC) -o $@ memline.c

objects/menu.o: menu.c
	$(CCC) -o $@ menu.c

objects/message.o: message.c
	$(CCC) -o $@ message.c

objects/message_test.o: message_test.c
	$(CCC) -o $@ message_test.c

objects/misc1.o: misc1.c
	$(CCC) -o $@ misc1.c

objects/misc2.o: misc2.c
	$(CCC) -o $@ misc2.c

objects/mouse.o: mouse.c
	$(CCC) -o $@ mouse.c

objects/move.o: move.c
	$(CCC) -o $@ move.c

objects/mbyte.o: mbyte.c
	$(CCC) -o $@ mbyte.c

objects/normal.o: normal.c
	$(CCC) -o $@ normal.c

objects/ops.o: ops.c
	$(CCC) -o $@ ops.c

objects/option.o: option.c optiondefs.h
	$(CCC_NF) $(ALL_IF_CFLAGS) $(ALL_CFLAGS) $(ALL_IF_CFLAGS_EXTRA) -o $@ option.c

objects/optionstr.o: optionstr.c
	$(CCC_NF) $(ALL_IF_CFLAGS) $(ALL_CFLAGS) $(ALL_IF_CFLAGS_EXTRA) -o $@ optionstr.c

objects/os_qnx.o: os_qnx.c $(VIM_H_DEPENDENCIES)
	$(CCC) -o $@ os_qnx.c

objects/os_haiku.rsrc: auto/os_haiku.rdef
	cat $< | $(CCC) -E - | grep -v '^

objects/os_macosx.o: os_macosx.m
	$(CCC) -o $@ os_macosx.m

objects/os_mac_conv.o: os_mac_conv.c $(VIM_H_DEPENDENCIES)
	$(CCC) -o $@ os_mac_conv.c

objects/os_unix.o: os_unix.c
	$(CCC) -o $@ os_unix.c

objects/os_mswin.o: os_mswin.c $(VIM_H_DEPENDENCIES)
	$(CCC) -o $@ os_mswin.c

objects/winclip.o: winclip.c $(VIM_H_DEPENDENCIES)
	$(CCC) -o $@ winclip.c

objects/pathdef.o: auto/pathdef.c
	$(CCC) -o $@ auto/pathdef.c

objects/popupmenu.o: popupmenu.c
	$(CCC) -o $@ popupmenu.c

objects/popupwin.o: popupwin.c
	$(CCC) -o $@ popupwin.c

objects/profiler.o: profiler.c
	$(CCC) -o $@ profiler.c

objects/pty.o: pty.c
	$(CCC) -o $@ pty.c

objects/quickfix.o: quickfix.c
	$(CCC) -o $@ quickfix.c

objects/regexp.o: regexp.c regexp_bt.c regexp_nfa.c
	$(CCC) -o $@ regexp.c

objects/register.o: register.c
	$(CCC) -o $@ register.c

objects/scriptfile.o: scriptfile.c
	$(CCC) -o $@ scriptfile.c

objects/screen.o: screen.c
	$(CCC) -o $@ screen.c

objects/search.o: search.c
	$(CCC) -o $@ search.c

objects/session.o: session.c
	$(CCC) -o $@ session.c

objects/sha256.o: sha256.c
	$(CCC) -o $@ sha256.c

objects/sign.o: sign.c
	$(CCC) -o $@ sign.c

objects/sound.o: sound.c
	$(CCC) -o $@ sound.c

objects/spell.o: spell.c
	$(CCC) -o $@ spell.c

objects/spellfile.o: spellfile.c
	$(CCC) -o $@ spellfile.c

objects/spellsuggest.o: spellsuggest.c
	$(CCC) -o $@ spellsuggest.c

objects/strings.o: strings.c
	$(CCC) -o $@ strings.c

objects/syntax.o: syntax.c
	$(CCC) -o $@ syntax.c

objects/tag.o: tag.c
	$(CCC) -o $@ tag.c

objects/term.o: term.c
	$(CCC) -o $@ term.c

objects/terminal.o: terminal.c $(TERM_DEPS)
	$(CCC) -o $@ terminal.c

objects/testing.o: testing.c
	$(CCC) -o $@ testing.c

objects/textformat.o: textformat.c
	$(CCC) -o $@ textformat.c

objects/textobject.o: textobject.c
	$(CCC) -o $@ textobject.c

objects/textprop.o: textprop.c
	$(CCC) -o $@ textprop.c

objects/time.o: time.c
	$(CCC) -o $@ time.c

objects/typval.o: typval.c
	$(CCC) -o $@ typval.c

objects/ui.o: ui.c
	$(CCC) -o $@ ui.c

objects/undo.o: undo.c
	$(CCC) -o $@ undo.c

objects/usercmd.o: usercmd.c
	$(CCC) -o $@ usercmd.c

objects/userfunc.o: userfunc.c
	$(CCC) -o $@ userfunc.c

objects/version.o: version.c
	$(CCC) -o $@ version.c

objects/vim9class.o: vim9class.c
	$(CCC) -o $@ vim9class.c

objects/vim9cmds.o: vim9cmds.c
	$(CCC) -o $@ vim9cmds.c

objects/vim9compile.o: vim9compile.c
	$(CCC) -o $@ vim9compile.c

objects/vim9execute.o: vim9execute.c
	$(CCC) -o $@ vim9execute.c

objects/vim9expr.o: vim9expr.c
	$(CCC) -o $@ vim9expr.c

objects/vim9instr.o: vim9instr.c
	$(CCC) -o $@ vim9instr.c

objects/vim9script.o: vim9script.c
	$(CCC) -o $@ vim9script.c

objects/vim9type.o: vim9type.c
	$(CCC) -o $@ vim9type.c

objects/viminfo.o: viminfo.c
	$(CCC) -o $@ viminfo.c

objects/window.o: window.c
	$(CCC) -o $@ window.c

objects/netbeans.o: netbeans.c
	$(CCC) -o $@ netbeans.c

objects/channel.o: channel.c
	$(CCC) -o $@ channel.c


objects/gui_gtk_x11.o: version.h





CCCTERM = $(CCC_NF) $(VTERM_CFLAGS) $(ALL_CFLAGS) -DINLINE="" \
	  -DVSNPRINTF=vim_vsnprintf \
	  -DSNPRINTF=vim_snprintf \
	  -DIS_COMBINING_FUNCTION=utf_iscomposing_uint \
	  -DWCWIDTH_FUNCTION=utf_uint2cells

objects/vterm_encoding.o: libvterm/src/encoding.c $(TERM_DEPS)
	$(CCCTERM) -o $@ libvterm/src/encoding.c

objects/vterm_keyboard.o: libvterm/src/keyboard.c $(TERM_DEPS)
	$(CCCTERM) -o $@ libvterm/src/keyboard.c

objects/vterm_mouse.o: libvterm/src/mouse.c $(TERM_DEPS)
	$(CCCTERM) -o $@ libvterm/src/mouse.c

objects/vterm_parser.o: libvterm/src/parser.c $(TERM_DEPS)
	$(CCCTERM) -o $@ libvterm/src/parser.c

objects/vterm_pen.o: libvterm/src/pen.c $(TERM_DEPS)
	$(CCCTERM) -o $@ libvterm/src/pen.c

objects/vterm_screen.o: libvterm/src/screen.c $(TERM_DEPS)
	$(CCCTERM) -o $@ libvterm/src/screen.c

objects/vterm_state.o: libvterm/src/state.c $(TERM_DEPS)
	$(CCCTERM) -o $@ libvterm/src/state.c

objects/vterm_unicode.o: libvterm/src/unicode.c $(TERM_DEPS) libvterm/src/fullwidth.inc
	$(CCCTERM) -o $@ libvterm/src/unicode.c

objects/vterm_vterm.o: libvterm/src/vterm.c $(TERM_DEPS)
	$(CCCTERM) -o $@ libvterm/src/vterm.c

CCCDIFF = $(CCC_NF) $(ALL_CFLAGS)

objects/xdiffi.o: xdiff/xdiffi.c $(XDIFF_INCL)
	$(CCCDIFF) -o $@ xdiff/xdiffi.c

objects/xprepare.o: xdiff/xprepare.c $(XDIFF_INCL)
	$(CCCDIFF) -o $@ xdiff/xprepare.c

objects/xutils.o: xdiff/xutils.c $(XDIFF_INCL)
	$(CCCDIFF) -o $@ xdiff/xutils.c

objects/xemit.o: xdiff/xemit.c $(XDIFF_INCL)
	$(CCCDIFF) -o $@ xdiff/xemit.c

objects/xhistogram.o: xdiff/xhistogram.c $(XDIFF_INCL)
	$(CCCDIFF) -o $@ xdiff/xhistogram.c

objects/xpatience.o: xdiff/xpatience.c $(XDIFF_INCL)
	$(CCCDIFF) -o $@ xdiff/xpatience.c


Makefile:
	@echo 'The name of the makefile MUST be "Makefile" (with capital M)!'











install_haiku_extra: $(DEST_BIN)/$(VIMTARGET) objects/os_haiku.rsrc
	xres -o $(DEST_BIN)/$(VIMTARGET) objects/os_haiku.rsrc
	mimeset	$(DEST_BIN)/$(VIMTARGET)




HAIKU_GLINKS = 	$(DEST_BIN)/$(GVIMTARGET) \
				$(DEST_BIN)/$(GVIEWTARGET) \
				$(DEST_BIN)/$(GVIMDIFFTARGET) \
				$(DEST_BIN)/$(RGVIMTARGET) \
				$(DEST_BIN)/$(RGVIEWTARGET)








installglinks_haiku: $(HAIKU_GLINKS) install_haiku_extra
	@catattr -r "BEOS:ICON" $(DEST_BIN)/$(GVIMTARGET) > ~icon.attr
	for i in $(HAIKU_GLINKS); do \
		rm $$i ; \
		echo "
		case $$i in \
			$(DEST_BIN)/$(GVIMTARGET)) \
				cp $(DEST_BIN)/$(VIMTARGET) $$i ; \
				if [ -f $(VIMTARGET).con ] ; then \
					$(STRIP) $(VIMTARGET).con ; \
					mv $(VIMTARGET).con $(DEST_BIN)/$(VIMTARGET) ; \
				fi ;; \
			$(DEST_BIN)/$(GVIEWTARGET))    printf "%s -R %c%c" $(GVIMTARGET) '$$' '*' >> $$i;; \
			$(DEST_BIN)/$(GVIMDIFFTARGET)) printf "%s -d %c%c" $(GVIMTARGET) '$$' '*' >> $$i;; \
			$(DEST_BIN)/$(RGVIMTARGET))    printf "%s -Z %c%c" $(GVIMTARGET) '$$' '*' >> $$i;; \
			$(DEST_BIN)/$(RGVIEWTARGET))   printf "%s -Z -R %c%c" $(GVIMTARGET) '$$' '*' >> $$i;; \
			*) printf "%s %c%c" $(GVIMTARGET) '$$' '*' >> $$i;; \
		esac ; \
		chmod $(BINMOD) $$i ; \
		addattr -f ~icon.attr  -t \'VICN\' BEOS:ICON $$i ; \
	done
	addattr -f ~icon.attr  -t \'VICN\' BEOS:ICON $(DEST_BIN)/$(VIMNAME)tutor
	@rm ~icon.attr




objects/alloc.o: alloc.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/arabic.o: arabic.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/arglist.o: arglist.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/autocmd.o: autocmd.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/beval.o: beval.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/blob.o: blob.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/blowfish.o: blowfish.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/buffer.o: buffer.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h version.h
objects/change.o: change.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/charset.o: charset.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/cindent.o: cindent.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/clientserver.o: clientserver.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/clipboard.o: clipboard.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/cmdexpand.o: cmdexpand.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/cmdhist.o: cmdhist.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/crypt.o: crypt.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/crypt_zip.o: crypt_zip.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/debugger.o: debugger.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/dict.o: dict.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/diff.o: diff.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h xdiff/xdiff.h xdiff/../vim.h
objects/digraph.o: digraph.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/drawline.o: drawline.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/drawscreen.o: drawscreen.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/edit.o: edit.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/eval.o: eval.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/evalbuffer.o: evalbuffer.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/evalfunc.o: evalfunc.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h version.h
objects/evalvars.o: evalvars.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h version.h
objects/evalwindow.o: evalwindow.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/ex_cmds.o: ex_cmds.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h version.h
objects/ex_cmds2.o: ex_cmds2.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h version.h
objects/ex_docmd.o: ex_docmd.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h ex_cmdidxs.h
objects/ex_eval.o: ex_eval.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/ex_getln.o: ex_getln.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/fileio.o: fileio.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/filepath.o: filepath.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/findfile.o: findfile.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/float.o: float.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/fold.o: fold.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/getchar.o: getchar.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/gui_xim.o: gui_xim.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/hardcopy.o: hardcopy.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h version.h
objects/hashtab.o: hashtab.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/help.o: help.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/highlight.o: highlight.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/if_cscope.o: if_cscope.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/if_xcmdsrv.o: if_xcmdsrv.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h version.h
objects/indent.o: indent.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/insexpand.o: insexpand.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/json.o: json.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/list.o: list.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/locale.o: locale.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/logfile.o: logfile.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/main.o: main.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/map.o: map.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/mark.o: mark.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/match.o: match.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/mbyte.o: mbyte.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/memfile.o: memfile.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/memline.o: memline.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/menu.o: menu.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/message.o: message.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/misc1.o: misc1.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h version.h
objects/misc2.o: misc2.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/mouse.o: mouse.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/move.o: move.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/normal.o: normal.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h nv_cmds.h nv_cmdidxs.h
objects/ops.o: ops.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/option.o: option.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h optiondefs.h
objects/optionstr.o: optionstr.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/os_unix.o: os_unix.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h os_unixx.h
objects/pathdef.o: auto/pathdef.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/popupmenu.o: popupmenu.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/popupwin.o: popupwin.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/profiler.o: profiler.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/pty.o: pty.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/quickfix.o: quickfix.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/regexp.o: regexp.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h regexp_bt.c regexp_nfa.c
objects/register.o: register.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/screen.o: screen.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/scriptfile.o: scriptfile.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/search.o: search.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/session.o: session.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/sha256.o: sha256.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/sign.o: sign.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/sound.o: sound.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/spell.o: spell.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/spellfile.o: spellfile.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/spellsuggest.o: spellsuggest.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/strings.o: strings.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/syntax.o: syntax.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/tag.o: tag.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/term.o: term.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/terminal.o: terminal.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/testing.o: testing.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/textformat.o: textformat.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/textobject.o: textobject.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/textprop.o: textprop.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/time.o: time.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/typval.o: typval.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/ui.o: ui.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/undo.o: undo.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/usercmd.o: usercmd.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/userfunc.o: userfunc.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/version.o: version.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h version.h
objects/vim9class.o: vim9class.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h vim9.h
objects/vim9cmds.o: vim9cmds.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h vim9.h
objects/vim9compile.o: vim9compile.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h vim9.h
objects/vim9execute.o: vim9execute.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h vim9.h
objects/vim9expr.o: vim9expr.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h vim9.h
objects/vim9instr.o: vim9instr.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h vim9.h
objects/vim9script.o: vim9script.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h vim9.h
objects/vim9type.o: vim9type.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h vim9.h
objects/viminfo.o: viminfo.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h version.h
objects/window.o: window.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/bufwrite.o: bufwrite.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/gui.o: gui.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/gui_gtk.o: gui_gtk.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h gui_gtk_f.h
objects/gui_gtk_f.o: gui_gtk_f.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h gui_gtk_f.h
objects/gui_motif.o: gui_motif.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h gui_xmebw.h \
 ../pixmaps/alert.xpm ../pixmaps/error.xpm ../pixmaps/generic.xpm \
 ../pixmaps/info.xpm ../pixmaps/quest.xpm gui_x11_pm.h \
 ../pixmaps/tb_new.xpm ../pixmaps/tb_open.xpm ../pixmaps/tb_close.xpm \
 ../pixmaps/tb_save.xpm ../pixmaps/tb_print.xpm ../pixmaps/tb_cut.xpm \
 ../pixmaps/tb_copy.xpm ../pixmaps/tb_paste.xpm ../pixmaps/tb_find.xpm \
 ../pixmaps/tb_find_next.xpm ../pixmaps/tb_find_prev.xpm \
 ../pixmaps/tb_find_help.xpm ../pixmaps/tb_exit.xpm \
 ../pixmaps/tb_undo.xpm ../pixmaps/tb_redo.xpm ../pixmaps/tb_help.xpm \
 ../pixmaps/tb_macro.xpm ../pixmaps/tb_make.xpm \
 ../pixmaps/tb_save_all.xpm ../pixmaps/tb_jump.xpm \
 ../pixmaps/tb_ctags.xpm ../pixmaps/tb_load_session.xpm \
 ../pixmaps/tb_save_session.xpm ../pixmaps/tb_new_session.xpm \
 ../pixmaps/tb_blank.xpm ../pixmaps/tb_maximize.xpm \
 ../pixmaps/tb_split.xpm ../pixmaps/tb_minimize.xpm \
 ../pixmaps/tb_shell.xpm ../pixmaps/tb_replace.xpm \
 ../pixmaps/tb_vsplit.xpm ../pixmaps/tb_maxwidth.xpm \
 ../pixmaps/tb_minwidth.xpm
objects/gui_xmdlg.o: gui_xmdlg.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/gui_xmebw.o: gui_xmebw.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h gui_xmebwp.h gui_xmebw.h
objects/gui_gtk_x11.o: gui_gtk_x11.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h auto/gui_gtk_gresources.h \
 gui_gtk_f.h ../runtime/vim32x32.xpm ../runtime/vim16x16.xpm \
 ../runtime/vim48x48.xpm
objects/gui_x11.o: gui_x11.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h ../runtime/vim32x32.xpm ../runtime/vim16x16.xpm \
 ../runtime/vim48x48.xpm
objects/gui_haiku.o: gui_haiku.cc vim.h protodef.h auto/config.h feature.h \
 os_unix.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h version.h
objects/json_test.o: json_test.c main.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h json.c
objects/kword_test.o: kword_test.c main.c vim.h protodef.h auto/config.h \
 feature.h os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h \
 option.h beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h charset.c
objects/memfile_test.o: memfile_test.c main.c vim.h protodef.h auto/config.h \
 feature.h os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h \
 option.h beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h memfile.c
objects/message_test.o: message_test.c main.c vim.h protodef.h auto/config.h \
 feature.h os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h \
 option.h beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h message.c
objects/if_lua.o: if_lua.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h version.h
objects/if_mzsch.o: if_mzsch.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h if_mzsch.h
objects/if_perl.o: auto/if_perl.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/if_perlsfio.o: if_perlsfio.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/if_python.o: if_python.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h if_py_both.h
objects/if_python3.o: if_python3.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h if_py_both.h
objects/if_tcl.o: if_tcl.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/if_ruby.o: if_ruby.c protodef.h auto/config.h vim.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h version.h
objects/gui_beval.o: gui_beval.c vim.h protodef.h auto/config.h feature.h \
 os_unix.h auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h \
 beval.h proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h errors.h
objects/netbeans.o: netbeans.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h version.h
objects/job.o: job.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/channel.o: channel.c vim.h protodef.h auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h ex_cmds.h spell.h proto.h \
 globals.h errors.h
objects/gui_gtk_gresources.o: auto/gui_gtk_gresources.c
objects/vterm_encoding.o: libvterm/src/encoding.c libvterm/src/vterm_internal.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h \
 libvterm/src/encoding/DECdrawing.inc libvterm/src/encoding/uk.inc
objects/vterm_keyboard.o: libvterm/src/keyboard.c libvterm/src/vterm_internal.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h \
 libvterm/src/utf8.h
objects/vterm_mouse.o: libvterm/src/mouse.c libvterm/src/vterm_internal.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h \
 libvterm/src/utf8.h
objects/vterm_parser.o: libvterm/src/parser.c libvterm/src/vterm_internal.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h
objects/vterm_pen.o: libvterm/src/pen.c libvterm/src/vterm_internal.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h
objects/vterm_screen.o: libvterm/src/screen.c libvterm/src/vterm_internal.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h \
 libvterm/src/rect.h libvterm/src/utf8.h
objects/vterm_state.o: libvterm/src/state.c libvterm/src/vterm_internal.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h
objects/vterm_unicode.o: libvterm/src/unicode.c libvterm/src/vterm_internal.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h \
 libvterm/src/fullwidth.inc
objects/vterm_vterm.o: libvterm/src/vterm.c libvterm/src/vterm_internal.h \
 libvterm/include/vterm.h libvterm/include/vterm_keycodes.h \
 libvterm/src/utf8.h
objects/xdiffi.o: xdiff/xdiffi.c xdiff/xinclude.h auto/config.h \
 xdiff/xmacros.h xdiff/xdiff.h vim.h protodef.h \
 auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h \
 termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h \
 errors.h xdiff/xtypes.h xdiff/xutils.h xdiff/xprepare.h \
 xdiff/xdiffi.h xdiff/xemit.h
objects/xemit.o: xdiff/xemit.c xdiff/xinclude.h auto/config.h \
 xdiff/xmacros.h xdiff/xdiff.h vim.h protodef.h \
 auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h \
 termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h \
 errors.h xdiff/xtypes.h xdiff/xutils.h xdiff/xprepare.h \
 xdiff/xdiffi.h xdiff/xemit.h
objects/xprepare.o: xdiff/xprepare.c xdiff/xinclude.h auto/config.h \
 xdiff/xmacros.h xdiff/xdiff.h vim.h protodef.h \
 auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h \
 termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h \
 errors.h xdiff/xtypes.h xdiff/xutils.h xdiff/xprepare.h \
 xdiff/xdiffi.h xdiff/xemit.h
objects/xutils.o: xdiff/xutils.c xdiff/xinclude.h auto/config.h \
 xdiff/xmacros.h xdiff/xdiff.h vim.h protodef.h \
 auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h \
 termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h \
 errors.h xdiff/xtypes.h xdiff/xutils.h xdiff/xprepare.h \
 xdiff/xdiffi.h xdiff/xemit.h
objects/xhistogram.o: xdiff/xhistogram.c xdiff/xinclude.h auto/config.h \
 xdiff/xmacros.h xdiff/xdiff.h vim.h protodef.h \
 auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h \
 termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h \
 errors.h xdiff/xtypes.h xdiff/xutils.h xdiff/xprepare.h \
 xdiff/xdiffi.h xdiff/xemit.h
objects/xpatience.o: xdiff/xpatience.c xdiff/xinclude.h auto/config.h \
 xdiff/xmacros.h xdiff/xdiff.h vim.h protodef.h \
 auto/config.h feature.h os_unix.h \
 auto/osdef.h ascii.h keymap.h \
 termdefs.h macros.h option.h beval.h \
 proto/gui_beval.pro structs.h regexp.h gui.h \
 libvterm/include/vterm.h \
 libvterm/include/vterm_keycodes.h alloc.h \
 ex_cmds.h spell.h proto.h globals.h \
 errors.h xdiff/xtypes.h xdiff/xutils.h xdiff/xprepare.h \
 xdiff/xdiffi.h xdiff/xemit.h
