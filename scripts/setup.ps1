## check admin rights
If (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Warning "You do not have Administrator rights to run this script!`nPlease re-run this script as an Administrator!"
    Break
}

## Install Git
winget install Git.Git -i
Write-Host -NoNewLine "`n Environment config complete, Press any key to continue...";
$null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown');

# install nerd fonts
git clone --filter=blob:none --sparse https://github.com/ryanoasis/nerd-fonts "$env:USERPROFILE/Downloads/nerd-fonts"

Invoke-Expression "$env:USERPROFILE/Downloads/nerd-fonts/install.ps1"

## Install VS 2022
# https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-community?view=vs-2022&preserve-view=true#desktop-development-with-c
Write-Host "`n Install VS 2022" -ForegroundColor Green
winget install Microsoft.VisualStudio.2022.Community.Preview --override "install --config $PSScriptRoot\..\vs2022\.vsconfig --lang en-US --passive --wait --allowUnsignedExtensions"

## Start SDI Project
$PSScriptRoot\..\vs2022\SDI.sln