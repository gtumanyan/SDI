for /F %%i in ('dir /b *.7z') do call bin\repack_single_lzma2.bat %%i 128
rd temp /s /q
