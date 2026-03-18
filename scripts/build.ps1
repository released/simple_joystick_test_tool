param(
    [ValidateSet("Debug", "Release")][string]$Configuration = "Release",
    [ValidateSet("Win32", "x64")][string]$Platform = "x64",
    [switch]$NoRun
)

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere not found. Please run from a Visual Studio Developer PowerShell or install Visual Studio."
    exit 1
}

$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) {
    Write-Error "MSBuild not found."
    exit 1
}

& $msbuild "JoyStickTestTool.sln" /p:Configuration=$Configuration /p:Platform=$Platform /m
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$exe = Join-Path "build" "JoyStickTestTool.exe"
if (Test-Path $exe) {
    Write-Host "Built: $exe"

    if ($NoRun) {
        Write-Host "Skip launch: -NoRun specified."
        exit 0
    }

    $procName = [System.IO.Path]::GetFileNameWithoutExtension($exe)
    $running = Get-Process -Name $procName -ErrorAction SilentlyContinue
    if ($running) {
        $running | Stop-Process -Force
        Start-Sleep -Milliseconds 300
    }

    $exePath = (Resolve-Path $exe).Path
    Start-Process -FilePath $exePath | Out-Null
    Start-Sleep -Milliseconds 800
    $launched = Get-Process -Name $procName -ErrorAction SilentlyContinue
    if (-not $launched) {
        cmd /c start "" "$exePath" | Out-Null
        Start-Sleep -Milliseconds 800
        $launched = Get-Process -Name $procName -ErrorAction SilentlyContinue
    }

    if ($launched) {
        Write-Host "Launched: $exe"
    } else {
        Write-Error "Build succeeded but launch failed: $exe"
        exit 1
    }
} else {
    Write-Warning "Build succeeded but $exe not found."
    exit 1
}
