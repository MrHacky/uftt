ADD_LIBRARY(lib-util
	Base64.h               Base64.cpp
	ScopeExitSignal.h      ScopeExitSignal.cpp
	Filesystem.h
	PosixError.h
	asio_timer_oneshot.h
	StrFormat.h
)

ADD_EXECUTABLE(bin2c
	bin2c.c
)
