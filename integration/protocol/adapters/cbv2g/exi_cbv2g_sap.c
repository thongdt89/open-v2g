#include "openv2g/exi_cbv2g_sap.h"

#include <string.h>
#include "cbv2g/app_handshake/appHand_Datatypes.h"
#include "cbv2g/app_handshake/appHand_Encoder.h"
#include "cbv2g/app_handshake/appHand_Decoder.h"
#include "cbv2g/common/exi_error_codes.h"
#include "cbv2g_pin.h"

_Static_assert(appHand_AppProtocolType_5_ARRAY_SIZE == OPENV2G_EXI_SAP_OFFERS_MAX,
               "Pinned SAP offer capacity changed");
_Static_assert(appHand_ProtocolNamespace_CHARACTER_SIZE == OPENV2G_EXI_SAP_NAMESPACE_MAX + 1u,
               "Pinned SAP namespace capacity changed");

static bool valid(const OpenV2g_ExiSapDocument *document)
{
    size_t index;
    if (document->message_type_id == OPENV2G_EXI_SAP_REQUEST) {
        if ((document->body.request.count == 0u) ||
            (document->body.request.count > OPENV2G_EXI_SAP_OFFERS_MAX)) { return false; }
        for (index = 0u; index < document->body.request.count; ++index) {
            const OpenV2g_ExiSapOffer *offer = &document->body.request.offers[index];
            size_t character;
            size_t other;
            if ((offer->namespace_length == 0u) ||
                (offer->namespace_length > OPENV2G_EXI_SAP_NAMESPACE_MAX) ||
                (offer->priority == 0u) || (offer->priority > 20u)) { return false; }
            /* POC profile: printable ASCII URI, no implicit string termination. */
            for (character = 0u; character < offer->namespace_length; ++character) {
                unsigned char value = (unsigned char)offer->namespace_text[character];
                if ((value < 0x21u) || (value > 0x7eu)) { return false; }
            }
            for (other = 0u; other < index; ++other) {
                if (offer->wire_schema_id == document->body.request.offers[other].wire_schema_id) {
                    return false;
                }
            }
        }
        return true;
    }
    if (document->message_type_id == OPENV2G_EXI_SAP_RESPONSE) {
        switch (document->body.response.code) {
        case OPENV2G_EXI_SAP_SUCCESS:
        case OPENV2G_EXI_SAP_MINOR_DEVIATION:
            return document->body.response.has_schema_id;
        case OPENV2G_EXI_SAP_NO_NEGOTIATION:
            return !document->body.response.has_schema_id;
        default:
            return false;
        }
    }
    return false;
}

static OpenV2g_Result failure(OpenV2g_ExiDiagnostic *diagnostic,
                             const exi_bitstream_t *stream, int upstream_error)
{
    if (diagnostic != NULL) {
        diagnostic->reason = OPENV2G_REASON_FORMAT;
        diagnostic->rejected_constraint = (uint16_t)(upstream_error < 0 ? -upstream_error : 0);
        diagnostic->bit_offset = stream == NULL ? 0u :
            (uint32_t)(stream->byte_pos * 8u + stream->bit_count);
    }
    return OPENV2G_RESULT_REJECTED;
}

static OpenV2g_Result encode(void *context, uint16_t message_type_id,
    const void *typed_object, size_t typed_object_size, uint8_t *output,
    size_t output_capacity, size_t *encoded_length, OpenV2g_ExiDiagnostic *diagnostic)
{
    const OpenV2g_ExiSapDocument *document = typed_object;
    struct appHand_exiDocument generated;
    uint8_t guarded[OPENV2G_EXI_SAP_PAYLOAD_MAX + 1u] = {0};
    exi_bitstream_t stream;
    size_t capacity;
    size_t length;
    int error;
    size_t index;
    (void)context;
    if (encoded_length != NULL) { *encoded_length = 0u; }
    if ((document == NULL) || (typed_object_size != sizeof(*document)) ||
        (output == NULL) || (output_capacity == 0u) || (encoded_length == NULL) ||
        (document->message_type_id != message_type_id) || !valid(document)) {
        return failure(diagnostic, NULL, 0);
    }
    (void)memset(&generated, 0, sizeof(generated));
    init_appHand_exiDocument(&generated);
    if (message_type_id == OPENV2G_EXI_SAP_REQUEST) {
        generated.supportedAppProtocolReq_isUsed = 1u;
        generated.supportedAppProtocolReq.AppProtocol.arrayLen = document->body.request.count;
        for (index = 0u; index < document->body.request.count; ++index) {
            const OpenV2g_ExiSapOffer *source = &document->body.request.offers[index];
            struct appHand_AppProtocolType *target = &generated.supportedAppProtocolReq.AppProtocol.array[index];
            target->ProtocolNamespace.charactersLen = source->namespace_length;
            (void)memcpy(target->ProtocolNamespace.characters, source->namespace_text, source->namespace_length);
            target->VersionNumberMajor = source->version_major;
            target->VersionNumberMinor = source->version_minor;
            target->SchemaID = source->wire_schema_id;
            target->Priority = source->priority;
        }
    } else {
        generated.supportedAppProtocolRes_isUsed = 1u;
        switch (document->body.response.code) {
        case OPENV2G_EXI_SAP_SUCCESS:
            generated.supportedAppProtocolRes.ResponseCode = appHand_responseCodeType_OK_SuccessfulNegotiation; break;
        case OPENV2G_EXI_SAP_MINOR_DEVIATION:
            generated.supportedAppProtocolRes.ResponseCode = appHand_responseCodeType_OK_SuccessfulNegotiationWithMinorDeviation; break;
        default:
            generated.supportedAppProtocolRes.ResponseCode = appHand_responseCodeType_Failed_NoNegotiation; break;
        }
        generated.supportedAppProtocolRes.SchemaID_isUsed = document->body.response.has_schema_id ? 1u : 0u;
        generated.supportedAppProtocolRes.SchemaID = document->body.response.wire_schema_id;
    }
    capacity = output_capacity < OPENV2G_EXI_SAP_PAYLOAD_MAX ? output_capacity : OPENV2G_EXI_SAP_PAYLOAD_MAX;
    /* Upstream bitstream can access one byte past data_size. The owned guard
     * absorbs that access; publication still requires length <= real capacity. */
    exi_bitstream_init(&stream, guarded, capacity, 0u, NULL);
    error = encode_appHand_exiDocument(&stream, &generated);
    length = exi_bitstream_get_length(&stream);
    if ((error != EXI_ERROR__NO_ERROR) || (length > capacity)) {
        return failure(diagnostic, &stream, error);
    }
    (void)memcpy(output, guarded, length);
    *encoded_length = length;
    return OPENV2G_RESULT_OK;
}

static bool complete(const exi_bitstream_t *stream, const uint8_t *input, size_t length)
{
    uint32_t padding;
    if ((exi_bitstream_get_length(stream) != length) ||
        (stream->byte_pos * 8u + stream->bit_count > length * 8u)) { return false; }
    padding = stream->bit_count < 8u ? ((1u << (8u - stream->bit_count)) - 1u) : 0u;
    return (input[length - 1u] & padding) == 0u;
}

static OpenV2g_Result decode(void *context, const uint8_t *input, size_t input_length,
    void *candidate_object, size_t object_capacity, uint8_t *workspace,
    size_t workspace_capacity, uint16_t *decoded_message_type_id,
    OpenV2g_ExiDiagnostic *diagnostic)
{
    OpenV2g_ExiSapDocument *candidate = candidate_object;
    struct appHand_exiDocument generated;
    exi_bitstream_t stream;
    int error;
    size_t index;
    (void)context;
    if ((input == NULL) || (input_length == 0u) || (input_length > OPENV2G_EXI_SAP_PAYLOAD_MAX) ||
        (candidate == NULL) || (object_capacity != sizeof(*candidate)) ||
        (workspace == NULL) || (workspace_capacity < OPENV2G_EXI_SAP_WORKSPACE_SIZE) ||
        (decoded_message_type_id == NULL)) { return failure(diagnostic, NULL, 0); }
    *decoded_message_type_id = 0u;
    (void)memset(workspace, 0, workspace_capacity);
    (void)memcpy(workspace, input, input_length);
    (void)memset(&generated, 0, sizeof(generated));
    init_appHand_exiDocument(&generated);
    exi_bitstream_init(&stream, workspace, input_length, 0u, NULL);
    error = decode_appHand_exiDocument(&stream, &generated);
    if ((error != EXI_ERROR__NO_ERROR) || !complete(&stream, workspace, input_length) ||
        (generated.supportedAppProtocolReq_isUsed == generated.supportedAppProtocolRes_isUsed)) {
        return failure(diagnostic, &stream, error);
    }
    (void)memset(candidate, 0, sizeof(*candidate));
    if (generated.supportedAppProtocolReq_isUsed) {
        candidate->message_type_id = OPENV2G_EXI_SAP_REQUEST;
        candidate->body.request.count = generated.supportedAppProtocolReq.AppProtocol.arrayLen;
        if (candidate->body.request.count > OPENV2G_EXI_SAP_OFFERS_MAX) { return failure(diagnostic, &stream, 0); }
        for (index = 0u; index < candidate->body.request.count; ++index) {
            const struct appHand_AppProtocolType *source = &generated.supportedAppProtocolReq.AppProtocol.array[index];
            OpenV2g_ExiSapOffer *target = &candidate->body.request.offers[index];
            target->namespace_length = source->ProtocolNamespace.charactersLen;
            if (target->namespace_length > OPENV2G_EXI_SAP_NAMESPACE_MAX) { return failure(diagnostic, &stream, 0); }
            (void)memcpy(target->namespace_text, source->ProtocolNamespace.characters, target->namespace_length);
            target->version_major = source->VersionNumberMajor;
            target->version_minor = source->VersionNumberMinor;
            target->wire_schema_id = source->SchemaID;
            target->priority = source->Priority;
        }
    } else {
        candidate->message_type_id = OPENV2G_EXI_SAP_RESPONSE;
        switch (generated.supportedAppProtocolRes.ResponseCode) {
        case appHand_responseCodeType_OK_SuccessfulNegotiation:
            candidate->body.response.code = OPENV2G_EXI_SAP_SUCCESS; break;
        case appHand_responseCodeType_OK_SuccessfulNegotiationWithMinorDeviation:
            candidate->body.response.code = OPENV2G_EXI_SAP_MINOR_DEVIATION; break;
        case appHand_responseCodeType_Failed_NoNegotiation:
            candidate->body.response.code = OPENV2G_EXI_SAP_NO_NEGOTIATION; break;
        default:
            return failure(diagnostic, &stream, 0);
        }
        candidate->body.response.has_schema_id = generated.supportedAppProtocolRes.SchemaID_isUsed != 0u;
        candidate->body.response.wire_schema_id = generated.supportedAppProtocolRes.SchemaID;
    }
    if (!valid(candidate)) { return failure(diagnostic, &stream, 0); }
    *decoded_message_type_id = candidate->message_type_id;
    return OPENV2G_RESULT_OK;
}

static void reset(void *context, uint8_t *workspace, size_t capacity)
{
    (void)context;
    if (workspace != NULL) { (void)memset(workspace, 0, capacity); }
}

OpenV2g_Result OpenV2g_Cbv2gSap_Bind(OpenV2g_ExiCodec *codec)
{
    static const OpenV2g_SchemaDescriptor descriptor = {
        OPENV2G_SCHEMA_SAP, OPENV2G_PROTOCOL_NONE, OPENV2G_MODE_NONE,
        OPENV2G_EXI_SAP_PAYLOAD_MAX, OPENV2G_EXI_SAP_WORKSPACE_SIZE,
        sizeof(OpenV2g_ExiSapDocument), OPENV2G_EXI_SAP_OFFERS_MAX,
        "libcbv2g-sap-host-poc/" OPENV2G_CBV2G_COMMIT,
        "upstream appHand; exact XSD edition/provenance OPEN",
        OPENV2G_CBV2G_DIGEST
    };
    const OpenV2g_ExiBackend backend = {NULL, encode, decode, reset};
    return OpenV2g_ExiCodec_Bind(codec, &descriptor, &backend);
}
