# STM32F103 Bare-Metal Bring-Up

Minimal bare-metal firmware for an STM32F103C8/CB-class board. The current program configures PC13 as a 2 MHz push-pull output and blinks the onboard LED using direct register access. No HAL or vendor library is used.

## Hardware

- STM32F103 board, such as a Blue Pill
- ST-Link V2 or compatible SWD debugger/programmer
- USB data cable

The firmware uses the default 8 MHz HSI clock after reset. The blink timing is a software busy-wait and is not a precise millisecond timer.

## ST-Link SWD Connections

| ST-Link | STM32F103 | Purpose |
| --- | --- | --- |
| 3.3V | 3.3V / VDD | Target power or voltage reference |
| GND | GND / VSS | Common ground |
| SWDIO | PA13/DIO | SWD data |
| SWCLK | PA14/CLK | SWD clock |
| NRST | NRST | Target reset; recommended |

Do not connect the ST-Link 5V output to the STM32 3.3V supply. Confirm that the board and ST-Link share ground.

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

## Flash the Board

Connect the ST-Link and board, then run:

```bash
make clean
make
make flash
```

The `flash` target programs `build/firmware.elf`, verifies it, resets the target, and exits OpenOCD.

A successful flash contains messages similar to:

```text
** Programming Finished **
** Verified OK **
** Resetting Target **
```

## Run the Program

After flashing, the MCU resets and starts the firmware automatically. The program:

1. Runs `Reset_Handler` from the vector table.
2. Initializes `.data` and `.bss`.
3. Calls `main()`.
4. Enables the GPIOC clock.
5. Configures PC13 as a push-pull output.
6. Sets and resets PC13 repeatedly.

On common Blue Pill boards, the PC13 LED is active-low: PC13 low turns the LED on and PC13 high turns it off. If there is no visible blink, the board may use a different LED connection.

The blink speed can be changed in `src/main.c` by changing:

```c
#define DELAY 500000
```

A larger value makes the blink slower; a smaller value makes it faster.

## Debug with GDB

Start OpenOCD in one terminal:

```bash
make openocd
```

Leave it running. In a second terminal, start GDB:

```bash
make debug
```

Useful GDB commands:

```gdb
break main
continue
step
info registers
x/1wx 0x4001100c
continue
quit
```

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
