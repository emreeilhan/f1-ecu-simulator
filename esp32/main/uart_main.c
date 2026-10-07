#include "ecu_transport.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

enum { TX_CAPACITY = 4096, RX_BUDGET = 64, TX_BUDGET = 128, EVENT_BUDGET = 4 };
static char tx[TX_CAPACITY];
static size_t tx_head, tx_length;
static uint64_t tx_dropped;
static EcuContext ctx;
static EcuLineTransport input;

static uint64_t now_ms(void)
{
    return (uint64_t)esp_timer_get_time() / 1000U;
}

static void enqueue(const char *line, size_t length)
{
    if (length > TX_CAPACITY - tx_length) {
        if (tx_dropped < UINT64_MAX) ++tx_dropped;
        return;
    }
    for (size_t i = 0; i < length; ++i)
        tx[(tx_head + tx_length + i) % TX_CAPACITY] = line[i];
    tx_length += length;
}

static void send_snapshot(const char *event, const char *result, uint16_t id)
{
    const uint64_t now = now_ms();
    const EcuControlOutput output = ecu_compute_command_at(&ctx, now);
    unsigned present = 0;
    for (unsigned i = 0; i < ECU_SENSOR_COUNT; ++i)
        if (ctx.sensors[i].has_valid_value) present |= 1U << i;
    char line[512];
    int n = snprintf(line, sizeof line,
        "{\"t\":%" PRIu64 ",\"event\":\"%s\",\"result\":\"%s\",\"id\":%u,"
        "\"command\":%u,\"active\":%" PRIu32 ",\"history\":%" PRIu32 ","
        "\"present\":%u,\"value\":[%" PRId32 ",%" PRId32 ",%" PRId32 "],"
        "\"counter\":[%u,%u,%u],\"received\":[%" PRIu64 ",%" PRIu64 ",%" PRIu64 "],"
        "\"tx_dropped\":%" PRIu64 "}\n",
        now, event, result, id, output.throttle_command_permille,
        output.active_faults, ctx.fault_history, present,
        ctx.sensors[0].value, ctx.sensors[1].value, ctx.sensors[2].value,
        ctx.sensors[0].counter, ctx.sensors[1].counter, ctx.sensors[2].counter,
        ctx.sensors[0].last_valid_received_ms, ctx.sensors[1].last_valid_received_ms,
        ctx.sensors[2].last_valid_received_ms, tx_dropped);
    if (n > 0 && (size_t)n < sizeof line) enqueue(line, (size_t)n);
    else if (tx_dropped < UINT64_MAX) ++tx_dropped;
}

static void drain_tx(void)
{
    size_t length = tx_length;
    if (length > TX_CAPACITY - tx_head) length = TX_CAPACITY - tx_head;
    if (length > TX_BUDGET) length = TX_BUDGET;
    if (length == 0U) return;
    /* tx_buffer_size=0: uart_tx_chars copies only available FIFO space. It
     * never waits for a full response to transmit or a ring-buffer slot. */
    const int written = uart_tx_chars(UART_NUM_0, tx + tx_head, (uint32_t)length);
    if (written > 0) {
        tx_head = (tx_head + (size_t)written) % TX_CAPACITY;
        tx_length -= (size_t)written;
    }
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_NONE);
    const uart_config_t config = {
        .baud_rate = 115200, .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT
    };
    QueueHandle_t events;
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, 1, 3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024, 0, 8, &events, 0));
    ecu_line_init(&input);
    const EcuConfig ecu_config = ecu_default_config();
    if (!ecu_init(&ctx, &ecu_config, now_ms())) return;
    send_snapshot("startup", "ready", 0);
    TickType_t wake = xTaskGetTickCount();
    uint64_t heartbeat = now_ms();
    uint32_t previous_active = ctx.active_faults;
    uint16_t previous_command = 0;
    for (;;) {
        uart_event_t event;
        for (unsigned i = 0; i < EVENT_BUDGET; ++i) {
            if (xQueueReceive(events, &event, 0) != pdTRUE) break;
            if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL ||
                event.type == UART_FRAME_ERR || event.type == UART_PARITY_ERR) {
                (void)uart_flush_input(UART_NUM_0);
                ecu_line_discard(&input);
                ecu_note_transport_error(&ctx);
                send_snapshot("transport", "driver-error", 0);
            }
        }
        uint8_t bytes[RX_BUDGET];
        const int read = uart_read_bytes(UART_NUM_0, bytes, sizeof bytes, 0);
        for (int i = 0; i < read; ++i) {
            EcuOwnedFrame owned;
            const EcuLineResult parsed = ecu_line_feed(&input, bytes[i], &owned);
            if (parsed == ECU_LINE_FRAME) {
                /* Timestamp means complete line observed in this bounded read,
                 * not physical arrival at the hardware FIFO (not timestamped). */
                const uint64_t received = now_ms();
                const EcuFrame frame = {owned.id, owned.payload, sizeof owned.payload};
                const EcuResult result = ecu_ingest_frame(&ctx, &frame, received, now_ms());
                send_snapshot("frame", ecu_result_name(result), owned.id);
            } else if (parsed == ECU_LINE_INVALID || parsed == ECU_LINE_OVERFLOW) {
                ecu_note_transport_error(&ctx);
                send_snapshot("transport", parsed == ECU_LINE_OVERFLOW
                               ? "line-overflow" : "line-invalid", 0);
            }
        }
        const uint64_t now = now_ms();
        const EcuControlOutput output = ecu_compute_command_at(&ctx, now);
        if (now - heartbeat >= 100U || output.active_faults != previous_active ||
            output.throttle_command_permille != previous_command) {
            send_snapshot("tick", "status", 0);
            heartbeat = now;
        }
        previous_active = output.active_faults;
        previous_command = output.throttle_command_permille;
        drain_tx();
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(10));
    }
}
