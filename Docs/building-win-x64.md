# Build instructions for Windows 64-bit

- [Prepare folder](#prepare-folder)
- [Clone source code and prepare libraries](#clone-source-code-and-prepare-libraries)
- [Build the project](#build-the-project)

## Prepare folder

The build is done in **Visual Studio 2022**.

Choose an empty folder for the future build, for example, **D:\\SDI**. It will be named ***BuildPath*** in the rest of this document.

## Clone source code and prepare libraries

Open Project with **Visual Studio 2022** and Clone a repository. You may use [Initial setup script](/scripts/setup.ps1) if you don't have git or the latest Visual Studio installed. 

* Clone and update submodules with git submodule update --init --recursive
* Bootstrap Boost with ext/boost/bootstrap.bat
* Build Boost using b2 --build-type=complete --with-thread --with-chrono --with-date_time address-model=64

## Build the project

Go to ***BuildPath*** and run

    Version.cmd

* Open ***BuildPath***\\vs2022\\SDI.sln in Visual Studio 2022
* Select SDI project and press Build > Build Solution (F6)
* The result SDI*.exe will be located in ***BuildPath***
* To create portable archive run make_portable(.zip).cmd
