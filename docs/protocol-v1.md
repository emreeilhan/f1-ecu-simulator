# v1 protocol and state contract

This is the implemented educational application protocol. The wire payload has
no separate version byte; `0x01` is a fixed prefix in the CRC calculation.

## Payload and validation

The accepted IDs are RPM `0x100`, throttle `0x101` and coolant `0x102`. Each
payload is exactly four bytes: a big-endian 16-bit value, a per-ID counter, then
CRC-8/SMBUS. Coolant uses signed two's complement in deci-degrees Celsius.
Accepted values are RPM 0–65535, throttle 0–1000 permille and coolant −400–1500
deci°C.

CRC input is `[01, ID_hi, ID_lo, value_hi, value_lo, counter]`. Polynomial `07`,
init `00`, no reflection and xorout `00` produce check `F4` for ASCII `123456789`.
CRC protects accidental corruption in this exercise, not malicious replay or
message authenticity.

`ecu_ingest_frame(ctx, frame, received_ms, now_ms)` validates context/clock,
frame pointer/ID, payload pointer/exact length/CRC/range, receive-time ordering
and age, then counter progression. Only after all checks pass does it publish
value, receive time and counter together. Encode/decode also preserve caller
output on failure. Rejection can change channel integrity and diagnostics, so
measurement preservation does not mean the entire context stays unchanged.

## Time

Initialization, receive times and processing times use one caller-supplied
`uint64_t` monotonic millisecond domain. Timeouts must be positive; each defaults
to 500 ms. Startup grace is zero. Missing samples block a nonzero command;
present samples become stale when `now_ms - received_ms >= timeout_ms`.
Invalid traffic never refreshes them.

Receive times before initialization, after processing time or before the last
accepted receive time are rejected. Equal receive times are permitted if all
other validation, including the counter, succeeds. Already-stale queued frames
are rejected instead of being restamped.

Backward processing time latches `ECU_ACTIVE_CLOCK`; only reinitialization
recovers it. `uint64_t` wrap is backward time, not supported modular rollover.
The ESP32 adapter uses 64-bit `esp_timer`; this version has no 32-bit wrap-aware
clock adapter or optional startup-grace extension.

## Counter and resynchronization

For each ID, the first accepted frame establishes a baseline. Later delta is
`(new_counter - last_counter) mod 256`:

| Delta | Result | Measurement update |
|---|---|---|
| 1 | `ECU_ACCEPTED` | Yes |
| 0 | `ECU_REJECT_DUPLICATE` | No |
| 2–127 | `ECU_ACCEPTED_GAP` | Yes; add `delta - 1` to missed counter steps |
| 128–255 | `ECU_REJECT_COUNTER` | No |

`255 → 0` is delta 1. Gap diagnostics do not independently close the command.
Missed counter steps are sequence observations, not proof of physical packet
loss. Long interruptions can make an 8-bit counter ambiguous.

`ecu_resync_stream(ctx, id)` removes presence, the counter baseline and channel
integrity. Values, statistics and the previous accepted receive timestamp remain.
That timestamp is an ordering floor, so resync cannot admit an older queued
sample. The channel waits for a valid, non-stale frame at or after that floor.
Resync does not remove a latched clock fault; full init resets the context.

## Faults and control

`active_faults` includes current missing, stale, channel-integrity and latched
API/configuration/clock conditions. Any active fault yields zero. A known-channel
NULL payload, incorrect length, CRC, range, receive time or counter sets channel
integrity; a fully valid frame on the same channel clears it. A NULL frame pointer
is a latched API error. Invalid initialization disables and clears the context
while recording the configuration failure.

`fault_history` retains active conditions and diagnostic events. Unknown ID,
counter gap and transport-event bits do not themselves enter `active_faults`.
History clearing changes only that bitmask; the next tick can record active
conditions again. Accepted/rejected/unknown/transport and per-result/missed-step
counters saturate at `UINT64_MAX`.

Fresh RPM ≥15000 yields zero; fresh coolant ≥1100 deci°C caps at 500 permille.
Otherwise the command matches a valid throttle request. Reason bits retain
simultaneous causes; zero-command conditions take priority over derating.
The policy helper assumes a context maintained through init/ingest/tick, not
manually altered public fields.

## Legacy boundary

The `EcuState` API in `ecu.h` / `ecu.c` remains explicit legacy-v0: it accepts
the first two payload bytes when length is at least two and uses sticky faults.
It has no v1 timing, presence, CRC or counter guarantees. The host runner and
ESP32 adapter use v1 exclusively, with no fallback for rejected two-byte input.
