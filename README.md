Wiegand reader using libgpiod (Raspberry Pi 5)

This small C++ project reads a Wiegand stream (common RFID/door readers) using libgpiod.

Requirements
- Raspberry Pi 5 (libgpiod supports the gpiochip interface on Pi 5)
- libgpiod development files (on Raspberry Pi OS: sudo apt install libgpiod-dev libgpiod2)
- CMake and a C++17 compiler

Build

mkdir build
cd build
cmake ..
make

Run

# the program reads BCM pin numbers for D0 and D1. Defaults: 17 and 27
sudo ./wiegand_reader 17 27

Wiring
- Connect Wiegand Data0 to the chosen BCM GPIO (e.g. 17)
- Connect Wiegand Data1 to the chosen BCM GPIO (e.g. 27)
- Connect GND

Notes
- libgpiod accesses GPIO via /dev/gpiochipX; on Raspberry Pi 5 the kernel/gpiolib supports this interface.
- You may need to run as root or grant access to /dev/gpiochip0.
- The program validates parity bits for both 26-bit and 34-bit Wiegand formats
- For 26-bit format: even parity (bit 0) over bits 1-12, odd parity (bit 25) over bits 13-24
- For 34-bit format: even parity (bit 0) over bits 1-16, odd parity (bit 33) over bits 17-32
- Press Ctrl+C to cleanly exit the program
