#include "ecu_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned tests_run;
#define RUN(test) do { test(); ++tests_run; printf("PASS: %s\n", #test); } while (0)
static const uint16_t ids[] = {ECU_FRAME_ID_RPM, ECU_FRAME_ID_THROTTLE,
                               ECU_FRAME_ID_COOLANT_TEMP};

static EcuContext context(uint64_t start)
{
    EcuContext ctx;
    EcuConfig config = ecu_default_config();
    assert(ecu_init(&ctx, &config, start));
    return ctx;
}

static EcuResult send_raw(EcuContext *ctx, unsigned sensor, int32_t value,
                           uint8_t counter, uint64_t received, uint64_t now)
{
    const uint16_t raw = (uint16_t)value;
    uint8_t p[] = {(uint8_t)(raw >> 8U), (uint8_t)raw, counter, 0};
    assert(ecu_protocol_set_crc(ids[sensor], p));
    const EcuFrame frame = {ids[sensor], p, sizeof p};
    return ecu_ingest_frame(ctx, &frame, received, now);
}

static void nominal(EcuContext *ctx, uint8_t counter, uint64_t now)
{
    assert(ecu_result_accepted(send_raw(ctx, 0, 3000, counter, now, now)));
    assert(ecu_result_accepted(send_raw(ctx, 1, 750, counter, now, now)));
    assert(ecu_result_accepted(send_raw(ctx, 2, 905, counter, now, now)));
}

static void same_measurement(const EcuSensor *a, const EcuSensor *b)
{
    assert(a->value == b->value);
    assert(a->has_valid_value == b->has_valid_value);
    assert(a->has_receive_timestamp == b->has_receive_timestamp);
    assert(a->counter_initialized == b->counter_initialized);
    assert(a->counter == b->counter);
    assert(a->last_valid_received_ms == b->last_valid_received_ms);
}

static void test_initial_missing_and_throttle_only(void)
{
    EcuContext ctx = context(1000);
    assert(ctx.active_faults == ECU_MISSING_MASK);
    assert(ecu_compute_command_at(&ctx, 1000).reasons & ECU_REASON_MISSING);
    assert(send_raw(&ctx, 1, 750, 42, 1000, 1000) == ECU_ACCEPTED);
    assert(ctx.active_faults == (ECU_ACTIVE_MISSING_RPM | ECU_ACTIVE_MISSING_COOLANT));
    assert(ecu_compute_command_at(&ctx, 1000).throttle_command_permille == 0);
}

static void test_age_499_500_501_and_refresh(void)
{
    EcuContext ctx = context(1000);
    nominal(&ctx, 10, 1000);
    assert(ecu_compute_command_at(&ctx, 1499).throttle_command_permille == 750);
    assert(ecu_compute_command_at(&ctx, 1500).throttle_command_permille == 0);
    assert(ctx.active_faults == ECU_STALE_MASK);
    assert(ecu_compute_command_at(&ctx, 1501).reasons & ECU_REASON_STALE);
    nominal(&ctx, 11, 1501);
    assert(ecu_compute_command_at(&ctx, 1501).throttle_command_permille == 750);
    assert(ctx.fault_history & ECU_STALE_MASK);
}

static void test_independent_timeout_config(void)
{
    EcuContext ctx;
    const EcuConfig config = {{1, 500, 1000}};
    assert(ecu_init(&ctx, &config, 0));
    nominal(&ctx, 0, 0);
    assert(ecu_tick(&ctx, 1));
    assert(ctx.active_faults == ECU_ACTIVE_STALE_RPM);
    assert(ecu_tick(&ctx, 500));
    assert(ctx.active_faults == (ECU_ACTIVE_STALE_RPM | ECU_ACTIVE_STALE_THROTTLE));
}

static void test_one_channel_stale(void)
{
    EcuContext ctx = context(0);
    nominal(&ctx, 1, 0);
    assert(send_raw(&ctx, 0, 3000, 2, 499, 499) == ECU_ACCEPTED);
    assert(send_raw(&ctx, 1, 750, 2, 499, 499) == ECU_ACCEPTED);
    assert(ecu_tick(&ctx, 500));
    assert(ctx.active_faults == ECU_ACTIVE_STALE_COOLANT);
}

static void test_invalid_config_disables_and_init_alias(void)
{
    EcuContext ctx = context(10);
    nominal(&ctx, 0, 10);
    assert(ecu_init(&ctx, &ctx.config, 20));
    assert(ctx.config.timeout_ms[0] == 500 && ctx.active_faults == ECU_MISSING_MASK);
    for (unsigned i = 0; i < ECU_SENSOR_COUNT; ++i) {
        EcuConfig c = ecu_default_config(); c.timeout_ms[i] = 0;
        assert(!ecu_init(&ctx, &c, 30));
        assert(!ctx.initialized && ctx.active_faults == ECU_ACTIVE_CONFIG);
        assert(ecu_compute_command_at(&ctx, 30).throttle_command_permille == 0);
    }
    assert(!ecu_init(&ctx, NULL, 0));
    EcuConfig c = ecu_default_config();
    assert(!ecu_init(NULL, &c, 0));
}

static void test_crc_reference_and_protocol_encoder_atomicity(void)
{
    const uint8_t check[] = "123456789";
    uint8_t crc = 99;
    assert(ecu_crc8_smbus(check, 9, &crc) && crc == 0xF4);
    assert(ecu_crc8_smbus(NULL, 0, &crc) && crc == 0);
    crc = 99;
    assert(!ecu_crc8_smbus(NULL, 1, &crc) && crc == 99);
    assert(!ecu_crc8_smbus(check, 9, NULL));
    uint8_t output[] = {0xAA, 0xAA, 0xAA, 0xAA};
    const uint8_t before[] = {0xAA, 0xAA, 0xAA, 0xAA};
    assert(!ecu_protocol_encode(0x101, 1001, 0, output));
    assert(!ecu_protocol_encode(0x100, -1, 0, output));
    assert(!ecu_protocol_encode(0x100, 65536, 0, output));
    assert(!ecu_protocol_encode(0x999, 0, 0, output));
    assert(!ecu_protocol_encode(0x100, 0, 0, NULL));
    assert(!ecu_protocol_set_crc(0x100, NULL));
    assert(memcmp(before, output, sizeof output) == 0);
    assert(ecu_protocol_encode(0x102, -400, 255, output));
    const EcuFrame f = {0x102, output, sizeof output}; EcuDecoded decoded;
    assert(ecu_protocol_decode(&f, &decoded) == ECU_ACCEPTED);
    assert(decoded.value == -400 && decoded.counter == 255);
}

static void test_protocol_length_null_and_decode_output_preservation(void)
{
    uint8_t p[5] = {0};
    assert(ecu_protocol_set_crc(0x100, p));
    EcuDecoded decoded = {ECU_SENSOR_THROTTLE, 123, 20};
    const EcuDecoded before = decoded;
    for (size_t n = 0; n < 9; ++n) {
        if (n == 4) continue;
        EcuContext ctx = context(0); nominal(&ctx, 0, 0);
        const EcuSensor sensor = ctx.sensors[0];
        const EcuFrame f = {0x100, p, n};
        assert(ecu_ingest_frame(&ctx, &f, 1, 1) == ECU_REJECT_LENGTH);
        same_measurement(&sensor, &ctx.sensors[0]);
        assert(ecu_protocol_decode(&f, &decoded) == ECU_REJECT_LENGTH);
        assert(decoded.value == before.value && decoded.counter == before.counter);
    }
    const EcuFrame null_data = {0x100, NULL, 4};
    assert(ecu_protocol_decode(&null_data, &decoded) == ECU_REJECT_NULL);
    assert(ecu_protocol_decode(NULL, &decoded) == ECU_REJECT_NULL);
    const EcuFrame f = {0x100, p, 4};
    assert(ecu_protocol_decode(&f, NULL) == ECU_REJECT_NULL);
}

static void test_all_payload_single_bit_corruptions_preserve_measurement(void)
{
    uint8_t original[4];
    assert(ecu_protocol_encode(0x101, 800, 11, original));
    for (unsigned bit = 0; bit < 32; ++bit) {
        EcuContext ctx = context(0); nominal(&ctx, 10, 0);
        const EcuSensor before = ctx.sensors[1];
        uint8_t p[4]; memcpy(p, original, sizeof p);
        p[bit / 8U] ^= (uint8_t)(1U << (bit % 8U));
        const EcuFrame f = {0x101, p, 4};
        assert(ecu_ingest_frame(&ctx, &f, 490, 490) == ECU_REJECT_CRC);
        same_measurement(&before, &ctx.sensors[1]);
        assert(ecu_compute_command_at(&ctx, 500).throttle_command_permille == 0);
        assert(ctx.active_faults & ECU_ACTIVE_STALE_THROTTLE);
    }
}

static void test_id_is_covered_by_crc(void)
{
    EcuContext ctx = context(0); nominal(&ctx, 1, 0);
    uint8_t p[4]; assert(ecu_protocol_encode(0x100, 750, 2, p));
    const EcuFrame f = {0x101, p, 4};
    assert(ecu_ingest_frame(&ctx, &f, 1, 1) == ECU_REJECT_CRC);
}

static void test_range_boundaries_and_signed_temperature(void)
{
    const int32_t values[] = {0, 1000, 1001, -400, -401, 1500, 1501};
    for (unsigned i = 0; i < 7; ++i) {
        const unsigned sensor = i < 3 ? 1 : 2;
        const bool valid = i != 2 && i != 4 && i != 6;
        EcuContext ctx = context(0); nominal(&ctx, 10, 0);
        const EcuSensor before = ctx.sensors[sensor];
        EcuResult result = send_raw(&ctx, sensor, values[i], 11, 1, 1);
        assert(result == (valid ? ECU_ACCEPTED : ECU_REJECT_RANGE));
        if (valid) assert(ctx.sensors[sensor].value == values[i]);
        else same_measurement(&before, &ctx.sensors[sensor]);
    }
    EcuContext ctx = context(0);
    assert(send_raw(&ctx, 0, 65535, 0, 0, 0) == ECU_ACCEPTED);
    assert(ctx.sensors[0].value == 65535);
}

static void test_duplicate_old_and_half_range_counters(void)
{
    const uint8_t counters[] = {10, 9, 138, 137};
    for (unsigned i = 0; i < 4; ++i) {
        EcuContext ctx = context(0); nominal(&ctx, 10, 0);
        EcuSensor before = ctx.sensors[1];
        EcuResult result = send_raw(&ctx, 1, 900, counters[i], 1, 1);
        if (i == 3) {
            assert(result == ECU_ACCEPTED_GAP);
            assert(ctx.sensors[1].missed_counter_steps == 126);
        } else {
            assert(result == (i == 0 ? ECU_REJECT_DUPLICATE : ECU_REJECT_COUNTER));
            same_measurement(&before, &ctx.sensors[1]);
        }
    }
}

static void test_counter_wrap_gap_independence_and_rejected_baseline(void)
{
    EcuContext ctx = context(0);
    assert(send_raw(&ctx, 0, 3000, 255, 0, 0) == ECU_ACCEPTED);
    assert(send_raw(&ctx, 1, 750, 90, 0, 0) == ECU_ACCEPTED);
    assert(send_raw(&ctx, 2, 900, 7, 0, 0) == ECU_ACCEPTED);
    assert(send_raw(&ctx, 0, 3000, 0, 1, 1) == ECU_ACCEPTED);
    assert(ctx.sensors[1].counter == 90 && ctx.sensors[2].counter == 7);
    assert(send_raw(&ctx, 1, 1001, 91, 2, 2) == ECU_REJECT_RANGE);
    assert(ctx.sensors[1].counter == 90);
    assert(send_raw(&ctx, 1, 800, 92, 3, 3) == ECU_ACCEPTED_GAP);
    assert(ctx.sensors[1].missed_counter_steps == 1);
    assert(ctx.fault_history & ECU_HISTORY_COUNTER_GAP);
    assert(ecu_compute_command_at(&ctx, 3).throttle_command_permille == 800);
}

static void test_received_timestamp_and_queue_age(void)
{
    const uint64_t received[] = {99, 601, 499, 100, 101};
    const EcuResult expected[] = {ECU_REJECT_TIMESTAMP, ECU_REJECT_TIMESTAMP,
                                  ECU_REJECT_TIMESTAMP, ECU_REJECT_TIMESTAMP,
                                  ECU_REJECT_TIMESTAMP};
    for (unsigned i = 0; i < 5; ++i) {
        EcuContext ctx = context(100); nominal(&ctx, 10, 500);
        const EcuSensor before = ctx.sensors[1];
        assert(send_raw(&ctx, 1, 800, 11, received[i], 600) == expected[i]);
        same_measurement(&before, &ctx.sensors[1]);
    }
    EcuContext ctx = context(0);
    assert(send_raw(&ctx, 1, 800, 10, 0, 499) == ECU_ACCEPTED);
    assert(ctx.sensors[1].last_valid_received_ms == 0);
    EcuContext old = context(0);
    assert(send_raw(&old, 1, 800, 10, 0, 500) == ECU_REJECT_STALE);
    assert(!old.sensors[1].has_valid_value && !old.sensors[1].counter_initialized);
    assert(old.sensors[1].last_valid_received_ms == 0);
    /* Equal receive time with a new sequence is valid; time never regresses. */
    assert(send_raw(&ctx, 1, 900, 11, 0, 499) == ECU_ACCEPTED);
}

static void test_clock_rollback_is_latched_and_sensor_atomic(void)
{
    EcuContext ctx = context(1000); nominal(&ctx, 10, 1000);
    assert(ecu_tick(&ctx, 1100));
    const EcuSensor before = ctx.sensors[1];
    assert(send_raw(&ctx, 1, 900, 11, 1099, 1099) == ECU_REJECT_CLOCK);
    same_measurement(&before, &ctx.sensors[1]);
    assert(ctx.last_now_ms == 1100);
    assert(ctx.rejected_frames == 1);
    assert(!ecu_resync_stream(&ctx, 0x101));
    same_measurement(&before, &ctx.sensors[1]);
    assert(ecu_compute_command_at(&ctx, 1101).reasons & ECU_REASON_CLOCK);
    ecu_clear_fault_history(&ctx);
    assert(!ecu_tick(&ctx, 1101));
    assert(ctx.active_faults & ECU_ACTIVE_CLOCK);
    assert(ctx.fault_history & ECU_ACTIVE_CLOCK);
    EcuConfig c = ecu_default_config();
    assert(ecu_init(&ctx, &c, 1101));
    nominal(&ctx, 0, 1101);
    assert(ecu_compute_command_at(&ctx, 1101).throttle_command_permille == 750);
}

static void test_uint64_boundaries_do_not_wrap(void)
{
    EcuContext ctx = context(UINT64_MAX - 500U);
    nominal(&ctx, 255, UINT64_MAX - 500U);
    assert(ecu_compute_command_at(&ctx, UINT64_MAX - 1U).throttle_command_permille == 750);
    assert(ecu_compute_command_at(&ctx, UINT64_MAX).throttle_command_permille == 0);
    assert(!ecu_tick(&ctx, 0));
    assert(ctx.active_faults & ECU_ACTIVE_CLOCK);
    assert(!ecu_time_ordered(100, 110, 109));
    assert(ecu_time_stale(100, 99, 500));
    assert(ecu_time_stale(0, 0, 0));
}

static void test_active_recovery_history_clear_and_resync(void)
{
    EcuContext ctx = context(0); nominal(&ctx, 10, 0);
    assert(send_raw(&ctx, 1, 1001, 11, 1, 1) == ECU_REJECT_RANGE);
    ecu_clear_fault_history(&ctx);
    assert(ctx.fault_history == 0);
    assert(ctx.active_faults & ECU_ACTIVE_INVALID_THROTTLE);
    assert(ecu_compute_command_at(&ctx, 1).throttle_command_permille == 0);
    assert(send_raw(&ctx, 1, 800, 11, 2, 2) == ECU_ACCEPTED);
    assert(!(ctx.active_faults & ECU_ACTIVE_INVALID_THROTTLE));
    assert(ctx.fault_history & ECU_ACTIVE_INVALID_THROTTLE);
    ecu_clear_fault_history(&ctx);
    assert(ctx.sensors[1].rejected_by_result[ECU_REJECT_RANGE] == 1);
    const EcuSensor before = ctx.sensors[1];
    assert(ecu_resync_stream(&ctx, 0x101));
    assert(!ctx.sensors[1].has_valid_value && !ctx.sensors[1].counter_initialized);
    assert(ctx.sensors[1].value == before.value);
    assert(ctx.sensors[1].last_valid_received_ms == before.last_valid_received_ms);
    assert(ecu_compute_command_at(&ctx, 2).throttle_command_permille == 0);
    assert(send_raw(&ctx, 1, 900, 200, 1, 3) == ECU_REJECT_TIMESTAMP);
    assert(ctx.sensors[1].last_valid_received_ms == before.last_valid_received_ms);
    assert(ctx.sensors[1].value == before.value);
    assert(!ctx.sensors[1].has_valid_value && !ctx.sensors[1].counter_initialized);
    assert(send_raw(&ctx, 1, 800, 200, 3, 3) == ECU_ACCEPTED);
    assert(ecu_compute_command_at(&ctx, 3).throttle_command_permille == 800);
    assert(!ecu_resync_stream(&ctx, 0x999));
    assert(!ecu_resync_stream(NULL, 0x100));
}

static void test_unknown_diagnostic_and_transport_dont_open_or_block_command(void)
{
    EcuContext ctx = context(0); nominal(&ctx, 10, 0);
    const EcuFrame unknown = {0x999, NULL, 0};
    assert(ecu_ingest_frame(&ctx, &unknown, 1, 1) == ECU_REJECT_ID);
    ecu_note_transport_error(&ctx);
    assert(ctx.unknown_frames == 1 && ctx.transport_errors == 1);
    assert(ctx.fault_history & ECU_HISTORY_UNKNOWN_ID);
    assert(ctx.fault_history & ECU_HISTORY_TRANSPORT);
    assert(ecu_compute_command_at(&ctx, 1).throttle_command_permille == 750);
    assert(ecu_compute_command_at(&ctx, 500).throttle_command_permille == 0);
    ecu_note_transport_error(NULL);
}

static void test_null_api_uninitialized_and_latched_programming_error(void)
{
    EcuContext ctx = {0}; const EcuFrame f = {0x100, NULL, 0};
    assert(ecu_ingest_frame(&ctx, &f, 0, 0) == ECU_REJECT_NOT_INITIALIZED);
    assert(ecu_ingest_frame(NULL, &f, 0, 0) == ECU_REJECT_NULL);
    assert(!ecu_tick(NULL, 0));
    assert(!ecu_tick(&ctx, 0));
    assert(!ecu_resync_stream(&ctx, 0x100));
    ecu_clear_fault_history(NULL);
    assert(ecu_compute_command_at(NULL, 0).throttle_command_permille == 0);
    ctx = context(0); nominal(&ctx, 0, 0);
    const EcuSensor before = ctx.sensors[0];
    assert(ecu_ingest_frame(&ctx, NULL, 1, 1) == ECU_REJECT_NULL);
    same_measurement(&before, &ctx.sensors[0]);
    ecu_clear_fault_history(&ctx);
    assert(ecu_compute_command_at(&ctx, 1).reasons & ECU_REASON_API);
    EcuContext payload = context(0); nominal(&payload, 0, 0);
    assert(ecu_ingest_frame(&payload, &f, 1, 1) == ECU_REJECT_NULL);
    assert(payload.active_faults & ECU_ACTIVE_INVALID_RPM);
    assert(send_raw(&payload, 0, 3000, 1, 1, 1) == ECU_ACCEPTED);
    assert(payload.active_faults == 0);
}

static void test_policy_thresholds_multiple_reasons_and_range(void)
{
    EcuContext ctx = context(0); nominal(&ctx, 0, 0);
    assert(send_raw(&ctx, 0, 14999, 1, 1, 1) == ECU_ACCEPTED);
    assert(send_raw(&ctx, 2, 1099, 1, 1, 1) == ECU_ACCEPTED);
    assert(ecu_compute_command_at(&ctx, 1).throttle_command_permille == 750);
    assert(send_raw(&ctx, 2, 1100, 2, 2, 2) == ECU_ACCEPTED);
    EcuControlOutput out = ecu_compute_command_at(&ctx, 2);
    assert(out.throttle_command_permille == 500 && out.reasons & ECU_REASON_THERMAL_DERATE);
    assert(send_raw(&ctx, 1, 200, 1, 3, 3) == ECU_ACCEPTED);
    assert(ecu_compute_command_at(&ctx, 3).throttle_command_permille == 200);
    assert(send_raw(&ctx, 0, 15000, 2, 3, 3) == ECU_ACCEPTED);
    out = ecu_compute_command_at(&ctx, 3);
    assert(out.throttle_command_permille == 0);
    assert(out.reasons & ECU_REASON_RPM_LIMIT);
    assert(out.reasons & ECU_REASON_THERMAL_DERATE);
    assert(send_raw(&ctx, 1, 1001, 2, 4, 4) == ECU_REJECT_RANGE);
    assert(ecu_compute_command_at(&ctx, 4).reasons & ECU_REASON_INTEGRITY);
    for (int32_t value = 0; value <= 1000; ++value) {
        EcuContext x = context(0); nominal(&x, 0, 0);
        assert(send_raw(&x, 1, value, 1, 0, 0) == ECU_ACCEPTED);
        assert(ecu_compute_command_at(&x, 0).throttle_command_permille == value);
    }
}

static void test_saturating_statistics_and_result_names(void)
{
    EcuContext ctx = context(0); nominal(&ctx, 10, 0);
    ctx.sensors[1].accepted_frames = UINT64_MAX;
    ctx.sensors[1].missed_counter_steps = UINT64_MAX;
    assert(send_raw(&ctx, 1, 800, 12, 1, 1) == ECU_ACCEPTED_GAP);
    assert(ctx.sensors[1].accepted_frames == UINT64_MAX);
    assert(ctx.sensors[1].missed_counter_steps == UINT64_MAX);
    ctx.sensors[1].rejected_by_result[ECU_REJECT_DUPLICATE] = UINT64_MAX;
    ctx.rejected_frames = UINT64_MAX;
    assert(send_raw(&ctx, 1, 800, 12, 2, 2) == ECU_REJECT_DUPLICATE);
    assert(ctx.rejected_frames == UINT64_MAX);
    assert(ctx.sensors[1].rejected_by_result[ECU_REJECT_DUPLICATE] == UINT64_MAX);
    assert(strcmp(ecu_result_name(ECU_REJECT_CRC), "crc") == 0);
    assert(strcmp(ecu_result_name((EcuResult)-1), "unknown-result") == 0);
    EcuSensorId s = ECU_SENSOR_THROTTLE;
    assert(!ecu_sensor_for_id(0x999, &s) && s == ECU_SENSOR_THROTTLE);
    assert(!ecu_sensor_for_id(0x100, NULL));
}

int main(void)
{
    RUN(test_initial_missing_and_throttle_only);
    RUN(test_age_499_500_501_and_refresh);
    RUN(test_independent_timeout_config);
    RUN(test_one_channel_stale);
    RUN(test_invalid_config_disables_and_init_alias);
    RUN(test_crc_reference_and_protocol_encoder_atomicity);
    RUN(test_protocol_length_null_and_decode_output_preservation);
    RUN(test_all_payload_single_bit_corruptions_preserve_measurement);
    RUN(test_id_is_covered_by_crc);
    RUN(test_range_boundaries_and_signed_temperature);
    RUN(test_duplicate_old_and_half_range_counters);
    RUN(test_counter_wrap_gap_independence_and_rejected_baseline);
    RUN(test_received_timestamp_and_queue_age);
    RUN(test_clock_rollback_is_latched_and_sensor_atomic);
    RUN(test_uint64_boundaries_do_not_wrap);
    RUN(test_active_recovery_history_clear_and_resync);
    RUN(test_unknown_diagnostic_and_transport_dont_open_or_block_command);
    RUN(test_null_api_uninitialized_and_latched_programming_error);
    RUN(test_policy_thresholds_multiple_reasons_and_range);
    RUN(test_saturating_statistics_and_result_names);
    printf("Core: %u named test functions passed.\n", tests_run);
    return 0;
}
