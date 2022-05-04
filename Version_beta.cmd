:: Batch file for BETA version
@echo off
setlocal
set _VERPATCH_=beta
@rem echo."_%_VERPATCH_%">.\_buildname.txt
Version -VerPatch "%_VERPATCH_%"
endlocal
