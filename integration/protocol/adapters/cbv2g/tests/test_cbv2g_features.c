#include "openv2g/exi_cbv2g_dc.h"
#include "test.h"
#include <limits.h>
#include <string.h>

static OpenV2g_SchemaRegistry registry;
static OpenV2g_ExiCodec codec;
static uint8_t bytes[OPENV2G_EXI_DC_PAYLOAD_MAX], workspace[OPENV2G_EXI_DC_WORKSPACE_SIZE];
static uint16_t type_id(const OpenV2g_ExiDcDocument *value)
{
    return (uint16_t)((unsigned)value->kind + (value->response ? OPENV2G_EXI_DC_RESPONSE_OFFSET : 0u));
}
static int roundtrip(OpenV2g_ExiDcDocument *document, OpenV2g_ExiDcDocument *output)
{
    OpenV2g_ExiDcDocument candidate;
    OpenV2g_ExiResult result;
    OpenV2g_ExiEncodeRequest encode = {OPENV2G_SCHEMA_ISO_15118_2, type_id(document), document,
        sizeof(*document), bytes, sizeof(bytes)};
    OpenV2g_ExiDecodeRequest decode = {OPENV2G_SCHEMA_ISO_15118_2, type_id(document), bytes, 0u,
        &candidate, output, sizeof(*output), workspace, sizeof(workspace)};
    size_t length;
    TEST_ASSERT(OpenV2g_ExiCodec_Encode(&codec, 1u, &encode, &result) == OPENV2G_RESULT_OK);
    length = result.encoded_length; decode.input_length = length;
    TEST_ASSERT(OpenV2g_ExiCodec_Decode(&codec, 1u, &decode, &result) == OPENV2G_RESULT_OK);
    for (decode.input_length = 1u; decode.input_length < length; ++decode.input_length) {
        OpenV2g_ExiDcDocument untouched = *output;
        TEST_ASSERT(OpenV2g_ExiCodec_Decode(&codec, 1u, &decode, &result) != OPENV2G_RESULT_OK);
        TEST_ASSERT(memcmp(output, &untouched, sizeof(*output)) == 0);
    }
    return 0;
}
static int rejected(OpenV2g_ExiDcDocument *document)
{
    OpenV2g_ExiResult result;
    OpenV2g_ExiEncodeRequest request = {OPENV2G_SCHEMA_ISO_15118_2, type_id(document), document,
        sizeof(*document), bytes, sizeof(bytes)};
    TEST_ASSERT(OpenV2g_ExiCodec_Encode(&codec, 1u, &request, &result) == OPENV2G_RESULT_REJECTED);
    return 0;
}
static int metering(void)
{
    OpenV2g_ExiDcDocument document = OpenV2g_ExiDc_Default(OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND, true), output;
    unsigned index;
    document.has_meter_info = true; document.receipt_required = true;
    document.meter_info.id_length = OPENV2G_EXI_METER_ID_MAX;
    (void)memset(document.meter_info.id, 'M', sizeof(document.meter_info.id));
    document.meter_info.has_reading = true; document.meter_info.reading_wh = UINT64_MAX;
    document.meter_info.has_time = true; document.meter_info.time_s = INT64_MIN;
    document.meter_info.has_status = true; document.meter_info.status = INT16_MIN;
    document.meter_info.signature_length = OPENV2G_EXI_METER_SIGNATURE_MAX;
    for (index = 0u; index < sizeof(document.meter_info.signature); ++index) { document.meter_info.signature[index] = (uint8_t)index; }
    TEST_ASSERT(roundtrip(&document, &output) == 0);
    TEST_ASSERT(output.has_meter_info && output.receipt_required && memcmp(&document.meter_info, &output.meter_info, sizeof(output.meter_info)) == 0);
    document.kind = OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT; document.response = false;
    document.receipt_required = false; document.session_id[0] = 7u;
    TEST_ASSERT(roundtrip(&document, &output) == 0);
    TEST_ASSERT(output.kind == document.kind && output.session_id[0] == 7u && output.schedule_id == document.schedule_id);
    TEST_ASSERT(memcmp(&document.meter_info, &output.meter_info, sizeof(output.meter_info)) == 0);
    document.meter_info.id_length = OPENV2G_EXI_METER_ID_MAX + 1u;
    TEST_ASSERT(rejected(&document) == 0);
    document.meter_info.id_length = 1u; document.meter_info.signature_length = OPENV2G_EXI_METER_SIGNATURE_MAX + 1u;
    TEST_ASSERT(rejected(&document) == 0);
    document.meter_info.signature_length = 0u; document.has_meter_info = false;
    TEST_ASSERT(rejected(&document) == 0);
    document = OpenV2g_ExiDc_Default(OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT, true);
    TEST_ASSERT(roundtrip(&document, &output) == 0);
    TEST_ASSERT(output.response && output.accepted && output.ready);
    return 0;
}
static int ac_profile(void)
{
    static const OpenV2g_ProtocolMessageKind kinds[] = {
        OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY, OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER,
        OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START, OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS,
        OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT
    };
    size_t i;
    unsigned response;
    for (i = 0u; i < sizeof(kinds) / sizeof(kinds[0]); ++i) {
        for (response = 0u; response < 2u; ++response) {
            OpenV2g_ExiDcDocument document = OpenV2g_ExiDc_Default(kinds[i], response != 0u), output;
            document.ac_mode = true; document.voltage_mv = 230000u; document.max_voltage_mv = 250000u;
            document.max_current_ma = 32000; document.min_current_ma = 6000;
            document.ac_energy_wh = 1000u; document.schedule_power_w = 7000;
            if (document.kind == OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT && !document.response) {
                document.has_meter_info = true; document.meter_info.id_length = 1u; document.meter_info.id[0] = 'M';
            }
            TEST_ASSERT(roundtrip(&document, &output) == 0);
            if (document.response || document.kind == OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER ||
                document.kind == OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START || document.kind == OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS) { TEST_ASSERT(output.ac_mode); }
            if (document.kind == OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER && !document.response) {
                TEST_ASSERT(output.ac_energy_wh == 1000u && output.max_voltage_mv == 250000u && output.max_current_ma == 32000 && output.min_current_ma == 6000);
            }
            if (document.kind == OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS && document.response) {
                document.rcd_fault = true;
                TEST_ASSERT(roundtrip(&document, &output) == 0); TEST_ASSERT(output.rcd_fault && !output.ready);
            }
        }
    }
    return 0;
}
int main(void)
{
    OpenV2g_SchemaRegistry_Init(&registry); OpenV2g_ExiCodec_Init(&codec, &registry);
    TEST_ASSERT(OpenV2g_Cbv2gIso2_Bind(&codec) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ExiCodec_Start(&codec, 1u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(metering() == 0);
    TEST_ASSERT(ac_profile() == 0);
    {
        OpenV2g_ExiDcDocument document = OpenV2g_ExiDc_Default(OPENV2G_PROTOCOL_MESSAGE_RENEGOTIATE, false), output;
        TEST_ASSERT(roundtrip(&document, &output) == 0 && output.kind == OPENV2G_PROTOCOL_MESSAGE_RENEGOTIATE);
        document.ac_mode = true;
        TEST_ASSERT(roundtrip(&document, &output) == 0 && output.ac_mode && output.kind == OPENV2G_PROTOCOL_MESSAGE_RENEGOTIATE);
    }
    return 0;
}
