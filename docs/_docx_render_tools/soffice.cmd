@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0soffice_word.ps1" %*
exit /b %ERRORLEVEL%
