bin_PROGRAMS = uftt
uftt_SOURCES = gladebuf.cpp gladebuf.h gooey.cpp gooey.h main.cpp main.h \
	stdafx.h yarn.cpp
noinst_HEADERS = gladebuf.h gooey.h main.h stdafx.h yarn.h

uftt_LDADD = $(DEPS_LIBS)
AM_CPPFLAGS = $(DEPS_CFLAGS)

gladebuf.cpp: gladebuf.h stdafx.h uftt.glade gladegen.sh
	sh gladegen.sh
