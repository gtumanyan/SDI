# Build instructions for Windows 64-bit

- [Prepare folder](#prepare-folder)
- [Clone source code and prepare libraries](#clone-source-code-and-prepare-libraries)
- [Build the project](#build-the-project)

## Prepare folder

The build is done in **Visual Studio 2022 or later**.

Choose an empty folder for the future build, for example, **D:\\SDI**.

## Clone source code and prepare libraries

Open Project with **Visual Studio 2026** and Clone a repository. You may run [Initial setup script](/scripts/setup.ps1) if you don't have git or the latest Visual Studio installed. The script downloads only required boost libraries reducing the boost footprint drastically.

* Clone and update submodules: `git submodule update --init --recursive`
* Open the **x64 Native Tools Command Prompt for VS 2026** (search in Start menu for "Developer Command Prompt").
* `cd ext/boost`
* Bootstrap Boost: `bootstrap`
  * If **"Unknown toolset: vcunk"** error: `bootstrap msvc`
* Generate Boost headers (required for libtorrent): `b2 headers`

## Build the project

* Run `Version.cmd` in the root of the repository.
* Open `SDI.slnx` in the root of the repository.
* Select SDI project and press Build > Build Solution (F6).
* The compiled SDI*.exe will be generated in the same directory as the solution file.

* To create portable archive run `make_portable(.zip).cmd`
