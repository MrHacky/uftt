cmake_policy(SET CMP0007 NEW)

File(STRINGS "${INPUT}" LINES)
List(GET LINES 3 REV)

File(WRITE  "${OUTPUT}" "// generated file\n")
File(APPEND "${OUTPUT}" "#define SVN_REVISION_STRING \"${REV}\"\n")
