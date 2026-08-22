@echo off
setlocal
REM Authenticode-sign one file with Azure Artifact Signing.
REM
REM This is the single signing command for the project. It is called both as a
REM workflow step (for the payload exe and the installer) and by
REM !uninstfinalize in Audionaut-Installer.nsi (for the uninstaller), so all
REM three binaries are signed with identical flags and there is one place to
REM change them.
REM
REM   Usage: sign.cmd <file>
REM
REM Configuration comes from the environment so nothing site-specific is
REM committed:
REM   SIGNTOOL       full path to signtool.exe (Windows SDK, /dlib capable)
REM   SIGN_DLIB      full path to Azure.CodeSigning.Dlib.dll
REM   SIGN_METADATA  full path to the JSON naming endpoint / account / profile
REM
REM Timestamping is NOT optional here. Azure Artifact Signing leaf certificates
REM are short-lived (around 72 hours), so an untimestamped signature stops
REM validating a few days after release. /tr binds the signature to a time when
REM the certificate was valid.

if "%~1"=="" (
  echo sign.cmd: no file given
  exit /b 1
)
if not exist "%~1" (
  echo sign.cmd: "%~1" does not exist
  exit /b 1
)
if "%SIGNTOOL%"=="" (
  echo sign.cmd: SIGNTOOL is not set
  exit /b 1
)
if "%SIGN_DLIB%"=="" (
  echo sign.cmd: SIGN_DLIB is not set
  exit /b 1
)
if "%SIGN_METADATA%"=="" (
  echo sign.cmd: SIGN_METADATA is not set
  exit /b 1
)

"%SIGNTOOL%" sign ^
  /v ^
  /fd SHA256 ^
  /tr http://timestamp.acs.microsoft.com ^
  /td SHA256 ^
  /dlib "%SIGN_DLIB%" ^
  /dmdf "%SIGN_METADATA%" ^
  "%~1"

exit /b %ERRORLEVEL%
