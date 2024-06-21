### NOTE!
Pico based HID host & hid device proxy, used in my mini keyboard adapter.
I use this adapter to 
* speed up password input(lock screen, 1passwd, etc.)
* lock my mac screen just one click
* other frequently used shortcuts

although i clone the code from glenmurphy/keyboard
I acutually combined the follow two projects into one piece.

* https://github.com/glenmurphy/keyboard
* https://github.com/sekigon-gonnoc/Pico-PIO-USB


### original readme
Keyboard HID implementation for the Raspberry Pico / RP2040, based on the TinyUSB hid_composite example. Runs at 1000hz.

### Build instructions

Follow the Pico build instructions (I could never get them to work on Windows, and had more luck on macOS), then run b.sh in the build directory. Mount the Pico as a drive by holding the button when plugging it in, then run d.sh from the same dir (or copy the keyboard.uf2 file to the mounted drive).
