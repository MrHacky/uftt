File(STRINGS "${INPUT}" LINES REGEX "^r[0-9]*$")
List(REVERSE LINES)
List(GET LINES 0 LINE)
String(SUBSTRING "${LINE}" 1 -1 REV)

File(WRITE  "${OUTPUT}" "// generated file\n")
File(APPEND "${OUTPUT}" "#define SVN_REVISION_STRING \"${REV}\"\n")
