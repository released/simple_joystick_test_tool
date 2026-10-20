@echo off
call "%~dp0clean_build.bat" %*
exit /b %errorlevel%
