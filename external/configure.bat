@echo off
for /F "tokens=1,2 delims=#" %%a in ('"prompt #$H#$E# & echo on & for %%b in (1) do rem"') do (set "DEL=%%a")

rem Toolset
set TOOLSET=gcc
//set TOOLSET=msvc

rem Colors
set c_menu=03
set c_normal=07
set c_done=0A
set c_fail=0C
set c_do=0D
set c_skip=02

rem Versions
rem boost 1.65.0 21 Aug 2017
set BOOST_VER=1_65_0
set BOOST_VER2=1.65.0
set BOOST_VER3=1_65

rem libtorrent 1.0.11 5 Feb 2017
set LIBTORRENT_VER2=1.0.11
set LIBTORRENT_VER=1_0_11

rem libwebp 0.6.1 29 Nov 2017
set LIBWEBP_VER=0.6.1

rem mingw-w64 8.1.0
set GCC_VERSION=8.1.0
set GCC_VERSION2=81

set MSVC_VERSION=2019\Preview

set MINGW_PATH=C:\mingw

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

rem MSYS
set MSYS_PATH=C:\msys64
set MSYS_BIN=%MSYS_PATH%\usr\bin
set ADR64=\adrs-mdl-64

rem BOOST
set BOOST_ROOT=%CD%\boost_%BOOST_VER%
set BOOST_BUILD_PATH=%BOOST_ROOT%\tools\build
set BOOST_INSTALL_PATH=D:\BOOST32_%GCC_VERSION2%
set BOOST64_INSTALL_PATH=D:\BOOST64_%GCC_VERSION2%

rem Configure paths
set LIBTORRENT_PATH=%CD%\libtorrent-libtorrent-%LIBTORRENT_VER%
set WEBP_PATH=%CD%\webp
set path=%BOOST_ROOT%;%MSYS_BIN%;%path%
if %TOOLSET%==gcc set path=%GCC_PATH%\bin;%GCC64_PATH%\bin;%path%

rem Visual Studio
if %TOOLSET%==gcc goto skipmscv
set TOOLSET=msvc-14.2
set TOOLSET2=msvc
set EXTRA_OPTIONS=link=static runtime-link=static i2p=off extensions=off streaming=off super-seeding=off
set LIBDIR=%CD%\..\lib
set MSVC_PATH=C:\Program Files (x86)\Microsoft Visual Studio %MSVC_VERSION%
msvc call "%MSVC_PATH%\VC\vcvarsall"
set LIBBOOST32=%LIBDIR%\Release_Win32\libboost_system.lib
set LIBBOOST64=%LIBDIR%\Release_x64\libboost_system.lib
set LIBWEBP=%LIBDIR%\Release_x64\libwebp.lib
set LIBTORREN32=%LIBDIR%\Release_Win32\libtorrent.lib
set LIBTORREN64=%LIBDIR%\Release_x64\libtorrent.lib
:skipmscv

rem Check for MinGW
@rem if /I not exist "%GCC_PATH%\bin" (color %c_fail%&echo ERROR: MinGW not found in %GCC_PATH% & goto fatalError)

rem Check for MinGW64
if /I not exist "%GCC64_PATH%\bin" (color %c_fail%&echo ERROR: MinGW_64 not found in %GCC64_PATH% & goto fatalError)

if %TOOLSET2%==msvc goto mainmenu
rem Check for MSYS
if /I not exist "%MSYS_PATH%" (color %c_fail%&echo ERROR: MSYS not found in %MSYS_PATH% & goto fatalError)

:mainmenu
cls
color %c_menu%
echo.
echo   ÉÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ»
echo   º MAIN MENU               º
echo   ÇÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¶
echo   º A - Install all         º
echo   º C - Check all           º
echo   º D - Delete all          º
echo   º T - Rebuild libttorrent º
echo   º W - Rebuild WebP        º
echo   º B - Build BOOST         º
echo   º Q - Quit                º
echo   ÈÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ¼
echo.
set /p MENU=Enter command:

color %c_normal%
cls
if /I "%menu%"=="A" call :installall
if /I "%menu%"=="D" call :delall
if /I "%menu%"=="C" call :checkall
if /I "%menu%"=="T" goto installtorrent
if /I "%menu%"=="W" goto installwebp
if /I "%menu%"=="B" call :buildboost
if /I "%menu%"=="Q" exit
goto mainmenu

:buildboost
rem Install BOOST (32-bit)
if /I exist "%BOOST_INSTALL_PATH%\include\boost\version.hpp" (call :ColorText %c_skip% "Skipping installing BOOST32"&echo. & goto skipinstallboost32)
@rem if %BOOST_VER2% LSS 1.65.0 (call :copyecho "libtorrent_patch\socket_types.hpp" "%BOOST_ROOT%\boost\asio\detail\socket_types.hpp" /Y) // WSPiApi.h not required on Windows XP or newer
pushd %BOOST_ROOT%
call :ColorText %c_do% "Installing BOOST32"&echo.
rem BOOST_USE_WINAPI_VERSION=0x0501 = Win XP
b2.exe install toolset=%TOOLSET% release --layout=tagged -j%NUMBER_OF_PROCESSORS% define=BOOST_USE_WINAPI_VERSION=0x05010000 --show-locate-target --without-mpi
popd
:skipinstallboost32

rem Install BOOST (64-bit)
if /I exist "%BOOST64_INSTALL_PATH%\include\boost\version.hpp" (call :ColorText %c_skip% "Skipping installing BOOST64"&echo. & goto skipinstallboost64)
pushd %BOOST_ROOT%
set oldpath=%path%
set path=%GCC64_PATH%\bin;%BOOST_ROOT%;%MSYS_BIN%;%path%
call :ColorText %c_do% "Installing BOOST64"&echo.
bjam.exe install toolset=%TOOLSET% release --layout=tagged -j%NUMBER_OF_PROCESSORS% --prefix=%BOOST64_INSTALL_PATH% address-model=64 define=BOOST_USE_WINAPI_VERSION=0x05010000
set path=%oldpath%
popd
:skipinstallboost64

echo.
call :ColorText %c_done% "DONE"&echo.
echo.
pause
goto :eof

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
del %LIBBOOST32% 2>nul
del %LIBBOOST64% 2>nul
rd /S /Q "%LIBTORRENT_PATH%"

echo Deleting webp
rd /S /Q "%GCC_PATH%%GCC_PREFIX%\include\webp" 2>nul
rd /S /Q "%GCC64_PATH%%GCC64_PREFIX%\include\webp" 2>nul
del "%GCC_PATH%%GCC_PREFIX%\lib\libwebp.*" 2>nul
del "%GCC64_PATH%%GCC64_PREFIX%\lib\libwebp.*" 2>nul
rd /S /Q "%MSYS_PATH%\home\libwebp-%LIBWEBP_VER%" 2>nul
del "%MSYS_PATH%\makewebp.bat" 2>nul
del "%MSYS_PATH%\home\makewebp.bat" 2>nul
del "%MSYS_PATH%\home\libwebp-%LIBWEBP_VER%.tar.gz" 2>nul

echo Deleting boost
rd /S /Q "%BOOST_ROOT%"

call :ColorText %c_done% "DONE"
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

echo|set /p=Checking wget...................
if /I exist "%MSYS_BIN%\wget.exe" (call :ColorText %c_done% "OK") else (call :ColorText %c_fail% "FAIL")
echo.

echo|set /p=Checking tar....................
if /I exist "%MSYS_BIN%\tar.exe" (call :ColorText %c_done% "OK") else (call :ColorText %c_fail% "FAIL")
echo.

echo|set /p=Checking make...................
if /I exist "%MSYS_BIN%\make.exe" (call :ColorText %c_done% "OK") else (call :ColorText %c_fail% "FAIL")
echo.

echo|set /p=Checking libwebp-%LIBWEBP_VER%.tar.gz...
if /I exist "%WEBP_PATH%\mingw\msys\1.0\home\libwebp-%LIBWEBP_VER%.tar.gz" (call :ColorText %c_done% "OK") else (call :ColorText %c_fail% "FAIL")
echo.

echo|set /p=Checking boost_%BOOST_VER%.tar.gz....
if /I exist "%LIBTORRENT_PATH%\..\boost_%BOOST_VER%.tar.gz" (call :ColorText %c_done% "OK") else (call :ColorText %c_fail% "FAIL")
echo.

echo|set /p=Checking libtorrent.tar.gz......
if /I exist "%LIBTORRENT_PATH%\..\libtorrent-rasterbar-%LIBTORRENT_VER%.tar.gz" (call :ColorText %c_done% "OK") else (call :ColorText %c_fail% "FAIL")
echo.

echo|set /p=Checking BOOST(source)..........
if /I exist "%BOOST_ROOT%\boost.png" (call :ColorText %c_done% "OK") else (call :ColorText %c_fail% "FAIL")
echo.

echo|set /p=Checking BOOST(binaries32)......
if /I exist %LIBBOOST32% (call :ColorText %c_done% "OK") else (call :ColorText %c_fail% "FAIL")
echo.

echo|set /p=Checking BOOST(binaries64)......
if /I exist %LIBBOOST64% (call :ColorText %c_done% "OK") else (call :ColorText %c_fail% "FAIL")
echo.

echo|set /p=Checking BJAM...................
if /I exist "%BOOST_ROOT%\bjam.exe" (call :ColorText %c_done% "OK") else (call :ColorText %c_fail% "FAIL")
echo.

echo|set /p=Checking WebP...................
if /I exist %LIBWEBP% (call :ColorText %c_done% "OK") else (call :ColorText %c_fail% "FAIL")
echo.

echo|set /p=Checking libtorrent(source).....
if /I exist "%LIBTORRENT_PATH%\examples\client_test.cpp" (call :ColorText %c_done% "OK") else (call :ColorText %c_fail% "FAIL")
echo.

echo|set /p=Checking libtorrent(binaries32).
if /I exist %LIBTORREN32% (call :ColorText %c_done% "OK") else (call :ColorText %c_fail% "FAIL")
echo.

echo|set /p=Checking libtorrent(binaries64).
if /I exist %LIBTORREN64% (call :ColorText %c_done% "OK") else (call :ColorText %c_fail% "FAIL")
echo.

echo.
pause
goto :eof

:installall
echo.

del "%MSYS_PATH%\var\lib\pacman\db.lck" 2>nul

rem download wget
if /I exist "%MSYS_BIN%\wget.exe" (call :ColorText %c_skip% "Skipping downloading wget"&echo. & goto skipwget)
call :ColorText %c_do% "Downloading wget"&echo.
%MSYS_BIN%\pacman.exe -S wget --noconfirm
:skipwget

rem download tar
if /I exist "%MSYS_BIN%\tar.exe" (call :ColorText %c_skip% "Skipping downloading tar"&echo. & goto skiptar)
call :ColorText %c_do% "Downloading tar"&echo.
%MSYS_BIN%\pacman.exe -S tar --noconfirm
:skiptar

rem download make
if /I exist "%MSYS_BIN%\make.exe" (call :ColorText %c_skip% "Skipping downloading make"&echo. & goto skipmake)
call :ColorText %c_do% "Downloading make"&echo.
%MSYS_BIN%\pacman.exe -S make --noconfirm
:skipmake

rem update toolchain
call :ColorText %c_do% "Updating toolchain"&echo.
%MSYS_BIN%\pacman.exe -Syu --noconfirm
call :copyecho %GCC64_PATH%\bin\libwinpthread-1.dll %GCC64_PATH%\libexec\gcc\x86_64-w64-mingw32\%GCC_VERSION%

rem download WebP
if /I exist "%WEBP_PATH%\mingw\msys\1.0\home\libwebp-%LIBWEBP_VER%.tar.gz" (call :ColorText %c_skip% "Skipping downloading WebP"&echo. & goto skipdownloadwebp)
call :ColorText %c_do% "Downloading WebP"&echo.
%MSYS_BIN%\wget http://downloads.webmproject.org/releases/webp/libwebp-%LIBWEBP_VER%.tar.gz -Owebp\mingw\msys\1.0\home\libwebp-%LIBWEBP_VER%.tar.gz
:skipdownloadwebp

rem download BOOST
if /I exist "boost_%BOOST_VER%.tar.gz" (call :ColorText %c_skip% "Skipping downloading BOOST"&echo. & goto skipdownloadboost)
call :ColorText %c_do% "Downloading BOOST"&echo.
%MSYS_BIN%\wget http://sourceforge.net/projects/boost/files/boost/%BOOST_VER2%/boost_%BOOST_VER%.7z/download -Oboost_%BOOST_VER%.7z
:skipdownloadboost
if /I not exist "%BOOST_ROOT%\boost.png" (%MSYS_BIN%\tar -xf "boost_%BOOST_VER%.tar.gz" -v)
@rem if %BOOST_VER2% LSS 1.65.0 (call :copyecho "libtorrent_patch\socket_types.hpp" "%BOOST_ROOT%\boost\asio\detail\socket_types.hpp" /Y) // WSPiApi.h not required on Windows XP or newer

rem download libtorrent
if /I exist "libtorrent-rasterbar-%LIBTORRENT_VER%.tar.gz" (call :ColorText %c_skip% "Skipping downloading libtorrent"&echo. & goto skipdownloadlibtorrent)
call :ColorText %c_do% "Downloading libtorrent"&echo.
%MSYS_BIN%\wget https://github.com/arvidn/libtorrent/archive/libtorrent-%LIBTORRENT_VER%.tar.gz -Olibtorrent-rasterbar-%LIBTORRENT_VER%.tar.gz --no-check-certificate
:skipdownloadlibtorrent
if /I not exist "%LIBTORRENT_PATH%\examples\client_test.cpp" (%MSYS_BIN%\tar -xf "libtorrent-rasterbar-%LIBTORRENT_VER%.tar.gz" -v)
if /I exist "libtorrent-rasterbar-%LIBTORRENT_VER2%\examples\client_test.cpp" (move "libtorrent-rasterbar-%LIBTORRENT_VER2%" "libtorrent-libtorrent-%LIBTORRENT_VER%")

rem Creating dirs for libs
if %TOOLSET%==gcc goto skipcreatelibdir
mkdir %LIBDIR%\Release_Win32
mkdir %LIBDIR%\Release_x64
mkdir %LIBDIR%\Debug_Win32
mkdir %LIBDIR%\Debug_x64
:skipcreatelibdir

rem Install webp
if /I exist %LIBWEBP% (call :ColorText %c_skip% "Skipping installing WebP"&echo. & goto skipprepwebp)
:installwebp
call :ColorText %c_do% "Installing WebP"&echo.
xcopy webp\mingw\msys\1.0 %MSYS_PATH% /E /I /Y
echo %GCC_PATH% /mingw32> %MSYS_PATH%\etc\fstab
echo %GCC64_PATH% /mingw64>> %MSYS_PATH%\etc\fstab
pushd %MSYS_PATH%
rem if "%TOOLSET%"=="msvc" del %MSYS_PATH%\etc\fstab
call makewebp.bat %MSYS_BIN% /mingw32%GCC_PREFIX1% /mingw64%GCC64_PREFIX1%

if %TOOLSET%==gcc goto skiplibs
call :copyecho %MSYS_PATH%\home\libwebp-%LIBWEBP_VER%\output\release-static\x64\lib\libwebp.lib %LIBDIR%\Release_x64\libwebp.lib /Y
call :copyecho %MSYS_PATH%\home\libwebp-%LIBWEBP_VER%\output\debug-static\x64\lib\libwebp_debug.lib %LIBDIR%\Debug_x64\libwebp.lib /Y
call :copyecho %MSYS_PATH%\home\libwebp-%LIBWEBP_VER%\output\release-static\x32\lib\libwebp.lib %LIBDIR%\Release_Win32\libwebp.lib /Y
call :copyecho %MSYS_PATH%\home\libwebp-%LIBWEBP_VER%\output\debug-static\x32\lib\libwebp_debug.lib %LIBDIR%\Debug_Win32\libwebp.lib /Y
:skiplibs

popd
if /I "%menu%"=="W" (echo. & call :ColorText %c_done% "DONE"&echo. & echo. & pause&goto mainmenu)
:skipprepwebp

rem Build b2.exe
if /I exist "%BOOST_BUILD_PATH%\b2.exe" (call :ColorText %c_skip% "Skipping building bjam.exe"&echo. & goto skipbuildbjam)
call :ColorText %c_do% "Building B2"&echo.
pushd %BOOST_BUILD_PATH%
set SAVED_TOOLSET=%TOOLSET%
call bootstrap.bat %TOOLSET2%
set TOOLSET=%SAVED_TOOLSET%
popd
:skipbuildbjam

rem Rebuild libtorrent
goto skiprebuild
:installtorrent
rd /S /Q %BOOST_ROOT%\bin.v2\libs\system\build\gcc-mngw-%GCC_VERSION%
rd /S /Q %BOOST_ROOT%\bin.v2\libs\system\build\gcc-mngw-%GCC64_PATH%
rd /S /Q "%GCC_PATH%%GCC_PREFIX%\include\libtorrent"
rd /S /Q "%GCC64_PATH%%GCC64_PREFIX%\include\libtorrent"
rd /S /Q "%LIBTORRENT_PATH%\bin"
rd /S /Q "%LIBTORRENT_PATH%\examples\bin"
del %LIBTORREN32%
del %LIBTORREN64%
:skiprebuild

if %TOOLSET2%==msvc goto skipcopylibtorrentinc
rem Copy libtorrent headers

if /I exist "%GCC_PATH%%GCC_PREFIX%\include\libtorrent" (call :ColorText %c_skip% "Skipping copying headers for libtorrent"&echo. & goto skipcopylibtorrentinc)
call :ColorText %c_do% "Copying libtorrent headers"&echo.
xcopy "%LIBTORRENT_PATH%\include" "%GCC_PATH%%GCC_PREFIX%\include" /E /I /Y
xcopy "%LIBTORRENT_PATH%\include" "%GCC64_PATH%%GCC64_PREFIX%\include" /E /I /Y

:skipcopylibtorrentinc

rem Build libtorrent.a (32-bit)
if /I not exist %LIBTORREN32% goto buildtorrent32
if /I not exist %LIBBOOST32% goto buildtorrent32
call :ColorText %c_skip% "Skipping building libtorrent[32-bit]"&echo.
goto skipbuildlibtorrent
:buildtorrent32
call :ColorText %c_do% "Building libtorrent32"&echo.
if %LIBTORRENT_VER2% LSS 1.1.0 (call :copyecho "libtorrent_patch\Jamfile_fixed" "%LIBTORRENT_PATH%\examples\Jamfile" /Y)
if %LIBTORRENT_VER2% GEQ 1.1.0 (call :copyecho "libtorrent_patch\Jamfile_fixed_110" "%LIBTORRENT_PATH%\examples\Jamfile" /Y)
if %BOOST_VER2% GEQ 1.65.0 (call :copyecho "libtorrent_patch\export.hpp" "%LIBTORRENT_PATH%\include\libtorrent\export.hpp" /Y)
pushd "%LIBTORRENT_PATH%\examples"

bjam --abbreviate-paths client_test -j%NUMBER_OF_PROCESSORS% toolset=%TOOLSET% rls exception-handling=on %EXTRA_OPTIONS% define=BOOST_USE_WINAPI_VERSION=0x0501
bjam --abbreviate-paths client_test -j%NUMBER_OF_PROCESSORS% toolset=%TOOLSET% dbg exception-handling=on %EXTRA_OPTIONS% define=BOOST_USE_WINAPI_VERSION=0x0501

call :copyecho ..\bin\gcc-mngw-%GCC_VERSION%\rls\libtorrent.a %GCC_PATH%%GCC_PREFIX%\lib /Y
call :copyecho ..\bin\gcc-mngw-%GCC_VERSION%\dbg\libtorrent.a %GCC_PATH%%GCC_PREFIX%\lib\libtorrent_dbg.a /Y
call :copyecho "%BOOST_ROOT%\bin.v2\libs\system\build\gcc-mngw-%GCC_VERSION%\rls\libboost_system-mgw%GCC_VERSION2%-mt-s-%BOOST_VER3%.a" "%LIBBOOST32%" /Y

if %TOOLSET%==gcc goto skiplibtor32
call :copyecho ..\bin\msvc-%MSVC_VERSION%\myrls\libtorrent.lib %LIBDIR%\Release_Win32 /Y
call :copyecho ..\bin\msvc-%MSVC_VERSION%\mydbg\libtorrent.lib %LIBDIR%\Debug_Win32 /Y
call :copyecho "%BOOST_ROOT%\bin.v2\libs\system\build\msvc-%MSVC_VERSION%\myrls\libboost_system-vc120-mt-s-%BOOST_VER3%.lib" "%LIBDIR%\Release_Win32\libboost_system.lib" /Y
call :copyecho "%BOOST_ROOT%\bin.v2\libs\system\build\msvc-%MSVC_VERSION%\mydbg\libboost_system-vc120-mt-sg%BOOST_VER3%.lib" "%LIBDIR%\Debug_Win32\libboost_system.lib" /Y
:skiplibtor32

popd
:skipbuildlibtorrent

rem Build libtorrent.a (64-bit)
if /I not exist %LIBTORREN64% goto buildtorrent64
if /I not exist %LIBBOOST64% goto buildtorrent64
call :ColorText %c_skip% "Skipping building libtorrent[64-bit]"&echo.
goto skipbuildlibtorrent64
:buildtorrent64
call :ColorText %c_do% "Building libtorrent64"&echo.
if %LIBTORRENT_VER2% LSS 1.1.0 (call :copyecho "libtorrent_patch\Jamfile_fixed" "%LIBTORRENT_PATH%\examples\Jamfile" /Y)
if %LIBTORRENT_VER2% GEQ 1.1.0 (call :copyecho "libtorrent_patch\Jamfile_fixed_110" "%LIBTORRENT_PATH%\examples\Jamfile" /Y)
if %BOOST_VER2% GEQ 1.65.0 (call :copyecho "libtorrent_patch\export.hpp" "%LIBTORRENT_PATH%\include\libtorrent\export.hpp" /Y)
set oldpath=%path%
set path=%GCC64_PATH%\bin;%BOOST_ROOT%;%MSYS_BIN%;%path%
pushd "%LIBTORRENT_PATH%\examples"
bjam --abbreviate-paths client_test -j%NUMBER_OF_PROCESSORS% address-model=64 toolset=%TOOLSET% rls64 exception-handling=on %EXTRA_OPTIONS% define=BOOST_USE_WINAPI_VERSION=0x0501
bjam --abbreviate-paths client_test -j%NUMBER_OF_PROCESSORS% address-model=64 toolset=%TOOLSET% dbg64 exception-handling=on %EXTRA_OPTIONS% define=BOOST_USE_WINAPI_VERSION=0x0501

call :copyecho ..\bin\gcc-mngw-%GCC_VERSION%\rls64\adrs-mdl-64\libtorrent.a %GCC64_PATH%%GCC64_PREFIX%\lib /Y
call :copyecho ..\bin\gcc-mngw-%GCC_VERSION%\dbg64\adrs-mdl-64\libtorrent.a %GCC64_PATH%%GCC64_PREFIX%\lib\libtorrent_dbg.a /Y
@rem libboost_system is header only since boost 1.69
call :copyecho "%BOOST_ROOT%\bin.v2\libs\system\build\gcc-mngw-%GCC_VERSION%\rls64%ADR64%\libboost_system-mgw%GCC_VERSION2%-mt-s-%BOOST_VER3%.a" "%LIBBOOST64%" /Y

if %TOOLSET%==gcc goto skiplibtor64
call :copyecho ..\bin\msvc-%MSVC_VERSION%\myrls\adrs-mdl-64\libtorrent.lib %LIBDIR%\Release_x64 /Y
call :copyecho ..\bin\msvc-%MSVC_VERSION%\mydbg\adrs-mdl-64\libtorrent.lib %LIBDIR%\Debug_x64 /Y
call :copyecho %BOOST_ROOT%\bin.v2\libs\system\build\msvc-%MSVC_VERSION%\myrls%ADR64%\libboost_system-vc142-mt-s-%BOOST_VER3%.lib %LIBDIR%\Release_x64\libboost_system.lib /Y
call :copyecho %BOOST_ROOT%\bin.v2\libs\system\build\msvc-%MSVC_VERSION%\mydbg%ADR64%\libboost_system-vc142-mt-sg-%BOOST_VER3%.lib %LIBDIR%\Debug_x64\libboost_system.lib /Y
:skiplibtor64

set path=%oldpath%
popd
:skipbuildlibtorrent64

call :checkall
goto :eof

:fatalError
echo.
call :ColorText %c_normal% "Press any key to continue"
echo.
pause>nul
goto :eof

:ColorText
echo off
<nul set /p ".=%DEL%" > "%~2"
findstr /v /a:%1 /R "^$" "%~2" nul
del "%~2" > nul 2>&1
goto :eof

:copyecho
@echo off
echo Copying %1 %2 %3 %4 %5 %6 %7 %8 %9
copy %1 %2 %3 %4 %5 %6 %7 %8 %9
if errorlevel 1 call :ColorText  %c_fail% "Copy failed"&echo.&echo.
goto :eof