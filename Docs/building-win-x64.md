# Build instructions for Windows 64-bit

- [Prepare folder](#prepare-folder)
- [Clone source code and prepare libraries](#clone-source-code-and-prepare-libraries)
- [Build the project](#build-the-project)

## Prepare folder

The build is done in **Visual Studio 2022 or later**.

Choose an empty folder for the future build, for example, **D:\\SDI**.

## Clone source code and prepare libraries

Open Project with **Visual Studio 2026** and Clone a repository. You may use [Initial setup script](/scripts/setup.ps1) if you don't have git or the latest Visual Studio installed. 

* Clone and update submodules with git submodule update --init --recursive
* Bootstrap Boost with `ext/boost/bootstrap.bat`
* Build Boost using `b2 --build-type=complete --with-thread --with-chrono --with-date_time address-model=64`

## Build the project

* Run `Version.cmd` in the root of the repository.
* Open `SDI.slnx` in the root of the repository.
* Select SDI project and press Build > Build Solution (F6).
* The compiled SDI*.exe will be generated in the same directory as the solution file.

* To create portable archive run `make_portable(.zip).cmd`
