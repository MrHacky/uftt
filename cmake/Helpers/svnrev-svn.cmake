File(STRINGS "${INPUT}" LINES)
List(GET LINES 2 REV)

File(WRITE  "${OUTPUT}" "// generated file\n")
File(APPEND "${OUTPUT}" "#define SVN_REVISION_STRING \"${REV}\"\n")
