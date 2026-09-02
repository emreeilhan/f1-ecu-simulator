#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "ecu.h"

static void test_known_rpm_frame_updates_state(void)
{
    EcuState ecu = {0};
    const uint8_t payload[] = {0x0B, 0xB8};
    const EcuFrame frame = {
        .id = ECU_FRAME_ID_RPM,
        .data = payload,
        .length = sizeof payload
    };

    bool ok = ecu_process_frame(&ecu, &frame);

    assert(ok);
    assert(ecu.rpm == 3000U);
}

static void test_short_rpm_frame_is_rejected(void)
{
    EcuState ecu = {.rpm = 4200U};
    const uint8_t payload[] = {0x0B};
    const EcuFrame frame = {
        .id = ECU_FRAME_ID_RPM,
        .data = payload,
        .length = sizeof payload
    };

    bool ok = ecu_process_frame(&ecu, &frame);

    assert(!ok);
    assert(ecu.rpm == 4200U);
}

static void test_known_throttle_frame_updates_state(void)
{
    EcuState ecu = {0};
    const uint8_t payload[] = {0x02, 0xEE};
    const EcuFrame frame = {
        .id = ECU_FRAME_ID_THROTTLE,
        .data = payload,
        .length = sizeof payload
    };

    bool ok = ecu_process_frame(&ecu, &frame);

    assert(ok);
    assert(ecu.throttle_permille == 750U);
}

static void test_out_of_range_throttle_is_rejected(void)
{
    EcuState ecu = {.throttle_permille = 500U};
    const uint8_t payload[] = {0x03, 0xE9};
    const EcuFrame frame = {
        .id = ECU_FRAME_ID_THROTTLE,
        .data = payload,
        .length = sizeof payload
    };

    bool ok = ecu_process_frame(&ecu, &frame);

    assert(!ok);
    assert(ecu.throttle_permille == 500U);
}

static void test_unknown_frame_is_rejected(void)
{
    EcuState ecu = {.rpm = 4200U};
    const uint8_t payload[] = {0x0B, 0xB8};
    const EcuFrame frame = {
        .id = 0x07FFU,
        .data = payload,
        .length = sizeof payload
    };

    bool ok = ecu_process_frame(&ecu, &frame);

    assert(!ok);
    assert(ecu.rpm == 4200U);
}

static void test_null_inputs_are_rejected(void)
{
    EcuState ecu = {0};
    const uint8_t payload[] = {0x0B, 0xB8};
    const EcuFrame valid_frame = {
        .id = ECU_FRAME_ID_RPM,
        .data = payload,
        .length = sizeof payload
    };
    const EcuFrame null_payload_frame = {
        .id = ECU_FRAME_ID_RPM,
        .data = NULL,
        .length = 2U
    };

    assert(!ecu_process_frame(NULL, &valid_frame));
    assert(!ecu_process_frame(&ecu, NULL));
    assert(!ecu_process_frame(&ecu, &null_payload_frame));
}

static void test_positive_coolant_temp_frame_updates_state(void)
{
    EcuState ecu = {0};
    const uint8_t payload[] = {0x03, 0x89};
    const EcuFrame frame = {
        .id = ECU_FRAME_ID_COOLANT_TEMP,
        .data = payload,
        .length = sizeof payload
    };

    bool ok = ecu_process_frame(&ecu, &frame);

    assert(ok);
    assert(ecu.coolant_temp_deci_c == 905);
}

static void test_negative_coolant_temp_frame_updates_state(void)
{
    EcuState ecu = {0};
    const uint8_t payload[] = {0xFF, 0x38};
    const EcuFrame frame = {
        .id = ECU_FRAME_ID_COOLANT_TEMP,
        .data = payload,
        .length = sizeof payload
    };

    bool ok = ecu_process_frame(&ecu, &frame);

    assert(ok);
    assert(ecu.coolant_temp_deci_c == -200);
}

static void test_out_of_range_coolant_temp_is_rejected(void)
{
    EcuState ecu = {.coolant_temp_deci_c = 905};
    const uint8_t payload[] = {0x05, 0xDD};
    const EcuFrame frame = {
        .id = ECU_FRAME_ID_COOLANT_TEMP,
        .data = payload,
        .length = sizeof payload
    };

    bool ok = ecu_process_frame(&ecu, &frame);

    assert(!ok);
    assert(ecu.coolant_temp_deci_c == 905);
}

int main(void)
{
    test_known_rpm_frame_updates_state();
    test_short_rpm_frame_is_rejected();
    test_known_throttle_frame_updates_state();
    test_out_of_range_throttle_is_rejected();
    test_unknown_frame_is_rejected();
    test_null_inputs_are_rejected();
    test_positive_coolant_temp_frame_updates_state();
    test_negative_coolant_temp_frame_updates_state();
    test_out_of_range_coolant_temp_is_rejected();
    puts("All tests passed.");
    return 0;
}