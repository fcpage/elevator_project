[CmdletBinding()]
param(
    [string]$Scenario = "",
    [string]$SchemaProfile = "agreed-v1",
    [switch]$VerboseTrace,
    [switch]$PlantOnly
)

$ErrorActionPreference = "Stop"
$stateRoot = Join-Path ([Environment]::GetFolderPath("LocalApplicationData")) "Project6ElevatorSimulator"
$venvPython = Join-Path $stateRoot "venv\Scripts\python.exe"
$simulatorRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

if (-not (Test-Path -LiteralPath $venvPython)) {
    Write-Error "Simulator setup has not been run. Run .\simulator\setup.ps1 first."
}

$arguments = @("-m", "sim", "run", "--transport", "loopback", "--schema-profile", $SchemaProfile)
$env:PYTHONPATH = $simulatorRoot
if ($Scenario) {
    $scenarioPath = $Scenario
    if (-not [IO.Path]::IsPathRooted($scenarioPath)) {
        $scenarioPath = Join-Path $simulatorRoot $scenarioPath
    }
    $arguments += @("--scenario", $scenarioPath)
}
if ($PlantOnly) {
    $arguments += "--plant-only"
}
if ($VerboseTrace) {
    $arguments += "--verbose"
}

& $venvPython @arguments
