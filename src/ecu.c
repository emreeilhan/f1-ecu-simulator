#include "ecu.h"

enum
{
    THROTTLE_MAX_PERMILLE = 1000U,
    COOLANT_TEMP_MIN_DECI_C = -400,
    COOLANT_TEMP_MAX_DECI_C = 1500
};

static uint16_t decode_be_u16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8U) |
                      (uint16_t)data[1]);
}

static int16_t decode_be_i16(const uint8_t *data)
{
    uint16_t raw = decode_be_u16(data);

    if (raw <= INT16_MAX)
    {
        return (int16_t)raw;
    }

    return (int16_t)((int32_t)raw - 65536);
}

bool ecu_update_rpm(EcuState *ecu,
                    const uint8_t *frame,
                    size_t length)
{
    if (ecu == NULL || frame == NULL || length < 2U)
    {
        return false;
    }

    ecu->rpm = decode_be_u16(frame);

    return true;
}

bool ecu_update_throttle(EcuState *ecu,
                         const uint8_t *frame,
                         size_t length)
{
    uint16_t throttle_permille;

    if (ecu == NULL || frame == NULL || length < 2U)
    {
        return false;
    }

    throttle_permille = decode_be_u16(frame);

    if (throttle_permille > THROTTLE_MAX_PERMILLE)
    {
        return false;
    }

    ecu->throttle_permille = throttle_permille;

    return true;
}

bool ecu_update_coolant_temp(EcuState *ecu,
                             const uint8_t *frame,
                             size_t length)
{
    int16_t coolant_temp_deci_c;

    if (ecu == NULL || frame == NULL || length < 2U)
    {
        return false;
    }

    coolant_temp_deci_c = decode_be_i16(frame);

    if (coolant_temp_deci_c < COOLANT_TEMP_MIN_DECI_C ||
        coolant_temp_deci_c > COOLANT_TEMP_MAX_DECI_C)
    {
        return false;
    }

    ecu->coolant_temp_deci_c = coolant_temp_deci_c;

    return true;
}

bool ecu_process_frame(EcuState *ecu, const EcuFrame *frame)
{
    bool ok;

    if (ecu == NULL || frame == NULL)
    {
        return false;
    }

    switch (frame->id)
    {
        case ECU_FRAME_ID_RPM:
            ok = ecu_update_rpm(ecu, frame->data, frame->length);

            if (!ok)
            {
                ecu->fault_flags |= ECU_FAULT_RPM_FRAME_INVALID;
            }

            return ok;

        case ECU_FRAME_ID_THROTTLE:
            ok = ecu_update_throttle(ecu, frame->data, frame->length);

            if (!ok)
            {
                ecu->fault_flags |= ECU_FAULT_THROTTLE_FRAME_INVALID;
            }

            return ok;

        case ECU_FRAME_ID_COOLANT_TEMP:
            ok = ecu_update_coolant_temp(ecu,
                                         frame->data,
                                         frame->length);

            if (!ok)
            {
                ecu->fault_flags |= ECU_FAULT_COOLANT_TEMP_FRAME_INVALID;
            }

            return ok;

        default:
            ecu->fault_flags |= ECU_FAULT_UNKNOWN_FRAME_ID;
            return false;
    }
}

void ecu_clear_faults(EcuState *ecu)
{
    if (ecu != NULL)
    {
        ecu->fault_flags = ECU_FAULT_NONE;
    }
}
EcuCommand ecu_compute_command(const EcuState *ecu)
{
    EcuCommand command = {0};

    if (ecu == NULL)
    {
        return command;
    }

    if (ecu->fault_flags != ECU_FAULT_NONE)
    {
        return command;
    }

    if (ecu->rpm >= ECU_RPM_LIMIT)
    {
        return command;
    }

    command.throttle_command_permille = ecu->throttle_permille;

    if (ecu->coolant_temp_deci_c >= ECU_COOLANT_DERATE_START_DECI_C &&
        command.throttle_command_permille >
            ECU_DERATED_THROTTLE_MAX_PERMILLE)
    {
        command.throttle_command_permille =
            ECU_DERATED_THROTTLE_MAX_PERMILLE;
    }

    return command;
}