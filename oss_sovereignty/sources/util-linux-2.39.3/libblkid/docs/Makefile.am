


AUTOMAKE_OPTIONS = 1.10







DOC_MODULE=libblkid





DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.xml





DOC_SOURCE_DIR=../src


SCANGOBJ_OPTIONS=



SCAN_OPTIONS=--deprecated-guards="BLKID_DISABLE_DEPRECATED"



MKDB_OPTIONS=--sgml-mode --output-format=xml --name-space blkid



MKTMPL_OPTIONS=


MKHTML_OPTIONS=



FIXXREF_OPTIONS=




HFILE_GLOB=$(top_builddir)/libblkid/src/blkid.h
CFILE_GLOB=$(top_srcdir)/libblkid/src/*.c



EXTRA_HFILES=



IGNORE_HFILES=blkidP.h partitions.h superblocks.h \
	      topology.h aix.h dos.h



HTML_IMAGES=



content_files = $(builddir)/version.xml $(srcdir)/libblkid-config.xml




expand_content_files=






GTKDOC_CFLAGS=
GTKDOC_LIBS=


include $(top_srcdir)/config/gtk-doc.make



EXTRA_DIST += version.xml.in $(srcdir)/libblkid-config.xml




DISTCLEANFILES += version.xml

