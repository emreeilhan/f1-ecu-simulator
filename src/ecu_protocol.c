#include "ecu_core.h"
#include <string.h>

bool ecu_sensor_for_id(uint16_t id, EcuSensorId *sensor)
{
    EcuSensorId candidate;
    if (sensor == NULL) return false;
    switch (id) {
        case ECU_FRAME_ID_RPM: candidate = ECU_SENSOR_RPM; break;
        case ECU_FRAME_ID_THROTTLE: candidate = ECU_SENSOR_THROTTLE; break;
        case ECU_FRAME_ID_COOLANT_TEMP: candidate = ECU_SENSOR_COOLANT; break;
        default: return false;
    }
    *sensor = candidate;
    return true;
}

bool ecu_crc8_smbus(const uint8_t *data, size_t length, uint8_t *crc)
{
    uint8_t value = 0;
    if (crc == NULL || (data == NULL && length != 0U)) return false;
    for (size_t i = 0; i < length; ++i) {
        value ^= data[i];
        for (unsigned bit = 0; bit < 8U; ++bit) {
            value = (uint8_t)((value & 0x80U)
                         ? ((unsigned)value << 1U) ^ 0x07U
                         : (unsigned)value << 1U);
        }
    }
    *crc = value;
    return true;
}

bool ecu_protocol_set_crc(uint16_t id, uint8_t payload[ECU_V1_PAYLOAD_SIZE])
{
    if (payload == NULL) return false;
    const uint8_t input[] = {1U, (uint8_t)(id >> 8U), (uint8_t)id,
                            payload[0], payload[1], payload[2]};
    return ecu_crc8_smbus(input, sizeof input, &payload[3]);
}

static bool value_valid(EcuSensorId sensor, int32_t value)
{
    switch (sensor) {
        case ECU_SENSOR_RPM: return value >= 0 && value <= UINT16_MAX;
        case ECU_SENSOR_THROTTLE: return value >= 0 && value <= 1000;
        case ECU_SENSOR_COOLANT: return value >= -400 && value <= 1500;
        default: return false;
    }
}

bool ecu_protocol_encode(uint16_t id, int32_t value, uint8_t counter,
                         uint8_t payload[ECU_V1_PAYLOAD_SIZE])
{
    EcuSensorId sensor;
    if (payload == NULL || !ecu_sensor_for_id(id, &sensor) ||
        !value_valid(sensor, value)) return false;
    const uint16_t raw = (uint16_t)value;
    uint8_t candidate[] = {(uint8_t)(raw >> 8U), (uint8_t)raw, counter, 0};
    (void)ecu_protocol_set_crc(id, candidate);
    memcpy(payload, candidate, sizeof candidate);
    return true;
}

EcuResult ecu_protocol_decode(const EcuFrame *frame, EcuDecoded *decoded)
{
    EcuDecoded candidate;
    uint8_t crc_payload[ECU_V1_PAYLOAD_SIZE];
    if (frame == NULL || decoded == NULL) return ECU_REJECT_NULL;
    if (!ecu_sensor_for_id(frame->id, &candidate.sensor)) return ECU_REJECT_ID;
    if (frame->data == NULL) return ECU_REJECT_NULL;
    if (frame->length != ECU_V1_PAYLOAD_SIZE) return ECU_REJECT_LENGTH;
    memcpy(crc_payload, frame->data, sizeof crc_payload);
    (void)ecu_protocol_set_crc(frame->id, crc_payload);
    if (crc_payload[3] != frame->data[3]) return ECU_REJECT_CRC;
    const uint16_t raw = (uint16_t)(((uint16_t)frame->data[0] << 8U) |
                                   frame->data[1]);
    candidate.value = raw;
    if (candidate.sensor == ECU_SENSOR_COOLANT && raw > INT16_MAX)
        candidate.value = (int32_t)raw - 65536;
    if (!value_valid(candidate.sensor, candidate.value)) return ECU_REJECT_RANGE;
    candidate.counter = frame->data[2];
    *decoded = candidate;
    return ECU_ACCEPTED;
}

bool ecu_result_accepted(EcuResult result)
{
    return result == ECU_ACCEPTED || result == ECU_ACCEPTED_GAP;
}

const char *ecu_result_name(EcuResult result)
{
    static const char *const names[] = {
        "accepted", "accepted-gap", "null", "not-initialized", "clock",
        "id", "length", "crc", "range", "timestamp", "stale",
        "duplicate", "counter"
    };
    return (unsigned)result < ECU_RESULT_COUNT ? names[result] : "unknown-result";
}
