# TivaC Stepper Motor Controller

A fully interrupt-driven stepper motor controller for the **TM4C123GH6PM** (TivaC LaunchPad), controlled remotely over UART from a secondary MCU. No polling loops. No blocking main thread. The CPU sleeps between events. No use of HAL or RTOS. Bare-metal programming.

---

## Hardware Setup

| Peripheral | Pins | Role |
|---|---|---|
| UART0 | PA0 (Rx), PA1 (Tx) | Command interface from remote MCU |
| Port B | PB0–PB3 | Bipolar stepper motor coil drive |
| Port F | PF1 (RED), PF2 (BLUE), PF3 (GREEN) | Status LEDs |
| Port F | PF0 (SW1), PF4 (SW2) | Kill switch / Start switch |

The remote MCU exposes a keypad for command entry and an LCD for feedback. This controller receives commands over UART, parses them, and drives the motor accordingly.

---

## Command Interface

Commands are sent as ASCII strings over UART at **9600 baud**. The parser accepts the following:

```
RPM=<1–1200>          Set motor speed (required)
DIR={CW or CCW}       Set rotation direction (default: CW)
SPR=<1–2048>          Set steps per revolution (default: 200)
INFO                  Query current settings (must be sent alone)
```

Multiple commands can be sent in a single transmission:

```
RPM=600
DIR=CCW
SPR=400
```

`INFO` is a standalone query — it cannot be combined with configuration commands. The controller responds with 3 big-endian `uint32_t` values over UART: RPM, DIR, SPR.

### Rules
- RPM is **mandatory**. The motor will not start without it.
- SPR and DIR are optional — they default to 200 steps/rev and CW.
- Multiple instances of the same commands in a single transmission are rejected.
- Any unrecognized input is rejected and the system enters error state (red LED blink).

---

## System States

The RGB LED on Port F communicates system state at a glance:

| LED | Behaviour | State |
|---|---|---|
| 🔵 Blue | Blinking | Idle — awaiting commands |
| 🔴 Red | Blinking | Parse error — bad or missing command |
| 🟢 Green | Blinking | Motor stopped — valid config loaded, ready to start |
| 🟢 Green | Solid | Motor running |

---

## Architecture

### Everything is interrupt-driven. The CPU never polls.

```c
while (1) Wait_For_Interrupt();
```

`main()` initializes all peripherals and immediately suspends the CPU with the `WFI` instruction. Every action in the system: receiving a character, pressing a button, firing a motor step, blinking an LED, is handled in an interrupt handler. The CPU wakes, does its work, and returns to sleep.

### NVIC Priority Hierarchy

Three interrupt sources, deliberately prioritized:

| Source | Priority | Reason |
|---|---|---|
| UART0 | 0 (highest) | Must never miss a byte — FIFO is finite |
| Port F (buttons) | 1 | Kill switch must preempt motor stepping |
| SysTick | 2 (lowest) | A late LED blink is invisible and tolerable |

>Note: Kill switch cannot interrupt UART at all since UART is turned off while the motor is on. More on this below.

---

## Design Decisions

### The XOR Stepper Sequence

Motor commutation logic relies on bit manipulation with XOR operations. that generate a self-sustaining full-step bipolar sequence. No branching or lookup table.

```c
GPIO_PORTB_DATA_R ^= MASK1;
MASK1 ^= MASK2;
```

Here's how it works for CW rotation (`MASK1 = 0x0C`, `MASK2 = 0x03`, initial DATA = `0b1001`):

| Tick | Operation | PORTB [3:0] | MASK1 after |
|---|---|---|---|
| init | — | `1001` = 9 | `0x0C` |
| 1 | `DATA ^= 0x0C` → `MASK1 ^= 0x03` | `0101` = 5 | `0x0F` |
| 2 | `DATA ^= 0x0F` → `MASK1 ^= 0x03` | `1010` = 10 | `0x0C` |
| 3 | `DATA ^= 0x0C` → `MASK1 ^= 0x03` | `0110` = 6 | `0x0F` |
| 4 | `DATA ^= 0x0F` → `MASK1 ^= 0x03` | `1001` = 9 | `0x0C` |

The sequence `9 → 5 → 10 → 6 → 9 → ...` generates a full-step bipolar commutation pattern. MASK1 oscillates between `0x0C` and `0x0F` on every tick, producing alternating XOR operations that advance the magnetic field. Reversing direction is handled entirely in the PortF handler by swapping the initial values of MASK1 and MASK2. the SysTick handler itself is direction-agnostic.

### UART Timeout Parsing

The UART interrupt is enabled for two conditions: **receive** and **receive timeout**.

- The receive interrupt fires as bytes arrive and flushes them from the hardware FIFO into the command buffer.
- The timeout interrupt fires when data has stopped arriving for 32 bit periods — the natural end of a transmission.
- `parse()` is called only on timeout, and only if the buffer is non-empty.

This avoids scanning for newline characters, and ensures parsing only occurs when transmission has stopped. Moreover, the UART handler flushes the entire buffer to reduce the number of interrupt calls and context switching.

### SysTick Dual-Mode Operation

A single SysTick timer serves two completely different purposes depending on system state:

**Motor running:** SysTick is programmed to the step period: `960,000,000 / (RPM × SPR)` ticks at 16 MHz. The handler fires at the exact step frequency and drives the XOR commutation.

**Motor stopped:** SysTick runs at 1 ms. A `COUNTER` variable counts down from 500, producing a ~500 ms LED blink period with a single XOR:

```c
GPIO_PORTF_DATA_R ^= BLINKER;
```

Switching between modes is a single call to `SysTick_set()`, which reprograms the reload register and resets the counter.

### The BLINKER Register

`BLINKER` holds the Port F pin mask directly: `0x02` (red), `0x04` (blue), `0x08` (green). Changing which LED blinks is a single assignment: `BLINKER = 0x08`. The SysTick handler XORs the register with no branching or lookup. Switching the LED state machine costs one byte of RAM and one instruction per tick.

>Note: UART and PortF handlers turns the state LED on and adjust the `BLINKER`, leaving the rest for the SysTick handler.
>When motor is on, the green LED is turned on once and the start with no blinking. Ensuring the motor rotations cannot be interrupt wxcept with the kill-switch.

### PortB and UART Clock Gating

PortB is configured at boot but its clock is immediately disabled:

```c
// PortB_init():
SYSCTL_RCGCGPIO_R |= 0x02;   // enable clock
// ... configure outputs ...
SYSCTL_RCGCGPIO_R &= ~0x02;  // disable clock
```

Similarly, UART0 clock is disabled when the on-switch is pressed, and turned back on when the kill-switch is pressed.

This is done to take advantage of TM4C123's power saving technique. Also, ensures the UART cannot interrupt the motor while it is on. Moreover, turning off PortB clock when motor is off ensures that if one of PortB's pins is accidentally modified, a hard-fault occurs, which is safer than the motor spinning unexpectedly.

### Command Buffer Design

The command buffer is a `uint8_t commands[256]` with a `uint8_t INDEX` counter. The maximum index value (255) exactly equals the last valid array index. This means INDEX can never produce an out-of-bounds write without wrapping, and wrapping is not a real concern in practice: the largest legal command input (`RPM=123456789\nSPR=2048\nDIR=CCW\n`) is under 40 characters. A user would have to transmit more than 256 characters of unsupported commands, or multiple instances of the same command before the timeout fires to overflow the buffer, and the parser rejects both. No need for `INDEX` bounds checking.

### Parser Token Skipping

After tokenization, `=`, `\n`, `\r` become null terminators. The buffer transforms:

```
"RPM=600\nDIR=CW\n"  →  "RPM\0600\0DIR\0CW\0"
```

The parser uses `strcmp` against null-terminated strings directly in the buffer. When it matches a keyword, it skips to its value with `i += 4` — past the 3-character keyword and its trailing null. After processing a value, it advances to the next token with:

```c
while (i < INDEX && commands[i]) i++;    // skip to end of current value
while (i < INDEX && !commands[i+1]) i++; // park on last null before next token
```

The for-loop's own `i++` then lands on the first character of the next token. Parsing in-place with pointer arithmetic on a static buffer.

>Note: all supported commands: `RPM`, `SPR`, `DIR` are 3 characters. If this is to change, then `i += 4` would need to change.

---

## File Structure

```
├── include/
│   ├── gpio.h
│   ├── parser.h
│   ├── systick.h
│   ├── uart.h
│   └── util.h
├── src/
│   ├── gpio.c
│   ├── main.c
│   ├── parser.c
│   ├── systick.c
│   ├── uart.c
│   └── util.c
```

---

## Simulator

An [interactive browser-based simulator](./motor_simulator.html) is included. It replicates the exact parser logic, interrupt handler behaviour, XOR stepper sequence, SysTick dual-mode timing, and LED state machine.

> **AI-generated.** The simulator was built with Claude (Anthropic) as a development tool. The embedded C firmware is original work.

---

## License

The project is licensed under the MIT License.
See the [LICENSE](./LICENSE) file for license information.
