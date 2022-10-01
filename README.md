```sh
west build -b qemu_x86 -p -- -DCONF_FILE=prj_gw.conf
sudo hciconfig hci1 down
# Needs to be self-compiled if btproxy isn't shipped with BlueZ by default
sudo ~/work/bluez/tools/btproxy -u -i 1
west build -t run
# TODO: Figure out how to remotely debug it, as west flash isn't working
```
