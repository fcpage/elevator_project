# PCAN Setup

We have encountered a number of errors with the PCAN setup. 

If the R-Pi is updated, this will cause the can0 device to stop functioning.

If you get the following error when running the SC:

```
modprobe: FATAL: Module can not found in directory /lib/modules/5.15.32-v7+
```

Simply restart the R-Pi.

## Debugging Steps

```bash
lsmod | grep -E "peak|can"
```

Make sure that the following modules are available:

- `can_raw`
- `can`
- `peak_usb`
- `can_dev`

If they are not `modprobe` them. If that fails and you have rebooted try checking `/etc/modprobe.d` for blacklists.
