#include "ecu_core.h"
#include <string.h>

static void increment(uint64_t *value, uint64_t amount)
{
    *value = UINT64_MAX - *value < amount ? UINT64_MAX : *value + amount;
}

EcuConfig ecu_default_config(void)
{
    const EcuConfig config = {{500U, 500U, 500U}};
    return config;
}

bool ecu_init(EcuContext *ctx, const EcuConfig *config, uint64_t start_ms)
{
    if (ctx == NULL) return false;
    /* Copy first: reinitialization may pass &ctx->config. */
    EcuConfig chosen = {{0}};
    if (config != NULL) chosen = *config;
    memset(ctx, 0, sizeof *ctx);
    if (config == NULL || chosen.timeout_ms[0] == 0U ||
        chosen.timeout_ms[1] == 0U || chosen.timeout_ms[2] == 0U) {
        ctx->latched_faults = ECU_ACTIVE_CONFIG;
        ctx->active_faults = ECU_ACTIVE_CONFIG;
        ctx->fault_history = ECU_ACTIVE_CONFIG;
        return false;
    }
    ctx->config = chosen;
    ctx->start_ms = start_ms;
    ctx->last_now_ms = start_ms;
    ctx->initialized = true;
    (void)ecu_tick(ctx, start_ms);
    return true;
}

bool ecu_tick(EcuContext *ctx, uint64_t now_ms)
{
    if (ctx == NULL || !ctx->initialized) return false;
    if ((ctx->latched_faults & ECU_ACTIVE_CLOCK) != 0U ||
        !ecu_time_ordered(ctx->start_ms, ctx->last_now_ms, now_ms)) {
        ctx->latched_faults |= ECU_ACTIVE_CLOCK;
        ctx->active_faults |= ECU_ACTIVE_CLOCK;
        ctx->fault_history |= ECU_ACTIVE_CLOCK;
        return false;
    }
    ctx->last_now_ms = now_ms;
    uint32_t active = ctx->latched_faults;
    for (unsigned i = 0; i < ECU_SENSOR_COUNT; ++i) {
        const EcuSensor *s = &ctx->sensors[i];
        if (!s->has_valid_value) active |= 1U << i;
        else if (ecu_time_stale(s->last_valid_received_ms, now_ms,
                               ctx->config.timeout_ms[i])) active |= 1U << (i + 3U);
        if (s->integrity_fault) active |= 1U << (i + 6U);
    }
    ctx->active_faults = active;
    ctx->fault_history |= active;
    return true;
}

static EcuResult reject(EcuContext *ctx, EcuSensorId sensor, EcuResult result,
                         uint64_t now_ms)
{
    EcuSensor *s = &ctx->sensors[sensor];
    s->integrity_fault = true;
    s->last_rejection = result;
    increment(&s->rejected_by_result[result], 1U);
    increment(&ctx->rejected_frames, 1U);
    (void)ecu_tick(ctx, now_ms);
    return result;
}

EcuResult ecu_ingest_frame(EcuContext *ctx, const EcuFrame *frame,
                           uint64_t received_ms, uint64_t now_ms)
{
    EcuSensorId sensor;
    EcuDecoded decoded;
    if (ctx == NULL) return ECU_REJECT_NULL;
    if (!ctx->initialized) return ECU_REJECT_NOT_INITIALIZED;
    if (!ecu_tick(ctx, now_ms)) {
        increment(&ctx->rejected_frames, 1U);
        return ECU_REJECT_CLOCK;
    }
    if (frame == NULL) {
        ctx->latched_faults |= ECU_ACTIVE_API;
        ctx->active_faults |= ECU_ACTIVE_API;
        ctx->fault_history |= ECU_ACTIVE_API;
        increment(&ctx->rejected_frames, 1U);
        return ECU_REJECT_NULL;
    }
    if (!ecu_sensor_for_id(frame->id, &sensor)) {
        increment(&ctx->unknown_frames, 1U);
        increment(&ctx->rejected_frames, 1U);
        ctx->fault_history |= ECU_HISTORY_UNKNOWN_ID;
        return ECU_REJECT_ID;
    }
    const EcuResult protocol = ecu_protocol_decode(frame, &decoded);
    if (!ecu_result_accepted(protocol)) return reject(ctx, sensor, protocol, now_ms);
    EcuSensor *s = &ctx->sensors[sensor];
    if (received_ms < ctx->start_ms || received_ms > now_ms ||
        (s->has_receive_timestamp && received_ms < s->last_valid_received_ms))
        return reject(ctx, sensor, ECU_REJECT_TIMESTAMP, now_ms);
    if (ecu_time_stale(received_ms, now_ms, ctx->config.timeout_ms[sensor]))
        return reject(ctx, sensor, ECU_REJECT_STALE, now_ms);

    uint8_t delta = 1U;
    if (s->counter_initialized) {
        delta = (uint8_t)((unsigned)decoded.counter - (unsigned)s->counter);
        if (delta == 0U) return reject(ctx, sensor, ECU_REJECT_DUPLICATE, now_ms);
        if (delta >= 128U) return reject(ctx, sensor, ECU_REJECT_COUNTER, now_ms);
    }
    /* All validation completed: publish value, timestamp and counter together. */
    s->value = decoded.value;
    s->last_valid_received_ms = received_ms;
    s->counter = decoded.counter;
    s->has_valid_value = true;
    s->has_receive_timestamp = true;
    s->counter_initialized = true;
    s->integrity_fault = false;
    increment(&s->accepted_frames, 1U);
    if (delta > 1U) {
        increment(&s->missed_counter_steps, (uint64_t)delta - 1U);
        ctx->fault_history |= ECU_HISTORY_COUNTER_GAP;
    }
    (void)ecu_tick(ctx, now_ms);
    return delta > 1U ? ECU_ACCEPTED_GAP : ECU_ACCEPTED;
}

EcuControlOutput ecu_compute_command_at(EcuContext *ctx, uint64_t now_ms)
{
    (void)ecu_tick(ctx, now_ms);
    return ecu_policy_compute(ctx);
}

void ecu_clear_fault_history(EcuContext *ctx)
{
    if (ctx != NULL) ctx->fault_history = 0U;
}

bool ecu_resync_stream(EcuContext *ctx, uint16_t id)
{
    EcuSensorId sensor;
    if (ctx == NULL || !ctx->initialized ||
        (ctx->latched_faults & ECU_ACTIVE_CLOCK) != 0U ||
        !ecu_sensor_for_id(id, &sensor))
        return false;
    ctx->sensors[sensor].has_valid_value = false;
    ctx->sensors[sensor].counter_initialized = false;
    ctx->sensors[sensor].integrity_fault = false;
    (void)ecu_tick(ctx, ctx->last_now_ms);
    return true;
}

void ecu_note_transport_error(EcuContext *ctx)
{
    if (ctx != NULL && ctx->initialized) {
        ctx->fault_history |= ECU_HISTORY_TRANSPORT;
        increment(&ctx->transport_errors, 1U);
    }
}
