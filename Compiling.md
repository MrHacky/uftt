# Visual Studio #

**Install Visual Studio**
  * Get [Visual C++ Express Edition](http://www.microsoft.com/express/Downloads/#2010-Visual-CPP)
  * Select language to start installer download

**Install CMake**
  * http://www.cmake.org/files/v2.8/cmake-2.8.4-win32-x86.exe

**Prepare Library Directory**
  * Create an 'extralibs' directory inside your uftt source checkout, or next to your source checkout (one directory up)
  * We will install external libraries here

**Install Qt**
  * Get Qt (pick one corresponding to your compiler)
    * [Qt For Visual Studio 2010](http://qt-win-binaries.googlecode.com/files/Qt-4.7.3-dev-msvc2010-rdh.7z)
    * [Qt For Visual Studio 2008](http://qt-win-binaries.googlecode.com/svn/tags/Qt-4.4.3-dev-msvc2008e-rdh.7z)
    * [Qt For Visual Studio 2005](http://qt-win-binaries.googlecode.com/svn/tags/Qt-4.4.3-dev-msvc2005e-rdh.7z)
  * Unpack it somewhere ('extralibs' directory)
    * `%QTDIR%` will refer to the directory you unpacked
  * Add `%QTDIR%\bin` to the System Path
    * My Computer, right click, properties, advanced, Environment Variables, PATH system variable

**Install Boost**
  * Download this [installboost.bat](http://servertje.homeip.net:40080/misc/installboost.bat?boostver=1_44_0&libs=date_time,filesystem,serialization,signals,system,thread&arch=x86&variants=sdh,srh) batch file.
  * Put it in your 'extralibs' directory
    * Run it to compile boost
    * You must keep the 'boost\_1\_44\_0' directory
    * You can delete the created 'build' directory, the 'boost\_1\_44\_0.7z' file, and the 'installboost.bat' file

**Compile**

  * Run CMake
    * Source dir is trunk of the checkout
    * Create builddir somewhere (ex: trunk/build)
    * Configure
      * Select your compiler (Visual Studio 10)
    * Configure again
    * Generate

  * Open project file in builddir (UFTT.sln)
    * and compile!