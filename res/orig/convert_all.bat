mkdir outpng
mkdir outwebp
for /F %%i in ('dir /b *.webp') do ..\..\ext\libwebp\output\release-static\x64\bin\dwebp.exe %%i -o outpng\%%~ni.png
for /F %%i in ('dir /b *.png')  do ..\..\ext\libwebp\output\release-static\x64\bin\cwebp.exe %%i -o outwebp\%%~ni.webp -lossless
