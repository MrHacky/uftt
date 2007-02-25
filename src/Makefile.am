bin_PROGRAMS = uftt
uftt_SOURCES = gooey.cpp main.cpp gladebuf.cpp gooey.h main.h gladebuf.h stdafx.h 
noinst_HEADERS = gooey.h main.h gladebuf.h stdafx.h 

uftt_LDADD = $(DEPS_LIBS)
AM_CPPFLAGS = $(DEPS_CFLAGS)

gladebuf.cpp: gladebuf.h stdafx.h uftt.glade gladegen.sh
	sh gladegen.sh
