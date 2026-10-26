#include "wire.h"
#include <string.h>
bool OvWireCodec_Init(OvWireCodec *codec, uint32_t generation)
{
    (void)memset(codec, 0, sizeof(*codec));
    OpenV2g_SchemaRegistry_Init(&codec->registry);
    OpenV2g_ExiCodec_Init(&codec->codec, &codec->registry);
    return OpenV2g_Cbv2gSap_Bind(&codec->codec) == OPENV2G_RESULT_OK &&
        OpenV2g_Cbv2gDin_Bind(&codec->codec) == OPENV2G_RESULT_OK &&
        OpenV2g_Cbv2gIso2_Bind(&codec->codec) == OPENV2G_RESULT_OK &&
        OpenV2g_ExiCodec_CreateClientServerPort(&codec->codec, &codec->port) == OPENV2G_RESULT_OK &&
        OpenV2g_ExiCodec_Start(&codec->codec, generation) == OPENV2G_RESULT_OK;
}
bool OvWireEncode(OvWireCodec *codec, uint32_t generation, OpenV2g_SchemaId schema,
    uint16_t id, const void *object, size_t size, uint8_t *frame, size_t *length)
{
    OpenV2g_ExiResult result;
    OpenV2g_ExiEncodeRequest request = {schema, id, object, size, codec->encoded, sizeof(codec->encoded)};
    return codec->port.encode(codec->port.context, generation, &request, &result) == OPENV2G_RESULT_OK &&
        OpenV2g_V2gtp_Encode(0x8001u, codec->encoded, (uint32_t)result.encoded_length,
                             frame, OV_WIRE_FRAME_MAX, length) == OPENV2G_RESULT_OK;
}
bool OvWireDecode(OvWireCodec *codec, uint32_t generation, OpenV2g_SchemaId schema,
    uint16_t id, const OpenV2g_V2gtpFrameView *frame, void *object, size_t size)
{
    OpenV2g_ExiResult result;
    OpenV2g_ExiDecodeRequest request;
    if (frame->generation != generation || frame->payload_type != 0x8001u) { return false; }
    (void)memset(&request, 0, sizeof(request));
    request.schema = schema; request.expected_message_type_id = id;
    request.input = frame->payload; request.input_length = frame->payload_length;
    request.candidate_object = schema == OPENV2G_SCHEMA_SAP ? (void *)&codec->sap_candidate : (void *)&codec->dc_candidate;
    request.output_object = object; request.object_size = size;
    request.workspace = codec->workspace; request.workspace_capacity = sizeof(codec->workspace);
    return codec->port.decode(codec->port.context, generation, &request, &result) == OPENV2G_RESULT_OK;
}
