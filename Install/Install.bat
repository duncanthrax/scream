@echo off
net session >nul 2>&1
if %errorLevel% == 0 (
  cd %~dp0\driver
  %~d0
  ..\helpers\devcon remove '*Scream >nul 2>&1
  ..\helpers\devcon install Scream.inf *Scream
) else (
  echo Installing driver requires to run this batch file with admin rights.
)
pause
