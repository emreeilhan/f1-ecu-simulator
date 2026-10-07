#ifndef ECU_CORE_H
#define ECU_CORE_H

#include "ecu.h"

/* v1 is an educational application protocol, not a CAN wire protocol. */
enum { ECU_V1_PAYLOAD_SIZE = 4, ECU_SENSOR_COUNT = 3 };
typedef enum {
    ECU_SENSOR_RPM, ECU_SENSOR_THROTTLE, ECU_SENSOR_COOLANT
} EcuSensorId;

typedef enum {
    ECU_ACCEPTED, ECU_ACCEPTED_GAP,
    ECU_REJECT_NULL, ECU_REJECT_NOT_INITIALIZED, ECU_REJECT_CLOCK,
    ECU_REJECT_ID, ECU_REJECT_LENGTH, ECU_REJECT_CRC, ECU_REJECT_RANGE,
    ECU_REJECT_TIMESTAMP, ECU_REJECT_STALE, ECU_REJECT_DUPLICATE,
    ECU_REJECT_COUNTER, ECU_RESULT_COUNT
} EcuResult;

enum {
    ECU_ACTIVE_MISSING_RPM = 1U << 0,
    ECU_ACTIVE_MISSING_THROTTLE = 1U << 1,
    ECU_ACTIVE_MISSING_COOLANT = 1U << 2,
    ECU_ACTIVE_STALE_RPM = 1U << 3,
    ECU_ACTIVE_STALE_THROTTLE = 1U << 4,
    ECU_ACTIVE_STALE_COOLANT = 1U << 5,
    ECU_ACTIVE_INVALID_RPM = 1U << 6,
    ECU_ACTIVE_INVALID_THROTTLE = 1U << 7,
    ECU_ACTIVE_INVALID_COOLANT = 1U << 8,
    ECU_ACTIVE_CLOCK = 1U << 9,
    ECU_ACTIVE_API = 1U << 10,
    ECU_HISTORY_UNKNOWN_ID = 1U << 11,
    ECU_HISTORY_COUNTER_GAP = 1U << 12,
    ECU_HISTORY_TRANSPORT = 1U << 13,
    ECU_ACTIVE_CONFIG = 1U << 14,
    ECU_MISSING_MASK = 7U,
    ECU_STALE_MASK = 7U << 3,
    ECU_INVALID_MASK = 7U << 6
};

enum {
    ECU_REASON_MISSING = 1U << 0,
    ECU_REASON_STALE = 1U << 1,
    ECU_REASON_INTEGRITY = 1U << 2,
    ECU_REASON_CLOCK = 1U << 3,
    ECU_REASON_API = 1U << 4,
    ECU_REASON_NOT_INITIALIZED = 1U << 5,
    ECU_REASON_RPM_LIMIT = 1U << 6,
    ECU_REASON_THERMAL_DERATE = 1U << 7
};

typedef struct { uint64_t timeout_ms[ECU_SENSOR_COUNT]; } EcuConfig;
typedef struct {
    int32_t value;
    bool has_valid_value;
    bool has_receive_timestamp; /* Ordering floor survives explicit resync. */
    bool counter_initialized;
    uint8_t counter;
    uint64_t last_valid_received_ms;
    bool integrity_fault;
    EcuResult last_rejection;
    uint64_t accepted_frames;
    uint64_t rejected_by_result[ECU_RESULT_COUNT];
    uint64_t missed_counter_steps;
} EcuSensor;
typedef struct {
    bool initialized;
    EcuConfig config;
    uint64_t start_ms;
    uint64_t last_now_ms;
    EcuSensor sensors[ECU_SENSOR_COUNT];
    uint32_t active_faults;
    uint32_t fault_history;
    uint32_t latched_faults;
    uint64_t rejected_frames;
    uint64_t unknown_frames;
    uint64_t transport_errors;
} EcuContext;
typedef struct {
    uint16_t throttle_command_permille;
    uint32_t reasons;
    uint32_t active_faults;
} EcuControlOutput;
typedef struct { EcuSensorId sensor; int32_t value; uint8_t counter; } EcuDecoded;

EcuConfig ecu_default_config(void);
/* A failed initialization disables/zeros the context. Check the return value. */
bool ecu_init(EcuContext *ctx, const EcuConfig *config, uint64_t start_ms);
bool ecu_tick(EcuContext *ctx, uint64_t now_ms);
EcuResult ecu_ingest_frame(EcuContext *ctx, const EcuFrame *frame,
                           uint64_t received_ms, uint64_t now_ms);
EcuControlOutput ecu_compute_command_at(EcuContext *ctx, uint64_t now_ms);
void ecu_clear_fault_history(EcuContext *ctx);
bool ecu_resync_stream(EcuContext *ctx, uint16_t id);
void ecu_note_transport_error(EcuContext *ctx);
bool ecu_result_accepted(EcuResult result);
const char *ecu_result_name(EcuResult result);

/* Pure protocol/time/policy boundaries: no clock, I/O or allocation. */
bool ecu_sensor_for_id(uint16_t id, EcuSensorId *sensor);
bool ecu_crc8_smbus(const uint8_t *data, size_t length, uint8_t *crc);
bool ecu_protocol_set_crc(uint16_t id, uint8_t payload[ECU_V1_PAYLOAD_SIZE]);
bool ecu_protocol_encode(uint16_t id, int32_t value, uint8_t counter,
                         uint8_t payload[ECU_V1_PAYLOAD_SIZE]);
EcuResult ecu_protocol_decode(const EcuFrame *frame, EcuDecoded *decoded);
bool ecu_time_ordered(uint64_t start, uint64_t last_now, uint64_t now);
bool ecu_time_stale(uint64_t received, uint64_t now, uint64_t timeout);
EcuControlOutput ecu_policy_compute(const EcuContext *ctx);

#endif
