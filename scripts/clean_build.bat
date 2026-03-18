@echo off
setlocal

set "ROOT=%~dp0.."
set "BUILD_DIR=%ROOT%\build"
set "KEEP_EXE=JoyStickTestTool.exe"

if not exist "%BUILD_DIR%" (
  echo Build directory not found: "%BUILD_DIR%"
  exit /b 0
)

for /f "delims=" %%F in ('dir /b /a "%BUILD_DIR%" 2^>nul') do (
  if /i not "%%F"=="%KEEP_EXE%" (
    if exist "%BUILD_DIR%\%%F\" (
      rmdir /s /q "%BUILD_DIR%\%%F"
    ) else (
      del /f /q "%BUILD_DIR%\%%F" >nul 2>nul
    )
  )
)

if exist "%BUILD_DIR%\%KEEP_EXE%" (
  echo Clean complete. Kept: build\%KEEP_EXE%
) else (
  echo Clean complete. No exe found to keep: build\%KEEP_EXE%
)

endlocal
exit /b 0
