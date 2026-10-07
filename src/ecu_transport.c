#include "ecu_transport.h"
#include <string.h>

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void ecu_line_init(EcuLineTransport *transport)
{
    if (transport != NULL) memset(transport, 0, sizeof *transport);
}

void ecu_line_discard(EcuLineTransport *transport)
{
    if (transport != NULL) {
        transport->length = 0U;
        transport->discarding = true;
    }
}

EcuLineResult ecu_line_feed(EcuLineTransport *transport, uint8_t byte,
                            EcuOwnedFrame *frame)
{
    if (transport == NULL || frame == NULL) return ECU_LINE_INVALID;
    if (transport->discarding) {
        if (byte == '\n') transport->discarding = false;
        return ECU_LINE_WAIT;
    }
    if (byte != '\n') {
        if (transport->length >= ECU_LINE_CAPACITY - 1U) {
            ecu_line_discard(transport);
            return ECU_LINE_OVERFLOW;
        }
        transport->line[transport->length++] = (char)byte;
        return ECU_LINE_WAIT;
    }
    size_t length = transport->length;
    transport->length = 0U;
    if (length != 0U && transport->line[length - 1U] == '\r') --length;
    if (length != 12U || transport->line[3] != '#') return ECU_LINE_INVALID;
    EcuOwnedFrame candidate = {0};
    for (unsigned i = 0; i < 3U; ++i) {
        const int h = hex_value(transport->line[i]);
        if (h < 0) return ECU_LINE_INVALID;
        candidate.id = (uint16_t)((unsigned)candidate.id * 16U + (unsigned)h);
    }
    for (unsigned i = 0; i < ECU_V1_PAYLOAD_SIZE; ++i) {
        const int hi = hex_value(transport->line[4U + i * 2U]);
        const int lo = hex_value(transport->line[5U + i * 2U]);
        if (hi < 0 || lo < 0) return ECU_LINE_INVALID;
        candidate.payload[i] = (uint8_t)((unsigned)hi * 16U + (unsigned)lo);
    }
    *frame = candidate;
    return ECU_LINE_FRAME;
}
