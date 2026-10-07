#ifndef ECU_TRANSPORT_H
#define ECU_TRANSPORT_H

#include "ecu_core.h"

enum { ECU_LINE_CAPACITY = 64 };
typedef struct {
    char line[ECU_LINE_CAPACITY];
    size_t length;
    bool discarding;
} EcuLineTransport;
typedef struct { uint16_t id; uint8_t payload[ECU_V1_PAYLOAD_SIZE]; } EcuOwnedFrame;
typedef enum { ECU_LINE_WAIT, ECU_LINE_FRAME, ECU_LINE_INVALID,
               ECU_LINE_OVERFLOW } EcuLineResult;

void ecu_line_init(EcuLineTransport *transport);
/* LF-delimited, optional terminal CR; exactly HHH#HHHHHHHH. No allocations.
 * Caller output is changed only when ECU_LINE_FRAME is returned. */
EcuLineResult ecu_line_feed(EcuLineTransport *transport, uint8_t byte,
                            EcuOwnedFrame *frame);
/* On driver overflow/error, discard through the next LF, including a suffix
 * that could otherwise be mistaken for a new complete line. */
void ecu_line_discard(EcuLineTransport *transport);

#endif
