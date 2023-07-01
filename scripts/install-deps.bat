rem Install Visual Studio Community edition with Desktop development with C++ package.
rem ----------------------------------------------------------------------------------------
rem
rem The config in vsconfig.json is the config for "Desktop development with C++".
rem
rem The config is determined by first installing Visual Studio Community Edition on your
rem devbox. Then run `c:\Program Files (x86)\Microsoft Visual Studio\Installer\setup.exe` and click
rem `More -> Export Configuration Settings`, select an config file to write, then `Review Details`
rem and select "Desktop development with C++".
echo.
echo.* Step 3: Install Visual Studio Code Community edition with Desktop development with C++ package.
winget install Microsoft.VisualStudio.2022.Community.Preview  --override "install --config %~dp0\SDI.vsconfig --passive"

echo.
winget install Git.Git -i

echo.
start "" https://download.tortoisegit.org/tgit/previews/