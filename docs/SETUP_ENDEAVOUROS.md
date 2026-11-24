# EndeavourOS setup

## System packages and serial access

```sh
sudo pacman -S --needed git cmake ninja gperf ccache dfu-util dtc wget python python-pip python-virtualenv nodejs npm base-devel
sudo usermod -aG uucp "$USER"
```

Log out and back in after changing groups. Install the optional Pico USB rule from `scripts/setup_endeavouros.sh` (it writes `/etc/udev/rules.d/60-gridguard-pico.rules` and reloads udev). Use `lsusb` and `/dev/ttyACM*` to identify the CDC device.

## Zephyr workspace

```sh
mkdir -p ~/zephyrproject
python -m venv ~/zephyrproject/.venv
source ~/zephyrproject/.venv/bin/activate
pip install west pyelftools
west init ~/zephyrproject --mr v4.3.0
cd ~/zephyrproject
west update
west zephyr-export
pip install -r ~/zephyrproject/zephyr/scripts/requirements.txt
west sdk install -t arm-zephyr-eabi
```

Run `west` from the workspace environment. Since this checkout is outside the west workspace, invoke the build from the workspace and give west the application and build paths:

```sh
GRIDGUARD_REPO="$HOME/Projects/GridGuard" # change this to the checkout path
cd ~/zephyrproject
west build -p -b rpi_pico2/rp2350a/m33/w -s "$GRIDGUARD_REPO/firmware" -d "$GRIDGUARD_REPO/build/zephyr"
```

Hold BOOTSEL while connecting the Pico 2 W, then copy `$GRIDGUARD_REPO/build/zephyr/zephyr.uf2` to `/run/media/$USER/RP2350/` (the mount name can differ).

## Host stack

```sh
cd host/backend
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
uvicorn app.main:app --reload --port 8000
```

In a second terminal: `cd host/frontend && npm install && npm run dev`. Set `GRIDGUARD_REPLAY=1` in the backend shell to use labeled replay data without hardware.

For a one-time dependency installation, `scripts/setup_endeavouros.sh` installs the documented packages and prints the Zephyr workspace commands. The system package and udev operations require sudo.
