```sh
west build -b qemu_x86 -p -- -DCONF_FILE=prj_gw.conf
sudo tools/btproxy -u -i 0
sudo hciconfig hci0 down
west build -t run
```
