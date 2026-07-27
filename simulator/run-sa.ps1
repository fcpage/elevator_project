[CmdletBinding()]
param(
    [switch]$VerboseBuild
)

$ErrorActionPreference = "Stop"
$simulatorRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $simulatorRoot
$saRoot = Join-Path $projectRoot "elevator_project\SupervisoryController"
$stateRoot = Join-Path ([Environment]::GetFolderPath("LocalApplicationData")) "Project6ElevatorSimulator"
$sessionPath = Join-Path $stateRoot "runtime\active-session.json"

if (-not (Test-Path -LiteralPath $sessionPath)) {
    Write-Error "No active simulator session. Start .\simulator\run.ps1 in the first terminal."
}

$session = Get-Content -Raw -LiteralPath $sessionPath | ConvertFrom-Json
if (-not (Get-Process -Id $session.simulator_pid -ErrorAction SilentlyContinue)) {
    Write-Error "The active simulator session is stale. Start .\simulator\run.ps1 again."
}
if ($session.transport -ne "loopback") {
    Write-Error "The active session is not using the Windows loopback transport."
}
$plantOnly = $session.PSObject.Properties.Name -contains "mode" -and $session.mode -eq "plant_only"

$env:ELEVATOR_SIM_CAN_HOST = $session.can_host
$env:ELEVATOR_SIM_CAN_PORT = [string]$session.can_port
if (-not $plantOnly) {
    $env:ELEVATOR_SIM_DIAGNOSTICS_HOST = $session.diagnostics_host
    $env:ELEVATOR_SIM_DIAGNOSTICS_PORT = [string]$session.diagnostics_port
    $env:ELEVATOR_DB_URL = $session.database_url
    $env:ELEVATOR_DB_USER = $session.database_user
    $env:ELEVATOR_DB_PASSWORD = $session.database_password
    $env:ELEVATOR_DB_SCHEMA = $session.database_schema
}

$connectorRoots = @(
    (Get-ChildItem -Path "$env:ProgramFiles\MySQL" -Directory -Filter "MySQL Connector C++ *" -ErrorAction SilentlyContinue),
    (Get-ChildItem -Path "$env:ProgramFiles\MySQL" -Directory -Filter "Connector C++ *" -ErrorAction SilentlyContinue)
) | Where-Object { $_ } | Sort-Object Name -Descending
$connectorRoot = $connectorRoots | Select-Object -First 1
if ($connectorRoot) {
    $connectorRuntime = Join-Path $connectorRoot.FullName "lib64"
    if (Test-Path -LiteralPath $connectorRuntime) {
        $env:PATH = "$connectorRuntime;$env:PATH"
    }
}

$buildRoot = Join-Path $stateRoot "build\windows"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $session.sa_log_path) | Out-Null

function Invoke-NativeCapture {
    param([scriptblock]$Command)

    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $lines = @(
            & $Command 2>&1 |
                ForEach-Object {
                    if ($_ -is [System.Management.Automation.ErrorRecord]) {
                        $_.Exception.Message
                    }
                    else {
                        $_.ToString()
                    }
                }
        )
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }
    return [pscustomobject]@{ ExitCode = $exitCode; Lines = $lines }
}

function Write-BuildFailure {
    param(
        [string]$Phase,
        [object]$Result
    )

    Write-Host "FAILED" -ForegroundColor Red
    $important = @(
        $Result.Lines |
            Where-Object { $_ -match "(?i)\b(error|fatal|failed)\b" } |
            Select-Object -Last 20
    )
    if ($important.Count -eq 0) {
        $important = @($Result.Lines | Select-Object -Last 20)
    }
    $important | ForEach-Object { Write-Host "  $_" }
    Write-Host "$Phase failed. Full details: $($session.sa_log_path)" -ForegroundColor Red
}

"=== SA build ===" | Set-Content -LiteralPath $session.sa_log_path
Write-Host "Configuring CMake... " -NoNewline
$cmakeArguments = @("-S", $saRoot, "-B", $buildRoot, "-DSUPERVISORY_BUILD_SIMULATOR=ON")
if (-not $plantOnly) {
    $cmakeArguments += @("-DSUPERVISORY_ENABLE_SIM_DIAGNOSTICS=ON", "-DSUPERVISORY_ENABLE_SIM_TESTPOINTS=ON")
}
$configure = Invoke-NativeCapture {
    cmake @cmakeArguments
}
"=== CMake configure ===" | Add-Content -LiteralPath $session.sa_log_path
$configure.Lines | Add-Content -LiteralPath $session.sa_log_path
if ($VerboseBuild) {
    $configure.Lines | ForEach-Object { Write-Host $_ }
}
if ($configure.ExitCode -ne 0) {
    Write-BuildFailure "CMake configuration" $configure
    return
}
Write-Host "OK" -ForegroundColor Green
if ($plantOnly) {
    Write-Host "Plant-only session: database and diagnostics are intentionally disabled." -ForegroundColor Yellow
}

Write-Host "Building SA... " -NoNewline
$build = Invoke-NativeCapture {
    cmake --build $buildRoot --config Release --target supervisory_controller
}
"=== CMake build: supervisory_controller ===" |
    Add-Content -LiteralPath $session.sa_log_path
$build.Lines | Add-Content -LiteralPath $session.sa_log_path
if ($VerboseBuild) {
    $build.Lines | ForEach-Object { Write-Host $_ }
}
if ($build.ExitCode -ne 0) {
    Write-BuildFailure "SA build" $build
    return
}
Write-Host "OK" -ForegroundColor Green
Write-Host "Build log: $($session.sa_log_path)"

$candidatePaths = @(
    (Join-Path $buildRoot "Release\supervisory_controller.exe"),
    (Join-Path $buildRoot "Debug\supervisory_controller.exe"),
    (Join-Path $buildRoot "supervisory_controller.exe")
)
$executable = $candidatePaths | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $executable) {
    Write-Error "CMake completed without producing supervisory_controller.exe."
}

"=== SA runtime ===" | Tee-Object -FilePath $session.sa_log_path -Append
$previousErrorActionPreference = $ErrorActionPreference
$adapterExitCode = 0
try {
    # Windows PowerShell wraps native stderr as ErrorRecord objects. With the
    # script-wide Stop preference, an SA diagnostic would otherwise terminate
    # the launcher even while the native process is still healthy and running.
    $ErrorActionPreference = "Continue"
    & $executable 2>&1 |
        ForEach-Object {
            if ($_ -is [System.Management.Automation.ErrorRecord]) {
                $_.Exception.Message
            }
            else {
                $_.ToString()
            }
        } |
        Tee-Object -FilePath $session.sa_log_path -Append
    $adapterExitCode = $LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousErrorActionPreference
}
if ($adapterExitCode -ne 0) {
    Write-Host "SA exited with code $adapterExitCode. Full details: $($session.sa_log_path)" `
        -ForegroundColor Red
    return
}
