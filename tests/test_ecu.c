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
    assert(ecu.fault_flags == ECU_FAULT_NONE);
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
    assert((ecu.fault_flags & ECU_FAULT_RPM_FRAME_INVALID) != 0U);
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
    assert(ecu.fault_flags == ECU_FAULT_NONE);
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
    assert((ecu.fault_flags & ECU_FAULT_THROTTLE_FRAME_INVALID) != 0U);
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
    assert(ecu.fault_flags == ECU_FAULT_NONE);
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
    assert(ecu.fault_flags == ECU_FAULT_NONE);
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
    assert((ecu.fault_flags & ECU_FAULT_COOLANT_TEMP_FRAME_INVALID) != 0U);
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
    assert((ecu.fault_flags & ECU_FAULT_UNKNOWN_FRAME_ID) != 0U);
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
    bool ok;

    assert(!ecu_process_frame(NULL, &valid_frame));
    assert(!ecu_process_frame(&ecu, NULL));

    ok = ecu_process_frame(&ecu, &null_payload_frame);

    assert(!ok);
    assert((ecu.fault_flags & ECU_FAULT_RPM_FRAME_INVALID) != 0U);
}
static void test_multiple_faults_are_preserved(void)
{
    EcuState ecu = {0};
    const uint8_t invalid_throttle_payload[] = {0x03, 0xE9};
    const EcuFrame invalid_throttle_frame = {
        .id = ECU_FRAME_ID_THROTTLE,
        .data = invalid_throttle_payload,
        .length = sizeof invalid_throttle_payload
    };
    const uint8_t unknown_payload[] = {0x00, 0x00};
    const EcuFrame unknown_frame = {
        .id = 0x07FFU,
        .data = unknown_payload,
        .length = sizeof unknown_payload
    };

    assert(!ecu_process_frame(&ecu, &invalid_throttle_frame));
    assert(!ecu_process_frame(&ecu, &unknown_frame));

    assert((ecu.fault_flags & ECU_FAULT_THROTTLE_FRAME_INVALID) != 0U);
    assert((ecu.fault_flags & ECU_FAULT_UNKNOWN_FRAME_ID) != 0U);
}

static void test_faults_can_be_cleared(void)
{
    EcuState ecu = {
        .fault_flags = ECU_FAULT_RPM_FRAME_INVALID |
                       ECU_FAULT_UNKNOWN_FRAME_ID
    };

    ecu_clear_faults(&ecu);

    assert(ecu.fault_flags == ECU_FAULT_NONE);
}
static void test_nominal_command_matches_throttle_request(void)
{
    EcuState ecu = {
        .rpm = 12000U,
        .throttle_permille = 750U,
        .coolant_temp_deci_c = 900
    };

    EcuCommand command = ecu_compute_command(&ecu);

    assert(command.throttle_command_permille == 750U);
}

static void test_rpm_limiter_closes_throttle(void)
{
    EcuState ecu = {
        .rpm = ECU_RPM_LIMIT,
        .throttle_permille = 750U,
        .coolant_temp_deci_c = 900
    };

    EcuCommand command = ecu_compute_command(&ecu);

    assert(command.throttle_command_permille == 0U);
}

static void test_hot_coolant_derates_throttle(void)
{
    EcuState ecu = {
        .rpm = 12000U,
        .throttle_permille = 800U,
        .coolant_temp_deci_c = ECU_COOLANT_DERATE_START_DECI_C
    };

    EcuCommand command = ecu_compute_command(&ecu);

    assert(command.throttle_command_permille ==
           ECU_DERATED_THROTTLE_MAX_PERMILLE);
}

static void test_fault_forces_safe_throttle_command(void)
{
    EcuState ecu = {
        .rpm = 12000U,
        .throttle_permille = 750U,
        .coolant_temp_deci_c = 900,
        .fault_flags = ECU_FAULT_THROTTLE_FRAME_INVALID
    };

    EcuCommand command = ecu_compute_command(&ecu);

    assert(command.throttle_command_permille == 0U);
}

static void test_null_state_returns_safe_command(void)
{
    EcuCommand command = ecu_compute_command(NULL);

    assert(command.throttle_command_permille == 0U);
}

int main(void)
{
    test_known_rpm_frame_updates_state();
    test_short_rpm_frame_is_rejected();
    test_known_throttle_frame_updates_state();
    test_out_of_range_throttle_is_rejected();
    test_positive_coolant_temp_frame_updates_state();
    test_negative_coolant_temp_frame_updates_state();
    test_out_of_range_coolant_temp_is_rejected();
    test_unknown_frame_is_rejected();
    test_null_inputs_are_rejected();
    test_multiple_faults_are_preserved();
    test_faults_can_be_cleared();
    test_nominal_command_matches_throttle_request();
    test_rpm_limiter_closes_throttle();
    test_hot_coolant_derates_throttle();
    test_fault_forces_safe_throttle_command();
    test_null_state_returns_safe_command();
    puts("All tests passed.");
    return 0;
}