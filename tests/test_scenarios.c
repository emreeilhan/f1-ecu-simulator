#include "ecu_scenarios.h"
#include <assert.h>
#include <stdio.h>

static void compare(FILE *a, FILE *b)
{
    rewind(a); rewind(b);
    int ca, cb;
    do { ca = fgetc(a); cb = fgetc(b); assert(ca == cb); } while (ca != EOF);
    assert(!ferror(a) && !ferror(b));
}

int main(void)
{
    FILE *a = tmpfile(), *b = tmpfile(); assert(a && b);
    assert(ecu_run_scenarios(a, UINT32_C(0xB17)) == 0);
    assert(ecu_run_scenarios(b, UINT32_C(0xB17)) == 0);
    compare(a, b); puts("PASS: deterministic same-seed trace");
    FILE *golden = fopen("scenarios/expected.csv", "r"); assert(golden);
    compare(a, golden); puts("PASS: fault-injection trace matches independent expected decisions");
    assert(ecu_run_scenarios(NULL, 0) != 0); puts("PASS: NULL output rejected");
    fclose(a); fclose(b); fclose(golden);
    puts("Scenarios: 3 named checks passed.");
    return 0;
}
