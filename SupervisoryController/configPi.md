456  lsusb | grep -i "peak"

457  dmesg | grep -E -i "usb|peak|can" | tail -n 20

458  sudo dmesg | grep -E -i "usb|peak|can" | tail -n 20

459  usb-devices | grep -A 5 -i "peak"

460  sudo rmmod pcan

461  sudo modprobe pcan type=netdev

462  ip link show

463  sudo rmmod pcan

464  echo "blacklist pcan" | sudo tee /etc/modprobe.d/blacklist-pcan.conf

465  sudo modprobe can

466  sudo modprobe can_raw

467  sudo modprobe peak_usb

468  ip link show

469  ./scripts/build_rpi.sh --hardware can0 125000

470  ./scripts/initialize_can.sh can0 125000

471  ./scripts/build_rpi.sh --hardware can0 125000

472  run sudo ./build-rpi/supervisory_controller can0


chmod +x fix_peak_socketcan.sh
./fix_peak_socketcan.sh

```bash
#!/usr/bin/env bash
set -euo pipefail

echo "=== PEAK / SocketCAN cleanup for Raspberry Pi ==="

echo
echo "[1] Current CAN / PEAK modules:"
lsmod | grep -E '(^pcan|peak_usb|can|can_raw|can_dev)' || true

echo
echo "[2] Bring down CAN interfaces if they exist..."
for iface in $(ip -br link | awk '/^can[0-9]+/ {print $1}'); do
    echo "Bringing down $iface"
    sudo ip link set "$iface" down || true
done

echo
echo "[3] Unload proprietary PEAK driver if loaded..."
sudo modprobe -r pcan 2>/dev/null || true

echo
echo "[4] Unload/reload SocketCAN modules cleanly..."
sudo modprobe -r peak_usb 2>/dev/null || true
sudo modprobe -r can_raw 2>/dev/null || true
sudo modprobe -r can 2>/dev/null || true

sudo modprobe can
sudo modprobe can_raw
sudo modprobe peak_usb

echo
echo "[5] Blacklist proprietary PEAK driver..."
sudo tee /etc/modprobe.d/blacklist-peak-pcan.conf >/dev/null <<'EOF'
# Prevent proprietary PEAK PCAN driver from claiming the adapter.
# We want Linux SocketCAN instead, using the kernel peak_usb driver.
blacklist pcan
install pcan /bin/false
EOF

echo
echo "[6] Update initramfs if available..."
if command -v update-initramfs >/dev/null 2>&1; then
    sudo update-initramfs -u
else
    echo "update-initramfs not found; skipping."
fi

echo
echo "[7] Reload udev rules..."
sudo udevadm control --reload-rules
sudo udevadm trigger

echo
echo "[8] Current CAN interfaces:"
ip link show type can || true

echo
echo "Done. Unplug/replug the PCAN adapter, or reboot:"
echo "  sudo reboot"
```

sudo modprobe can_raw
sudo modprobe can_usb
ip link show

sudo ip link set can0 down
sudo ip link set can0 type can bitrate 500000
sudo ip link set can0 up