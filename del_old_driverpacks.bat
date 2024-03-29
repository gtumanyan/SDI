@echo off
echo Snappy Driver Installer
echo Driver Pack Cleanup
echo by Computer Bloke
echo.
@setlocal enableextensions
@cd /d "%~dp0/drivers"
attrib *.* -R

rem out of date driver packs
for /f "tokens=1,2,3,4,5,6,7 delims=_. usebackq" %%i in (`dir /b *.7z`) do call :cleanup %%i %%j %%k %%l %%m %%n %%o
rem redundant driver packs
del DP_Sound_ADI_*.7z
cd ..
goto :end

:cleanup
if /i "%7"=="7z" call :clean %1_%2_%3_%4_%5 %6 && goto :eof
if /i "%6"=="7z" call :clean %1_%2_%3_%4 %5 && goto :eof
if /i "%5"=="7z" call :clean %1_%2_%3 %4 && goto :eof
if /i "%4"=="7z" call :clean %1_%2 %3 && goto :eof
if /i "%3"=="7z" call :clean %1 %2 && goto :eof
goto :eof

:clean
for /f "tokens=* usebackq" %%f in (`dir /b /on "%1_*.7z"`) do set "GOODFILE=%%f"
rem echo Keeping most recent driver file: %GOODFILE%
for %%f in (%1_*.7z) do if not "%%f"=="%GOODFILE%" echo "%%f" & del "%%f"
goto :eof

:end
