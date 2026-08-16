[CmdletBinding()]
param(
    [string]$SchemaProfile = "agreed-v1",
    [switch]$RecreateSchema
)

$ErrorActionPreference = "Stop"
$simulatorRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$stateRoot = Join-Path ([Environment]::GetFolderPath("LocalApplicationData")) "Project6ElevatorSimulator"
$venvRoot = Join-Path $stateRoot "venv"
$venvPython = Join-Path $venvRoot "Scripts\python.exe"

New-Item -ItemType Directory -Force -Path $stateRoot | Out-Null

if (-not (Test-Path -LiteralPath $venvPython)) {
    $pyLauncher = Get-Command py -ErrorAction SilentlyContinue
    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($pyLauncher) {
        & $pyLauncher.Source -3 -m venv $venvRoot
    }
    elseif ($python) {
        & $python.Source -m venv $venvRoot
    }
    else {
        Write-Error "Python 3.11+ is required. Install it with: winget install Python.Python.3.12"
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to create the simulator Python environment."
    }
}

& $venvPython -c "import sys; raise SystemExit(0 if sys.version_info >= (3, 11) else 1)"
if ($LASTEXITCODE -ne 0) {
    throw "Python 3.11+ is required. Remove '$venvRoot' after installing a supported Python, then rerun setup."
}
$env:PYTHONPATH = $simulatorRoot
& $venvPython -m pip install --disable-pip-version-check "mysql-connector-python==9.0.0"
if ($LASTEXITCODE -ne 0) { throw "Failed to install simulator Python dependencies." }
$setupArguments = @("-m", "sim", "setup", "--schema-profile", $SchemaProfile)
if ($RecreateSchema) {
    $setupArguments += "--recreate-schema"
}
& $venvPython @setupArguments
if ($LASTEXITCODE -ne 0) { throw "Simulator setup did not complete." }

Write-Host ""
Write-Host "Setup complete."
Write-Host "Start the simulator with: .\simulator\run.ps1"
