#ifndef ECU_H
#define ECU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Legacy-v0 learning API. It has no presence/freshness metadata.
 * New applications use ecu_core.h; the original 16 tests remain regression
 * checks for two-byte decoding and explicit sticky-fault behavior. */
typedef struct
{
    uint16_t rpm;
    uint16_t throttle_permille;
    int16_t coolant_temp_deci_c;
    uint32_t fault_flags;
} EcuState;

typedef struct
{
    uint16_t throttle_command_permille;
} EcuCommand;

typedef struct
{
    uint16_t id;
    const uint8_t *data;
    size_t length;
} EcuFrame;

enum
{
    ECU_FRAME_ID_RPM = 0x0100U,
    ECU_FRAME_ID_THROTTLE = 0x0101U,
    ECU_FRAME_ID_COOLANT_TEMP = 0x0102U
};

enum
{
    ECU_FAULT_NONE = 0U,
    ECU_FAULT_RPM_FRAME_INVALID = 1U << 0,
    ECU_FAULT_THROTTLE_FRAME_INVALID = 1U << 1,
    ECU_FAULT_COOLANT_TEMP_FRAME_INVALID = 1U << 2,
    ECU_FAULT_UNKNOWN_FRAME_ID = 1U << 3
};

enum
{
    ECU_RPM_LIMIT = 15000U,
    ECU_COOLANT_DERATE_START_DECI_C = 1100,
    ECU_DERATED_THROTTLE_MAX_PERMILLE = 500U
};

bool ecu_update_rpm(EcuState *ecu,
                    const uint8_t *frame,
                    size_t length);

bool ecu_update_throttle(EcuState *ecu,
                         const uint8_t *frame,
                         size_t length);

bool ecu_update_coolant_temp(EcuState *ecu,
                             const uint8_t *frame,
                             size_t length);

bool ecu_process_frame(EcuState *ecu, const EcuFrame *frame);

void ecu_clear_faults(EcuState *ecu);

EcuCommand ecu_compute_command(const EcuState *ecu);

#endif
