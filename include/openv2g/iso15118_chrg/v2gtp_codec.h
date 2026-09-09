#ifndef OPENV2G_ISO15118_CHRG_V2GTP_CODEC_H
#define OPENV2G_ISO15118_CHRG_V2GTP_CODEC_H

#include <stddef.h>
#include <stdint.h>

#include "openv2g/common/result.h"

#define OPENV2G_V2GTP_HEADER_SIZE 8u
#define OPENV2G_V2GTP_VERSION 0x01u
#define OPENV2G_V2GTP_INVERSE_VERSION 0xFEu

typedef enum {
    OPENV2G_V2GTP_UNINITIALIZED = 0,
    OPENV2G_V2GTP_HEADER = 1,
    OPENV2G_V2GTP_PAYLOAD = 2,
    OPENV2G_V2GTP_FRAME_READY = 3,
    OPENV2G_V2GTP_ERROR = 4
} OpenV2g_V2gtpParserState;

typedef struct {
    uint16_t payload_type;
    const uint8_t *payload;
    uint32_t payload_length;
    uint32_t generation;
} OpenV2g_V2gtpFrameView;

typedef struct {
    uint8_t header[OPENV2G_V2GTP_HEADER_SIZE];
    uint8_t *payload_storage;
    size_t payload_capacity;
    size_t header_used;
    uint32_t payload_length;
    uint32_t payload_used;
    uint32_t generation;
    uint16_t payload_type;
    OpenV2g_V2gtpParserState state;
} OpenV2g_V2gtpParser;

OpenV2g_Result OpenV2g_V2gtpParser_Init(
    OpenV2g_V2gtpParser *parser,
    uint8_t *payload_storage,
    size_t payload_capacity);
OpenV2g_Result OpenV2g_V2gtpParser_Begin(
    OpenV2g_V2gtpParser *parser,
    uint32_t generation);
OpenV2g_Result OpenV2g_V2gtpParser_Feed(
    OpenV2g_V2gtpParser *parser,
    uint32_t generation,
    const uint8_t *data,
    size_t length,
    size_t *consumed);
OpenV2g_Result OpenV2g_V2gtpParser_GetFrame(
    const OpenV2g_V2gtpParser *parser,
    uint32_t generation,
    OpenV2g_V2gtpFrameView *frame);
OpenV2g_Result OpenV2g_V2gtpParser_Reset(
    OpenV2g_V2gtpParser *parser,
    uint32_t generation);

OpenV2g_Result OpenV2g_V2gtp_Encode(
    uint16_t payload_type,
    const uint8_t *payload,
    uint32_t payload_length,
    uint8_t *output,
    size_t output_capacity,
    size_t *written);

#endif
