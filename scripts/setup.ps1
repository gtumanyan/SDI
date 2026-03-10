## Check admin rights
If (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
  Write-Warning "You do not have Administrator rights to run this script!`nPlease re-run this script as an Administrator!"
  throw
}

## Check winget
Write-Host "Checking Winget installation..." -ForegroundColor Green
try {winget update}
catch {
    Write-Host "Installing Winget..." -ForegroundColor Yellow
    irm https://raw.githubusercontent.com/gtumanyan/windows-tools/master/tools/Winget_LTSC_Installer.ps1 | iex
}

## Install Git
Write-Host "Checking Git installation..." -ForegroundColor Green
try {
    $gitVersion = git --version 2>$null
    Write-Host "Git is already installed: $gitVersion" -ForegroundColor Cyan
} catch {
    Write-Host "Installing Git..." -ForegroundColor Yellow
    winget install Git.Git --silent --accept-package-agreements --accept-source-agreements
    # Refresh PATH
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
}

## Install VS 2026
Write-Host "`nChecking Visual Studio 2026 installation..." -ForegroundColor Green
# Uncomment the following line to auto-install VS 2026 Insiders
winget install Microsoft.VisualStudio.Community.Insiders --silent --override "--config `"$PSScriptRoot\..\.vs\.vsconfig`" --locale en-US --passive --wait --allowUnsignedExtensions"

## Initialize Main repo
git init
git remote add upstream https://github.com/gtumanyan/SDI.git
git fetch upstream
git reset upstream/dev
## Initialize submodules
Push-Location "$PSScriptRoot\..\ext"
git clone --depth=1 https://github.com/boostorg/boost.git
git clone https://github.com/arvidn/libtorrent
git clone https://github.com/webmproject/libwebp 
### Initialize libtorrent's nested submodules (try_signal is required for build)
cd libtorrent
git submodule update --init deps/try_signal
### Initialize required Boost libraries
cd ..\boost
### Initialize Boost.Build tools first (required for bootstrap.bat and b2 headers)
git submodule update --init tools/build tools/boost_install
### Core libraries required by libtorrent
$libs = @(
  'libs/config', 'libs/crc', 'libs/headers', 'libs/assert', 'libs/core',
  'libs/asio', 'libs/optional', 'libs/variant', 'libs/multiprecision',
  'libs/logic', 'libs/range', 'libs/utility', 'libs/smart_ptr', 'libs/date_time',
  'libs/functional', 'libs/pool', 'libs/multi_index', 'libs/intrusive', 'libs/predef',
  'libs/system', 'libs/fusion',
  # Dependencies of above
  'libs/throw_exception', 'libs/static_assert', 'libs/type_traits', 'libs/mpl',
  'libs/preprocessor', 'libs/move', 'libs/winapi', 'libs/iterator', 'libs/tuple',
  'libs/bind', 'libs/io', 'libs/numeric/conversion', 'libs/integer', 'libs/detail',
  'libs/array', 'libs/container_hash', 'libs/describe', 'libs/mp11', 'libs/align',
  'libs/typeof', 'libs/function', 'libs/concept_check', 'libs/conversion',
  'libs/regex', 'libs/tokenizer', 'libs/lexical_cast', 'libs/container',
  'libs/exception', 'libs/algorithm', 'libs/function_types', 'libs/type_index'
)
git submodule update -j12 --init -f $libs
 
## Install boost
    Write-Host "Configuring MSVC build environment..." -ForegroundColor Green
    # Use vswhere.exe to find Visual Studio with C++ tools
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        $vswhere = "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
    }

    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found. Please install Visual Studio."
    }

    # Find VS with C++ tools, including prereleases (for VS 2026 Insiders)
    $vsPath = & $vswhere -latest -prerelease -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath

    if (-not $vsPath) {
        throw "Could not find Visual Studio with C++ tools installed!"
    }

    Write-Host "Found Visual Studio at: $vsPath" -ForegroundColor Green

    # Build the path to VsDevCmd.bat
    $vsDevCmd = Join-Path $vsPath "Common7\Tools\VsDevCmd.bat"

    if (-not (Test-Path $vsDevCmd)) {
        throw "VsDevCmd.bat not found at: $vsDevCmd"
    }

    # Import VS environment variables into PowerShell session
    Write-Host "Importing Visual Studio environment variables..." -ForegroundColor Cyan
    cmd.exe /c "`"$vsDevCmd`" -no_logo && set" | ForEach-Object {
        if ($_ -match "^([^=]+)=(.*)$") {
            [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }
    & .\bootstrap.bat msvc
    .\b2 headers

## Set_version
Push-Location "$PSScriptRoot\..\"
& .\Version.cmd "(ALPHA)"

## Start SDI Project
Write-Verbose -Message $PSScriptRoot
Start-Process "$PSScriptRoot\..\SDI.slnx"