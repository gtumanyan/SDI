@ECHO OFF
SETLOCAL ENABLEEXTENSIONS
:: encoding: UTF-8
CHCP 65001 >NUL 2>&1

rem ******************************************************************************
rem *                                                                            *
rem * SDI                                                                   *
rem *                                                                            *
rem * make_portable(.zip).cmd                                                    *
rem *   Batch file for creating "Portable (*.zip)" packages                      *
rem *                                                                            *
rem * See License.txt for details about distribution and modification.           *
rem *                                                                            *
rem *                                                 (c) Rizonesoft 2008-2023   *
rem *                                                   https://rizonesoft.com   *
rem *                                                                            *
rem ******************************************************************************

CD /D %~dp0

rem Check for the help switches
IF /I "%~1" == "help"   GOTO SHOWHELP
IF /I "%~1" == "help"   GOTO SHOWHELP
IF /I "%~1" == "/help"  GOTO SHOWHELP
IF /I "%~1" == "-help"  GOTO SHOWHELP
IF /I "%~1" == "--help" GOTO SHOWHELP
IF /I "%~1" == "/?"     GOTO SHOWHELP

SET INPUTDIRx86=.
SET INPUTDIRx64=.
SET TEMP_NAME="make_portable_temp"

IF NOT EXIST "SDI.exe"   CALL :SUBMSG "ERROR" "Compile SDI x86 first!"
IF NOT EXIST "SDI64.exe"   CALL :SUBMSG "ERROR" "Compile SDI x64 first!"

CALL :SubGetVersion
CALL :SubDetectSevenzipPath

IF /I "%SEVENZIP%" == "" CALL :SUBMSG "ERROR" "7za wasn't found!"

CALL :SubZipFiles %INPUTDIR%

rem Compress everything into a single ZIP file

IF EXIST "SDI_%SDI_VER%.zip" DEL "SDI_%SDI_VER%.zip"
IF EXIST "%TEMP_NAME%"      RD /S /Q "%TEMP_NAME%"
IF NOT EXIST "%TEMP_NAME%"  MD "%TEMP_NAME%"

IF EXIST "SDI_%SDI_VER%*.zip" COPY /Y /V "SDI_%SDI_VER%*.zip" "%TEMP_NAME%\" >NUL
IF EXIST "%TEMP_NAME%\SDI_%SDI_VER%*.zip" DEL /F /Q "SDI_%SDI_VER%*.zip" >NUL

PUSHD "%TEMP_NAME%"

"%SEVENZIP%" a -tzip -mm=deflate64 -mfb=257 -mpass=15 SDI_%SDI_VER%.zip *
IF %ERRORLEVEL% NEQ 0 CALL :SUBMSG "ERROR" "Compilation failed!"

CALL :SUBMSG "INFO" "SDI_%SDI_VER%.zip created successfully!"

MOVE /Y "SDI_%SDI_VER%.zip" "..\"

POPD
IF EXIST "%TEMP_NAME%" RD /S /Q "%TEMP_NAME%"

POPD

:END
TITLE Finished!
ECHO.

:: Pause of 4 seconds to verify the logfile before exiting 
:: ===========================================================================================
ping -n 5 127.0.0.1>nul

ENDLOCAL
EXIT /B


:SubZipFiles
SET "ZIP_NAME=SDI_%SDI_VER%"
TITLE Creating %ZIP_NAME%.zip...
CALL :SUBMSG "INFO" "Creating %ZIP_NAME%.zip..."
IF EXIST "%TEMP_NAME%"     RD /S /Q "%TEMP_NAME%"
IF NOT EXIST "%TEMP_NAME%" MD "%TEMP_NAME%"


FOR %%A IN ("SDI64.exe" ^
    "SDI.exe" SDI2.cfg) DO COPY /Y /V "%%A" "%TEMP_NAME%\"

SET "LNG=%TEMP_NAME%\Tools\Langs"
SET "THEMES=%TEMP_NAME%\Tools\Themes"
SET "DOCS=%TEMP_NAME%\Docs"
IF NOT EXIST %LNG% MD %LNG%
IF NOT EXIST %THEMES% MD %THEMES%
IF NOT EXIST %DOCS% MD %DOCS%
XCOPY /E /Y /V "Tools\Langs" "%LNG%"
XCOPY /E /Y /V "Tools\Themes" "%THEMES%"
XCOPY /E /Y /V "Docs" "%DOCS%"
COPY /Y /V "Changes.txt" "%DOCS%"

PUSHD "%TEMP_NAME%"
"%SEVENZIP%" a -tzip -mm=deflate64 -mfb=257 -mpass=15^
 "%ZIP_NAME%.zip" "SDI*.exe" "sdi2.cfg" "SDI_auto.bat"^
 "Tools" "Docs"
IF %ERRORLEVEL% NEQ 0 CALL :SUBMSG "ERROR" "Compilation failed!"

CALL :SUBMSG "INFO" "%ZIP_NAME%.zip created successfully!"

MOVE /Y "%ZIP_NAME%.zip" "..\"
POPD
IF EXIST "%TEMP_NAME%" RD /S /Q "%TEMP_NAME%"
EXIT 


:SubDetectSevenzipPath
FOR %%G IN (%commander_path%\Plugins\WCX\Total7zip\64\7z.exe) DO (SET "SEVENZIP_PATH=%%~$PATH:G")
IF EXIST "%SEVENZIP_PATH%" (SET "SEVENZIP=%SEVENZIP_PATH%" & EXIT /B)

FOR %%G IN (7za.exe) DO (SET "SEVENZIP_PATH=%%~$PATH:G")
IF EXIST "%SEVENZIP_PATH%" (SET "SEVENZIP=%SEVENZIP_PATH%" & EXIT /B)

FOR /F "tokens=2*" %%A IN (
  'REG QUERY "HKLM\SOFTWARE\7-Zip" /v "Path" 2^>NUL ^| FIND "REG_SZ" ^|^|
   REG QUERY "HKLM\SOFTWARE\Wow6432Node\7-Zip" /v "Path" 2^>NUL ^| FIND "REG_SZ"') DO SET "SEVENZIP=%%B7z.exe"
EXIT /B


:SubGetVersion
rem Get the version
FOR /F "tokens=3,4 delims= " %%K IN (
  'FINDSTR /I /L /C:"define VERSION_MAJOR" "src\VersionEx.h"') DO (SET "VerMajor=%%K")
FOR /F "tokens=3,4 delims= " %%K IN (
  'FINDSTR /I /L /C:"define VERSION_MINOR" "src\VersionEx.h"') DO (SET "VerMinor=%%K")
FOR /F "tokens=3,4 delims= " %%K IN (
  'FINDSTR /I /L /C:"define VERSION_REV" "src\VersionEx.h"') DO (SET "VerRev=%%K")


SET SDI_VER=%VerMajor%.%VerMinor%.%VerRev%
EXIT /B


:SHOWHELP
TITLE %~nx0 %1
ECHO. & ECHO.
ECHO Usage:  %~nx0 [VS2010^|VS2012^|VS2013^|VS2015^|WDK]
ECHO.
ECHO Notes:  You can also prefix the commands with "-", "--" or "/".
ECHO         The arguments are not case sensitive.
ECHO. & ECHO.
ECHO Executing %~nx0 without any arguments is equivalent to "%~nx0 WDK"
ECHO.
ENDLOCAL
EXIT /B


:SUBMSG
ECHO. & ECHO ______________________________
ECHO [%~1] %~2
ECHO ______________________________ & ECHO.
IF /I "%~1" == "ERROR" (
  PAUSE
  EXIT
) ELSE (
  EXIT /B
)
