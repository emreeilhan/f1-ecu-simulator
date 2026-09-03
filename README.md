# F1 ECU Simulator

A host-based C11 simulator for learning ECU-style sensor-frame parsing,
defensive raw-buffer handling, state updates, fault tracking and safety control
decisions.

This is an educational, F1-inspired project. It runs as a normal macOS command-line
program; it does not communicate with real vehicle hardware.

## Architecture

```text
Raw sensor frame
      |
      v
Frame ID dispatch
      |
      v
Length, NULL and range validation
      |
      v
Typed ECU state update
      |
      v
Fault flag tracking
      |
      v
Safety policy
      |
      v
Throttle command
```

A frame contains an ID, a pointer to raw bytes and its payload length. The ECU
dispatches recognised IDs to the correct decoder.

## Supported sensor frames

| Frame ID | Signal | Payload format | Valid range |
|---|---|---|---|
| `0x0100` | Engine speed | 2-byte big-endian unsigned integer | Any `uint16_t` RPM value |
| `0x0101` | Throttle request | 2-byte big-endian unsigned integer | `0` to `1000` permille |
| `0x0102` | Coolant temperature | 2-byte big-endian signed integer | `-40.0 C` to `150.0 C` |

Throttle uses permille instead of floating point:

```text
0    -> 0.0%
750  -> 75.0%
1000 -> 100.0%
```

Coolant temperature uses signed deci-degrees Celsius:

```text
905   -> 90.5 C
-200  -> -20.0 C
1100  -> 110.0 C
```

Example frame decoding:

```text
RPM payload:      [0x0B, 0xB8] -> 0x0BB8 -> 3000 RPM
Throttle payload: [0x02, 0xEE] -> 0x02EE -> 750 -> 75.0%
Coolant payload:  [0x03, 0x89] -> 0x0389 -> 905 -> 90.5 C
```

## Validation and fault handling

Invalid input is rejected before it can update ECU state:

- `NULL` ECU state, frame or payload pointers
- payloads shorter than two bytes
- unknown frame IDs
- throttle values above `1000` permille
- coolant temperatures outside `-40.0 C` to `150.0 C`

The ECU records invalid-frame events as bit flags in `EcuState.fault_flags`.
Several faults can coexist, and `ecu_clear_faults()` clears them explicitly.

## Safety control policy

`ecu_compute_command()` converts sensor state into an actuator command.

| Condition | Throttle command |
|---|---|
| Any fault flag is set | `0.0%` |
| RPM is at or above `15000` | `0.0%` |
| Coolant is at or above `110.0 C` | Capped at `50.0%` |
| Otherwise | Matches the throttle request |

The command is deliberately separate from the throttle request:

```text
Throttle request = what the driver asks for
Throttle command = what the ECU safely permits
```

## Build and run

```bash
make
make run
```

Or run a clean verification from scratch:

```bash
make clean && make test && make run
```

The demo runs four scenarios:

```text
Normal operation         -> request 75.0%, command 75.0%
RPM limiter active       -> request 75.0%, command 0.0%
Coolant derating active  -> request 80.0%, command 50.0%
Throttle frame fault     -> request 75.0%, command 0.0%
```

## Run tests

```bash
make test
```

The unit tests cover:

- big-endian unsigned RPM and throttle decoding
- big-endian signed coolant-temperature decoding
- short-payload and `NULL`-pointer rejection
- unknown-frame rejection
- range validation and state preservation after invalid input
- fault flag setting, preservation and clearing
- normal throttle commands, RPM limiting, thermal derating and fail-safe output

## Project structure

```text
include/    Public interfaces, frame definitions and ECU state
src/        ECU implementation and command-line demo
tests/      Unit tests
build/      Generated binaries; ignored by Git
```