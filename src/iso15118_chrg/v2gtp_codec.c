#include "openv2g/iso15118_chrg/v2gtp_codec.h"

#include <string.h>

#include "openv2g/config.h"

static uint16_t OpenV2g_V2gtp_ReadU16Be(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8u) | (uint16_t)data[1]);
}

static uint32_t OpenV2g_V2gtp_ReadU32Be(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24u) |
           ((uint32_t)data[1] << 16u) |
           ((uint32_t)data[2] << 8u) |
           (uint32_t)data[3];
}

static void OpenV2g_V2gtp_ClearFrame(OpenV2g_V2gtpParser *parser)
{
    (void)memset(parser->header, 0, sizeof(parser->header));
    parser->header_used = 0u;
    parser->payload_length = 0u;
    parser->payload_used = 0u;
    parser->payload_type = 0u;
    parser->state = OPENV2G_V2GTP_HEADER;
}

OpenV2g_Result OpenV2g_V2gtpParser_Init(
    OpenV2g_V2gtpParser *parser,
    uint8_t *payload_storage,
    size_t payload_capacity)
{
    if ((parser == NULL) || (payload_storage == NULL) ||
        (payload_capacity == 0u) ||
        (payload_capacity > (size_t)OPENV2G_CFG_V2GTP_PAYLOAD_MAX)) {
        return OPENV2G_RESULT_REJECTED;
    }

    (void)memset(parser, 0, sizeof(*parser));
    parser->payload_storage = payload_storage;
    parser->payload_capacity = payload_capacity;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_V2gtpParser_Begin(
    OpenV2g_V2gtpParser *parser,
    uint32_t generation)
{
    if ((parser == NULL) || (parser->payload_storage == NULL) ||
        (parser->payload_capacity == 0u) || (generation == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }

    OpenV2g_V2gtp_ClearFrame(parser);
    parser->generation = generation;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_V2gtpParser_Feed(
    OpenV2g_V2gtpParser *parser,
    uint32_t generation,
    const uint8_t *data,
    size_t length,
    size_t *consumed)
{
    size_t available;
    size_t copy_length;

    if (consumed == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    *consumed = 0u;

    if ((parser == NULL) || ((data == NULL) && (length != 0u)) ||
        (generation == 0u) || (generation != parser->generation)) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (parser->state == OPENV2G_V2GTP_FRAME_READY) {
        return OPENV2G_RESULT_BUSY;
    }
    if ((parser->state != OPENV2G_V2GTP_HEADER) &&
        (parser->state != OPENV2G_V2GTP_PAYLOAD)) {
        return OPENV2G_RESULT_REJECTED;
    }

    while (*consumed < length) {
        if (parser->state == OPENV2G_V2GTP_HEADER) {
            available = (size_t)OPENV2G_V2GTP_HEADER_SIZE - parser->header_used;
            copy_length = length - *consumed;
            if (copy_length > available) {
                copy_length = available;
            }
            (void)memcpy(
                &parser->header[parser->header_used],
                &data[*consumed],
                copy_length);
            parser->header_used += copy_length;
            *consumed += copy_length;

            if (parser->header_used == (size_t)OPENV2G_V2GTP_HEADER_SIZE) {
                if ((parser->header[0] != OPENV2G_V2GTP_VERSION) ||
                    (parser->header[1] != OPENV2G_V2GTP_INVERSE_VERSION)) {
                    parser->state = OPENV2G_V2GTP_ERROR;
                    return OPENV2G_RESULT_REJECTED;
                }

                parser->payload_type = OpenV2g_V2gtp_ReadU16Be(&parser->header[2]);
                parser->payload_length = OpenV2g_V2gtp_ReadU32Be(&parser->header[4]);
                if ((parser->payload_length > OPENV2G_CFG_V2GTP_PAYLOAD_MAX) ||
                    ((size_t)parser->payload_length > parser->payload_capacity)) {
                    parser->state = OPENV2G_V2GTP_ERROR;
                    return OPENV2G_RESULT_OVERFLOW;
                }

                if (parser->payload_length == 0u) {
                    parser->state = OPENV2G_V2GTP_FRAME_READY;
                    return OPENV2G_RESULT_OK;
                }
                parser->state = OPENV2G_V2GTP_PAYLOAD;
            }
        } else {
            available = (size_t)(parser->payload_length - parser->payload_used);
            copy_length = length - *consumed;
            if (copy_length > available) {
                copy_length = available;
            }
            (void)memcpy(
                &parser->payload_storage[parser->payload_used],
                &data[*consumed],
                copy_length);
            parser->payload_used += (uint32_t)copy_length;
            *consumed += copy_length;

            if (parser->payload_used == parser->payload_length) {
                parser->state = OPENV2G_V2GTP_FRAME_READY;
                return OPENV2G_RESULT_OK;
            }
        }
    }

    return OPENV2G_RESULT_ACCEPTED;
}

OpenV2g_Result OpenV2g_V2gtpParser_GetFrame(
    const OpenV2g_V2gtpParser *parser,
    uint32_t generation,
    OpenV2g_V2gtpFrameView *frame)
{
    if ((parser == NULL) || (frame == NULL) || (generation == 0u) ||
        (generation != parser->generation) ||
        (parser->state != OPENV2G_V2GTP_FRAME_READY)) {
        return OPENV2G_RESULT_REJECTED;
    }

    frame->payload_type = parser->payload_type;
    frame->payload = parser->payload_storage;
    frame->payload_length = parser->payload_length;
    frame->generation = parser->generation;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_V2gtpParser_Reset(
    OpenV2g_V2gtpParser *parser,
    uint32_t generation)
{
    if ((parser == NULL) || (generation == 0u) ||
        (generation != parser->generation)) {
        return OPENV2G_RESULT_REJECTED;
    }

    OpenV2g_V2gtp_ClearFrame(parser);
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_V2gtp_Encode(
    uint16_t payload_type,
    const uint8_t *payload,
    uint32_t payload_length,
    uint8_t *output,
    size_t output_capacity,
    size_t *written)
{
    size_t required;

    if (written == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    *written = 0u;

    if ((output == NULL) || ((payload == NULL) && (payload_length != 0u)) ||
        (payload_length > OPENV2G_CFG_V2GTP_PAYLOAD_MAX)) {
        return OPENV2G_RESULT_REJECTED;
    }

    required = (size_t)OPENV2G_V2GTP_HEADER_SIZE + (size_t)payload_length;
    if (output_capacity < required) {
        return OPENV2G_RESULT_OVERFLOW;
    }

    output[0] = OPENV2G_V2GTP_VERSION;
    output[1] = OPENV2G_V2GTP_INVERSE_VERSION;
    output[2] = (uint8_t)(payload_type >> 8u);
    output[3] = (uint8_t)payload_type;
    output[4] = (uint8_t)(payload_length >> 24u);
    output[5] = (uint8_t)(payload_length >> 16u);
    output[6] = (uint8_t)(payload_length >> 8u);
    output[7] = (uint8_t)payload_length;
    if (payload_length != 0u) {
        (void)memcpy(&output[OPENV2G_V2GTP_HEADER_SIZE], payload, payload_length);
    }
    *written = required;
    return OPENV2G_RESULT_OK;
}
