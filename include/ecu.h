#ifndef ECU_H
#define ECU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint16_t rpm;
} EcuState;

bool ecu_update_rpm(EcuState *ecu, const uint8_t *frame, size_t length);
#endif