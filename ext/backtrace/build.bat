@echo off
set path=c:\mingw\mingw32\bin
mingw32-make.exe -f Makefile_32

set path=c:\mingw\mingw64\bin
mingw32-make.exe -f Makefile_64