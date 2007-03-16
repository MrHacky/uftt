bin_PROGRAMS = uftt uftt.old
uftt_SOURCES = gladebuf.h gooey.cpp gooey.h main.cpp main.h packet.cpp \
	servert.cpp sharelister.cpp sharelister_gui.cpp stdafx.h yarn.cpp
noinst_HEADERS = gladebuf.h gooey.h main.h packet.h servert.h sharelister.h \
	sharelister_gui.h stdafx.h yarn.h

nodist_uftt_SOURCES = gladebuf.cpp
CLEANFILES = gladebuf.cpp

uftt_LDADD = $(DEPS_LIBS)
AM_CPPFLAGS = $(DEPS_CFLAGS)

gladebuf.cpp: gladebuf.h stdafx.h uftt.glade gladegen.sh
	sh $(srcdir)/gladegen.sh "$(srcdir)/uftt.glade" >$@

uftt.old: uftt
	cp uftt uftt.old
