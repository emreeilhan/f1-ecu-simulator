#include "ecu_transport.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned tests_run;
#define RUN(test) do { test(); ++tests_run; printf("PASS: %s\n", #test); } while (0)
static EcuLineResult line(EcuLineTransport *t, const char *s, EcuOwnedFrame *f)
{
    EcuLineResult result = ECU_LINE_WAIT;
    for (size_t i = 0; s[i]; ++i) result = ecu_line_feed(t, (uint8_t)s[i], f);
    return result;
}

static void test_partial_combined_and_crlf_lines(void)
{
    EcuLineTransport t; ecu_line_init(&t); EcuOwnedFrame f;
    assert(line(&t, "100#0bb8", &f) == ECU_LINE_WAIT);
    assert(line(&t, "014a\r\n", &f) == ECU_LINE_FRAME);
    assert(f.id == 0x100 && f.payload[0] == 0x0B && f.payload[3] == 0x4A);
    assert(line(&t, "101#02EE0100\n102#FF3801AA\n", &f) == ECU_LINE_FRAME);
    assert(f.id == 0x102 && f.payload[0] == 0xFF);
}

static void test_strict_line_shape_and_atomic_output(void)
{
    const char *bad[] = {"\n", "100#000000\n", "0100#00000000\n",
        "100#0000000000\n", "100#0000000G\n", "+00#00000000\n",
        " 100#00000000\n", "100#00000000 \n", "100:00000000\n",
        "100#00000000\r\r\n", "1z0#00000000\n"};
    EcuOwnedFrame f = {.id = 0xAAA, .payload = {1, 2, 3, 4}};
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        EcuLineTransport t; ecu_line_init(&t);
        assert(line(&t, bad[i], &f) == ECU_LINE_INVALID);
        assert(f.id == 0xAAA && f.payload[0] == 1 && f.payload[3] == 4);
    }
    EcuLineTransport t; ecu_line_init(&t);
    assert(line(&t, "fff#00000000\n", &f) == ECU_LINE_FRAME);
    assert(f.id == 0xFFF); /* Transport parses; core classifies unknown IDs. */
}

static void test_bounded_overflow_discard_and_recovery(void)
{
    EcuLineTransport t; ecu_line_init(&t); EcuOwnedFrame f = {0};
    for (unsigned i = 0; i < 63; ++i)
        assert(ecu_line_feed(&t, 'x', &f) == ECU_LINE_WAIT);
    assert(ecu_line_feed(&t, 'x', &f) == ECU_LINE_OVERFLOW);
    assert(line(&t, "100#00000000\n", &f) == ECU_LINE_WAIT);
    assert(f.id == 0);
    assert(line(&t, "100#00000000\n", &f) == ECU_LINE_FRAME);
    assert(f.id == 0x100);
}

static void test_driver_error_discard_and_null_api(void)
{
    EcuLineTransport t; ecu_line_init(&t); EcuOwnedFrame f = {0};
    assert(line(&t, "100#", &f) == ECU_LINE_WAIT);
    ecu_line_discard(&t);
    assert(line(&t, "00000000\n", &f) == ECU_LINE_WAIT);
    assert(line(&t, "100#00000000\n", &f) == ECU_LINE_FRAME);
    assert(ecu_line_feed(NULL, 0, &f) == ECU_LINE_INVALID);
    assert(ecu_line_feed(&t, 0, NULL) == ECU_LINE_INVALID);
    ecu_line_init(NULL); ecu_line_discard(NULL);
    ecu_line_init(&t);
    assert(ecu_line_feed(&t, 0, &f) == ECU_LINE_WAIT);
    assert(line(&t, "100#00000000\n", &f) == ECU_LINE_INVALID);
}

int main(void)
{
    RUN(test_partial_combined_and_crlf_lines);
    RUN(test_strict_line_shape_and_atomic_output);
    RUN(test_bounded_overflow_discard_and_recovery);
    RUN(test_driver_error_discard_and_null_api);
    printf("Transport: %u named test functions passed.\n", tests_run);
    return 0;
}
