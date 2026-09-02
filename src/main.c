#include <stdio.h>
#include <stdint.h>

#include "ecu.h"

int main(void)
{
    EcuState ecu = {0};
    const uint8_t rpm_payload[] = {0x0B, 0xB8};
    const EcuFrame rpm_frame = {
        .id = ECU_FRAME_ID_RPM,
        .data = rpm_payload,
        .length = sizeof rpm_payload
    };

    if (!ecu_process_frame(&ecu, &rpm_frame))
    {
        fprintf(stderr, "RPM frame could not be processed\n");
        return 1;
    }

    printf("RPM: %u\n", (unsigned int)ecu.rpm);
    return 0;
}