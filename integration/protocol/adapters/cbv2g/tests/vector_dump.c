#include "openv2g/exi_cbv2g_dc.h"
#include "openv2g/exi_cbv2g_sap.h"
#include <stdio.h>
#include <string.h>
static void print_vector(const char *schema, unsigned id, const uint8_t *bytes, size_t length)
{
    size_t index; (void)printf("%s %u ", schema, id);
    for (index = 0u; index < length; ++index) { (void)printf("%02x", bytes[index]); }
    (void)putchar('\n');
}
int main(void)
{
    OpenV2g_SchemaRegistry registry;
    OpenV2g_ExiCodec codec;
    uint8_t bytes[OPENV2G_EXI_DC_PAYLOAD_MAX];
    unsigned schema, kind, response;
    OpenV2g_SchemaRegistry_Init(&registry); OpenV2g_ExiCodec_Init(&codec, &registry);
    if (OpenV2g_Cbv2gSap_Bind(&codec) != OPENV2G_RESULT_OK || OpenV2g_Cbv2gDin_Bind(&codec) != OPENV2G_RESULT_OK ||
        OpenV2g_Cbv2gIso2_Bind(&codec) != OPENV2G_RESULT_OK || OpenV2g_ExiCodec_Start(&codec, 1u) != OPENV2G_RESULT_OK) { return 1; }
    for (kind = 1u; kind <= 2u; ++kind) {
        OpenV2g_ExiSapDocument document;
        OpenV2g_ExiResult result;
        OpenV2g_ExiEncodeRequest request = {OPENV2G_SCHEMA_SAP, (uint16_t)kind, &document, sizeof(document), bytes, sizeof(bytes)};
        (void)memset(&document, 0, sizeof(document)); document.message_type_id = (uint16_t)kind;
        if (kind == 1u) {
            const char namespace_text[] = "urn:din:70121:2012:MsgDef";
            document.body.request.count = 1u;
            document.body.request.offers[0].namespace_length = sizeof(namespace_text) - 1u;
            (void)memcpy(document.body.request.offers[0].namespace_text, namespace_text, sizeof(namespace_text) - 1u);
            document.body.request.offers[0].version_major = 2u;
            document.body.request.offers[0].priority = 1u; document.body.request.offers[0].wire_schema_id = 10u;
        } else {
            document.body.response.code = OPENV2G_EXI_SAP_SUCCESS;
            document.body.response.has_schema_id = true; document.body.response.wire_schema_id = 10u;
        }
        if (OpenV2g_ExiCodec_Encode(&codec, 1u, &request, &result) != OPENV2G_RESULT_OK) { return 1; }
        print_vector("sap", kind, bytes, result.encoded_length);
    }
    for (schema = 0u; schema < 2u; ++schema) {
        for (kind = 1u; kind <= 14u; ++kind) {
            if (kind == 10u || kind == 11u) { continue; }
            for (response = 0u; response < 2u; ++response) {
                OpenV2g_ExiDcDocument document = OpenV2g_ExiDc_Default((OpenV2g_ProtocolMessageKind)kind, response != 0u);
                /* Historical differential fixture predates zero-voltage welding plant. */
                if (kind == OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION) { document.voltage_mv = 400000u; }
                OpenV2g_ExiResult result;
                OpenV2g_ExiEncodeRequest request = {
                    schema == 0u ? OPENV2G_SCHEMA_DIN_70121 : OPENV2G_SCHEMA_ISO_15118_2,
                    (uint16_t)(kind + (response ? OPENV2G_EXI_DC_RESPONSE_OFFSET : 0u)),
                    &document, sizeof(document), bytes, sizeof(bytes)
                };
                if (OpenV2g_ExiCodec_Encode(&codec, 1u, &request, &result) != OPENV2G_RESULT_OK) { return 1; }
                print_vector(schema == 0u ? "din" : "iso2", request.message_type_id, bytes, result.encoded_length);
            }
        }
    }
    return 0;
}
