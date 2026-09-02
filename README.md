# F1 ECU Simulator

Host-based C11 simulator for learning ECU-style sensor-frame parsing,

state updates and defensive raw-buffer handling.

## Current milestone

The simulator safely decodes a two-byte big-endian RPM sensor frame and

updates the ECU state.

```text

[0x0B, 0xB8] -> 0x0BB8 -> 3000 RPM

```

Invalid inputs are rejected before they can cause an out-of-bounds access

or modify ECU state.

## Build and run

```bash

make

make run

```

Expected output:

```text

RPM: 3000

```

## Run tests

```bash

make test

```

The tests cover:

- Valid big-endian RPM decoding

- Short-frame rejection

- NULL-pointer rejection

- State preservation after invalid input

## Project structure

```text

include/    Public interfaces and ECU state definitions

src/        Application and ECU implementation

tests/      Unit tests

build/      Generated binaries; ignored by Git

```