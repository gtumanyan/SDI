## check admin rights
If (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Warning "You do not have Administrator rights to run this script!`nPlease re-run this script as an Administrator!"
    Break
}

## Check Powershell version
if($PSversionTable.PsVersion.Major -lt 7){
    Write-Error "Please use Powershell 7 to execute this script!"
    #Break
}

winget install Git.Git -i
Write-Host -NoNewLine "`n Environment config complete, Press any key to continue...";
$null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown');

## Install Developer Font
##### https://gist.github.com/anthonyeden/0088b07de8951403a643a8485af2709b
##### https://gist.github.com/cosine83/e83c44878a6bdeac0c7c59e3dbfd1f71
Write-Host "`n Install Developer Font" -ForegroundColor Green
$fontUrl = "https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/YaHei%20Consolas.ttf";
$fontFile = "$PSScriptRoot\YaHei.ttf";
$fontNoto1Url = "https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/NotoSansCJKtc-Black.otf";
$fontNoto1File = "$PSScriptRoot\NotoSansCJKtc-Black.otf";
$fontNoto2Url = "https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/NotoSansCJKtc-Bold.otf";
$fontNoto2File = "$PSScriptRoot\NotoSansCJKtc-Bold.otf";
$fontNoto3Url = "https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/NotoSansCJKtc-DemiLight.otf";
$fontNoto3File = "$PSScriptRoot\NotoSansCJKtc-DemiLight.otf";
$fontNoto4Url = "https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/NotoSansCJKtc-Light.otf";
$fontNoto4File = "$PSScriptRoot\NotoSansCJKtc-Light.otf";
$fontNoto5Url = "https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/NotoSansCJKtc-Medium.otf";
$fontNoto5File = "$PSScriptRoot\NotoSansCJKtc-Medium.otf";
$fontNoto6Url = "https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/NotoSansCJKtc-Regular.otf";
$fontNoto6File = "$PSScriptRoot\NotoSansCJKtc-Regular.otf";
$fontNoto7Url = "https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/NotoSansCJKtc-Thin.otf";
$fontNoto7File = "$PSScriptRoot\NotoSansCJKtc-Thin.otf";
$fontNoto8Url = "https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/NotoSansMonoCJKtc-Bold.otf";
$fontNoto8File = "$PSScriptRoot\NotoSansMonoCJKtc-Bold.otf";
$fontNoto9Url = "https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/NotoSansMonoCJKtc-Regular.otf";
$fontNoto9File = "$PSScriptRoot\NotoSansMonoCJKtc-Regular.otf";
$fontFira01Url="https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/FiraCode/FiraCodeNerdFont-Bold.ttf"
$fontFira01File = "$PSScriptRoot\FiraCodeNerdFont-Bold.ttf";
$fontFira02Url="https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/FiraCode/FiraCodeNerdFont-Light.ttf"
$fontFira02File = "$PSScriptRoot\FiraCodeNerdFont-Light.ttf";
$fontFira03Url="https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/FiraCode/FiraCodeNerdFont-Medium.ttf"
$fontFira03File = "$PSScriptRoot\FiraCodeNerdFont-Medium.ttf";
$fontFira04Url="https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/FiraCode/FiraCodeNerdFontMono-Bold.ttf"
$fontFira04File = "$PSScriptRoot\FiraCodeNerdFontMono-Bold.ttf";
$fontFira05Url="https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/FiraCode/FiraCodeNerdFontMono-Light.ttf"
$fontFira05File = "$PSScriptRoot\FiraCodeNerdFontMono-Light.ttf";
$fontFira06Url="https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/FiraCode/FiraCodeNerdFontMono-Medium.ttf"
$fontFira06File = "$PSScriptRoot\FiraCodeNerdFontMono-Medium.ttf";
$fontFira07Url="https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/FiraCode/FiraCodeNerdFontMono-Regular.ttf"
$fontFira07File = "$PSScriptRoot\FiraCodeNerdFontMono-Regular.ttf";
$fontFira08Url="https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/FiraCode/FiraCodeNerdFontMono-Retina.ttf"
$fontFira08File = "$PSScriptRoot\FiraCodeNerdFontMono-Retina.ttf";
$fontFira09Url="https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/FiraCode/FiraCodeNerdFontMono-SemiBold.ttf"
$fontFira09File = "$PSScriptRoot\FiraCodeNerdFontMono-SemiBold.ttf";
$fontFira10Url="https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/FiraCode/FiraCodeNerdFontPropo-Bold.ttf"
$fontFira10File = "$PSScriptRoot\FiraCodeNerdFontPropo-Bold.ttf";
$fontFira11Url="https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/FiraCode/FiraCodeNerdFontPropo-Light.ttf"
$fontFira11File = "$PSScriptRoot\FiraCodeNerdFontPropo-Light.ttf";
$fontFira12Url="https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/FiraCode/FiraCodeNerdFontPropo-Medium.ttf"
$fontFira12File = "$PSScriptRoot\FiraCodeNerdFontPropo-Medium.ttf";
$fontFira13Url="https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/FiraCode/FiraCodeNerdFontPropo-Regular.ttf"
$fontFira13File = "$PSScriptRoot\FiraCodeNerdFontPropo-Regular.ttf";
$fontFira14Url="https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/FiraCode/FiraCodeNerdFontPropo-Retina.ttf"
$fontFira14File = "$PSScriptRoot\FiraCodeNerdFontPropo-Retina.ttf";
$fontFira15Url="https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/FiraCode/FiraCodeNerdFontPropo-SemiBold.ttf"
$fontFira15File = "$PSScriptRoot\FiraCodeNerdFontPropo-SemiBold.ttf";
$fontFira16Url="https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/FiraCode/FiraCodeNerdFont-Regular.ttf"
$fontFira16File = "$PSScriptRoot\FiraCodeNerdFont-Regular.ttf";
$fontFira17Url="https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/FiraCode/FiraCodeNerdFont-Retina.ttf"
$fontFira17File = "$PSScriptRoot\FiraCodeNerdFont-Retina.ttf";
$fontFira18Url="https://github.com/lettucebo/Ci.Environment/raw/master/Fonts/FiraCode/FiraCodeNerdFont-SemiBold.ttf"
$fontFira18File = "$PSScriptRoot\FiraCodeNerdFont-SemiBold.ttf";

Write-Host "`n Download fontFile..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontUrl -OutFile $fontFile
Write-Host "`n Download fontNoto1File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontNoto1Url -OutFile $fontNoto1File
Write-Host "`n Download fontNoto2File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontNoto2Url -OutFile $fontNoto2File
Write-Host "`n Download fontNoto3File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontNoto3Url -OutFile $fontNoto3File
Write-Host "`n Download fontNoto4File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontNoto4Url -OutFile $fontNoto4File
Write-Host "`n Download fontNoto5File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontNoto5Url -OutFile $fontNoto5File
Write-Host "`n Download fontNoto6File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontNoto6Url -OutFile $fontNoto6File
Write-Host "`n Download fontNoto7File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontNoto7Url -OutFile $fontNoto7File
Write-Host "`n Download fontNoto8File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontNoto8Url -OutFile $fontNoto8File
Write-Host "`n Download fontNoto9File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontNoto9Url -OutFile $fontNoto9File

Write-Host "`n Download fontFira01File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontFira01Url -OutFile $fontFira01File
Write-Host "`n Download fontFira02File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontFira02Url -OutFile $fontFira02File
Write-Host "`n Download fontFira03File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontFira03Url -OutFile $fontFira03File
Write-Host "`n Download fontFira04File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontFira04Url -OutFile $fontFira04File
Write-Host "`n Download fontFira05File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontFira05Url -OutFile $fontFira05File
Write-Host "`n Download fontFira06File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontFira06Url -OutFile $fontFira06File
Write-Host "`n Download fontFira07File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontFira07Url -OutFile $fontFira07File
Write-Host "`n Download fontFira08File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontFira08Url -OutFile $fontFira08File
Write-Host "`n Download fontFira09File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontFira09Url -OutFile $fontFira09File
Write-Host "`n Download fontFira10File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontFira10Url -OutFile $fontFira10File
Write-Host "`n Download fontFira11File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontFira11Url -OutFile $fontFira11File
Write-Host "`n Download fontFira12File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontFira12Url -OutFile $fontFira12File
Write-Host "`n Download fontFira13File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontFira13Url -OutFile $fontFira13File
Write-Host "`n Download fontFira14File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontFira14Url -OutFile $fontFira14File
Write-Host "`n Download fontFira15File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontFira15Url -OutFile $fontFira15File
Write-Host "`n Download fontFira16File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontFira16Url -OutFile $fontFira16File
Write-Host "`n Download fontFira17File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontFira17Url -OutFile $fontFira17File
Write-Host "`n Download fontFira18File..." -ForegroundColor Gray
Invoke-WebRequest -Uri $fontFira18Url -OutFile $fontFira18File

Write-Host "`n Installing NotoSans fonts..." -ForegroundColor Gray
$objFolder = (New-Object -ComObject Shell.Application).Namespace(0x14)
$objFolder.CopyHere($fontFile, 0x10)
$objFolder.CopyHere($fontNoto1File, 0x10)
$objFolder.CopyHere($fontNoto2File, 0x10)
$objFolder.CopyHere($fontNoto3File, 0x10)
$objFolder.CopyHere($fontNoto4File, 0x10)
$objFolder.CopyHere($fontNoto5File, 0x10)
$objFolder.CopyHere($fontNoto6File, 0x10)
$objFolder.CopyHere($fontNoto7File, 0x10)
$objFolder.CopyHere($fontNoto8File, 0x10)
$objFolder.CopyHere($fontNoto9File, 0x10)
Write-Host "`n NotoSans fonts installed..." -ForegroundColor Gray

Write-Host "`n Installing FiraCode fonts..." -ForegroundColor Gray
$objFolder.CopyHere($fontFira01File, 0x10)
$objFolder.CopyHere($fontFira02File, 0x10)
$objFolder.CopyHere($fontFira03File, 0x10)
$objFolder.CopyHere($fontFira04File, 0x10)
$objFolder.CopyHere($fontFira05File, 0x10)
$objFolder.CopyHere($fontFira06File, 0x10)
$objFolder.CopyHere($fontFira07File, 0x10)
$objFolder.CopyHere($fontFira08File, 0x10)
$objFolder.CopyHere($fontFira09File, 0x10)
$objFolder.CopyHere($fontFira10File, 0x10)
$objFolder.CopyHere($fontFira11File, 0x10)
$objFolder.CopyHere($fontFira12File, 0x10)
$objFolder.CopyHere($fontFira13File, 0x10)
$objFolder.CopyHere($fontFira14File, 0x10)
$objFolder.CopyHere($fontFira15File, 0x10)
$objFolder.CopyHere($fontFira16File, 0x10)
$objFolder.CopyHere($fontFira17File, 0x10)
$objFolder.CopyHere($fontFira18File, 0x10)
Write-Host "`n FiraCode fonts installed..." -ForegroundColor Gray

## Install VS 2022
# https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-community?view=vs-2022&preserve-view=true#desktop-development-with-c
Write-Host "`n Install VS 2022" -ForegroundColor Green
winget install Microsoft.VisualStudio.2022.Community.Preview  --override "install --config %~dp0\..\vs2022\.vsconfig --passive"
