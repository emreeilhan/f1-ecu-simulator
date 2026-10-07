#include "ecu_core.h"

EcuControlOutput ecu_policy_compute(const EcuContext *ctx)
{
    EcuControlOutput out = {0};
    if (ctx == NULL || !ctx->initialized) {
        out.reasons = ECU_REASON_NOT_INITIALIZED;
        if (ctx != NULL) out.active_faults = ctx->active_faults;
        return out;
    }
    out.active_faults = ctx->active_faults;
    if (out.active_faults & ECU_MISSING_MASK) out.reasons |= ECU_REASON_MISSING;
    if (out.active_faults & ECU_STALE_MASK) out.reasons |= ECU_REASON_STALE;
    if (out.active_faults & ECU_INVALID_MASK) out.reasons |= ECU_REASON_INTEGRITY;
    if (out.active_faults & ECU_ACTIVE_CLOCK) out.reasons |= ECU_REASON_CLOCK;
    if (out.active_faults & ECU_ACTIVE_API) out.reasons |= ECU_REASON_API;
    const EcuSensor *rpm = &ctx->sensors[ECU_SENSOR_RPM];
    const EcuSensor *coolant = &ctx->sensors[ECU_SENSOR_COOLANT];
    if (rpm->has_valid_value && !(out.active_faults & ECU_ACTIVE_STALE_RPM) &&
        rpm->value >= ECU_RPM_LIMIT) out.reasons |= ECU_REASON_RPM_LIMIT;
    if (coolant->has_valid_value && !(out.active_faults & ECU_ACTIVE_STALE_COOLANT) &&
        coolant->value >= ECU_COOLANT_DERATE_START_DECI_C)
        out.reasons |= ECU_REASON_THERMAL_DERATE;
    if (out.active_faults != 0U || (out.reasons & ECU_REASON_RPM_LIMIT)) return out;
    out.throttle_command_permille = (uint16_t)ctx->sensors[ECU_SENSOR_THROTTLE].value;
    if ((out.reasons & ECU_REASON_THERMAL_DERATE) &&
        out.throttle_command_permille > ECU_DERATED_THROTTLE_MAX_PERMILLE)
        out.throttle_command_permille = ECU_DERATED_THROTTLE_MAX_PERMILLE;
    return out;
}
