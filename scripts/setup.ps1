Write-Output("Step 2");
Write-Output(Get-Date);

Set-ExecutionPolicy RemoteSigned -Force

## check admin right
If (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Warning "You do not have Administrator rights to run this script!`nPlease re-run this script as an Administrator!"
    Break
}

## Check Powershell version
if($PSversionTable.PsVersion.Major -lt 7){
    Write-Error "Please use Powershell 7 to execute this script!"
    Break
}

winget install Microsoft.VisualStudio.2022.Community.Preview  --override "install --config %~dp0\SDI.vsconfig --passive"
winget install Git.Git -i
## Install Visual Studio Exntension
Write-Host "`n Install Visual Studio Exntension" -ForegroundColor Green
$vsixInstallScript = "$PSScriptRoot\install-vsix.ps1";
Invoke-WebRequest -Uri "https://gist.githubusercontent.com/lettucebo/1c791b21bf56f467254bc85fd70631f4/raw/5dc3ff85b38058208d203383c54d8b7818365566/install-vsix.ps1" -OutFile $vsixInstallScript
& $vsixInstallScript -PackageName "MadsKristensen.FileIcons"
& $vsixInstallScript -PackageName "MadsKristensen.Tweaks"
& $vsixInstallScript -PackageName "MikeWard-AnnArbor.VSColorOutput64"
& $vsixInstallScript -PackageName "MadsKristensen.RainbowBraces"
 
## Complete
Write-Host -NoNewLine "`n Environment config complete, Press any key to continue...";
$null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown');
