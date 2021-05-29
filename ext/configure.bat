<<<<<<< HEAD
@Echo off
rem Toolset
@rem set TOOLSET=gcc
set TOOLSET=msvc

rem echos
set c_menu=[36m
set c_normal=[0m
set c_done=[32m
set c_fail=[91m
set c_do=[95m
set c_skip=[32m

rem Versions
rem boost 1.76.0
set BOOST_VER=1_76_0
set BOOST_VER2=1.76.0
set BOOST_VER3=1_76

rem libtorrent 1.1.14 Mar 25, 2020
set LIBTORRENT_VER2=1.1.14
set LIBTORRENT_VER=1_1_14

rem libwebp 1.2.0
set LIBWEBP_VER=1.2.0

rem mingw-w64 8.1.0
set GCC_VERSION=8.1.0
set GCC_VERSION2=81

if %TOOLSET%==msvc set MSVC_VERSION=2019\Preview

if %TOOLSET%==gcc set MINGW_PATH=C:\mingw

rem GCC 32-bit
set GCC_PATH=%MINGW_PATH%\mingw32
set GCC_PREFIX1=/i686-w64-mingw32
set GCC_PREFIX=\i686-w64-mingw32

rem GCC 64-bit
set GCC64_PATH=%MINGW_PATH%\mingw64
set GCC64_PREFIX1=/x86_64-w64-mingw32
set GCC64_PREFIX=\x86_64-w64-mingw32

rem GCC (common)
rem -w inhibit all warning messages
if %TOOLSET%==gcc set TOOLSET2=gcc
set EXTRA_OPTIONS="cxxflags=-fexpensive-optimizations -fomit-frame-pointer -D IPV6_TCLASS=30 -w"
set LIBBOOST32="%GCC_PATH%%GCC_PREFIX%\lib\libboost_system_tr.a"
set LIBBOOST64="%GCC64_PATH%%GCC64_PREFIX%\lib\libboost_system_tr.a"
set LIBWEBP="%GCC64_PATH%%GCC64_PREFIX%\lib\libwebp.a"
set LIBTORREN32="%GCC_PATH%%GCC_PREFIX%\lib\libtorrent.a"
set LIBTORREN64="%GCC64_PATH%%GCC64_PREFIX%\lib\libtorrent.a"

rem BOOST
set BOOST_ROOT=%CD%\boost
set BOOST_BUILD_PATH=%BOOST_ROOT%\tools\build
echo %BOOST_ROOT%
echo %BOOST_BUILD_PATH%
set PATH=%PATH%;%BOOST_BUILD_PATH%
@rem set BOOST_INSTALL_PATH=D:\BOOST32_%GCC_VERSION2%
@rem set BOOST64_INSTALL_PATH=D:\BOOST64_%GCC_VERSION2%

rem Configure paths
cd %~dp0
set LIBTORRENT_PATH=%CD%\libtorrent
set WEBP_PATH=%CD%\libwebp
set path=%BOOST_ROOT%;%MSYS_BIN%;%path%
if %TOOLSET%==gcc set path=%GCC_PATH%\bin;%GCC64_PATH%\bin;%path%

rem Visual Studio
if %TOOLSET%==gcc goto skipmscv
set TOOLSET=msvc-14.2
set TOOLSET2=msvc
set EXTRA_OPTIONS=--abbreviate-paths install-dependencies=on install-type=lib runtime-link=static deprecated-functions=off i2p=off extensions=off
set LIBDIR=%CD%\..\lib
set MSVC_PATH=C:\Program Files (x86)\Microsoft Visual Studio\%MSVC_VERSION%
call "%MSVC_PATH%\VC\Auxiliary\Build\vcvars64.bat"
set LIBWEBP=%LIBDIR%\Release_x64\libwebp.lib
set LIBTORREN32=%LIBDIR%\Release_Win32\libtorrent.lib
set LIBTORREN64=%LIBDIR%\Release_x64\libtorrent.lib
:skipmscv

if %TOOLSET2%==msvc goto mainmenu

rem Check for MinGW
@rem if /I not exist "%GCC_PATH%\bin" (echo %c_fail%&echo ERROR: MinGW not found in %GCC_PATH% & goto fatalError)

rem Check for MinGW64
if /I not exist "%GCC64_PATH%\bin" (echo %c_fail%&echo ERROR: MinGW_64 not found in %GCC64_PATH%[0m & goto fatalError)

rem Check for MSYS
if /I not exist "%MSYS_PATH%" (echo %c_fail%\e[0m&echo ERROR: MSYS not found in %MSYS_PATH%[0m & goto fatalError)

:mainmenu
cls
echo %c_menu%
echo.
echo   ÉÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ»
echo   ºMAIN MENU                º
echo   ÇÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄº
echo   ºA - Install all          º 
echo   ºC - Check all            º
echo   ºD - Delete all           º
echo   ºB - Build BOOST          º
echo   ºT - Build libttorret     º
echo   ºW - Build WebP           º
echo   ºQ - Quit                 º
echo   ÈÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ¼
echo.
set /P MENU=Enter command:

echo %c_normal%
cls
if /I "%menu%"=="a" call :installall
if /I "%menu%"=="d" call :delall
if /I "%menu%"=="c" call :checkall
if /I "%menu%"=="t" goto :installtorrent
if /I "%menu%"=="w" goto :installwebp
if /I "%menu%"=="b" call :buildboost
if /I "%menu%"=="q" exit
goto mainmenu

:delall
echo.

echo Checking for hanging g++.exe
tasklist /FI "IMAGENAME eq g++.exe" 2>NUL | find /I /N "g++.exe" >NUL 2>NUL
if %ERRORLEVEL%==0 taskkill /im g++.exe /f /t
echo.

echo Deleting libtorrent
rd /S /Q "%GCC_PATH%%GCC_PREFIX%\include\libtorrent" 2>nul
rd /S /Q "%GCC64_PATH%%GCC64_PREFIX%\include\libtorrent" 2>nul
del %LIBTORREN32% 2>nul
del "%GCC_PATH%%GCC_PREFIX%\lib\libtorrent_dbg.a" 2>nul
del %LIBTORREN64% 2>nul
del "%GCC64_PATH%%GCC64_PREFIX%\lib\libtorrent_dbg.a" 2>nul
rd /S /Q "%LIBTORRENT_PATH%"

echo Deleting webp
rd /S /Q "%GCC_PATH%%GCC_PREFIX%\include\libwebp" 2>nul
rd /S /Q "%GCC64_PATH%%GCC64_PREFIX%\include\libwebp" 2>nul
del "%GCC_PATH%%GCC_PREFIX%\lib\libwebp.*" 2>nul
del "%GCC64_PATH%%GCC64_PREFIX%\lib\libwebp.*" 2>nul
rd /S /Q "%MSYS_PATH%\home\libwebp-%LIBWEBP_VER%" 2>nul
del "%MSYS_PATH%\makewebp.bat" 2>nul
del "%MSYS_PATH%\home\makewebp.bat" 2>nul
del "%MSYS_PATH%\home\libwebp-%LIBWEBP_VER%.tar.gz" 2>nul

echo Deleting boost
rd /S /Q "%BOOST_ROOT%"

echo %c_done% "DONE"
echo.
echo.
pause
goto :eof

:checkall
echo.
echo Toolset:       %TOOLSET%
if %TOOLSET%==msvc echo MSVC:          %MSVC_PATH%
if %TOOLSET%==gcc echo GCC (32 bit):  %GCC_PATH%
if %TOOLSET%==gcc echo GCC (64 bit):  %GCC64_PATH%
echo MSYS:          %MSYS_PATH%
echo WebP:          %WEBP_PATH%
echo libtorrent:    %LIBTORRENT_PATH%
echo BOOST_src:     %BOOST_ROOT%
echo BOOST_dest:    %BOOST_INSTALL_PATH%
echo BOOST64_dest:  %BOOST64_INSTALL_PATH%
echo.

echo|set /p=Checking make...................
if /I exist "%MSYS_BIN%\make.exe" (echo %c_done% "OK") else (echo %c_fail% "FAIL")
echo.

echo|set /p=Checking BOOST(source)..........
if /I exist "%BOOST_ROOT%\boost.png" (echo %c_done% "OK") else (echo %c_fail% "FAIL")
echo.


echo|set /p=Checking b2...................
if /I exist "%BOOST_BUILD_PATH%\b2.exe" (echo %c_done% "OK") else (echo %c_fail% "FAIL")
echo.

echo|set /p=Checking WebP...................
if /I exist %LIBWEBP% (echo %c_done% "OK") else (echo %c_fail% "FAIL")
echo.

echo|set /p=Checking libtorrent(source).....
if /I exist "%LIBTORRENT_PATH%\examples\client_test.cpp" (echo %c_done% "OK") else (echo %c_fail% "FAIL")
echo.

echo|set /p=Checking libtorrent(binaries32).
if /I exist %LIBTORREN32% (echo %c_done% "OK") else (echo %c_fail% "FAIL")
echo.

echo|set /p=Checking libtorrent(binaries64).
if /I exist %LIBTORREN64% (echo %c_done% "OK") else (echo %c_fail% "FAIL")
echo.

echo.
pause
goto :eof

:installall
echo.

del "%MSYS_PATH%\var\lib\pacman\db.lck" 2>nul

rem download wget
if /I exist "%MSYS_BIN%\wget.exe" (echo %c_skip% "Skipping downloading wget"[0m&echo. & goto skipwget)
echo %c_do% "Downloading wget"[0m&echo.
%MSYS_BIN%\pacman.exe -S wget --noconfirm
:skipwget

rem download make
if /I exist "%MSYS_BIN%\make.exe" (echo %c_skip% "Skipping downloading make"[0m&echo. & goto skipmake)
echo %c_do% "Downloading make"[0m&echo.
%MSYS_BIN%\pacman.exe -S make --noconfirm
:skipmake

rem update toolchain
echo %c_do% "Updating toolchain"[0m&echo.
%MSYS_BIN%\pacman.exe -Syu --noconfirm
call :copyecho %GCC64_PATH%\bin\libwinpthread-1.dll %GCC64_PATH%\libexec\gcc\x86_64-w64-mingw32\%GCC_VERSION%

rem update boost, libtorrent, libwebp
git submodule update --recursive --remote --merge

rem Creating dirs for libs
if %TOOLSET%==gcc goto skipcreatelibdir
mkdir %LIBDIR%\Release_Win32
mkdir %LIBDIR%\Release_x64
mkdir %LIBDIR%\Debug_Win32
mkdir %LIBDIR%\Debug_x64
:skipcreatelibdir

:buildboost
rem Build b2.exe
@rem if /I exist "%BOOST_BUILD_PATH%\b2.exe" (echo %c_skip% "Skipping building b2.exe"[0m&echo. & goto skipbuildb2)
echo %c_do% "Building B2"[0m&echo.
pushd %BOOST_BUILD_PATH%
set SAVED_TOOLSET=%TOOLSET%
call bootstrap.bat %TOOLSET2%
set TOOLSET=%SAVED_TOOLSET%
popd
:skipbuildb2

rem Install BOOST (32-bit)
if /I exist "%BOOST_INSTALL_PATH%\include\boost\version.hpp" (echo %c_skip% "Skipping installing BOOST32"[0m&echo. & goto skipinstallboost32)
@rem if %BOOST_VER2% LSS 1.75.0 (call :copyecho "libtorrent_patch\socket_types.hpp" "%BOOST_ROOT%\boost\asio\detail\socket_types.hpp" /Y) // WSPiApi.h not required on Windows XP or newer
@rem pushd %BOOST_ROOT%
@rem echo %c_do% "Installing BOOST32"[0m&echo.
@rem rem BOOST_USE_WINAPI_VERSION=0x0501 = Win XP
@rem b2.exe install toolset=%TOOLSET% release --layout=tagged -j%NUMBER_OF_PROCESSORS% define=BOOST_USE_WINAPI_VERSION=0x05010000 --show-locate-target --without-mpi
@rem popd
:skipinstallboost32

rem Install BOOST (64-bit)
if /I exist "%BOOST64_INSTALL_PATH%\include\boost\version.hpp" (echo %c_skip% "Skipping installing BOOST64"[0m&echo. & goto skipinstallboost64)
pushd %BOOST_ROOT%
set oldpath=%path%
set path=%GCC64_PATH%\bin;%BOOST_ROOT%;%MSYS_BIN%;%path%
echo %c_do% "Installing BOOST64"[0m&echo.
b2.exe toolset=%TOOLSET% --with-thread --layout=tagged address-model=64
set path=%oldpath%
popd
:skipinstallboost64

echo.
echo %c_done% "DONE"[0m&echo.
echo.
pause

rem Install webp
if /I exist %LIBWEBP% (echo %c_skip% "Skipping installing WebP"[0m&echo. & goto skipprepwebp)
:installwebp
echo %c_do% "Installing WebP"[0m&echo.
echo %GCC_PATH% /mingw32> %MSYS_PATH%\etc\fstab
echo %GCC64_PATH% /mingw64>> %MSYS_PATH%\etc\fstab
pushd %WEBP_PATH%
rem if "%TOOLSET%"=="msvc" del %MSYS_PATH%\etc\fstab
call "%MSVC_PATH%\VC\Auxiliary\Build\vcvars64.bat"
nmake /f Makefile.vc CFG=release-static RTLIBCFG=static OBJDIR=output
nmake /f Makefile.vc CFG=debug-static RTLIBCFG=static OBJDIR=output
call "%MSVC_PATH%\VC\Auxiliary\Build\vcvars32.bat"
nmake /f Makefile.vc CFG=release-static RTLIBCFG=static OBJDIR=output
nmake /f Makefile.vc CFG=debug-static RTLIBCFG=static OBJDIR=output

if %TOOLSET%==gcc goto skiplibs
call :copyecho %WEBP_PATH%\output\release-static\x64\lib\libwebp.lib %LIBDIR%\Release_x64\libwebp.lib /Y
call :copyecho %WEBP_PATH%\output\debug-static\x64\lib\libwebp_debug.lib %LIBDIR%\Debug_x64\libwebp.lib /Y
call :copyecho %WEBP_PATH%\output\debug-static\x64\lib\libwebp_debug.pdb %LIBDIR%\Debug_x64\ /Y
call :copyecho %WEBP_PATH%\output\release-static\x86\lib\libwebp.lib %LIBDIR%\Release_Win32\libwebp.lib /Y
call :copyecho %WEBP_PATH%\output\debug-static\x86\lib\libwebp_debug.lib %LIBDIR%\Debug_Win32\libwebp.lib /Y
call :copyecho %WEBP_PATH%\output\debug-static\x86\lib\libwebp_debug.pdb %LIBDIR%\Debug_Win32\ /Y
:skiplibs

popd
if /I "%menu%"=="W" (echo. & echo %c_done% "DONE"[0m&echo. & echo. & pause&goto mainmenu)
:skipprepwebp


rem Rebuild libtorrent
goto skiprebuild
:installtorrent
rd /S /Q %BOOST_ROOT%\bin.v2\libs\system\build\gcc-mngw-%GCC_VERSION% 2>nul
rd /S /Q %BOOST_ROOT%\bin.v2\libs\system\build\gcc-mngw-%GCC64_PATH% 2>nul
rd /S /Q "%GCC_PATH%%GCC_PREFIX%\include\libtorrent" 2>nul
rd /S /Q "%GCC64_PATH%%GCC64_PREFIX%\include\libtorrent" 2>nul
rd /S /Q "%LIBTORRENT_PATH%\bin" 2>nul
rd /S /Q "%LIBTORRENT_PATH%\examples\bin" 2>nul
del %LIBTORREN32% 2>nul
del %LIBTORREN64% 2>nul
:skiprebuild

if %TOOLSET2%==msvc goto skipcopylibtorrentinc
rem Copy libtorrent headers

if /I exist "%GCC_PATH%%GCC_PREFIX%\include\libtorrent" (echo %c_skip% "Skipping copying headers for libtorrent"[0m&echo. & goto skipcopylibtorrentinc)
echo %c_do% "Copying libtorrent headers"[0m&echo.
xcopy "%LIBTORRENT_PATH%\include" "%GCC_PATH%%GCC_PREFIX%\include" /E /I /Y
xcopy "%LIBTORRENT_PATH%\include" "%GCC64_PATH%%GCC64_PREFIX%\include" /E /I /Y

:skipcopylibtorrentinc

rem Build libtorrent.a (32-bit)
if /I not exist %LIBTORREN32% goto buildtorrent32
if /I not exist %LIBBOOST32% goto buildtorrent32
echo %c_skip% "Skipping building libtorrent[32-bit]"[0m&echo.
goto skipbuildlibtorrent
:buildtorrent32
echo %c_do% "Building libtorrent32"[0m&echo.
@rem if %LIBTORRENT_VER2% LSS 1.1.0 (call :copyecho "libtorrent_patch\Jamfile_fixed" "%LIBTORRENT_PATH%\examples\Jamfile" /Y)
@rem if %LIBTORRENT_VER2% GEQ 1.1.0 (call :copyecho "libtorrent_patch\Jamfile_fixed_110" "%LIBTORRENT_PATH%\examples\Jamfile" /Y)
@rem if %BOOST_VER2% GEQ 1.75.0 (call :copyecho "libtorrent_patch\export.hpp" "%LIBTORRENT_PATH%\include\libtorrent\export.hpp" /Y) 
pushd "%LIBTORRENT_PATH%\examples"

echo on
b2 client_test toolset=%TOOLSET% release exception-handling=on %EXTRA_OPTIONS%
call :copyecho ..\bin\%TOOLSET%\rls\dprct-fnctn-off\extns-off\i2p-off\lnk-sttc\rntm-lnk-sttc\thrd-mlt\libtorrent.lib %LIBDIR%\Release_Win32 /Y 
b2 client_test toolset=%TOOLSET% debug exception-handling=on %EXTRA_OPTIONS% define=BOOST_USE_WINAPI_VERSION=0x0501
call :copyecho ..\bin\%TOOLSET%\dbg\dprct-fnctn-off\extns-off\i2p-off\lnk-sttc\rntm-lnk-sttc\thrd-mlt\libtorrent.lib %LIBDIR%\Debug_Win32 /Y

call :copyecho ..\bin\gcc-mngw-%GCC_VERSION%\rls\libtorrent.a %GCC_PATH%%GCC_PREFIX%\lib /Y >nul
call :copyecho ..\bin\gcc-mngw-%GCC_VERSION%\dbg\libtorrent.a %GCC_PATH%%GCC_PREFIX%\lib\libtorrent_dbg.a /Y >nul
call :copyecho "%BOOST_ROOT%\bin.v2\libs\system\build\gcc-mngw-%GCC_VERSION%\rls\libboost_system-mgw%GCC_VERSION2%-mt-s-%BOOST_VER3%.a" "%LIBBOOST32%" /Y >nul

if %TOOLSET%==gcc goto skiplibtor32
@rem call :copyecho "%BOOST_ROOT%\bin.v2\libs\system\build\%TOOLSET%\rls\libboost_system-vc120-mt-s-%BOOST_VER3%.lib" "%LIBDIR%\Release_Win32\libboost_system.lib" /Y
@rem call :copyecho "%BOOST_ROOT%\bin.v2\libs\system\build\%TOOLSET%\mydbg\libboost_system-vc120-mt-sg%BOOST_VER3%.lib" "%LIBDIR%\Debug_Win32\libboost_system.lib" /Y
:skiplibtor32

popd
:skipbuildlibtorrent

rem Build libtorrent.a (64-bit)
if /I not exist %LIBTORREN64% goto buildtorrent64
if /I not exist %LIBBOOST64% goto buildtorrent64
echo %c_skip% "Skipping building libtorrent[64-bit]"[0m&echo.
goto skipbuildlibtorrent64
:buildtorrent64
echo %c_do% "Building libtorrent64"[0m&echo.
@rem if %LIBTORRENT_VER2% LSS 1.1.0 (call :copyecho "libtorrent_patch\Jamfile_fixed" "%LIBTORRENT_PATH%\examples\Jamfile" /Y)
@rem if %LIBTORRENT_VER2% GEQ 1.1.0 (call :copyecho "libtorrent_patch\Jamfile_fixed_110" "%LIBTORRENT_PATH%\examples\Jamfile" /Y)
@rem if %BOOST_VER2% GEQ 1.75.0 (call :copyecho "libtorrent_patch\export.hpp" "%LIBTORRENT_PATH%\include\libtorrent\export.hpp" /Y 2>nul)
set oldpath=%path%
set path=%GCC64_PATH%\bin;%BOOST_ROOT%;%MSYS_BIN%;%path%
pushd "%LIBTORRENT_PATH%\examples"
b2 client_test address-model=64 toolset=%TOOLSET% release exception-handling=on %EXTRA_OPTIONS%
call :copyecho ..\bin\%TOOLSET%\rls\adrs-mdl-64\dprct-fnctn-off\extns-off\i2p-off\lnk-sttc\rntm-lnk-sttc\thrd-mlt\libtorrent.lib %LIBDIR%\Release_x64 /Y
b2 client_test address-model=64 toolset=%TOOLSET% debug exception-handling=on %EXTRA_OPTIONS%
call :copyecho ..\bin\%TOOLSET%\dbg\adrs-mdl-64\dprct-fnctn-off\extns-off\i2p-off\lnk-sttc\rntm-lnk-sttc\thrd-mlt\libtorrent.lib %LIBDIR%\Debug_x64 /Y

if %TOOLSET%==gcc goto skiplibtor64
:skiplibtor64
call :copyecho ..\bin\gcc-mngw-%GCC_VERSION%\rls64\adrs-mdl-64\libtorrent.a %GCC64_PATH%%GCC64_PREFIX%\lib /Y
call :copyecho ..\bin\gcc-mngw-%GCC_VERSION%\dbg64\adrs-mdl-64\libtorrent.a %GCC64_PATH%%GCC64_PREFIX%\lib\libtorrent_dbg.a /Y
@rem libboost_system is header only since boost 1.69
@rem call :copyecho "%BOOST_ROOT%\bin.v2\libs\system\build\gcc-mngw-%GCC_VERSION%\rls64%ADR64%\libboost_system-mgw%GCC_VERSION2%-mt-s-%BOOST_VER3%.a" "%LIBBOOST64%" /Y

set path=%oldpath%
popd
:skipbuildlibtorrent64

call :checkall
goto :eof

:fatalError
echo.
echo %c_normal% "Press any key to continue"
echo.
pause>nul
goto :eof

:copyecho
@echo off
echo Copying %1 %2 %3 %4 %5 %6 %7 %8 %9
copy %1 %2 %3 %4 %5 %6 %7 %8 %9 2>nul
if errorlevel 1 echo %c_fail% "Copy failed"[0m&echo.
=======
@Echo off
rem Toolset
@rem set TOOLSET=gcc
set TOOLSET=msvc

rem echos
set c_menu=[36m
set c_normal=[0m
set c_done=[32m
set c_fail=[91m
set c_do=[95m
set c_skip=[32m

rem Versions
rem boost 1.76.0
set BOOST_VER=1_76_0
set BOOST_VER2=1.76.0
set BOOST_VER3=1_76

rem libtorrent 1.1.14 Mar 25, 2020
set LIBTORRENT_VER2=1.1.14
set LIBTORRENT_VER=1_1_14

rem libwebp 1.2.0
set LIBWEBP_VER=1.2.0

rem mingw-w64 8.1.0
set GCC_VERSION=8.1.0
set GCC_VERSION2=81

if %TOOLSET%==msvc set MSVC_VERSION=2019\Preview

if %TOOLSET%==gcc set MINGW_PATH=C:\mingw

rem GCC 32-bit
set GCC_PATH=%MINGW_PATH%\mingw32
set GCC_PREFIX1=/i686-w64-mingw32
set GCC_PREFIX=\i686-w64-mingw32

rem GCC 64-bit
set GCC64_PATH=%MINGW_PATH%\mingw64
set GCC64_PREFIX1=/x86_64-w64-mingw32
set GCC64_PREFIX=\x86_64-w64-mingw32

rem GCC (common)
rem -w inhibit all warning messages
if %TOOLSET%==gcc set TOOLSET2=gcc
set EXTRA_OPTIONS="cxxflags=-fexpensive-optimizations -fomit-frame-pointer -D IPV6_TCLASS=30 -w"
set LIBBOOST32="%GCC_PATH%%GCC_PREFIX%\lib\libboost_system_tr.a"
set LIBBOOST64="%GCC64_PATH%%GCC64_PREFIX%\lib\libboost_system_tr.a"
set LIBWEBP="%GCC64_PATH%%GCC64_PREFIX%\lib\libwebp.a"
set LIBTORREN32="%GCC_PATH%%GCC_PREFIX%\lib\libtorrent.a"
set LIBTORREN64="%GCC64_PATH%%GCC64_PREFIX%\lib\libtorrent.a"

rem BOOST
set BOOST_ROOT=%CD%\boost
set BOOST_BUILD_PATH=%BOOST_ROOT%\tools\build
echo %BOOST_ROOT%
echo %BOOST_BUILD_PATH%
set PATH=%PATH%;%BOOST_BUILD_PATH%
@rem set BOOST_INSTALL_PATH=D:\BOOST32_%GCC_VERSION2%
@rem set BOOST64_INSTALL_PATH=D:\BOOST64_%GCC_VERSION2%

rem Configure paths
cd %~dp0
set LIBTORRENT_PATH=%CD%\libtorrent
set WEBP_PATH=%CD%\libwebp
set path=%BOOST_ROOT%;%MSYS_BIN%;%path%
if %TOOLSET%==gcc set path=%GCC_PATH%\bin;%GCC64_PATH%\bin;%path%

rem Visual Studio
if %TOOLSET%==gcc goto skipmscv
set TOOLSET=msvc-14.2
set TOOLSET2=msvc
set EXTRA_OPTIONS=--abbreviate-paths install-dependencies=on install-type=lib runtime-link=static deprecated-functions=off i2p=off extensions=off
set LIBDIR=%CD%\..\lib
set MSVC_PATH=C:\Program Files (x86)\Microsoft Visual Studio\%MSVC_VERSION%
call "%MSVC_PATH%\VC\Auxiliary\Build\vcvars64.bat"
set LIBWEBP=%LIBDIR%\Release_x64\libwebp.lib
set LIBTORREN32=%LIBDIR%\Release_Win32\libtorrent.lib
set LIBTORREN64=%LIBDIR%\Release_x64\libtorrent.lib
:skipmscv

if %TOOLSET2%==msvc goto mainmenu

rem Check for MinGW
@rem if /I not exist "%GCC_PATH%\bin" (echo %c_fail%&echo ERROR: MinGW not found in %GCC_PATH% & goto fatalError)

rem Check for MinGW64
if /I not exist "%GCC64_PATH%\bin" (echo %c_fail%&echo ERROR: MinGW_64 not found in %GCC64_PATH%[0m & goto fatalError)

rem Check for MSYS
if /I not exist "%MSYS_PATH%" (echo %c_fail%\e[0m&echo ERROR: MSYS not found in %MSYS_PATH%[0m & goto fatalError)

:mainmenu
cls
echo %c_menu%
echo.
echo   ÉÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ»
echo   ºMAIN MENU                º
echo   ÇÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄº
echo   ºA - Install all          º 
echo   ºC - Check all            º
echo   ºD - Delete all           º
echo   ºB - Build BOOST          º
echo   ºT - Build libttorret     º
echo   ºW - Build WebP           º
echo   ºQ - Quit                 º
echo   ÈÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ¼
echo.
set /P MENU=Enter command:

echo %c_normal%
cls
if /I "%menu%"=="a" call :installall
if /I "%menu%"=="d" call :delall
if /I "%menu%"=="c" call :checkall
if /I "%menu%"=="t" goto :installtorrent
if /I "%menu%"=="w" goto :installwebp
if /I "%menu%"=="b" call :buildboost
if /I "%menu%"=="q" exit
goto mainmenu

:delall
echo.

echo Checking for hanging g++.exe
tasklist /FI "IMAGENAME eq g++.exe" 2>NUL | find /I /N "g++.exe" >NUL 2>NUL
if %ERRORLEVEL%==0 taskkill /im g++.exe /f /t
echo.

echo Deleting libtorrent
rd /S /Q "%GCC_PATH%%GCC_PREFIX%\include\libtorrent" 2>nul
rd /S /Q "%GCC64_PATH%%GCC64_PREFIX%\include\libtorrent" 2>nul
del %LIBTORREN32% 2>nul
del "%GCC_PATH%%GCC_PREFIX%\lib\libtorrent_dbg.a" 2>nul
del %LIBTORREN64% 2>nul
del "%GCC64_PATH%%GCC64_PREFIX%\lib\libtorrent_dbg.a" 2>nul
rd /S /Q "%LIBTORRENT_PATH%"

echo Deleting webp
rd /S /Q "%GCC_PATH%%GCC_PREFIX%\include\libwebp" 2>nul
rd /S /Q "%GCC64_PATH%%GCC64_PREFIX%\include\libwebp" 2>nul
del "%GCC_PATH%%GCC_PREFIX%\lib\libwebp.*" 2>nul
del "%GCC64_PATH%%GCC64_PREFIX%\lib\libwebp.*" 2>nul
rd /S /Q "%MSYS_PATH%\home\libwebp-%LIBWEBP_VER%" 2>nul
del "%MSYS_PATH%\makewebp.bat" 2>nul
del "%MSYS_PATH%\home\makewebp.bat" 2>nul
del "%MSYS_PATH%\home\libwebp-%LIBWEBP_VER%.tar.gz" 2>nul

echo Deleting boost
rd /S /Q "%BOOST_ROOT%"

echo %c_done% "DONE"
echo.
echo.
pause
goto :eof

:checkall
echo.
echo Toolset:       %TOOLSET%
if %TOOLSET%==msvc echo MSVC:          %MSVC_PATH%
if %TOOLSET%==gcc echo GCC (32 bit):  %GCC_PATH%
if %TOOLSET%==gcc echo GCC (64 bit):  %GCC64_PATH%
echo MSYS:          %MSYS_PATH%
echo WebP:          %WEBP_PATH%
echo libtorrent:    %LIBTORRENT_PATH%
echo BOOST_src:     %BOOST_ROOT%
echo BOOST_dest:    %BOOST_INSTALL_PATH%
echo BOOST64_dest:  %BOOST64_INSTALL_PATH%
echo.

echo|set /p=Checking make...................
if /I exist "%MSYS_BIN%\make.exe" (echo %c_done% "OK") else (echo %c_fail% "FAIL")
echo.

echo|set /p=Checking BOOST(source)..........
if /I exist "%BOOST_ROOT%\boost.png" (echo %c_done% "OK") else (echo %c_fail% "FAIL")
echo.


echo|set /p=Checking b2...................
if /I exist "%BOOST_BUILD_PATH%\b2.exe" (echo %c_done% "OK") else (echo %c_fail% "FAIL")
echo.

echo|set /p=Checking WebP...................
if /I exist %LIBWEBP% (echo %c_done% "OK") else (echo %c_fail% "FAIL")
echo.

echo|set /p=Checking libtorrent(source).....
if /I exist "%LIBTORRENT_PATH%\examples\client_test.cpp" (echo %c_done% "OK") else (echo %c_fail% "FAIL")
echo.

echo|set /p=Checking libtorrent(binaries32).
if /I exist %LIBTORREN32% (echo %c_done% "OK") else (echo %c_fail% "FAIL")
echo.

echo|set /p=Checking libtorrent(binaries64).
if /I exist %LIBTORREN64% (echo %c_done% "OK") else (echo %c_fail% "FAIL")
echo.

echo.
pause
goto :eof

:installall
echo.

del "%MSYS_PATH%\var\lib\pacman\db.lck" 2>nul

rem download wget
if /I exist "%MSYS_BIN%\wget.exe" (echo %c_skip% "Skipping downloading wget"[0m&echo. & goto skipwget)
echo %c_do% "Downloading wget"[0m&echo.
%MSYS_BIN%\pacman.exe -S wget --noconfirm
:skipwget

rem download make
if /I exist "%MSYS_BIN%\make.exe" (echo %c_skip% "Skipping downloading make"[0m&echo. & goto skipmake)
echo %c_do% "Downloading make"[0m&echo.
%MSYS_BIN%\pacman.exe -S make --noconfirm
:skipmake

rem update toolchain
echo %c_do% "Updating toolchain"[0m&echo.
%MSYS_BIN%\pacman.exe -Syu --noconfirm
call :copyecho %GCC64_PATH%\bin\libwinpthread-1.dll %GCC64_PATH%\libexec\gcc\x86_64-w64-mingw32\%GCC_VERSION%

rem update boost, libtorrent, libwebp
git submodule update --recursive --remote --merge

rem Creating dirs for libs
if %TOOLSET%==gcc goto skipcreatelibdir
mkdir %LIBDIR%\Release_Win32
mkdir %LIBDIR%\Release_x64
mkdir %LIBDIR%\Debug_Win32
mkdir %LIBDIR%\Debug_x64
:skipcreatelibdir

:buildboost
rem Build b2.exe
@rem if /I exist "%BOOST_BUILD_PATH%\b2.exe" (echo %c_skip% "Skipping building b2.exe"[0m&echo. & goto skipbuildb2)
echo %c_do% "Building B2"[0m&echo.
pushd %BOOST_BUILD_PATH%
set SAVED_TOOLSET=%TOOLSET%
call bootstrap.bat %TOOLSET2%
set TOOLSET=%SAVED_TOOLSET%
popd
:skipbuildb2

rem Install BOOST (32-bit)
if /I exist "%BOOST_INSTALL_PATH%\include\boost\version.hpp" (echo %c_skip% "Skipping installing BOOST32"[0m&echo. & goto skipinstallboost32)
@rem if %BOOST_VER2% LSS 1.75.0 (call :copyecho "libtorrent_patch\socket_types.hpp" "%BOOST_ROOT%\boost\asio\detail\socket_types.hpp" /Y) // WSPiApi.h not required on Windows XP or newer
@rem pushd %BOOST_ROOT%
@rem echo %c_do% "Installing BOOST32"[0m&echo.
@rem rem BOOST_USE_WINAPI_VERSION=0x0501 = Win XP
@rem b2.exe install toolset=%TOOLSET% release --layout=tagged -j%NUMBER_OF_PROCESSORS% define=BOOST_USE_WINAPI_VERSION=0x05010000 --show-locate-target --without-mpi
@rem popd
:skipinstallboost32

rem Install BOOST (64-bit)
if /I exist "%BOOST64_INSTALL_PATH%\include\boost\version.hpp" (echo %c_skip% "Skipping installing BOOST64"[0m&echo. & goto skipinstallboost64)
pushd %BOOST_ROOT%
set oldpath=%path%
set path=%GCC64_PATH%\bin;%BOOST_ROOT%;%MSYS_BIN%;%path%
echo %c_do% "Installing BOOST64"[0m&echo.
b2.exe toolset=%TOOLSET% --with-thread --layout=tagged address-model=64
set path=%oldpath%
popd
:skipinstallboost64

echo.
echo %c_done% "DONE"[0m&echo.
echo.
pause

rem Install webp
if /I exist %LIBWEBP% (echo %c_skip% "Skipping installing WebP"[0m&echo. & goto skipprepwebp)
:installwebp
echo %c_do% "Installing WebP"[0m&echo.
echo %GCC_PATH% /mingw32> %MSYS_PATH%\etc\fstab
echo %GCC64_PATH% /mingw64>> %MSYS_PATH%\etc\fstab
pushd %WEBP_PATH%
rem if "%TOOLSET%"=="msvc" del %MSYS_PATH%\etc\fstab
call "%MSVC_PATH%\VC\Auxiliary\Build\vcvars64.bat"
nmake /f Makefile.vc CFG=release-static RTLIBCFG=static OBJDIR=output
nmake /f Makefile.vc CFG=debug-static RTLIBCFG=static OBJDIR=output
call "%MSVC_PATH%\VC\Auxiliary\Build\vcvars32.bat"
nmake /f Makefile.vc CFG=release-static RTLIBCFG=static OBJDIR=output
nmake /f Makefile.vc CFG=debug-static RTLIBCFG=static OBJDIR=output

if %TOOLSET%==gcc goto skiplibs
call :copyecho %WEBP_PATH%\output\release-static\x64\lib\libwebp.lib %LIBDIR%\Release_x64\libwebp.lib /Y
call :copyecho %WEBP_PATH%\output\debug-static\x64\lib\libwebp_debug.lib %LIBDIR%\Debug_x64\libwebp.lib /Y
call :copyecho %WEBP_PATH%\output\debug-static\x64\lib\libwebp_debug.pdb %LIBDIR%\Debug_x64\ /Y
call :copyecho %WEBP_PATH%\output\release-static\x86\lib\libwebp.lib %LIBDIR%\Release_Win32\libwebp.lib /Y
call :copyecho %WEBP_PATH%\output\debug-static\x86\lib\libwebp_debug.lib %LIBDIR%\Debug_Win32\libwebp.lib /Y
call :copyecho %WEBP_PATH%\output\debug-static\x86\lib\libwebp_debug.pdb %LIBDIR%\Debug_Win32\ /Y
:skiplibs

popd
if /I "%menu%"=="W" (echo. & echo %c_done% "DONE"[0m&echo. & echo. & pause&goto mainmenu)
:skipprepwebp


rem Rebuild libtorrent
goto skiprebuild
:installtorrent
rd /S /Q %BOOST_ROOT%\bin.v2\libs\system\build\gcc-mngw-%GCC_VERSION% 2>nul
rd /S /Q %BOOST_ROOT%\bin.v2\libs\system\build\gcc-mngw-%GCC64_PATH% 2>nul
rd /S /Q "%GCC_PATH%%GCC_PREFIX%\include\libtorrent" 2>nul
rd /S /Q "%GCC64_PATH%%GCC64_PREFIX%\include\libtorrent" 2>nul
rd /S /Q "%LIBTORRENT_PATH%\bin" 2>nul
rd /S /Q "%LIBTORRENT_PATH%\examples\bin" 2>nul
del %LIBTORREN32% 2>nul
del %LIBTORREN64% 2>nul
:skiprebuild

if %TOOLSET2%==msvc goto skipcopylibtorrentinc
rem Copy libtorrent headers

if /I exist "%GCC_PATH%%GCC_PREFIX%\include\libtorrent" (echo %c_skip% "Skipping copying headers for libtorrent"[0m&echo. & goto skipcopylibtorrentinc)
echo %c_do% "Copying libtorrent headers"[0m&echo.
xcopy "%LIBTORRENT_PATH%\include" "%GCC_PATH%%GCC_PREFIX%\include" /E /I /Y
xcopy "%LIBTORRENT_PATH%\include" "%GCC64_PATH%%GCC64_PREFIX%\include" /E /I /Y

:skipcopylibtorrentinc

rem Build libtorrent.a (32-bit)
if /I not exist %LIBTORREN32% goto buildtorrent32
if /I not exist %LIBBOOST32% goto buildtorrent32
echo %c_skip% "Skipping building libtorrent[32-bit]"[0m&echo.
goto skipbuildlibtorrent
:buildtorrent32
echo %c_do% "Building libtorrent32"[0m&echo.
@rem if %LIBTORRENT_VER2% LSS 1.1.0 (call :copyecho "libtorrent_patch\Jamfile_fixed" "%LIBTORRENT_PATH%\examples\Jamfile" /Y)
@rem if %LIBTORRENT_VER2% GEQ 1.1.0 (call :copyecho "libtorrent_patch\Jamfile_fixed_110" "%LIBTORRENT_PATH%\examples\Jamfile" /Y)
@rem if %BOOST_VER2% GEQ 1.75.0 (call :copyecho "libtorrent_patch\export.hpp" "%LIBTORRENT_PATH%\include\libtorrent\export.hpp" /Y) 
pushd "%LIBTORRENT_PATH%\examples"

echo on
b2 client_test toolset=%TOOLSET% release exception-handling=on %EXTRA_OPTIONS%
call :copyecho ..\bin\%TOOLSET%\rls\dprct-fnctn-off\extns-off\i2p-off\lnk-sttc\rntm-lnk-sttc\thrd-mlt\libtorrent.lib %LIBDIR%\Release_Win32 /Y 
b2 client_test toolset=%TOOLSET% debug exception-handling=on %EXTRA_OPTIONS% define=BOOST_USE_WINAPI_VERSION=0x0501
call :copyecho ..\bin\%TOOLSET%\dbg\dprct-fnctn-off\extns-off\i2p-off\lnk-sttc\rntm-lnk-sttc\thrd-mlt\libtorrent.lib %LIBDIR%\Debug_Win32 /Y

call :copyecho ..\bin\gcc-mngw-%GCC_VERSION%\rls\libtorrent.a %GCC_PATH%%GCC_PREFIX%\lib /Y >nul
call :copyecho ..\bin\gcc-mngw-%GCC_VERSION%\dbg\libtorrent.a %GCC_PATH%%GCC_PREFIX%\lib\libtorrent_dbg.a /Y >nul
call :copyecho "%BOOST_ROOT%\bin.v2\libs\system\build\gcc-mngw-%GCC_VERSION%\rls\libboost_system-mgw%GCC_VERSION2%-mt-s-%BOOST_VER3%.a" "%LIBBOOST32%" /Y >nul

if %TOOLSET%==gcc goto skiplibtor32
@rem call :copyecho "%BOOST_ROOT%\bin.v2\libs\system\build\%TOOLSET%\rls\libboost_system-vc120-mt-s-%BOOST_VER3%.lib" "%LIBDIR%\Release_Win32\libboost_system.lib" /Y
@rem call :copyecho "%BOOST_ROOT%\bin.v2\libs\system\build\%TOOLSET%\mydbg\libboost_system-vc120-mt-sg%BOOST_VER3%.lib" "%LIBDIR%\Debug_Win32\libboost_system.lib" /Y
:skiplibtor32

popd
:skipbuildlibtorrent

rem Build libtorrent.a (64-bit)
if /I not exist %LIBTORREN64% goto buildtorrent64
if /I not exist %LIBBOOST64% goto buildtorrent64
echo %c_skip% "Skipping building libtorrent[64-bit]"[0m&echo.
goto skipbuildlibtorrent64
:buildtorrent64
echo %c_do% "Building libtorrent64"[0m&echo.
@rem if %LIBTORRENT_VER2% LSS 1.1.0 (call :copyecho "libtorrent_patch\Jamfile_fixed" "%LIBTORRENT_PATH%\examples\Jamfile" /Y)
@rem if %LIBTORRENT_VER2% GEQ 1.1.0 (call :copyecho "libtorrent_patch\Jamfile_fixed_110" "%LIBTORRENT_PATH%\examples\Jamfile" /Y)
@rem if %BOOST_VER2% GEQ 1.75.0 (call :copyecho "libtorrent_patch\export.hpp" "%LIBTORRENT_PATH%\include\libtorrent\export.hpp" /Y 2>nul)
set oldpath=%path%
set path=%GCC64_PATH%\bin;%BOOST_ROOT%;%MSYS_BIN%;%path%
pushd "%LIBTORRENT_PATH%\examples"
b2 client_test address-model=64 toolset=%TOOLSET% release exception-handling=on %EXTRA_OPTIONS%
call :copyecho ..\bin\%TOOLSET%\rls\adrs-mdl-64\dprct-fnctn-off\extns-off\i2p-off\lnk-sttc\rntm-lnk-sttc\thrd-mlt\libtorrent.lib %LIBDIR%\Release_x64 /Y
b2 client_test address-model=64 toolset=%TOOLSET% debug exception-handling=on %EXTRA_OPTIONS%
call :copyecho ..\bin\%TOOLSET%\dbg\adrs-mdl-64\dprct-fnctn-off\extns-off\i2p-off\lnk-sttc\rntm-lnk-sttc\thrd-mlt\libtorrent.lib %LIBDIR%\Debug_x64 /Y

if %TOOLSET%==gcc goto skiplibtor64
:skiplibtor64
call :copyecho ..\bin\gcc-mngw-%GCC_VERSION%\rls64\adrs-mdl-64\libtorrent.a %GCC64_PATH%%GCC64_PREFIX%\lib /Y
call :copyecho ..\bin\gcc-mngw-%GCC_VERSION%\dbg64\adrs-mdl-64\libtorrent.a %GCC64_PATH%%GCC64_PREFIX%\lib\libtorrent_dbg.a /Y
@rem libboost_system is header only since boost 1.69
@rem call :copyecho "%BOOST_ROOT%\bin.v2\libs\system\build\gcc-mngw-%GCC_VERSION%\rls64%ADR64%\libboost_system-mgw%GCC_VERSION2%-mt-s-%BOOST_VER3%.a" "%LIBBOOST64%" /Y

set path=%oldpath%
popd
:skipbuildlibtorrent64

call :checkall
goto :eof

:fatalError
echo.
echo %c_normal% "Press any key to continue"
echo.
pause>nul
goto :eof

:copyecho
@echo off
echo Copying %1 %2 %3 %4 %5 %6 %7 %8 %9
copy %1 %2 %3 %4 %5 %6 %7 %8 %9 2>nul
if errorlevel 1 echo %c_fail% "Copy failed"[0m&echo.
>>>>>>> 2224fa12b7f7f22cf5577530bd417d7c562217b8
goto :eof