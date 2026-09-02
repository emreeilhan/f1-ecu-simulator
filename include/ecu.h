#ifndef ECU_H
#define ECU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint16_t rpm;
} EcuState;

typedef struct
{
    uint16_t id;
    const uint8_t *data;
    size_t length;
} EcuFrame;

enum
{
    ECU_FRAME_ID_RPM = 0x0100U
};

bool ecu_update_rpm(EcuState *ecu,
                    const uint8_t *frame,
                    size_t length);

bool ecu_process_frame(EcuState *ecu, const EcuFrame *frame);

#endif