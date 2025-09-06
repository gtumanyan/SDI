## Check admin rights
If (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Warning "You do not have Administrator rights to run this script!`nPlease re-run this script as an Administrator!"
    Break
}

## Install Git
winget install Git.Git -i

## Install VS 2022
# https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-community?view=vs-2022&preserve-view=true#desktop-development-with-c
Write-Host "`n Install VS 2022" -ForegroundColor Green
winget install Microsoft.VisualStudio.2022.Community.Preview --override "install --config $PSScriptRoot\..\.vs\.vsconfig --locale en-US --passive --wait --allowUnsignedExtensions"

## Start SDI Project
Start-Process "$PSScriptRoot\..\vs2022\SDI.sln"