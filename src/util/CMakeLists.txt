ADD_LIBRARY(lib-util
	Base64.h               Base64.cpp
	ScopeExitSignal.h      ScopeExitSignal.cpp
	SettingsManager.h      SettingsManager.cpp
	Filesystem.h           Filesystem.cpp
	SignalConnection.h     SignalConnection.cpp
	Coro.h                 coroutine.hpp
	PosixError.h
	asio_timer_oneshot.h
	asio_sync_dispatcher.h
	StrFormat.h
)

ADD_EXECUTABLE(bin2c EXCLUDE_FROM_ALL
	bin2c.cpp
)
