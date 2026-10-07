#include "ecu_scenarios.h"
#include "ecu_core.h"
#include <inttypes.h>

static EcuResult send(EcuContext *ctx, uint16_t id, int32_t value,
                       uint8_t counter, uint64_t received, uint64_t now,
                       bool bitflip, uint32_t *seed)
{
    const uint16_t raw = (uint16_t)value;
    uint8_t p[] = {(uint8_t)(raw >> 8U), (uint8_t)raw, counter, 0};
    (void)ecu_protocol_set_crc(id, p);
    if (bitflip) {
        *seed = *seed * UINT32_C(1664525) + UINT32_C(1013904223);
        p[*seed % 3U] ^= 1U;
    }
    const EcuFrame frame = {id, p, sizeof p};
    return ecu_ingest_frame(ctx, &frame, received, now);
}

static void report(FILE *out, EcuContext *ctx, uint64_t now,
                     const char *event, EcuResult result)
{
    EcuControlOutput command = ecu_compute_command_at(ctx, now);
    fprintf(out, "%" PRIu64 ",%s,%s,%u,%u,%u,%" PRIu64 ",%u\n",
            now, event, ecu_result_name(result), command.throttle_command_permille,
            command.active_faults, ctx->fault_history,
            ctx->sensors[2].last_valid_received_ms, ctx->sensors[2].counter);
}

int ecu_run_scenarios(FILE *output, uint32_t seed)
{
    if (output == NULL) return 1;
    EcuContext ctx; const EcuConfig config = ecu_default_config();
    if (!ecu_init(&ctx, &config, 0)) return 1;
    fprintf(output, "time_ms,event,result,command_permille,active,history,coolant_received_ms,coolant_counter\n");
    report(output, &ctx, 0, "startup", ECU_ACCEPTED);
    (void)send(&ctx, 0x100, 3000, 10, 0, 0, false, &seed);
    (void)send(&ctx, 0x101, 750, 10, 0, 0, false, &seed);
    (void)send(&ctx, 0x102, 905, 10, 0, 0, false, &seed);
    report(output, &ctx, 0, "nominal", ECU_ACCEPTED);
    report(output, &ctx, 499, "age499", ECU_ACCEPTED);
    report(output, &ctx, 500, "age500", ECU_ACCEPTED);
    (void)send(&ctx, 0x100, 3000, 11, 510, 510, false, &seed);
    (void)send(&ctx, 0x101, 750, 11, 510, 510, false, &seed);
    (void)send(&ctx, 0x102, 905, 11, 510, 510, false, &seed);
    report(output, &ctx, 510, "refresh", ECU_ACCEPTED);
    /* Drop: no coolant ingest call, not a fake zero-valued sample. */
    (void)send(&ctx, 0x100, 3000, 12, 1009, 1009, false, &seed);
    (void)send(&ctx, 0x101, 750, 12, 1009, 1009, false, &seed);
    report(output, &ctx, 1010, "coolant-drop", ECU_ACCEPTED);
    EcuResult r = send(&ctx, 0x102, 905, 12, 1011, 1011, true, &seed);
    report(output, &ctx, 1011, "bitflip", r);
    r = send(&ctx, 0x102, 905, 13, 1012, 1012, false, &seed);
    report(output, &ctx, 1012, "gap-recovery", r);
    r = send(&ctx, 0x102, 905, 13, 1013, 1013, false, &seed);
    report(output, &ctx, 1013, "duplicate", r);
    r = send(&ctx, 0x102, 905, 14, 1014, 1014, false, &seed);
    report(output, &ctx, 1014, "duplicate-recovery", r);
    r = send(&ctx, 0x101, 1001, 13, 1020, 1020, false, &seed);
    report(output, &ctx, 1020, "range-with-valid-crc", r);
    r = send(&ctx, 0x101, 900, 14, 1021, 1021, false, &seed);
    report(output, &ctx, 1021, "range-recovery", r);
    /* Delay preserves original receive time instead of restamping. */
    r = send(&ctx, 0x100, 3000, 13, 1009, 1509, false, &seed);
    report(output, &ctx, 1509, "queued-delay", r);
    r = send(&ctx, 0x100, 3000, 13, 1508, 1508, false, &seed);
    report(output, &ctx, 1508, "clock-rollback", r);
    return ferror(output) ? 1 : 0;
}
