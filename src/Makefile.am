#/***************************************************************************
# * Все права на этот код 'Dream Land' пренадлежат Igor {Leo} и Olga {Varda}*
# * Некоторую помощь в написании этого кода, а также своими идеями помогали:*
# *    Igor S. Petrenko     {NoFate, Demogorgon}                            *
# *    Koval Nazar          {Nazar, Redrum}                                 *
# *    Doropey Vladimir     {Reorx}                                         *
# *    Kulgeyko Denis       {Burzum}                                        *
# *    Andreyanov Aleksandr {Manwe}                                         *
# *    Zadvinsky Alexandr   {Kiddy}                                         *
# *    и все остальные, кто советовал и играл в этот MUD                    *
# ***************************************************************************/

if WITH_MINGW
DUMMY_LIBS = libdummycpp.la
MY_CXXLD = $(CC)
LD_DLLGLUE = -Wl,--enable-runtime-pseudo-reloc -Wl,--script,$(top_srcdir)/win/i386pe.x-no-rdata -Wl,-S
CC_DLLGLUE = -fdata-sections
#LIBSTDCXX=/root/mingw/i586-pc-mingw32/lib/libstdc++.a
LIBSTDCXX=/usr/local/mingw/mingw32/lib/libstdc++.a

libdummycpp.la: $(LIBSTDCXX)
	cp $(LIBSTDCXX) .libs/libdummycpp.a
	echo "# Generated by libtool... oh, realy? :)" > libdummycpp.la
	echo "dlname=''" >> libdummycpp.la
	echo "library_names=''" >> libdummycpp.la
	echo "old_library='libdummycpp.a'" >> libdummycpp.la
	echo "dependency_libs=''" >> libdummycpp.la
	echo "current='5'" >> libdummycpp.la
	echo "age='0'" >> libdummycpp.la
	echo "revision='5'" >> libdummycpp.la
	echo "installed=no" >> libdummycpp.la
	echo "shouldnotlink=no" >> libdummycpp.la
	echo "dlopen=''" >> libdummycpp.la
	echo "dlpreopen=''" >> libdummycpp.la
	echo "libdir=''" >> libdummycpp.la
	
else
DUMMY_LIBS =
LD_DLLGLUE =
CC_DLLGLUE =
MY_CXXLD = $(CXX)
endif

lib_LTLIBRARIES = libdreamland.la libdreamland_impl.la

bin_PROGRAMS = dreamland

AM_LDFLAGS = -no-undefined $(LD_DLLGLUE)
AM_CXXFLAGS = -Wall $(CC_DLLGLUE) 
CXXLD = $(MY_CXXLD)

SUBDIRS =  \
regex      \
lang       \
alloc      \
util       \
l10n       \
xml        \
flags      \
gref       \
process    \
plugin     \
scheduler  \
io         \
fenia      \
core

include $(top_srcdir)/src/Makefile.inc
src_INCLUDES = $(INCLUDES_SRC)

#####################################################
# dreamland shared library
#
libdreamland_la_SOURCES =

libdreamland_la_LIBADD = \
fenia/libfenia.la \
xml/libxml.la \
lang/liblang.la \
l10n/libl10n.la \
io/libio.la \
plugin/libplugin.la \
util/libutil.la \
scheduler/libscheduler.la \
regex/libregex.la \
alloc/liballoc.la \
gref/libgref.la \
flags/libflags.la \
process/libprocess.la \
$(DUMMY_LIBS) @BDB_LIBS@ @MCCP_LIBS@ @GC_LIBS@ -lcrypto++

#####################################################
# dreamland implementation library
#
libdreamland_impl_la_SOURCES = \
autoflags.cpp \
db.cpp \
dreamland.cpp 

libdreamland_impl_la_MOC = \
dreamland.h 

libdreamland_impl_la_LIBADD = \
libdreamland.la \
version.lo \
core/libcore.la \
core/fenia/libcorefenia.la \
core/behavior/libbehavior.la \
core/scheduler/libcorescheduler.la \
core/clan/libclan.la \
core/skills/libskills.la 

#####################################################
# dreamland binary
#
dreamland_SOURCES = main.cpp 

dreamland_LDADD = libdreamland_impl.la libdreamland.la


#####################################################
# automaticaly generated sources
#
autoflags.cpp: $(srcdir)/makebits.pl $(srcdir)/bits.conf
	$(srcdir)/makebits.pl -s $(srcdir)/bits.conf -r -f $@

autoflags.h: $(srcdir)/makebits.pl $(srcdir)/bits.conf
	$(srcdir)/makebits.pl -s $(srcdir)/bits.conf -h -r -f $@

all-recursive: autoflags.h 

version.cpp: $(dreamland_OBJECTS) $(srcdir)/newvers.sh
	sh $(srcdir)/newvers.sh

CLEANFILES = autoflags.cpp autoflags.h version.cpp $(DUMMY_LIBS)
AM_CPPFLAGS = $(src_INCLUDES)
