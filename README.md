# F1 ECU Simulator

A C11 exercise in sensor-frame validation, freshness tracking, fault recovery and
control policy. The same v1 core runs in deterministic host tests and in an
ESP32 firmware image that accepts sensor frames over UART.

The project is F1-inspired; its messages and thresholds are educational design
choices. It has no vehicle integration or CAN physical layer and makes no safety
certification or real-time execution claim.

## What the core does

```text
host scenarios / ESP32 UART
            ↓
ID → exact length → CRC → value range → receive time → sequence counter
            ↓
accepted sensor value + receive time + counter
            ↓
missing/stale/integrity faults + diagnostic history
            ↓
control policy → throttle command + reasons
```

`EcuContext` tracks RPM, throttle and coolant independently. The core receives a
monotonic `uint64_t` millisecond clock from its caller; it performs no allocation,
clock reads or I/O. Accepted value, counter and receive timestamp are published
together. A rejected frame can change diagnostic state but cannot refresh the
last accepted measurement.

### v1 sensor frames

Each v1 payload is **exactly four bytes**:

| Bytes | Meaning |
|---|---|
| 0–1 | Big-endian sensor value |
| 2 | Per-ID rolling counter |
| 3 | CRC-8/SMBUS |

| ID | Signal | Representation and accepted range |
|---|---|---|
| `0x100` | RPM | Unsigned 16-bit, 0–65535 RPM |
| `0x101` | Throttle request | Unsigned 16-bit, 0–1000 permille |
| `0x102` | Coolant temperature | Signed 16-bit, −400–1500 deci-degrees Celsius |

The CRC covers `[0x01, ID_hi, ID_lo, value_hi, value_lo, counter]`, using polynomial
`0x07`, initial value `0`, no reflection and final XOR `0`. The `123456789` check
value is `0xF4`. This is an application checksum, not CAN's wire CRC or message
authentication. See the [v1 contract](docs/protocol-v1.md) for exact acceptance
and recovery rules.

Each ID establishes its own counter baseline. A modulo-256 delta of 1 is normal,
0 is a duplicate, 2–127 is accepted with a gap diagnostic, and 128–255 is rejected
as old or ambiguous. `255 → 0` is a normal increment. Rejected frames leave the
baseline unchanged.

### Freshness, faults and command policy

All three sensors are required. Their default timeouts are 500 ms each and can
be configured independently. A sample is stale at **age ≥ timeout**: 499 ms is
fresh, 500 ms is stale. Calling `ecu_tick()` or `ecu_compute_command_at()` checks
age even when no frame arrives.

Receive and processing timestamps must share the same monotonic clock. Future,
pre-initialization, already stale and older-than-last-accepted receive timestamps
are rejected. A backward processing clock latches a fault until reinitialization.

| Condition | Throttle command | Recovery |
|---|---|---|
| Required sample missing or stale | 0 | Fresh accepted sample on that channel |
| Known-channel length, CRC, range, timestamp or counter rejection | 0 | Fully valid frame on the same channel |
| Latched clock or NULL-frame API error | 0 | Reinitialize the context |
| Fresh RPM ≥15000 | 0 | Fresh RPM below the limit |
| Fresh coolant ≥110.0°C | At most 500 permille | Fresh coolant below the threshold |
| Otherwise | Requested throttle | — |

`active_faults` describes current blocking conditions. `fault_history` and
saturating event counters retain diagnostics after recovery. Unknown IDs,
forward counter gaps and transport errors are diagnostic; they do not by
themselves block an otherwise valid command. Continued data loss still triggers
the freshness checks.

`ecu_clear_fault_history()` clears only the history bitmask, not active faults,
sample age or event counters. `ecu_resync_stream()` removes a channel's presence
and sequence baseline, so it waits for a new valid sample. The previous accepted
receive timestamp remains an ordering floor; resync cannot admit an older queued
sample.

### Explicit legacy-v0 compatibility

The original two-byte `EcuState` API is retained in `include/ecu.h` and
`src/ecu.c`, with its original sticky-fault behavior and 16 regression functions.
It accepts payloads of at least two bytes and has no CRC, counter or freshness
tracking. It is exercised only by `build/test_ecu`; the host demo and ESP32 image
use v1. There is no automatic fallback from a rejected v1 frame to v0.

## Host build, scenarios and tests

Use Clang or GCC with C11 support, Make and Python 3:

```bash
make all test run
make sanitize
make coverage
```

`make run` emits a deterministic CSV trace, also stored as
[`scenarios/expected.csv`](scenarios/expected.csv). It demonstrates startup,
499/500 ms freshness boundaries, a dropped coolant update, a seeded bit flip,
gap recovery, duplicate rejection, a valid-CRC range failure, queued-data delay
and clock rollback. It uses a virtual clock, not real sleeps. Scenario tests
compare repeated runs with the same seed and the checked-in expected decisions.

The recorded native macOS ARM64 run passed:

| Suite | Count |
|---|---:|
| v1 core | 20 named test functions |
| legacy-v0 regression | 16 test functions |
| UART line parser | 4 named test functions |
| deterministic scenarios | 3 named checks |

These are suite-specific test units, not an assertion count. The same suites and
demo passed ASan/UBSan; leak detection was disabled for that macOS sanitizer run.

LLVM source coverage of **only** `ecu_core.c`, `ecu_protocol.c`, `ecu_time.c` and
`ecu_policy.c` measured **239/240 lines (99.58%)** and **191/196 branch outcomes
(97.45%)**. Legacy code, UART parsing, the ESP32 adapter, scenario/demo runners and
tests are outside that denominator. Raw summaries, commands, source hashes and
logs are in [the native host evidence](docs/evidence/2026-10-07/native-host/).
The [verification notes](docs/verification.md) explain uncovered branches and
what was actually checked.

`make coverage` needs matching `llvm-cov` and `llvm-profdata` tools. A separate
`make coverage-gcc` target needs genuine GNU GCC and matching gcov, with optional
LCOV/HTML output when those tools are installed. The Linux workflow runs GCC and
Clang regressions and both coverage targets; workflow configuration alone is not
a completed test result.

## ESP32 UART target

The firmware was built and flashed with **ESP-IDF v6.1** onto an
**ESP32-D0WD-V3 revision 3.1 with 4 MB flash**. UART0 uses TX GPIO1 and RX GPIO3
through the board's USB serial bridge at **115200, 8N1**.

Send an LF-delimited line `HHH#HHHHHHHH`, where the three hexadecimal ID digits
are followed by eight hexadecimal payload digits. A final CR is accepted for
CRLF. JSON responses report accepted values, counters, receive timestamps,
command, active faults, history and dropped telemetry count.

The final board exercise checked nominal flow, communication loss, corrupted
CRC, duplicate frames, gap diagnostics, valid-CRC out-of-range input, bounded
line overflow and recovery, and a sequential `255 → 0` counter wrap. Its
[final summary and trace](docs/evidence/2026-10-07/esp32-final/) contain **12 printed
PASS checks across 10 distinct labels**, 352 JSON records and no telemetry drops
in that exercise. This count includes repeated nominal checks.

The adapter uses a 10 ms scheduled loop with bounded RX, TX and event processing.
Receive time means the moment a complete line is observed by the application,
not its physical arrival in the hardware FIFO. The UART run does not measure a
CAN bus, real sensors, worst-case execution time or end-to-end real-time bounds.
See [ESP32 setup and reproduction](docs/esp32-uart.md).

## Layout

```text
include/       v1, transport and explicit legacy interfaces
src/           core, protocol, time, policy, legacy code and host runners
tests/         core, legacy, transport and scenario suites
scenarios/     expected deterministic CSV trace
esp32/         ESP-IDF UART adapter using the same v1 core
tools/         coverage and already-flashed UART exercise tools
docs/          protocol, setup, verification and dated evidence
build/         generated host binaries and measurement files (ignored)
```
