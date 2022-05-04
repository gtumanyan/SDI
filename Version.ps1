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
  $AppName = "SDI"
	$Major = [int]$(Get-Date -format yy)
	$Minor = [int]$(Get-Date -format MM)
	$Revis = [int]$(Get-Date -format dd)
	$Build = [int](Get-Content "Versions\build.txt")
	if (!$Build) { $Build = 0 }
	$LastBuildDay = [string](Get-Content "Versions\day.txt")

	$AppVeyorBuild = [int]($env:appveyor_build_number) # AppVeyor internal

	if ($AppVeyorEnv) {
		if ($LastBuildDay -ne "$Revis") {
			$Revis | Set-Content "Versions\day.txt"
			$Build = 1  # reset (AppVeyor)
		}
		$CommitID = ([string]($env:appveyor_repo_commit)).substring(0,8)
	}
	else {
		if ($LastBuildDay -ne "$Revis") {
			$Revis | Set-Content "Versions\day.txt"
			$Build = 0  # reset (local build)
		}
		# locally: increase build number and persit it
		$Build = $Build + 1
		# locally: we have no commit ID, create an arificial one
		$CommitID = [string](Get-Content "Versions\commit_id.txt")
		if (!$CommitID) { $CommitID = "---" }
		if ($CommitID -eq "computername") {
            $length = ([string]($env:computername)).length
			$CommitID = ([string]($env:computername)).substring(0,[math]::min($length,8)).ToLower()
		}
		else {
			if (!$CommitID) { $CommitID = "---" }
			$CommitID = $CommitID -replace '"', ''
			$CommitID = $CommitID -replace "'", ''
		    $length = $CommitID.length
			$CommitID = $CommitID.substring(0,[math]::min($length,8))
		}
	}
	if (!$CommitID) { $CommitID = "---" }
	$Build | Set-Content "Versions\build.txt"

	$CompleteVer = "$Major.$Minor.$Revis"
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
	[string](Get-Content "ext\libtorrent\build_dist.sh"-TotalCount 19)[-1] -match '[\d.]+$'
	$TorrentVer = $Matches.0
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
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$APPNAME\$', "$AppName" } | Set-Content "src\VersionEx.h"
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$MAJOR\$', "$Major" } | Set-Content "src\VersionEx.h"
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$MINOR\$', "$Minor" } | Set-Content "src\VersionEx.h"
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$MAINT\$', "$Revis" } | Set-Content "src\VersionEx.h"
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$BUILD\$', "$Build" } | Set-Content "src\VersionEx.h"
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$7ZVER\$', "$7zVer" } | Set-Content "src\VersionEx.h"
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$WEBPVER\$', "$WebPVer" } | Set-Content "src\VersionEx.h"
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$BOOSTVER\$', "$BoostVer" } | Set-Content "src\VersionEx.h"
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$TORRENTVER\$', "$TorrentVer" } | Set-Content "src\VersionEx.h"
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$VERPATCH\$', "$VerPatch" } | Set-Content "src\VersionEx.h"
	(Get-Content "src\VersionEx.h") | ForEach-Object { $_ -replace '\$COMMITID\$', "$CommitID" } | Set-Content "src\VersionEx.h"
	
	Copy-Item -LiteralPath "Versions\SDI.exe.manifest.tpl" -Destination "res\SDI.exe.manifest.conf" -Force
	(Get-Content "res\SDI.exe.manifest.conf") | ForEach-Object { $_ -replace '\$APPNAME\$', "$AppName" } | Set-Content "res\SDI.exe.manifest.conf"
	(Get-Content "res\SDI.exe.manifest.conf") | ForEach-Object { $_ -replace '\$VERPATCH\$', "$VerPatch" } | Set-Content "res\SDI.exe.manifest.conf"
	(Get-Content "res\SDI.exe.manifest.conf") | ForEach-Object { $_ -replace '\$VERSION\$', "$CompleteVer" } | Set-Content "res\SDI.exe.manifest.conf"
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
	Exit $LastExitCode
	Write-Host ""
	Write-Host "VersionPatching: Done! Elapsed time: $($stopwatch.Elapsed)."
	Exit $LastExitCode
}

