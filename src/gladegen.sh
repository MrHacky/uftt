echo "#include \"stdafx.h\""
echo "#include \"gladebuf.h\""
echo "char gladebuf[] ="
cat ${1} | sed s:\\\\:\\\\\\\\:g | sed s:\":\\\\\":g | sed s:$:\": | sed s:^:\":
echo ";"
echo "int gladebufsize = sizeof(gladebuf);"

