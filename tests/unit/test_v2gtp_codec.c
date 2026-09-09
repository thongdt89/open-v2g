#include <string.h>

#include "openv2g/iso15118_chrg/v2gtp_codec.h"
#include "test.h"

static int TestFragmentedFrame(void)
{
    const uint8_t payload[] = {0xDEu, 0xADu, 0xBEu, 0xEFu};
    uint8_t encoded[32];
    uint8_t storage[16];
    size_t encoded_length;
    size_t consumed;
    OpenV2g_V2gtpParser parser;
    OpenV2g_V2gtpFrameView frame;

    TEST_ASSERT(OpenV2g_V2gtp_Encode(
                    0x8001u,
                    payload,
                    (uint32_t)sizeof(payload),
                    encoded,
                    sizeof(encoded),
                    &encoded_length) == OPENV2G_RESULT_OK);
    TEST_ASSERT(encoded_length == 12u);
    TEST_ASSERT(OpenV2g_V2gtpParser_Init(&parser, storage, sizeof(storage)) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_V2gtpParser_Begin(&parser, 7u) == OPENV2G_RESULT_OK);

    TEST_ASSERT(OpenV2g_V2gtpParser_Feed(
                    &parser, 7u, encoded, 3u, &consumed) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(consumed == 3u);
    TEST_ASSERT(OpenV2g_V2gtpParser_Feed(
                    &parser, 7u, &encoded[3], 2u, &consumed) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(consumed == 2u);
    TEST_ASSERT(OpenV2g_V2gtpParser_Feed(
                    &parser, 7u, &encoded[5], encoded_length - 5u, &consumed) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(consumed == encoded_length - 5u);
    TEST_ASSERT(OpenV2g_V2gtpParser_GetFrame(&parser, 7u, &frame) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(frame.payload_type == 0x8001u);
    TEST_ASSERT(frame.payload_length == sizeof(payload));
    TEST_ASSERT(frame.generation == 7u);
    TEST_ASSERT(memcmp(frame.payload, payload, sizeof(payload)) == 0);
    return 0;
}

static int TestCoalescedFrames(void)
{
    const uint8_t first_payload[] = {1u, 2u};
    const uint8_t second_payload[] = {3u};
    uint8_t input[32];
    uint8_t storage[8];
    size_t first_length;
    size_t second_length;
    size_t consumed;
    OpenV2g_V2gtpParser parser;
    OpenV2g_V2gtpFrameView frame;

    TEST_ASSERT(OpenV2g_V2gtp_Encode(
                    0x9000u,
                    first_payload,
                    (uint32_t)sizeof(first_payload),
                    input,
                    sizeof(input),
                    &first_length) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_V2gtp_Encode(
                    0x9001u,
                    second_payload,
                    (uint32_t)sizeof(second_payload),
                    &input[first_length],
                    sizeof(input) - first_length,
                    &second_length) == OPENV2G_RESULT_OK);

    TEST_ASSERT(OpenV2g_V2gtpParser_Init(&parser, storage, sizeof(storage)) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_V2gtpParser_Begin(&parser, 8u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_V2gtpParser_Feed(
                    &parser,
                    8u,
                    input,
                    first_length + second_length,
                    &consumed) == OPENV2G_RESULT_OK);
    TEST_ASSERT(consumed == first_length);
    TEST_ASSERT(OpenV2g_V2gtpParser_Reset(&parser, 8u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_V2gtpParser_Feed(
                    &parser,
                    8u,
                    &input[first_length],
                    second_length,
                    &consumed) == OPENV2G_RESULT_OK);
    TEST_ASSERT(consumed == second_length);
    TEST_ASSERT(OpenV2g_V2gtpParser_GetFrame(&parser, 8u, &frame) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(frame.payload_type == 0x9001u);
    TEST_ASSERT(frame.payload_length == sizeof(second_payload));
    TEST_ASSERT(frame.payload[0] == second_payload[0]);
    return 0;
}

static int TestRejectedInput(void)
{
    uint8_t storage[4] = {0xA5u, 0xA5u, 0xA5u, 0xA5u};
    uint8_t oversized_header[OPENV2G_V2GTP_HEADER_SIZE] = {
        OPENV2G_V2GTP_VERSION,
        OPENV2G_V2GTP_INVERSE_VERSION,
        0x80u,
        0x01u,
        0u,
        0u,
        0u,
        5u};
    uint8_t invalid_header[OPENV2G_V2GTP_HEADER_SIZE] = {
        OPENV2G_V2GTP_VERSION,
        0u,
        0x80u,
        0x01u,
        0u,
        0u,
        0u,
        0u};
    size_t consumed;
    OpenV2g_V2gtpParser parser;

    TEST_ASSERT(OpenV2g_V2gtpParser_Init(&parser, storage, sizeof(storage)) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_V2gtpParser_Begin(&parser, 9u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_V2gtpParser_Feed(
                    &parser,
                    10u,
                    oversized_header,
                    sizeof(oversized_header),
                    &consumed) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(consumed == 0u);
    TEST_ASSERT(parser.header_used == 0u);

    TEST_ASSERT(OpenV2g_V2gtpParser_Feed(
                    &parser,
                    9u,
                    oversized_header,
                    sizeof(oversized_header),
                    &consumed) == OPENV2G_RESULT_OVERFLOW);
    TEST_ASSERT(consumed == sizeof(oversized_header));
    TEST_ASSERT(parser.state == OPENV2G_V2GTP_ERROR);
    TEST_ASSERT(storage[0] == 0xA5u);

    TEST_ASSERT(OpenV2g_V2gtpParser_Reset(&parser, 9u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_V2gtpParser_Feed(
                    &parser,
                    9u,
                    invalid_header,
                    sizeof(invalid_header),
                    &consumed) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(parser.state == OPENV2G_V2GTP_ERROR);
    return 0;
}

static int TestEncodeBounds(void)
{
    const uint8_t payload[] = {1u};
    uint8_t output[OPENV2G_V2GTP_HEADER_SIZE];
    size_t written = 99u;

    TEST_ASSERT(OpenV2g_V2gtp_Encode(
                    0x8001u,
                    payload,
                    (uint32_t)sizeof(payload),
                    output,
                    sizeof(output),
                    &written) == OPENV2G_RESULT_OVERFLOW);
    TEST_ASSERT(written == 0u);
    return 0;
}

int main(void)
{
    TEST_ASSERT(TestFragmentedFrame() == 0);
    TEST_ASSERT(TestCoalescedFrames() == 0);
    TEST_ASSERT(TestRejectedInput() == 0);
    TEST_ASSERT(TestEncodeBounds() == 0);
    return 0;
}
