# Windows 11 setup

## Prerequisites

Use an elevated PowerShell for package installation:

```powershell
winget install Kitware.CMake Ninja-build.Ninja Git.Git Python.Python.3.12 OpenJS.NodeJS.LTS 7zip.7zip
```

Install `gperf` and `dtc` through Chocolatey or MSYS2, and install the Zephyr SDK below. Reopen PowerShell so PATH changes take effect. `scripts/setup_windows.ps1` installs the winget packages and creates the Python environment.

## Zephyr workspace

```powershell
$zp = Join-Path $env:USERPROFILE 'zephyrproject'
New-Item -ItemType Directory -Force $zp | Out-Null
python -m venv (Join-Path $zp '.venv')
& (Join-Path $zp '.venv\Scripts\Activate.ps1')
pip install west pyelftools
west init $zp --mr v4.3.0
Set-Location $zp
west update
west zephyr-export
pip install -r (Join-Path $zp 'zephyr\scripts\requirements.txt')
west sdk install -t arm-zephyr-eabi
```

## Build and flash

Open PowerShell in the GridGuard checkout, save its path, then run west from its workspace:

```powershell
$gridguardRepo = (Get-Location).Path
Set-Location (Join-Path $env:USERPROFILE 'zephyrproject')
west build -p -b rpi_pico2/rp2350a/m33/w -s (Join-Path $gridguardRepo 'firmware') -d (Join-Path $gridguardRepo 'build\zephyr')
```

Hold BOOTSEL while connecting the Pico 2 W, then copy `build\zephyr\zephyr.uf2` in the checkout to the mounted RP2350 USB drive. Windows CDC ports are detected through pyserial; set `$env:GRIDGUARD_PORT='COM5'` to select a port, or use replay mode with `$env:GRIDGUARD_REPLAY='1'`.

## Host stack

```powershell
Set-Location host\backend
python -m venv .venv
& .venv\Scripts\Activate.ps1
pip install -r requirements.txt
$env:GRIDGUARD_REPLAY = '1' # omit or set to 0 for hardware
uvicorn app.main:app --reload --port 8000
```

In another terminal: `Set-Location host\frontend; npm install; npm run dev`.
