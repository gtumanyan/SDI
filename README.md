To build: 
Clone and update submodules with git submodule update --init --recursive
Bootstrap Boost with external/boost/bootstrap.bat
Build Boost using b2 --build-type=complete --with-thread --with-chrono --with-date_time address-model=64
