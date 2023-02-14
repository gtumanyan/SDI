# ------------------------------------------------------------
# PowerShell script to perform Version patching for SDI 
# ToDo:
# - adapt $Build number in case of local machine builds
# ------------------------------------------------------------
param(
	[switch]$AppVeyorEnv = $false,
	[string]$VerPatch = ""
)
# ------------------------------------------------------------
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"  ## Write-Error should throw ...
# ------------------------------------------------------------
$ScriptDir = Split-Path -Path "$(if ($PSVersionTable.PSVersion.Major -ge 3) { $PSCommandPath } else { & { $MyInvocation.ScriptName } })" -Parent
# ------------------------------------------------------------
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$LastExitCode = 0
# ------------------------------------------------------------

# ============================================================
function DebugOutput($msg)
# ------------------------------------------------------------
{
	#~return ## disabled debug output
	if ($msg -ne $null) { 
		Write-Host ""
		Write-Host "$msg"
	}
}
# ------------------------------------------------------------

# ============================================================

try 
{
	$AppName = "SDI 2023"
	$Major = [int]$(Get-Date -format yy)
	$Minor = [int]$(Get-Date -format MM)
	$Revis = [int]$(Get-Date -format dd)
	$BuildPath = "Versions\build.txt"
	if (!(Test-Path $BuildPath)) {
		New-Item -Path $BuildPath -ItemType "file" -Value "0"
	}
	$Build = [int](Get-Content $BuildPath)
	if (!$Build) { $Build = -1 }
	
	$DayPath = "Versions\day.txt"
	if (!(Test-Path $DayPath)) {
		New-Item -Path $DayPath -ItemType "file" -Value "0"
	}
	$LastBuildDay = [string](Get-Content $DayPath)
	if (!$LastBuildDay) { $LastBuildDay = 0 }

	$AppVeyorBuild = [int]($env:appveyor_build_number) # AppVeyor internal

	if ($AppVeyorEnv) {
		if ($LastBuildDay -ne "$Revis") {
			$Revis | Set-Content -Path $DayPath
			$Build = 1  # reset (AppVeyor)
		}
		$CommitID = ([string]($env:appveyor_repo_commit)).substring(0,8)
	}
	else {
		if ($LastBuildDay -ne "$Revis") {
			$Revis | Set-Content -Path $DayPath
			$Build = -1  # reset (local build)
		}
		# locally: increase build number and persist it
		$Build = $Build + 1
		# locally: read commit ID from .git\refs\heads\<first file>
		$HeadDir = ".git\refs\heads"
		$HeadMaster = Get-ChildItem -Path $HeadDir -Force -Recurse -File | Select-Object -First 1
		$CommitID = [string](Get-Content "$HeadDir\$HeadMaster" -TotalCount 8)
		if (!$CommitID) {
            $length = ([string]($env:computername)).length
			$CommitID = ([string]($env:computername)).substring(0,[math]::min($length,8)).ToLower()
		}
		$CommitID = $CommitID -replace '"', ''
		$CommitID = $CommitID -replace "'", ''
		$length = $CommitID.length
		$CommitID = $CommitID.substring(0,[math]::min($length,8))
	}
	if (!$CommitID) { $CommitID = "---" }
	$Build | Set-Content -Path $BuildPath

	$CompleteVer = "$Major.$Minor.$Revis.$Build"
	DebugOutput("SDI version number: 'v$CompleteVer $VerPatch'")
	
	if ($AppVeyorEnv) {
		# AppVeyor needs unique artefact build number
		$AppVeyorVer = "0.0.0.$AppVeyorBuild"
		DebugOutput("AppVeyor version number: 'v$AppVeyorVer $VerPatch'")
		Update-AppveyorBuild -Version $AppVeyorVer
	}

	[string](Get-Content "ext\libwebp\NEWS"-TotalCount 1) -match '[\d.]+$'
	$WebPVer = $Matches.0
	DebugOutput("WebP $WebPVer")
	if (!$WebPVer) { $WebPVer = 0 }
	$TorrentVer = ((Get-Content "ext\libtorrent\Makefile"-TotalCount 1) -split '=')[1]
	DebugOutput("Libtorrent $TorrentVer")
	if (!$TorrentVer) { $TorrentVer = "0.0.0" }
	$BoostVer = [string](Get-Content "ext\boost\tools\boost_install\test\BoostVersion.cmake"-TotalCount 1).Substring(18,6)
	if (!$BoostVer) { $BoostVer = "0.0.0" }
	DebugOutput("$BoostVer")
	[string](Get-Content "ext\SevenZ\C\7zVersion.h"-TotalCount 4)[-1]-match '[\d.]+'
	$7zVer=$Matches.0
	DebugOutput("7zip version number: 'v$7zVer'")
	if (!$7zVer) { $7zVer = 0 }

#~if ($VerPatch) { $VerPatch = " $VerPatch" }  # ensure space in front of string

	Copy-Item -LiteralPath "Versions\VersionEx.h.tpl" -Destination "src\VersionEx.h" -Force
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$APPNAME\$', "$AppName" } | Set-Content -Path "src\VersionEx.h"
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$MAJOR\$', "$Major" } | Set-Content -Path "src\VersionEx.h"
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$MINOR\$', "$Minor" } | Set-Content -Path "src\VersionEx.h"
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$MAINT\$', "$Revis" } | Set-Content -Path "src\VersionEx.h"
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$BUILD\$', "$Build" } | Set-Content -Path "src\VersionEx.h"
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$7ZVER\$', "$7zVer" } | Set-Content -Path "src\VersionEx.h"
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$WEBPVER\$', "$WebPVer" } | Set-Content -Path "src\VersionEx.h"
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$BOOSTVER\$', "$BoostVer" } | Set-Content -Path "src\VersionEx.h"
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$TORRENTVER\$', "$TorrentVer" } | Set-Content -Path "src\VersionEx.h"
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$VERPATCH\$', "$VerPatch" } | Set-Content -Path "src\VersionEx.h"
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$COMMITID\$', "$CommitID" } | Set-Content -Path "src\VersionEx.h"
	
	$confManifest = "res\SDI.exe.conf.manifest"
	Copy-Item -LiteralPath "Versions\SDI.exe.manifest.tpl" -Destination $ConfManifest -Force
	(Get-Content $ConfManifest) | ForEach-Object { $_ -replace '\$APPNAME\$', "$AppName" } | Set-Content -Path $ConfManifest
	(Get-Content $ConfManifest) | ForEach-Object { $_ -replace '\$VERPATCH\$', "$VerPatch" } | Set-Content -Path $ConfManifest
	(Get-Content $ConfManifest) | ForEach-Object { $_ -replace '\$VERSION\$', "$CompleteVer" } | Set-Content -Path $ConfManifest
}
catch 
{
	if ($LastExitCode -eq 0) { $LastExitCode = 99 }
	$errorMessage = $_.Exception.Message
	Write-Warning "Exception caught: `"$errorMessage`"!"
	throw $_
}
finally
{
	[Environment]::SetEnvironmentVariable("LASTEXITCODE", $LastExitCode, "User")
	$host.SetShouldExit($LastExitCode)
	Write-Host ""
	Write-Host "VersionPatching: Done! Elapsed time: $($stopwatch.Elapsed)."
	Exit $LastExitCode
}
