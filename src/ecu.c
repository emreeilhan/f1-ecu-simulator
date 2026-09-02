#include "ecu.h"

enum
{
    THROTTLE_MAX_PERMILLE = 1000U
};

bool ecu_update_rpm(EcuState *ecu,
                    const uint8_t *frame,
                    size_t length)
{
    if (ecu == NULL || frame == NULL || length < 2U)
    {
        return false;
    }

    ecu->rpm = (uint16_t)(((uint16_t)frame[0] << 8U) |
                          (uint16_t)frame[1]);

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

    throttle_permille = (uint16_t)(((uint16_t)frame[0] << 8U) |
                                   (uint16_t)frame[1]);

    if (throttle_permille > THROTTLE_MAX_PERMILLE)
    {
        return false;
    }

    ecu->throttle_permille = throttle_permille;

    return true;
}

bool ecu_process_frame(EcuState *ecu, const EcuFrame *frame)
{
    if (ecu == NULL || frame == NULL)
    {
        return false;
    }

    switch (frame->id)
    {
        case ECU_FRAME_ID_RPM:
            return ecu_update_rpm(ecu, frame->data, frame->length);

        case ECU_FRAME_ID_THROTTLE:
            return ecu_update_throttle(ecu, frame->data, frame->length);

        default:
            return false;
    }
}