for /F %%i in ('dir /b *.7z') do call bin\repack_single_merge.bat %%i 128
rem rd temp /s /q
