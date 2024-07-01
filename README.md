To build, install latest Visual Studio using [Initial setup script](scripts/setup.ps1) and open vs2022\SDI.sln solution.
Clone and update submodules with git submodule update --init --recursive
Bootstrap Boost with external/boost/bootstrap.bat
Build Boost using b2 --build-type=complete --with-thread --with-chrono --with-date_time address-model=64

# SDI

![SDI Logo](.github/logo128.jpg)
