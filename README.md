# GridGuard

GridGuard is a Raspberry Pi Pico 2 W engineering demonstrator for waveform simulation, power metrology, protection logic, relay feedback verification, and event-based appliance classification. The firmware creates its own 230 V / 50 Hz mathematical grid waveform. It does not measure or switch household mains.

The relay output is for an isolated, current-limited **3.3–12 V DC** load only. **Never connect household AC mains to the Pico, ADC dividers, relay, or breadboard.** The GP26 and GP27 inputs are not 5 V tolerant and must remain at or below 3.3 V. Read [Wiring and safety](docs/WIRING_AND_SAFETY.md) before connecting hardware.

## What it does

- Synthesizes 50 Hz voltage and appliance current waveforms in firmware, quantizes them as 12-bit sensor samples, and feeds them through a shared sensor buffer.
- Computes RMS, active/apparent/reactive power, power factor, harmonics, THD, frequency, and energy using C99 code and a Goertzel engine.
- Evaluates IEC 60255 inverse-time and threshold protection.
- Uses a relay state machine that confirms a disconnect only from independent source/load feedback. A low relay command alone is never treated as proof of an open circuit.
- Detects NILM switching events with CUSUM and classifies them with a small pure-C MLP and a Mahalanobis unknown-load gate. NumPy is used only by the offline training/export script.
- Streams little-endian COBS + CRC-16-CCITT frames over native USB CDC-ACM to a FastAPI host service.
- Provides a React/ECharts console with overview, waveform/harmonics, NILM, and fault/scenario views. Replay mode is explicitly labeled.

## Repository layout

| Path | Purpose |
| --- | --- |
| `firmware/lib/gridguard_core/` | Portable C99 plant, metrology, protection, relay FSM, NILM, framing, and CMake/CTest acceptance tests |
| `firmware/src/` | Zephyr threads, ADC/GPIO HAL, USB CDC commands/telemetry, NVS fault storage |
| `firmware/boards/` | Pico 2 W devicetree overlay |
| `scripts/train_nilm.py` | Deterministic synthetic event generation, NumPy training, validation, and C weights export |
| `host/backend/` | FastAPI, COBS/CRC parser, serial auto-detection/reconnect, replay generator, and pytest suite |
| `host/frontend/` | React + TypeScript + Vite + Tailwind + Apache ECharts console |
| `docs/` | EndeavourOS, Windows, and low-voltage wiring guides |

## Hardware and safety boundary

The board target is `rpi_pico2/rp2350a/m33/w`.

| Pin | Signal | Use |
| --- | --- | --- |
| GP14 | Relay driver | Active-high output; external MOSFET/relay driver, boot-safe LOW |
| GP15 | Status LED | External LED and 330 Ω resistor |
| GP16 | Auxiliary contact | Optional active-low contact with internal pull-up |
| GP26 / ADC0 | Source feedback | Divider output, maximum 3.3 V |
| GP27 / ADC1 | Load feedback | Divider output, maximum 3.3 V |

No onboard LED alias is used, and the firmware does not enable RP2350 internal pull-downs. The recommended 33 kΩ / 10 kΩ dividers, bleeder, MOSFET options, and low-side feedback mode are described in [WIRING_AND_SAFETY.md](docs/WIRING_AND_SAFETY.md).

Disconnect states have distinct meanings:

- `PHYSICAL_DISCONNECT_CONFIRMED`: source voltage is present and five consecutive samples show the load deenergized; the optional auxiliary contact check must also pass.
- `DISCONNECT_UNVERIFIED`: the source is absent, so opening the relay cannot be proven.
- `TRIP_FAILURE`: source remains present but the load does not deenergize before the verification timeout.

These are prototype behaviors, not a certification or a substitute for a correctly rated protective device.

## Setup

### EndeavourOS / Arch Linux

The setup script installs the listed system tools, adds the current user to `uucp`, installs a Pico udev rule, and prepares the Zephyr virtual environment:

```sh
./scripts/setup_endeavouros.sh
```

Log out and back in after the group change. Activate the environment and initialize Zephyr 4.3.0:

```sh
source ~/zephyrproject/.venv/bin/activate
pip install west pyelftools
west init ~/zephyrproject --mr v4.3.0
cd ~/zephyrproject
west update
west zephyr-export
pip install -r ~/zephyrproject/zephyr/scripts/requirements.txt
west sdk install -t arm-zephyr-eabi
```

Build from within the west workspace, pointing at this checkout. Change the path if needed:

```sh
GRIDGUARD_REPO="$HOME/Projects/GridGuard"
cd ~/zephyrproject
west build -p -b rpi_pico2/rp2350a/m33/w \
  -s "$GRIDGUARD_REPO/firmware" -d "$GRIDGUARD_REPO/build/zephyr"
```

To flash, hold BOOTSEL while connecting the Pico 2 W, then copy `$GRIDGUARD_REPO/build/zephyr/zephyr.uf2` to its mounted `RP2350` drive. Mount names vary by system.

Full details: [SETUP_ENDEAVOUROS.md](docs/SETUP_ENDEAVOUROS.md).

### Windows 11

Run PowerShell as appropriate for package installation, then execute:

```powershell
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
.\scripts\setup_windows.ps1
```

Install `gperf` and `dtc` through Chocolatey or MSYS2, reopen PowerShell so PATH updates apply, then initialize Zephyr:

```powershell
$zp = Join-Path $env:USERPROFILE 'zephyrproject'
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

In PowerShell opened at the repository root, save its path and build from the west workspace:

```powershell
$gridguardRepo = (Get-Location).Path
Set-Location (Join-Path $env:USERPROFILE 'zephyrproject')
west build -p -b rpi_pico2/rp2350a/m33/w `
  -s (Join-Path $gridguardRepo 'firmware') `
  -d (Join-Path $gridguardRepo 'build\zephyr')
```

Hold BOOTSEL while connecting the board, then copy `build\zephyr\zephyr.uf2` from the repository to the RP2350 USB drive. Pyserial discovers CDC ports; set `GRIDGUARD_PORT=COM5` to select one explicitly.

Full details: [SETUP_WINDOWS.md](docs/SETUP_WINDOWS.md).

## Build and test the portable core

Python 3 with NumPy, CMake, and a C99 compiler are needed for this gate. From the repository root:

```sh
python scripts/train_nilm.py
cmake -S firmware/lib/gridguard_core -B build/core
cmake --build build/core --parallel
ctest --test-dir build/core --output-on-failure
```

The trainer uses seed 42, generates 400 synthetic switching events for each of four appliance classes, splits the generated set into train/held-out subsets, and exports `firmware/lib/gridguard_core/include/nilm_weights.h`. The run performed for this repository reported **100% held-out accuracy** on that synthetic split.

The C acceptance executable covers:

| Check | Coverage |
| --- | --- |
| T1 | Quantized 230 V / 4.35 A resistive waveform: RMS, active power, PF, frequency, and THD |
| T2 | SMPS third/fifth harmonic ratios and analytical current THD |
| T3 | EI IDMT thermal integrator timing at 16 A |
| T4 | Unknown-load Mahalanobis rejection; classifier weights generated and evaluated on synthetic held-out events |
| T5 | Relay verification with healthy load disconnect, welded contact, and lost source |

The separate T3 test calls the IDMT thermal step directly because 16 A also meets the independent 12 A three-cycle overcurrent threshold; the integrated protection path retains that faster overcurrent rule.

## Run the host service and console

### Replay mode, no board required

Terminal 1, from `host/backend`:

```sh
python -m venv .venv
source .venv/bin/activate                 # Windows: .venv\Scripts\Activate.ps1
pip install -r requirements.txt
GRIDGUARD_REPLAY=1 uvicorn app.main:app --reload --port 8000
```

Alternatively run `python -m app.main --replay`. Replay output carries `mode: "REPLAY"`; replay values are generated demo data and are not hardware measurements.

Terminal 2, from `host/frontend`:

```sh
npm install
npm run dev
```

Open the Vite URL shown in the terminal. The development server proxies `/api` and `/ws` to `127.0.0.1:8000`.

### Live serial mode

Leave `GRIDGUARD_REPLAY` unset or set it to `0`. Connect the Pico by USB. Linux auto-detects `/dev/ttyACM*`; Windows uses pyserial COM-port discovery. Set `GRIDGUARD_PORT` to override selection. If no device is connected, the backend reports disconnected status and does not fabricate live telemetry.

### HTTP and WebSocket API

- `WS /ws/live`: telemetry, waveform, NILM, protection, health, status, and fault record events.
- `POST /api/cmd/appliance`: `{ "appliance_id": 0, "target_state": true }`.
- `POST /api/cmd/fault`: `{ "fault_scenario": 1, "magnitude": 170 }`.
- `POST /api/cmd/reset-relay`: `{ "force_clear_latch": false }`.
- `POST /api/cmd/feedback-mode`: `{ "feedback_mode": 2 }`.
- `GET /api/faults` and `GET /api/faults/export.csv`: fault history.

The binary protocol uses a 6-byte little-endian header, typed payload, CRC-16-CCITT (poly `0x1021`, init `0xFFFF`), COBS encoding, and a zero delimiter. Shared C packed structs and Python decoders are in `gg_protocol.h` and `host/backend/app/protocol.py`.

## Test results and data limits

The following automated gates were run for this implementation:

| Gate | Result |
| --- | --- |
| NILM synthetic training/held-out split | Passed, 100% reported accuracy |
| Core CMake/CTest | Passed, T1–T5 |
| Backend pytest | Passed, 4 tests including COBS/CRC corruption and replay |
| Frontend production build | Passed, TypeScript and Vite |
| Zephyr 4.3.0 Pico 2 W build | Passed, generated `zephyr.uf2` |

**No external or real household appliance dataset was used.** NILM accuracy is measured on generated samples from the specified plant model; it does not establish accuracy on real appliances, ADC hardware, or unseen homes. The test data varies modeled voltage, appliance tolerance, backgrounds, and noise, but remains synthetic and correlated with the same equations used by the simulator. T7 (physical relay bench and reboot persistence) was not run; it requires the board and low-voltage hardware.

The Zephyr build was performed with Zephyr v4.3.0 and the installed GNU Arm Embedded compiler. Zephyr emitted deprecation notices for the legacy USB device stack configuration in the specification, test VID/PID warnings, and linker RWX-segment warnings. Vite also reports a large JavaScript chunk; the build still succeeds.
