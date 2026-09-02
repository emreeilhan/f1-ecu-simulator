# F1 ECU Simulator

Host-based C11 simulator for learning ECU-style sensor-frame parsing,
state updates and defensive raw-buffer handling.

## Current behavior

The simulator receives a frame ID, a raw payload pointer and a payload
length. It dispatches recognised frames to the correct decoder.

| Frame ID | Signal | Payload format | Valid range |
|---|---|---|---|
| `0x0100` | Engine speed | 2-byte big-endian unsigned integer | Any `uint16_t` RPM value |
| `0x0101` | Throttle position | 2-byte big-endian unsigned integer | `0` to `1000` permille |

Throttle is represented in permille to avoid floating-point arithmetic:

```text
0    -> 0.0%
750  -> 75.0%
1000 -> 100.0%
```

Example input:

```text
RPM payload:      [0x0B, 0xB8] -> 0x0BB8 -> 3000 RPM
Throttle payload: [0x02, 0xEE] -> 0x02EE -> 750 -> 75.0%
```

Unknown frame IDs, NULL pointers, short payloads and out-of-range throttle
values are rejected without modifying ECU state.

## Build and run

```bash
make
make run
```

Expected output:

```text
RPM: 3000
Throttle: 75.0%
```

## Run tests

```bash
make test
```

The tests cover:

- Big-endian RPM decoding
- Big-endian throttle decoding
- Short-payload rejection
- NULL-pointer rejection
- Unknown-frame rejection
- Out-of-range throttle rejection
- State preservation after invalid input

## Project structure

```text
include/    Public interfaces and ECU state definitions
src/        Application and ECU implementation
tests/      Unit tests
build/      Generated binaries; ignored by Git
```