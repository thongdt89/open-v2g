#include "openv2g/exi_cbv2g_dc.h"
#include "test.h"
#include <string.h>

static const OpenV2g_ProtocolMessageKind kinds[] = {
    OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP, OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY, OPENV2G_PROTOCOL_MESSAGE_SERVICE_DETAIL,
    OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION, OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION,
    OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER, OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK,
    OPENV2G_PROTOCOL_MESSAGE_PRECHARGE, OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START,
    OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND, OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP,
    OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION, OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP
};
static uint16_t type_id(const OpenV2g_ExiDcDocument *document)
{
    return (uint16_t)((unsigned)document->kind + (document->response ? OPENV2G_EXI_DC_RESPONSE_OFFSET : 0u));
}
static int test_schema(OpenV2g_SchemaId schema)
{
    OpenV2g_SchemaRegistry registry;
    OpenV2g_ExiCodec codec;
    uint8_t encoded[OPENV2G_EXI_DC_PAYLOAD_MAX];
    uint8_t workspace[OPENV2G_EXI_DC_WORKSPACE_SIZE];
    size_t index;
    unsigned response;
    OpenV2g_SchemaRegistry_Init(&registry); OpenV2g_ExiCodec_Init(&codec, &registry);
    TEST_ASSERT((schema == OPENV2G_SCHEMA_DIN_70121 ? OpenV2g_Cbv2gDin_Bind(&codec) : OpenV2g_Cbv2gIso2_Bind(&codec)) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ExiCodec_Start(&codec, 1u) == OPENV2G_RESULT_OK);
    for (index = 0u; index < sizeof(kinds) / sizeof(kinds[0]); ++index) {
        for (response = 0u; response < 2u; ++response) {
            OpenV2g_ExiDcDocument document = OpenV2g_ExiDc_Default(kinds[index], response != 0u);
            OpenV2g_ExiDcDocument output, candidate;
            OpenV2g_ExiResult result;
            OpenV2g_ExiEncodeRequest encode = {schema, type_id(&document), &document, sizeof(document), encoded, sizeof(encoded)};
            OpenV2g_ExiDecodeRequest decode = {schema, type_id(&document), encoded, 0u, &candidate, &output, sizeof(output), workspace, sizeof(workspace)};
            size_t length;
            if (document.response && document.kind == OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP) {
                decode.expected_message_type_id = OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START + OPENV2G_EXI_DC_RESPONSE_OFFSET;
            }
            if (document.kind == OPENV2G_PROTOCOL_MESSAGE_SERVICE_DETAIL && document.response) {
                document.parameter_count = 2u;
                document.parameters[0].set_id = 1; document.parameters[0].name_length = 3u;
                (void)memcpy(document.parameters[0].name, "int", 3u); document.parameters[0].integer_value = -17;
                document.parameters[1].set_id = 2; document.parameters[1].name_length = 3u;
                (void)memcpy(document.parameters[1].name, "str", 3u); document.parameters[1].is_string = true;
                document.parameters[1].text_length = 4u; (void)memcpy(document.parameters[1].text, "host", 4u);
            }
            if (document.kind == OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND) {
                document.charging_complete = true;
                if (document.response) {
                    document.has_max_voltage = document.has_max_current = document.has_max_power = true;
                    document.max_voltage_mv = 390000u; document.max_current_ma = 70000; document.max_power_w = 24000;
                    document.current_limit_achieved = document.voltage_limit_achieved = document.power_limit_achieved = true;
                }
            }
            if (schema == OPENV2G_SCHEMA_ISO_15118_2) {
                if (document.kind == OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP) { document.pause_session = true; }
                if (document.kind == OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP && document.response) { document.new_session = false; document.resumed_session = true; }
                if (document.kind == OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION && document.response) { document.waiting_for_customer = true; }
            }
            TEST_ASSERT(OpenV2g_ExiCodec_Encode(&codec, 1u, &encode, &result) == OPENV2G_RESULT_OK);
            length = result.encoded_length; decode.input_length = length;
            if (OpenV2g_ExiCodec_Decode(&codec, 1u, &decode, &result) != OPENV2G_RESULT_OK) {
                (void)fprintf(stderr, "DC decode failed schema=%d kind=%d response=%u\n", (int)schema, (int)document.kind, response); return 1;
            }
            TEST_ASSERT(output.response == document.response);
            if (document.kind == OPENV2G_PROTOCOL_MESSAGE_SERVICE_DETAIL && document.response) {
                TEST_ASSERT(output.parameter_count == 2u && output.parameters[0].integer_value == -17);
                TEST_ASSERT(output.parameters[1].is_string && output.parameters[1].text_length == 4u && memcmp(output.parameters[1].text, "host", 4u) == 0);
            }

            if (document.kind == OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND) {
                if (!document.response) { TEST_ASSERT(output.charging_complete); }
                else {
                    TEST_ASSERT(output.has_max_voltage && output.has_max_current && output.has_max_power);
                    TEST_ASSERT(output.max_voltage_mv == 390000u && output.max_current_ma == 70000 && output.max_power_w == 24000);
                    TEST_ASSERT(output.current_limit_achieved && output.voltage_limit_achieved && output.power_limit_achieved);
                }
            }
            if (schema == OPENV2G_SCHEMA_ISO_15118_2) {
                if (document.kind == OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP && !document.response) { TEST_ASSERT(output.pause_session); }
                if (document.kind == OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP && document.response) { TEST_ASSERT(output.accepted && output.resumed_session && !output.new_session); }
                if (document.kind == OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION && document.response) { TEST_ASSERT(output.ongoing && output.waiting_for_customer); }
            }

            TEST_ASSERT(output.kind == document.kind || (document.response && document.kind == OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP && output.kind == OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START));
            if (document.kind == OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER && document.response) {
                document.ongoing = true; document.omit_pending_parameters = true; document.omit_pending_schedules = true;
                TEST_ASSERT(OpenV2g_ExiCodec_Encode(&codec, 1u, &encode, &result) == OPENV2G_RESULT_OK);
                decode.input_length = result.encoded_length;
                TEST_ASSERT(OpenV2g_ExiCodec_Decode(&codec, 1u, &decode, &result) == OPENV2G_RESULT_OK);
                TEST_ASSERT(output.ongoing && output.omit_pending_parameters && output.omit_pending_schedules);
                document.ongoing = false;
                TEST_ASSERT(OpenV2g_ExiCodec_Encode(&codec, 1u, &encode, &result) == OPENV2G_RESULT_REJECTED);
                document.omit_pending_parameters = false; document.omit_pending_schedules = false;
                document.min_voltage_mv = 100000u; document.min_current_ma = 1000;
                TEST_ASSERT(OpenV2g_ExiCodec_Encode(&codec, 1u, &encode, &result) == OPENV2G_RESULT_OK);
                decode.input_length = result.encoded_length;
                TEST_ASSERT(OpenV2g_ExiCodec_Decode(&codec, 1u, &decode, &result) == OPENV2G_RESULT_OK);
                TEST_ASSERT(output.min_voltage_mv == 100000u && output.min_current_ma == 1000);
                document.schedule_count = 2u;
                document.schedules[0].id = 1u; document.schedules[0].count = 2u;
                document.schedules[0].points[0] = (OpenV2g_ExiSchedulePoint){0u, 10u, 20000};
                document.schedules[0].points[1] = (OpenV2g_ExiSchedulePoint){10u, 20u, 10000};
                document.schedules[1].id = 2u; document.schedules[1].count = 1u;
                document.schedules[1].points[0] = (OpenV2g_ExiSchedulePoint){0u, 30u, 15000};
                TEST_ASSERT(OpenV2g_ExiCodec_Encode(&codec, 1u, &encode, &result) == OPENV2G_RESULT_OK);
                decode.input_length = result.encoded_length;
                TEST_ASSERT(OpenV2g_ExiCodec_Decode(&codec, 1u, &decode, &result) == OPENV2G_RESULT_OK);
                TEST_ASSERT(output.schedule_count == 2u && output.schedules[0].count == 2u && output.schedules[1].id == 2u);
                TEST_ASSERT(output.schedule_duration_s == 30u && output.schedule_power_w == 10000);
                document.schedules[1].id = 1u;
                TEST_ASSERT(OpenV2g_ExiCodec_Encode(&codec, 1u, &encode, &result) == OPENV2G_RESULT_REJECTED);
                document.schedules[1].id = 2u; document.schedules[0].points[1].start_s = 11u;
                TEST_ASSERT(OpenV2g_ExiCodec_Encode(&codec, 1u, &encode, &result) == OPENV2G_RESULT_REJECTED);
                document.schedules[0].points[1].start_s = 10u;
                TEST_ASSERT(OpenV2g_ExiCodec_Encode(&codec, 1u, &encode, &result) == OPENV2G_RESULT_OK);
                length = result.encoded_length;
            }
            for (decode.input_length = 1u; decode.input_length < length; ++decode.input_length) {
                OpenV2g_ExiDcDocument before;
                (void)memset(&output, 0xa5, sizeof(output)); (void)memcpy(&before, &output, sizeof(before));
                TEST_ASSERT(OpenV2g_ExiCodec_Decode(&codec, 1u, &decode, &result) != OPENV2G_RESULT_OK);
                TEST_ASSERT(memcmp(&output, &before, sizeof(output)) == 0);
            }
        }
    }
    /* Deterministic malformed corpus, separately selected schema, never infer
     * schema from bytes. Sanitizers instrument the generated decoders too. */
    {
        uint32_t random = 0x70121182u;
        unsigned iteration;
        for (iteration = 0u; iteration < 10000u; ++iteration) {
            OpenV2g_ExiDcDocument output, candidate, before;
            OpenV2g_ExiResult result;
            size_t length = (size_t)iteration % sizeof(encoded) + 1u;
            size_t byte;
            OpenV2g_ExiDecodeRequest decode = {schema, 1u, encoded, length, &candidate, &output,
                sizeof(output), workspace, sizeof(workspace)};
            for (byte = 0u; byte < length; ++byte) {
                random = random * 1664525u + 1013904223u; encoded[byte] = (uint8_t)(random >> 24u);
            }
            if (iteration % 2u == 0u) { encoded[0] = 0x80u; }
            (void)memset(&output, 0xa5, sizeof(output)); (void)memcpy(&before, &output, sizeof(before));
            if (OpenV2g_ExiCodec_Decode(&codec, 1u, &decode, &result) != OPENV2G_RESULT_OK) {
                TEST_ASSERT(memcmp(&output, &before, sizeof(output)) == 0);
            }
        }
    }
    return 0;
}
int main(void)
{
    TEST_ASSERT(test_schema(OPENV2G_SCHEMA_DIN_70121) == 0);
    TEST_ASSERT(test_schema(OPENV2G_SCHEMA_ISO_15118_2) == 0);
    return 0;
}
