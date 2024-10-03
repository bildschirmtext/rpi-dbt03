# rpi-dbt03
Hardware, Software and Firmware for a Raspberry Pi to DBT-03 adapter


There is now a possibility to directly patch a Bookworm Raspbian image.

Run `./patch_image.sh <imagename>` as root. It only works on uncompressed images. You can then `dd` that image onto your SD-card or use the Raspberry Pi Imager. The later also allows you to set settings.
The image will reboot multiple times.

The boot partition can also hold a `btx_config.txt` file where you can configure the host to connect to.

```
BTX_HOST=btx.clarke-3.de
BTX_PORT=20000
```

## Old way of installing it

On the Raspberry PI check out this repository. 

Install the packages in install-packages.sh (run it as root). 

Enable spi with raspi-config. 

Change the baudrate of the programmer "linuxspi" to a smaller value 40000 is OK. This setting is found in /etc/avrdude.conf



This currently is still incomplete.
