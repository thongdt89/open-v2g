#include "openv2g/exi_cbv2g_dc.h"
#include <stdio.h>
#include <string.h>
static OpenV2g_ExiCodec codec;
static int emit(const char *profile, OpenV2g_ExiDcDocument *document)
{
    uint8_t bytes[OPENV2G_EXI_DC_PAYLOAD_MAX];
    OpenV2g_ExiResult result;
    size_t index;
    uint16_t id = (uint16_t)((unsigned)document->kind + (document->response ? OPENV2G_EXI_DC_RESPONSE_OFFSET : 0u));
    OpenV2g_ExiEncodeRequest request = {OPENV2G_SCHEMA_ISO_15118_2, id, document, sizeof(*document), bytes, sizeof(bytes)};
    if (OpenV2g_ExiCodec_Encode(&codec, 1u, &request, &result) != OPENV2G_RESULT_OK) { return 1; }
    (void)printf("%s %u ", profile, id);
    for (index = 0u; index < result.encoded_length; ++index) { (void)printf("%02x", bytes[index]); }
    (void)putchar('\n'); return 0;
}
static void meter(OpenV2g_ExiDcDocument *document)
{
    document->has_meter_info = true;
    document->meter_info.id_length = 6u; (void)memcpy(document->meter_info.id, "METER1", 6u);
    document->meter_info.has_reading = true; document->meter_info.reading_wh = 12345u;
    document->meter_info.has_status = true; document->meter_info.status = 1;
    document->meter_info.has_time = true; document->meter_info.time_s = 1700000000;
}
int main(void)
{
    OpenV2g_SchemaRegistry registry;
    static const OpenV2g_ProtocolMessageKind ac_kinds[] = {
        OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY, OPENV2G_PROTOCOL_MESSAGE_SERVICE_DETAIL,
        OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER, OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START,
        OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS, OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT,
        OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP, OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP
    };
    size_t index;
    unsigned response;
    OpenV2g_SchemaRegistry_Init(&registry); OpenV2g_ExiCodec_Init(&codec, &registry);
    if (OpenV2g_Cbv2gIso2_Bind(&codec) != OPENV2G_RESULT_OK || OpenV2g_ExiCodec_Start(&codec, 1u) != OPENV2G_RESULT_OK) { return 1; }
    for (index = 0u; index < sizeof(ac_kinds) / sizeof(ac_kinds[0]); ++index) {
        for (response = 0u; response < 2u; ++response) {
            OpenV2g_ExiDcDocument document = OpenV2g_ExiDc_Default(ac_kinds[index], response != 0u);
            document.ac_mode = true; document.voltage_mv = 230000u; document.max_voltage_mv = 250000u;
            document.max_current_ma = 32000; document.min_current_ma = 6000;
            document.ac_energy_wh = 1000u; document.schedule_power_w = 7000;
            if (document.kind == OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS && document.response) { meter(&document); document.receipt_required = true; }
            if (document.kind == OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT && !document.response) { meter(&document); }
            if (document.kind == OPENV2G_PROTOCOL_MESSAGE_SERVICE_DETAIL && document.response) {
                document.parameter_count = 1u; document.parameters[0].name_length = 4u;
                (void)memcpy(document.parameters[0].name, "host", 4u);
                document.parameters[0].set_id = 1; document.parameters[0].integer_value = 1;
            }
            if (emit("iso2-ac", &document) != 0) { return 1; }
        }
    }
    {
        OpenV2g_ExiDcDocument document = OpenV2g_ExiDc_Default(OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND, true);
        meter(&document); document.receipt_required = true;
        if (emit("iso2-meter", &document) != 0) { return 1; }
        document.kind = OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT; document.response = false; document.receipt_required = false;
        if (emit("iso2-meter", &document) != 0) { return 1; }
        document = OpenV2g_ExiDc_Default(OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT, true);
        if (emit("iso2-meter", &document) != 0) { return 1; }
    }
    return 0;
}
