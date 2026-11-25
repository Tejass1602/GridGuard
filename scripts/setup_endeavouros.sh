#!/usr/bin/env bash
set -euo pipefail

sudo pacman -S --needed git cmake ninja gperf ccache dfu-util dtc wget python python-pip python-virtualenv nodejs npm base-devel
sudo usermod -aG uucp "$USER"
printf '%s\n' \
  'SUBSYSTEM=="tty", ATTRS{idVendor}=="2e8a", MODE="0660", GROUP="uucp", TAG+="uaccess"' \
  'SUBSYSTEM=="tty", ATTRS{idVendor}=="2fe3", MODE="0660", GROUP="uucp", TAG+="uaccess"' \
  | sudo tee /etc/udev/rules.d/60-gridguard-pico.rules >/dev/null
sudo udevadm control --reload-rules
sudo udevadm trigger
mkdir -p "$HOME/zephyrproject"
python -m venv "$HOME/zephyrproject/.venv"
printf '%s\n' 'System packages, uucp membership and Pico udev access are configured.' \
  'Log out and back in, then run:' \
  'source ~/zephyrproject/.venv/bin/activate' \
  'pip install west pyelftools' \
  'west init ~/zephyrproject --mr v4.3.0' \
  'cd ~/zephyrproject && west update && west zephyr-export' \
  'pip install -r ~/zephyrproject/zephyr/scripts/requirements.txt' \
  'west sdk install -t arm-zephyr-eabi'
