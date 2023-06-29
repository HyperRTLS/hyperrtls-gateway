Running GW (`<board>` is `qemu_x86` or `qemu_cortex_m3`):
```sh
# Build the app
west build -b <board> -p -- -DCONF_FILE=prj_gw.conf

# Prepare Ethernet proxy
sudo <net_tools_dir>/net-setup.sh # Consumes shell
# Find appropriate interface name using ifconfig
sudo ./setup_networking.sh <interface>

# Prepare Bluetooth proxy

# Find available Bluetooth controllers
hciconfig

# Run the proxy
sudo hciconfig hci<iface_idx> down

# Needs to be self-compiled if btproxy isn't shipped with BlueZ by default
sudo <bluez_dir>/tools/btproxy -u -i <iface_idx> # Consumes shell
# Run the application fairly quickly or the OS will take ownership of the interface; alternatively find out how to prevent the OS from doing that

# Run
west build -t run

# Debug
west build -t debugserver
# In another shell:
gdb-multiarch build/zephyr/zephyr.elf
# Inside gdb shell call target remote localhost:1234 and run
```

Running tag or anchor (`<target>` is `tag` or `anchor`):
```sh
# Build the app
west build -b decawave_dwm1001_dev -p -- -DCONF_FILE=prj_<target>.conf

# Flash
west flash

# Debug
west attach
# Inside gdb shell call monitor reset to reboot the board
```
