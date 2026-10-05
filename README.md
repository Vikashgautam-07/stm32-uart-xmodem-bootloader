# STM32F103 USART XMODEM Bootloader

Bare-metal firmware for an STM32F103C8/CB-class board. The bootloader accepts
an application image over USART1 using XMODEM, programs the application Flash
region, checks the programmed image, and jumps to it. On reset, it waits up to
5 seconds for `U`/`u` to enter update mode; otherwise, it starts a valid
application. The included sample application prints a startup message and
blinks PC13. No HAL or vendor library is used.

## Hardware

- STM32F103 board, such as a Black Pill
- ST-Link V2 or compatible SWD debugger/programmer
- CP2102 USB-to-UART adapter with 3.3V logic
- USB data cable

The firmware uses the default 8 MHz HSI clock after reset. The blink timing is a software busy-wait and is not a precise millisecond timer.

## ST-Link SWD Connections

| ST-Link | STM32F103 | Purpose |
| --- | --- | --- |
| 3.3V | 3.3V / VDD | Target power or voltage reference |
| GND | GND | Common ground |
| SWDIO | PA13/DIO | SWD data |
| SWCLK | PA14/CLK | SWD clock |

Confirm that the board and ST-Link share ground.

## CP2102 USART1 Connections

| STM32F103 | CP2102 | Purpose |
| --- | --- | --- |
| PA9 | RXD | USART1 TX |
| PA10 | TXD | USART1 RX |
| GND | GND | Common ground |

The TX and RX connections are crossed. Leave CP2102 VCC disconnected when the board is powered separately. Use 3.3 V logic only.

## USART1 Configuration

The firmware uses USART1 with:

- Baud rate: `115200`
- Data bits: `8`
- Parity: none
- Stop bits: `1`
- Flow control: none
- Clock: default 8 MHz HSI

The USART implementation is separated into:

- `src/uart.h`: public function declarations
- `src/uart.c`: USART1 register definitions and implementation
- `src/main.c`: boot policy, Flash programming, image validation, and app jump
- `src/xmodem.c`: XMODEM receiver
- `src/application_main.c`: example application

## Transfer an Application

### Build and flash the bootloader

From this directory, build and flash the bootloader with an ST-Link:

```bash
make clean
make
make flash
```

The application linker script places the example application at `0x08002000`;
the bootloader occupies the first 8 KiB of Flash.

### Send the application from Minicom

1. Build the sample application:

2. Open the board's USART1 serial port at `115200` baud, `8-N-1`. Disable both
   hardware and software flow control.
3. Reset the board. During the 5-second boot window, send uppercase or
   lowercase `U` to enter update mode. If there is no valid app installed, the
   bootloader enters update mode automatically.
4. When the bootloader prints `Send XMODEM CRC/checksum transfer` and starts
   displaying `C`, start Minicom's file-send operation (`Ctrl-A`, then `S`),
   choose **Xmodem**, and select `build/application.bin`.
5. Wait for Minicom to report transfer completion, then return to the serial
   console. The bootloader should print
   `Update verified; starting application`, followed by
   `Application started at 0x08002000`.

The receiver supports 128-byte XMODEM (`SOH`) and 1024-byte XMODEM-1K (`STX`)
blocks, with CRC or checksum. Do not select YMODEM. `C` is the receiver's
request for CRC-mode XMODEM; it repeats while waiting for the first packet.
It should stop once a valid packet is received. A reported byte count can
exceed the binary's actual size because XMODEM pads the final fixed-size block.

The update writes directly into the active application region. Do not remove
power or reset the board during transfer; an interrupted update may require
reflashing a valid application.

## Build Tools

Install or provide these commands in `PATH`:

- `arm-none-eabi-gcc`
- `arm-none-eabi-objcopy`
- `arm-none-eabi-size`
- `make`
- `openocd`
- `gdb-multiarch` for debugging

## Build

From this directory:

```bash
make clean
make
```

Generated files:

- `build/firmware.elf`: ELF image used by OpenOCD and GDB
- `build/firmware.bin`: raw binary image
- `build/firmware.map`: linker map
- `build/application.elf`: sample application ELF linked at `0x08002000`
- `build/application.bin`: sample application binary to transfer over XMODEM

## Flash the Board

Connect the ST-Link and board, then run:

```bash
make flash
```
Build the application:

```bash
make application
```

The `flash` target programs `build/firmware.elf`, verifies it, resets the target, and exits OpenOCD.

A successful flash contains messages similar to:

```text
** Programming Finished **
** Verified OK **
** Resetting Target **
```

## Normal Boot

After reset, the bootloader prints `Bootloader Ready` and listens for `U`/`u`
for up to 5 seconds. If no update is requested and the application vector
table is valid, the bootloader starts the application. The sample application
prints `Application started at 0x08002000` and blinks PC13. If no valid
application exists, the bootloader stays in update mode.

On common Black Pill boards, the PC13 LED is active-low. The example app
toggles PC13 with a software delay; timing is not precise.

## Test USART1

After flashing, disconnect the ST-Link and power the board with its USB cable or another safe single power source. Keep CP2102 connected to PA9, PA10, and GND, then find the serial device:

```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

Open it with Minicom:

```bash
minicom -D /dev/ttyUSB0 -b 115200
```

Before connecting, configure Minicom's serial settings to 115200 baud, 8-N-1,
with hardware and software flow control disabled. Press reset once. The
terminal should show the bootloader startup message. To observe the
application startup message, wait for the 5-second window to expire without
sending `U`.

```text
Bootloader Ready: send U within 5 seconds to update
Application started at 0x08002000
```

To initiate an update, send `U` during the boot window and use the Minicom
XMODEM procedure above. Exit Minicom with `Ctrl-A`, then `X`.

On WSL, the CP2102 may need to be attached through `usbipd`. If `/dev/ttyUSB0` is missing, reconnect or reattach the CP2102 and check the device path again. 
<!-- If permission is denied, add the user to `dialout` and restart WSL:

```bash
sudo usermod -aG dialout "$USER"
``` -->

<!-- Do not use `0x271` for the baud register unless the system clock has explicitly been configured to 72 MHz.  -->
With the default 8 MHz HSI clock, the USART baud register value is `0x45`.

## Debug with GDB

Start OpenOCD in one terminal:

```bash
make openocd
```

Leave it running. In a second terminal, start GDB:

```bash
make debug
```

<!-- Useful GDB commands:

```gdb
break main
continue
step
info registers
x/1wx 0x4001100c
continue
quit
``` -->

The GDB connection path is:

```text
GDB -> OpenOCD -> ST-Link -> STM32F103
```

## WSL USB Setup

When using WSL, Windows may detect the ST-Link while Linux does not. In Administrator PowerShell:

```powershell
usbipd list
usbipd attach --wsl --busid 1-1
```

Replace `1-1` with the ST-Link bus ID shown by `usbipd list`. Then, in WSL:

```bash
lsusb
make flash
```

The ST-Link should appear as USB ID `0483:3748` or a similar ST-Link device.

## Clean Build

```bash
make clean
```

This removes the generated `build/` directory.
