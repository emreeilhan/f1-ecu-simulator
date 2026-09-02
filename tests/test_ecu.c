#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "ecu.h"

static void test_valid_rpm_frame(void)
{
    EcuState ecu = {0};
    const uint8_t frame[] = {0x0B, 0xB8};

    bool ok = ecu_update_rpm(&ecu, frame, sizeof frame);

    assert(ok);
    assert(ecu.rpm == 3000U);
}

static void test_short_frame_is_rejected(void)
{
    EcuState ecu = {.rpm = 4200U};
    const uint8_t frame[] = {0x0B};

    bool ok = ecu_update_rpm(&ecu, frame, sizeof frame);

    assert(!ok);
    assert(ecu.rpm == 4200U);
}

static void test_null_inputs_are_rejected(void)
{
    EcuState ecu = {0};
    const uint8_t frame[] = {0x0B, 0xB8};

    assert(!ecu_update_rpm(NULL, frame, sizeof frame));
    assert(!ecu_update_rpm(&ecu, NULL, 2U));
}

int main(void)
{
    test_valid_rpm_frame();
    test_short_frame_is_rejected();
    test_null_inputs_are_rejected();

    puts("All tests passed.");
    return 0;
}