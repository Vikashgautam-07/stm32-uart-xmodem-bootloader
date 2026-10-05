# STM32F103 Bootloader Project Tracker

## Project Goal

Build a bare-metal STM32F103 bootloader that receives application firmware over USART1 using the XMODEM protocol, writes it to Flash, verifies it, and starts the application.

## Overall Progress

- [x] Phase 1: Bare-metal hardware bring-up
- [x] Phase 2: USART1 communication
- [ ] Phase 3: XMODEM bootloader and application update

---

## Phase 1: Bare-Metal Hardware Bring-Up

Status: COMPLETE

Completed:

- [x] Created an STM32F103 bare-metal Makefile project.
- [x] Added the STM32F103 linker script.
- [x] Added the Cortex-M3 startup file and vector table.
- [x] Initialized `.data` and `.bss` in `Reset_Handler`.
- [x] Enabled the GPIOC peripheral clock.
- [x] Configured PC13 as a 2 MHz push-pull output.
- [x] Implemented PC13 LED blinking with a software delay.
- [x] Used GPIO BSRR for atomic set/reset operations.
- [x] Built the ELF and BIN firmware images successfully.
- [x] Flashed the board through ST-Link and OpenOCD.
- [x] Verified the programmed Flash contents successfully.
- [x] Documented SWD wiring, build, flash, and debug commands in `README.md`.

Acceptance result:

```text
Programming Finished
Verified OK
Resetting Target
```

---

## Phase 2: USART1 Communication

Status: COMPLETE

Objective: Prove reliable serial communication before implementing XMODEM.

Hardware connections:

| STM32F103 | CP2102 | Purpose |
| --- | --- | --- |
| PA9 | RXD | USART1 transmit from STM32 |
| PA10 | TXD | USART1 receive by STM32 |
| GND | GND | Common ground |

Tasks:

- [x] Add USART1 register definitions in `src/uart.c`.
- [x] Enable the GPIOA and USART1 clocks.
- [x] Configure PA9 as USART1 alternate-function push-pull output.
- [x] Configure PA10 as USART1 floating input.
- [x] Configure USART1 for 115200 8-N-1 using the default 8 MHz HSI clock.
- [x] Implement `uart1_init()`.
- [x] Implement `uart1_putc()`.
- [x] Implement `uart1_getc()`.
- [x] Implement `uart1_puts()`.
- [x] Send a startup message: `Bootloader Ready`.
- [x] Implement a UART echo test.
- [x] Test with a serial terminal at 115200 baud.
- [x] Document CP2102 wiring and terminal settings in `README.md`.

### Phase 2 Step-by-Step Procedure

1. Confirm the MCU is an STM32F103 and identify the USART pins:
	- PA9 is USART1 TX.
	- PA10 is USART1 RX.
2. Wire the CP2102 with crossed data lines:
	- CP2102 RXD -> STM32 PA9.
	- CP2102 TXD -> STM32 PA10.
	- CP2102 GND -> STM32 GND.
	- Leave CP2102 VCC disconnected when the board has another power source.
3. Power the board from one source only. USB power or regulated 3.3 V may be used.
4. Confirm `src/uart.h` declarations match the functions implemented in `src/uart.c`.
5. Confirm `main.c` calls `uart1_init()`, prints the startup message, receives with `uart1_getc()`, and echoes with `uart1_putc()`.
6. Confirm the Makefile compiles and links `src/uart.c` and `build/uart.o`.
7. Build the firmware:

	```bash
	make clean
	make
	```

8. Connect the ST-Link for programming only. Use SWDIO, SWCLK, and GND; do not power the board from two sources.
9. Flash and verify the firmware:

	```bash
	make flash
	```

	Confirm `Programming Finished`, `Verified OK`, and `Resetting Target`.
10. Disconnect the ST-Link after flashing and connect the CP2102 to the PC.
11. Find the serial device. On Linux or WSL it is commonly `/dev/ttyUSB0`:

	```bash
	ls /dev/ttyUSB* /dev/ttyACM*
	```

12. If access is denied, add the user to `dialout`, restart WSL, and reconnect the CP2102:

	```bash
	sudo usermod -aG dialout "$USER"
	```

13. Open the serial terminal:

	```bash
	picocom -b 115200 /dev/ttyUSB0
	```

14. Press the board reset button once. Confirm `Bootloader Ready` appears.
15. Type a character without pressing Enter. Confirm the same character is echoed and the PC13 LED blinks.
16. Exit `picocom` with `Ctrl+A`, then `Ctrl+X`.

### Phase 2 Troubleshooting

- Blank terminal: confirm the board is powered and reset it after opening the terminal.
- Garbled text: use 115200 baud and confirm the firmware uses `0x45` for the default 8 MHz clock. `0x271` is for a 72 MHz USART clock and is incorrect without system-clock setup.
- No echo but the LED blinks: verify CP2102 TXD -> PA10 and that `main.c` calls `uart1_getc()`.
- Permission denied: check `ls -l /dev/ttyUSB0`; the user must belong to `dialout`.
- No such file or directory: reconnect or reattach the CP2102 to WSL and check which `/dev/ttyUSB*` device exists.
- OpenOCD cannot connect: close `picocom` before flashing, check PA13/PA14 SWD wiring, and use only one board power source.

Acceptance test:

1. Flash the firmware with ST-Link.
2. Open the CP2102 serial port at 115200 baud, 8 data bits, no parity, 1 stop bit.
3. Confirm the startup message is received.
4. Type characters and confirm they are echoed back.

Do not start XMODEM until this phase passes.

---

## Phase 3: XMODEM Bootloader and Application Update

Status: IN PROGRESS

Objective: Receive an application image over USART1, program it into a reserved Flash region, verify it, and jump to it.

### 3.1 Bootloader memory layout

- [x] Reserve Flash for the bootloader at `0x08000000`.
- [x] Choose and document the application start address, initially `0x08002000`.
- [x] Create a separate application linker script using the application start address.
- [x] Ensure the application vector table is linked at the application address.
- [x] Add vector-table relocation support using `SCB->VTOR` before jumping to the application.
- [x] Ensure the bootloader and application do not overlap.

### 3.2 Bootloader entry decision

- [x] Define the bootloader startup policy.
- [x] Add a UART command or timeout to enter update mode.
- [x] Define how normal application boot is selected.
- [x] Add an application validity check.
- [x] Keep the bootloader in update mode when no valid application exists.

Selected startup policy:

- Listen on USART1 for `U`/`u` for 5 seconds after reset; that enters update mode.
- If no update command arrives, boot only an application with a valid stack pointer and reset vector.
- If no valid application exists, remain in update mode.
- Update mode receives firmware over XMODEM and programs the application Flash region.

### 3.3 Flash programming

- [x] Add Flash key, status, control, and address register definitions.
- [x] Implement Flash unlock.
- [x] Implement page erase.
- [x] Implement half-word programming.
- [x] Wait for Flash busy operations to finish.
- [x] Check Flash error flags.
- [x] Lock Flash after programming.
- [x] Reject images that exceed the application Flash region.

### 3.4 XMODEM protocol

- [x] Support XMODEM CRC and checksum modes.
- [x] Implement 128-byte `SOH` and 1024-byte `STX` packet handling.
- [x] Receive packet number and complement.
- [x] Receive 128-byte and 1024-byte payloads.
- [x] Validate the checksum.
- [x] Send `ACK` for valid packets.
- [x] Send `NAK` for invalid packets.
- [x] Handle duplicate packets safely.
- [x] Handle `EOT` and send the final acknowledgement.
- [x] Add timeout and retry limits.
- [x] Handle `CAN` cancellation.

### 3.5 Verification and application start

- [x] Track the received image length.
- [x] Verify each programmed half-word by reading it back.
- [x] Store or calculate an image checksum.
- [x] Validate the application initial stack pointer.
- [x] Validate that the reset handler points into the application region.
- [x] Disable interrupts and peripherals before the jump.
- [x] Set the MSP from the application vector table.
- [x] Set `SCB->VTOR` to the application vector table.
- [x] Jump to the application reset handler.

Acceptance test:

1. Start the bootloader through USART1.
2. Send an application image with an XMODEM-capable terminal tool.
3. Confirm packets are acknowledged.
4. Confirm Flash programming and image verification succeed.
5. Reset the board.
6. Confirm the new application starts.
7. Test invalid, interrupted, oversized, and retransmitted transfers.

Hardware verification completed: the sample application was transferred from
Minicom using XMODEM, PC13 blinked while the application ran, and after reset
the bootloader started the application again after the startup window.
Negative and retransmission cases in step 7 remain to be tested.

---

## Current Next Action

Test invalid, interrupted, oversized, and retransmitted transfers to complete the remaining robustness checks.

## Useful Commands

Build:

```bash
make clean
make
```

Build the sample application linked at `0x08002000`:

```bash
make application
```

Flash:

```bash
make flash
```

Debug:

```bash
make openocd
make debug
```

Check repository state:

```bash
git status
git log --oneline -5
```
