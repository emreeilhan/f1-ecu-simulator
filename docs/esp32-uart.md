# ESP32 UART exercise

## Recorded target

The final record uses ESP32-D0WD-V3 revision 3.1, 4 MB flash, ESP-IDF v6.1 and
UART0 at 115200, 8N1, GPIO1 TX / GPIO3 RX via the board's USB serial bridge.
The observed macOS device was `/dev/cu.usbserial-0001`; select your board's port.
[Final evidence](evidence/2026-10-07/esp32-final/) retains build/flash logs, JSON
trace, checks and source/firmware hashes. Source digests identify the measured
implementation rather than just its starting Git commit.

## Build and flash

Activate an ESP-IDF v6.1 environment with the ESP32 toolchain. From the repo root:

```bash
cd esp32
idf.py set-target esp32
idf.py build
export ECU_SERIAL_PORT=/dev/cu.usbserial-0001
idf.py -p "$ECU_SERIAL_PORT" -b 115200 flash
cd ..
```

Flashing installs this application's firmware on the selected board. Build and
host tests do not flash it. Keep any existing firmware backup private; it is
not public repository evidence. `sdkconfig.defaults` disables normal console
and bootloader logs and sets the FreeRTOS tick to 1 kHz. ROM boot text can still
appear at reset; it is not an application JSON response.

## Input and response

Each line is `HHH#HHHHHHHH\n`: three hexadecimal ID digits, `#`, four payload
bytes as eight hexadecimal digits. Both hex cases and one terminal CR for CRLF
are accepted. Construct payloads according to the [v1 contract](protocol-v1.md).

The parser retains partial lines and accepts combined lines. Its 64-byte buffer
holds at most 63 pre-newline bytes; a 64th byte enters discard mode until LF.
An overlong suffix cannot become a fresh frame. Driver overflow, frame and parity
errors also discard through the next LF.

| JSON field | Meaning |
|---|---|
| `t` | Device monotonic snapshot time, ms |
| `event`, `result`, `id` | Startup, frame, transport or tick and its result |
| `command`, `active`, `history` | Throttle permille, blocking faults and retained history |
| `present` | Accepted-sample presence bits; distinct from freshness |
| `value`, `counter`, `received` | Last accepted RPM/throttle/coolant data |
| `tx_dropped` | Responses dropped by the bounded application TX ring |

## Reproduce the already-flashed exercise

Use Python 3 with `pyserial` (included in the ESP-IDF Python environment):

```bash
python3 tools/uart_integration.py \
  --port "$ECU_SERIAL_PORT" \
  --exhaustive \
  --firmware-image esp32/build/ecu_uart_exercise.bin \
  --output build/esp32-trace.jsonl
```

The tool does not flash or reset. It writes the received JSON trace and a sibling
`.summary.json`, marked successful only after its assertions finish. The optional
firmware-image argument records the local binary hash; verified flash is a
separate step captured in the flash log.

The final exercise printed 12 PASS checks across 10 distinct labels: nominal
three-sensor command, all-channel timeout, CRC/duplicate measurement preservation,
active recovery with retained history, a diagnostic forward gap, valid-CRC range
failure/recovery, line overflow/recovery, sequential `255 → 0` and zero telemetry
drops. Nominal checks repeat three times. All 352 captured records reported zero
`tx_dropped` in that exercise.

## Scheduling and measurement boundary

The 10 ms scheduled loop handles at most four UART events, reads at most 64 bytes
and transmits at most 128 bytes per iteration from a 4096-byte application TX ring.
`uart_read_bytes(..., 0)` and `uart_tx_chars()` do not wait for a whole input line
or output response. Sustained overload can drop input or telemetry.

Receive time is when the application observes a completed line, not physical
FIFO arrival. It is passed separately from processing time; queueing before
application observation is not included in sample age. Host tests separately
exercise delayed receive timestamps through the core API.

Synthetic PC sensor values exercise the shared core on actual hardware. There
is no CAN transceiver, physical sensor/actuator, measured worst-case execution
time, bounded communication latency or safety certification. The timeout and
zero-drop results describe the recorded run only.
