#ifndef ECU_SCENARIOS_H
#define ECU_SCENARIOS_H
#include <stdint.h>
#include <stdio.h>
/* Host-only deterministic exercise. Never linked into the ESP32 image. */
int ecu_run_scenarios(FILE *output, uint32_t seed);
#endif
