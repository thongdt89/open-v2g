#include "openv2g/exi_cbv2g_dc.h"
#include "test.h"
#include <string.h>
static OpenV2g_ExiCodec codec;
static int roundtrip(const OpenV2g_ExiDcDocument *value, OpenV2g_ExiDcDocument *output, bool accepted)
{
    uint8_t bytes[OPENV2G_EXI_DC_PAYLOAD_MAX], workspace[OPENV2G_EXI_DC_WORKSPACE_SIZE];
    OpenV2g_ExiDcDocument candidate; OpenV2g_ExiResult result;
    uint16_t id = (uint16_t)((unsigned)value->kind+(value->response ? OPENV2G_EXI_DC_RESPONSE_OFFSET : 0u));
    OpenV2g_ExiEncodeRequest encode = {OPENV2G_SCHEMA_ISO_15118_2,id,value,sizeof(*value),bytes,sizeof(bytes)};
    OpenV2g_ExiDecodeRequest decode = {OPENV2G_SCHEMA_ISO_15118_2,id,bytes,0u,&candidate,output,sizeof(*output),workspace,sizeof(workspace)};
    OpenV2g_Result encoded = OpenV2g_ExiCodec_Encode(&codec,1u,&encode,&result);
    if (!accepted) { TEST_ASSERT(encoded != OPENV2G_RESULT_OK); return 0; }
    TEST_ASSERT(encoded == OPENV2G_RESULT_OK); decode.input_length = result.encoded_length;
    TEST_ASSERT(OpenV2g_ExiCodec_Decode(&codec,1u,&decode,&result) == OPENV2G_RESULT_OK);
    while (decode.input_length > 1u) {
        OpenV2g_ExiDcDocument before = *output; --decode.input_length;
        TEST_ASSERT(OpenV2g_ExiCodec_Decode(&codec,1u,&decode,&result) != OPENV2G_RESULT_OK && memcmp(&before,output,sizeof(before)) == 0);
    }
    return 0;
}
int main(void)
{
    OpenV2g_SchemaRegistry registry;
    OpenV2g_ExiDcDocument value = OpenV2g_ExiDc_Default(OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY,true), output;
    unsigned i;
    OpenV2g_SchemaRegistry_Init(&registry); OpenV2g_ExiCodec_Init(&codec,&registry);
    TEST_ASSERT(OpenV2g_Cbv2gIso2_Bind(&codec) == OPENV2G_RESULT_OK && OpenV2g_ExiCodec_Start(&codec,1u) == OPENV2G_RESULT_OK);
    value.payment_options = OPENV2G_PAYMENT_EXTERNAL|OPENV2G_PAYMENT_CONTRACT; value.transfer_mode_count = 6u;
    for (i = 0u; i < 6u; ++i) { value.transfer_modes[i] = (OpenV2g_ExiTransferMode)(i+1u); }
    value.charge_service.id = 1u; value.charge_service.category = OPENV2G_SERVICE_CHARGING; value.charge_service.free_service = false;
    value.charge_service.has_name = true; value.charge_service.name_length = 4u; (void)memcpy(value.charge_service.name,"host",4u);
    value.extra_service_count = 4u;
    for (i = 0u; i < 4u; ++i) { value.extra_services[i].id = (uint16_t)(i+2u); value.extra_services[i].category = OPENV2G_SERVICE_CUSTOM; value.extra_services[i].has_scope = true; }
    TEST_ASSERT(roundtrip(&value,&output,true) == 0);
    TEST_ASSERT(output.payment_options == 3u && output.transfer_mode_count == 6u && output.extra_service_count == 4u && !output.charge_service.free_service && output.charge_service.name_length == 4u && output.extra_services[3].has_scope);
    value.extra_services[3].id = 1u; TEST_ASSERT(roundtrip(&value,&output,false) == 0); value.extra_services[3].id = 5u;
    value.transfer_modes[5] = value.transfer_modes[0]; TEST_ASSERT(roundtrip(&value,&output,false) == 0); value.transfer_modes[5] = OPENV2G_TRANSFER_DC_UNIQUE;
    value.transfer_mode_count = 7u; TEST_ASSERT(roundtrip(&value,&output,false) == 0); value.transfer_mode_count = 6u;
    value.payment_options = 4u; TEST_ASSERT(roundtrip(&value,&output,false) == 0); value.payment_options = 3u;
    value.charge_service.name_length = 33u; TEST_ASSERT(roundtrip(&value,&output,false) == 0);
    value = OpenV2g_ExiDc_Default(OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION,false);
    value.selected_payment = OPENV2G_PAYMENT_CONTRACT; value.selected_service_count = 4u;
    for (i = 0u; i < 4u; ++i) { value.selected_services[i].id = (uint16_t)(i+1u); }
    value.selected_services[0].has_parameter_set = true; value.selected_services[0].parameter_set = -7;
    TEST_ASSERT(roundtrip(&value,&output,true) == 0 && output.selected_payment == OPENV2G_PAYMENT_CONTRACT && output.selected_service_count == 4u && output.selected_services[0].parameter_set == -7);
    value.selected_services[3].id = 1u; TEST_ASSERT(roundtrip(&value,&output,false) == 0); value.selected_services[3].id = 4u;
    value.selected_payment = 3u; TEST_ASSERT(roundtrip(&value,&output,false) == 0);
    {
        /* Captured from the unmodified pinned candidate: illegal repeat EE after six modes. */
        const uint8_t bad[] = {0x80u,0x98u,0x02u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x11u,0xc0u,0x01u,0x20u,0x04u,0x82u,0x80u,0x02u,0x08u,0x18u,0x40u,0xa4u,0x80u};
        uint8_t workspace[OPENV2G_EXI_DC_WORKSPACE_SIZE]; OpenV2g_ExiDcDocument candidate, before;
        OpenV2g_ExiResult result; OpenV2g_ExiDecodeRequest decode = {OPENV2G_SCHEMA_ISO_15118_2,102u,bad,sizeof(bad),
            &candidate,&output,sizeof(output),workspace,sizeof(workspace)};
        (void)memset(&output,0xa5,sizeof(output)); before = output;
        TEST_ASSERT(OpenV2g_ExiCodec_Decode(&codec,1u,&decode,&result) != OPENV2G_RESULT_OK && memcmp(&before,&output,sizeof(output)) == 0);
    }
    return 0;
}
