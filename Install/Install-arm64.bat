@echo off
net session >nul 2>&1
if %errorLevel% == 0 (
  cd %~dp0\driver\arm64
  %~d0
  ..\..\helpers\devcon-arm64 remove '*Scream >nul 2>&1
  ..\..\helpers\devcon-arm64 install Scream.inf *Scream
) else (
  echo Installing driver requires to run this batch file with admin rights.
)
pause
