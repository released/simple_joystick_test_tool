@echo off
setlocal

set "ROOT=%~dp0.."
set "EXE=%ROOT%\build\JoyStickTestTool.exe"

pushd "%ROOT%"
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$proc = Get-Process -Name 'JoyStickTestTool' -ErrorAction SilentlyContinue;" ^
  "if ($proc) { $proc | Stop-Process -Force }"
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe';" ^
  "if (!(Test-Path $vswhere)) { Write-Error 'vswhere not found. Please install Visual Studio.'; exit 1 };" ^
  "$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1;" ^
  "if (!$msbuild) { Write-Error 'MSBuild not found.'; exit 1 };" ^
  "& $msbuild 'JoyStickTestTool.sln' /p:Configuration=Release /p:Platform=x64 /m;" ^
  "exit $LASTEXITCODE"
if errorlevel 1 (
  set "RC=%ERRORLEVEL%"
  popd
  endlocal
  exit /b %RC%
)

echo Build complete: build\JoyStickTestTool.exe
if not exist "%EXE%" (
  echo Build succeeded but executable not found: "%EXE%"
  popd
  endlocal
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$proc = Get-Process -Name 'JoyStickTestTool' -ErrorAction SilentlyContinue;" ^
  "if ($proc) { $proc | Stop-Process -Force; Start-Sleep -Milliseconds 300 };" ^
  "Start-Process -FilePath '%EXE%'"

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "Start-Sleep -Milliseconds 800;" ^
  "if (Get-Process -Name 'JoyStickTestTool' -ErrorAction SilentlyContinue) { exit 0 } else { exit 1 }"
if errorlevel 1 (
  start "" "%EXE%"
  powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "Start-Sleep -Milliseconds 800;" ^
    "if (Get-Process -Name 'JoyStickTestTool' -ErrorAction SilentlyContinue) { exit 0 } else { exit 1 }"
  if errorlevel 1 (
    echo Launch failed: JoyStickTestTool.exe is not running.
    popd
    endlocal
    exit /b 1
  )
  echo Launched: "%EXE%" (fallback)
) else (
  echo Launched: "%EXE%"
)

popd
endlocal
exit /b 0
