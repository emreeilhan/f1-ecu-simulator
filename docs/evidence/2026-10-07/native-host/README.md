# Native ARM64 host snapshot — 2026-10-07 UTC

These files are selected copies of the successful host verification and LLVM
coverage run. Source hashes match the measured implementation. `base_revision`
is its starting Git commit; the measurement recorded a dirty working tree.

| File | Origin / purpose |
|---|---|
| `manifest.json` | `build/verification/manifest.json`; command exits, units, platform and source hashes |
| `test-run.log` | `build/verification/1-run.log`; build, suites and scenario CSV |
| `sanitizer.log` | `build/verification/2-sanitize.log`; ASan/UBSan suites and demo |
| `coverage.log` | `build/verification/3-coverage.log`; completed target's metrics |
| `coverage-summary.json` | `build/coverage/summary.json`; scope, flags, tools and hashes |
| `coverage-commands.json` | `build/coverage/commands.json`; exact commands, outputs and exits |
| `llvm-coverage-export.json` | `build/coverage/llvm-coverage-export.json`; per-file counts and totals |

Counts: 16 legacy functions, 20 named core functions, 4 named transport functions
and 3 scenario checks. Core LLVM coverage: 239/240 lines and 191/196 branch
outcomes. macOS ASan leak detection was disabled. Profiler binaries, build
artifacts and private firmware backups are not included.

See [verification details](../../../verification.md) for exclusions and gaps,
and the separate ESP32 snapshot for the hardware measurement boundary.
