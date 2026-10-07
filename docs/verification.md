# Verification record and reproduction

Native host execution, source coverage and the ESP32 UART exercise are separate
evidence layers. The coverage percentage excludes and does not replace hardware.

## Native host

[The native-host snapshot](evidence/2026-10-07/native-host/) was recorded on
2026-10-07 UTC with native ARM64 macOS and Apple Clang/LLVM 21. All three recorded
commands exited successfully:

```bash
make all test run
make sanitize
make coverage
```

The snapshot predates the implementation commit. `base_revision` identifies the
starting Git commit, not the measured implementation; SHA-256 maps identify
the measured source and match it. The working tree was recorded as dirty.

| Group | Passed units |
|---|---:|
| legacy-v0 regression | 16 test functions |
| v1 core | 20 named test functions |
| line transport | 4 named test functions |
| deterministic scenarios | 3 named checks |

These are 43 suite-specific units, not 43 end-to-end scenarios or an assertion
count. All groups and the demo also passed ASan/UBSan with no reported finding.
macOS leak detection was disabled (`detect_leaks=0`). No fuzz campaign is claimed.

## Requirements exercised

| Contract | Test |
|---|---|
| Required data and startup zero | `test_initial_missing_and_throttle_only` |
| 499/500/501 ms and independent channels | `test_age_499_500_501_and_refresh`, `test_independent_timeout_config`, `test_one_channel_stale` |
| Invalid config, alias-safe init | `test_invalid_config_disables_and_init_alias` |
| CRC reference, strict length, atomic encoder/decoder | `test_crc_reference_and_protocol_encoder_atomicity`, `test_protocol_length_null_and_decode_output_preservation` |
| Corruption and protected ID | `test_all_payload_single_bit_corruptions_preserve_measurement`, `test_id_is_covered_by_crc` |
| Signed coolant and range limits | `test_range_boundaries_and_signed_temperature` |
| Duplicate/old/gap/wrap, independent baseline | `test_duplicate_old_and_half_range_counters`, `test_counter_wrap_gap_independence_and_rejected_baseline` |
| Queue age and receive-time floor | `test_received_timestamp_and_queue_age` |
| Latched rollback and 64-bit boundary | `test_clock_rollback_is_latched_and_sensor_atomic`, `test_uint64_boundaries_do_not_wrap` |
| Recovery, history-only clear, resync ordering floor | `test_active_recovery_history_clear_and_resync` |
| Diagnostic and NULL API behavior | `test_unknown_diagnostic_and_transport_dont_open_or_block_command`, `test_null_api_uninitialized_and_latched_programming_error` |
| Thresholds, simultaneous reasons, command range | `test_policy_thresholds_multiple_reasons_and_range` |
| Saturation and result names | `test_saturating_statistics_and_result_names` |
| Partial/combined/CRLF, strict syntax, overflow | `tests/test_transport.c` |
| Same-seed repeatability and expected decisions | `tests/test_scenarios.c` |

## LLVM source coverage

The instrumented runner is `tests/test_core.c`. `tools/coverage.py` asserts the
exact four-module file set and denominator sum before recording its summary.

| Module | Lines | Branch outcomes |
|---|---:|---:|
| `src/ecu_core.c` | 129/129 | 75/76 |
| `src/ecu_protocol.c` | 77/78 | 70/74 |
| `src/ecu_time.c` | 6/6 | 10/10 |
| `src/ecu_policy.c` | 27/27 | 36/36 |
| **Total** | **239/240 (99.58%)** | **191/196 (97.45%)** |

Five branch outcomes were unexecuted in this core-only run: transport notification
on an uninitialized non-NULL context; negative throttle's encoder lower-bound
check; the private value-validator's invalid-sensor default; unknown ID passed
directly to the pure decoder (ingestion rejects it first); and `ECU_ACCEPTED_GAP`
passed directly to `ecu_result_accepted()`. The private invalid-sensor default is
the one unexecuted line. Gap ingestion/recovery is tested, but not that helper
branch in this instrumented runner.

Excluded from the denominator: legacy `src/ecu.c`, UART parser, ESP32 adapter,
scenario/demo runners and test code. `coverage-summary.json` records tools, flags,
scope and source hashes; `llvm-coverage-export.json` contains per-file counts;
`coverage-commands.json` contains exact commands and outputs. Binary profiles
and HTML are generated under ignored `build/coverage/`, not committed.

`make coverage-gcc` produces gcov with genuine GNU GCC and optional LCOV/HTML.
The Ubuntu workflow configures GCC/Clang normal/sanitized regressions and both
coverage targets. A configured job is not proof that a remote run has completed;
consult that run's result.

## Actual ESP32

[The final ESP32 record](evidence/2026-10-07/esp32-final/) contains source hashes,
ESP-IDF v6.1 build, verified flash and 352 device JSON records. The exhaustive
exercise printed 12 PASS checks across 10 distinct labels, with repeated nominal
checks. See [setup and interpretation](esp32-uart.md).

It checked nominal command, stale-data zero command, CRC/duplicate measurement
preservation, active recovery with history, forward gap, valid-CRC range failure,
line overflow recovery and sequential counter wrap. `tx_dropped` remained zero.
Physical FIFO arrival, CAN traffic, real sensors and worst-case timing were not
measured. Coverage and a passing exercise do not establish bug freedom or safety.

## Native Linux follow-up — 8 October

[Run 37689053307](https://github.com/emreeilhan/f1-ecu-simulator/actions/runs/37689053307)
passed for `d1f6dfcd27c41c0bf063932c24817f0bc670fed7`: GCC/Clang normal and
ASan/UBSan checks plus LLVM and GNU/LCOV coverage. Linux Clang18 maps the same
four core modules to 246/247 lines (99.60%) and 191/196 branch outcomes (97.45%).
GNU gcov reports 200/201 source lines (99.50%); tool denominators differ and are
not interchangeable. [Raw records](evidence/2026-10-08/linux-ci/) retain both.

The Linux build exposed missing final newlines in the two legacy C files. Adding
them changed no core or ESP32 source. Earlier native macOS manifests retain the
bytes measured at that time; current core/hardware source hashes still match.
