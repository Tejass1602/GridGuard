$ErrorActionPreference = 'Stop'
$packages = @('Kitware.CMake', 'Ninja-build.Ninja', 'Git.Git', 'Python.Python.3.12', 'OpenJS.NodeJS.LTS', '7zip.7zip')
foreach ($package in $packages) {
    winget install --id $package --exact --accept-package-agreements --accept-source-agreements
    if ($LASTEXITCODE -ne 0) { throw "winget failed for $package" }
}
$workspace = Join-Path $env:USERPROFILE 'zephyrproject'
New-Item -ItemType Directory -Force $workspace | Out-Null
python -m venv (Join-Path $workspace '.venv')
Write-Host 'Activate the environment, install west, then initialize Zephyr:'
Write-Host "  & '$workspace\.venv\Scripts\Activate.ps1'"
Write-Host '  pip install west pyelftools'
Write-Host "  west init '$workspace' --mr v4.3.0"
Write-Host "Install gperf and dtc through Chocolatey or MSYS2, then follow docs/SETUP_WINDOWS.md."
