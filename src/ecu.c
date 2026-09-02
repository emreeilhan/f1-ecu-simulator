#include "ecu.h"

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

        default:
            return false;
    }
}