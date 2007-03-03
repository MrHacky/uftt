bin_PROGRAMS = uftt
uftt_SOURCES = gladebuf.h gooey.cpp gooey.h main.cpp main.h 	sharelister.cpp \
	stdafx.h yarn.cpp sharelister_gui.cpp
noinst_HEADERS = gladebuf.h gooey.h main.h sharelister.h stdafx.h yarn.h \
	sharelister_gui.h

nodist_uftt_SOURCES = gladebuf.cpp
CLEANFILES = gladebuf.cpp

uftt_LDADD = $(DEPS_LIBS)
AM_CPPFLAGS = $(DEPS_CFLAGS)

gladebuf.cpp: gladebuf.h stdafx.h uftt.glade gladegen.sh
	sh $(srcdir)/gladegen.sh "$(srcdir)/uftt.glade" >$@
