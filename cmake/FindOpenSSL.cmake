find_package(ExtraLibDir)

FIND_LIB_IN_EXTRALIBS(OPENSSL *openssl* include lib)

SET(OPENSSL_FOUND FALSE)

Find_Path(OPENSSL_INCLUDE_DIR
	openssl/opensslv.h
	/usr/include/ /usr/local/include/
	${OPENSSL_EXTRALIB_INCLUDE_PATHS}
)

Find_Library(OPENSSL_LIBRARY
	ssleay32
	/usr/lib /usr/local/lib
	${OPENSSL_EXTRALIB_LIBRARY_PATHS}
)

Find_Library(EAY32_LIBRARY
	libeay32
	/usr/lib /usr/local/lib
	${OPENSSL_EXTRALIB_LIBRARY_PATHS}
)

IF(OPENSSL_INCLUDE_DIR AND OPENSSL_LIBRARY)
	SET(OPENSSL_FOUND TRUE)
	MESSAGE(STATUS "Found OpenSSL library")
ENDIF(OPENSSL_INCLUDE_DIR AND OPENSSL_LIBRARY)
