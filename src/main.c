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

    const uint8_t throttle_payload[] = {0x02, 0xEE};
    const EcuFrame throttle_frame = {
        .id = ECU_FRAME_ID_THROTTLE,
        .data = throttle_payload,
        .length = sizeof throttle_payload
    };

    if (!ecu_process_frame(&ecu, &rpm_frame))
    {
        fprintf(stderr, "RPM frame could not be processed\n");
        return 1;
    }

    if (!ecu_process_frame(&ecu, &throttle_frame))
    {
        fprintf(stderr, "Throttle frame could not be processed\n");
        return 1;
    }

    printf("RPM: %u\n", (unsigned int)ecu.rpm);
    printf("Throttle: %u.%u%%\n",
           (unsigned int)(ecu.throttle_permille / 10U),
           (unsigned int)(ecu.throttle_permille % 10U));

    return 0;
}