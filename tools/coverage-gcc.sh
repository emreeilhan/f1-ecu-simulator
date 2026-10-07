#!/bin/sh
set -eu
task_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
task_gcc=${GCC:-gcc}
task_gcov=${GCOV:-gcov}
if ! "$task_gcc" --version | head -n 3 | grep -q 'Free Software Foundation'; then
    printf '%s\n' 'A real GNU GCC and matching gcov are required (macOS gcc is often Apple clang).' >&2
    exit 2
fi
task_cov="$task_root/build/gcov"
mkdir -p "$task_cov"
rm -f "$task_cov/ecu_core.gcda" "$task_cov/ecu_protocol.gcda" "$task_cov/ecu_time.gcda" "$task_cov/ecu_policy.gcda" "$task_cov/test_core.gcda"
for task_source in ecu_core ecu_protocol ecu_time ecu_policy; do
    "$task_gcc" -std=c11 -Wall -Wextra -Werror -Wpedantic -I"$task_root/include" -O0 -g --coverage -c "$task_root/src/$task_source.c" -o "$task_cov/$task_source.o"
done
"$task_gcc" -std=c11 -Wall -Wextra -Werror -Wpedantic -I"$task_root/include" -O0 -g --coverage -c "$task_root/tests/test_core.c" -o "$task_cov/test_core.o"
"$task_gcc" --coverage "$task_cov/test_core.o" "$task_cov/ecu_core.o" "$task_cov/ecu_protocol.o" "$task_cov/ecu_time.o" "$task_cov/ecu_policy.o" -o "$task_cov/test_core"
"$task_cov/test_core" > "$task_cov/tests.log"
cd "$task_cov"
"$task_gcov" -b -c ecu_core.gcno ecu_protocol.gcno ecu_time.gcno ecu_policy.gcno > report.txt
"$task_gcc" --version > gcc-version.txt
"$task_gcov" --version > gcov-version.txt
if command -v lcov >/dev/null 2>&1; then
    lcov --capture --directory "$task_cov" --output-file coverage-all.info
    lcov --extract coverage-all.info "$task_root/src/ecu_core.c" "$task_root/src/ecu_protocol.c" "$task_root/src/ecu_time.c" "$task_root/src/ecu_policy.c" --output-file coverage.info
    if command -v genhtml >/dev/null 2>&1; then genhtml coverage.info --output-directory html; fi
fi
cat report.txt
