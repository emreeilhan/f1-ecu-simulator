#include <stdint.h>
#include <stdio.h>

#include "ecu.h"

static void print_temperature_deci_c(int16_t temperature_deci_c)
{
    int32_t magnitude = temperature_deci_c;
    const char *sign = "";

    if (magnitude < 0)
    {
        sign = "-";
        magnitude = -magnitude;
    }

    printf("Coolant: %s%ld.%ld C\n",
           sign,
           (long)(magnitude / 10),
           (long)(magnitude % 10));
}

static void print_control_scenario(const char *name, const EcuState *ecu)
{
    EcuCommand command = ecu_compute_command(ecu);

    printf("\nScenario: %s\n", name);
    printf("RPM: %u\n", (unsigned int)ecu->rpm);

    printf("Throttle request: %u.%u%%\n",
           (unsigned int)(ecu->throttle_permille / 10U),
           (unsigned int)(ecu->throttle_permille % 10U));

    print_temperature_deci_c(ecu->coolant_temp_deci_c);

    printf("Fault flags: 0x%08X\n",
           (unsigned int)ecu->fault_flags);

    printf("Throttle command: %u.%u%%\n",
           (unsigned int)(command.throttle_command_permille / 10U),
           (unsigned int)(command.throttle_command_permille % 10U));
}

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

    const uint8_t coolant_payload[] = {0x03, 0x89};
    const EcuFrame coolant_frame = {
        .id = ECU_FRAME_ID_COOLANT_TEMP,
        .data = coolant_payload,
        .length = sizeof coolant_payload
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

    if (!ecu_process_frame(&ecu, &coolant_frame))
    {
        fprintf(stderr, "Coolant frame could not be processed\n");
        return 1;
    }

    EcuState normal_ecu = ecu;

    EcuState rpm_limiter_ecu = ecu;
    rpm_limiter_ecu.rpm = ECU_RPM_LIMIT;

    EcuState hot_coolant_ecu = ecu;
    hot_coolant_ecu.throttle_permille = 800U;
    hot_coolant_ecu.coolant_temp_deci_c =
        ECU_COOLANT_DERATE_START_DECI_C;

    EcuState faulty_ecu = ecu;
    faulty_ecu.fault_flags |= ECU_FAULT_THROTTLE_FRAME_INVALID;

    print_control_scenario("Normal operation", &normal_ecu);
    print_control_scenario("RPM limiter active", &rpm_limiter_ecu);
    print_control_scenario("Coolant derating active", &hot_coolant_ecu);
    print_control_scenario("Throttle frame fault", &faulty_ecu);

    return 0;
}