mkdir outpng
mkdir outwebp
for /F %%i in ('dir /b *.webp') do dwebp.exe %%i -o outpng\%%i.png
for /F %%i in ('dir /b *.png')  do cwebp.exe %%i -o outwebp\%%i.webp -lossless
