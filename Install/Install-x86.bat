@echo off
net session >nul 2>&1
if %errorLevel% == 0 (
  cd %~dp0\driver\x86
  %~d0
  ..\..\helpers\devcon-x86 remove '*Scream >nul 2>&1
  ..\..\helpers\devcon-x86 install Scream.inf *Scream
) else (
  echo Installing driver requires to run this batch file with admin rights.
)
pause
