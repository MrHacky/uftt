# Boost includes and libraries.

IF ( WIN32 )
	# Assumes BOOST_ROOT and BOOST_THREAD_* are set in your path statement.
	# If not then either add them to your path statement or edit the following
	# lines putting in the absolute paths to the boost includes and libraries.
	SET ( Boost_Root
		#    $(BOOST_ROOT)
		#    $ENV{BOOST_ROOT}
		"C:/boost/boost_1_33_0"
		CACHE PATH
		"Path to the boost root directory."
	)
	SET(BOOST_INCLUDE_PATH
		${Boost_Root}
		CACHE PATH
		"Path to the Boost include files.")
ELSE ( WIN32 )
	# In Linux the includes are generally in the system paths. I.e /usr/include/boost
	# In Linux the libs are generally in the system paths. I.e. /usr/lib ->
	# If you build and install boost yourself, the headers are in: /usr/local/include/boost-1_33/
	# and the libraries are in: /usr/local/lib/

	SET(BOOST_LIB_SUFFIX
		-gcc41
		CACHE STRING
		"suffix to use for boost libraries"
	)

	#  "Path to the Boost multi-threaded thread debug libraries.")

	SET(BOOST_INCLUDE_PATH
		/usr/local/include/boost-1_33/
		CACHE PATH
		"Path to the Boost include files.")
ENDIF ( WIN32 )
