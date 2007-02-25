rm -f gladebuf.cpp
touch gladebuf.cpp
echo "#include \"stdafx.h\"" >> gladebuf.cpp
echo "#include \"gladebuf.h\"" >> gladebuf.cpp
echo "char gladebuf[] =" >> gladebuf.cpp
#echo \"\\ >> gladebuf.cpp
cat uftt.glade | sed s:\\\\:\\\\\\\\:g | sed s:\":\\\\\":g | sed s:$:\": | sed s:^:\": >> gladebuf.cpp
#echo \" >> gladebuf.cpp
echo ";" >> gladebuf.cpp
echo "int gladebufsize = sizeof(gladebuf);" >> gladebuf.cpp
