#include "openv2g/exi_cbv2g_dc.h"
#include "test.h"
#include <string.h>
static OpenV2g_ExiCodec codec;
static int roundtrip(OpenV2g_SchemaId schema, const OpenV2g_ExiDcDocument *value, OpenV2g_ExiDcDocument *output, bool accepted)
{
    uint8_t bytes[OPENV2G_EXI_DC_PAYLOAD_MAX], workspace[OPENV2G_EXI_DC_WORKSPACE_SIZE];
    OpenV2g_ExiDcDocument candidate; OpenV2g_ExiResult result;
    OpenV2g_ExiEncodeRequest encode = {schema,115u,value,sizeof(*value),bytes,sizeof(bytes)};
    OpenV2g_ExiDecodeRequest decode = {schema,115u,bytes,0u,&candidate,output,sizeof(*output),workspace,sizeof(workspace)};
    OpenV2g_Result encoded = OpenV2g_ExiCodec_Encode(&codec,1u,&encode,&result);
    if (!accepted) { TEST_ASSERT(encoded != OPENV2G_RESULT_OK); return 0; }
    TEST_ASSERT(encoded == OPENV2G_RESULT_OK); decode.input_length = result.encoded_length;
    TEST_ASSERT(OpenV2g_ExiCodec_Decode(&codec,1u,&decode,&result) == OPENV2G_RESULT_OK);
    while (decode.input_length > 1u) {
        OpenV2g_ExiDcDocument before = *output; --decode.input_length;
        TEST_ASSERT(OpenV2g_ExiCodec_Decode(&codec,1u,&decode,&result) != OPENV2G_RESULT_OK && memcmp(output,&before,sizeof(before)) == 0);
    }
    return 0;
}
static int schema_cases(OpenV2g_SchemaId schema)
{
    OpenV2g_ExiDcDocument value = OpenV2g_ExiDc_Default(OPENV2G_PROTOCOL_MESSAGE_SERVICE_DETAIL,true), output;
    unsigned kind, i; value.parameter_count = 1u;
    value.parameters[0].name_length = 1u; value.parameters[0].name[0] = 'P'; value.parameters[0].set_id = 1;
    for (kind = OPENV2G_PARAM_BOOL; kind <= OPENV2G_PARAM_STRING; ++kind) {
        OpenV2g_ExiParameter *parameter = &value.parameters[0];
        parameter->kind = (OpenV2g_ParameterKind)kind; parameter->integer_value = kind == OPENV2G_PARAM_BOOL ? 1 : -17;
        parameter->text_length = 3u; (void)memcpy(parameter->text,"abc",3u);
        parameter->physical_value = -123; parameter->physical_multiplier = -3; parameter->physical_unit = OPENV2G_PARAM_WATT;
        TEST_ASSERT(roundtrip(schema,&value,&output,true) == 0 && output.parameters[0].kind == parameter->kind);
        if (kind == OPENV2G_PARAM_PHYSICAL) { TEST_ASSERT(output.parameters[0].physical_value == -123 && output.parameters[0].physical_multiplier == -3 && output.parameters[0].physical_unit == OPENV2G_PARAM_WATT); }
        else if (kind != OPENV2G_PARAM_STRING) { TEST_ASSERT(output.parameters[0].integer_value == parameter->integer_value); }
    }
    value.parameters[0].kind = OPENV2G_PARAM_BOOL; value.parameters[0].integer_value = 2;
    TEST_ASSERT(roundtrip(schema,&value,&output,false) == 0);
    value.parameters[0].kind = OPENV2G_PARAM_BYTE; value.parameters[0].integer_value = 128;
    TEST_ASSERT(roundtrip(schema,&value,&output,false) == 0);
    value.parameters[0].kind = OPENV2G_PARAM_SHORT; value.parameters[0].integer_value = 32768;
    TEST_ASSERT(roundtrip(schema,&value,&output,false) == 0);
    value.parameters[0].kind = OPENV2G_PARAM_PHYSICAL; value.parameters[0].physical_multiplier = 4;
    TEST_ASSERT(roundtrip(schema,&value,&output,false) == 0);
    value.parameters[0].physical_multiplier = 3; value.parameters[0].physical_unit = OPENV2G_PARAM_AMPERE_HOUR;
    TEST_ASSERT(roundtrip(schema,&value,&output,schema == OPENV2G_SCHEMA_DIN_70121) == 0);
    value.parameter_count = 5u;
    for (i = 0u; i < 5u; ++i) {
        value.parameters[i] = value.parameters[0]; value.parameters[i].kind = OPENV2G_PARAM_INT;
        value.parameters[i].integer_value = (int32_t)i; value.parameters[i].set_id = 1;
        value.parameters[i].name[0] = (char)('A'+i);
    }
    TEST_ASSERT(roundtrip(schema,&value,&output,schema == OPENV2G_SCHEMA_ISO_15118_2) == 0);
    if (schema == OPENV2G_SCHEMA_ISO_15118_2) { TEST_ASSERT(output.parameter_count == 5u && output.parameters[4].integer_value == 4); }
    value.parameters[4].name[0] = 'A'; TEST_ASSERT(roundtrip(schema,&value,&output,false) == 0);
    for (i = 0u; i < 5u; ++i) { value.parameters[i].set_id = (int16_t)(i+1u); }
    TEST_ASSERT(roundtrip(schema,&value,&output,true) == 0 && output.parameter_count == 5u);
    value.parameter_count = 6u; TEST_ASSERT(roundtrip(schema,&value,&output,false) == 0);
    return 0;
}
int main(void)
{
    OpenV2g_SchemaRegistry registry;
    OpenV2g_SchemaRegistry_Init(&registry); OpenV2g_ExiCodec_Init(&codec,&registry);
    TEST_ASSERT(OpenV2g_Cbv2gIso2_Bind(&codec) == OPENV2G_RESULT_OK && OpenV2g_Cbv2gDin_Bind(&codec) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ExiCodec_Start(&codec,1u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(schema_cases(OPENV2G_SCHEMA_DIN_70121) == 0 && schema_cases(OPENV2G_SCHEMA_ISO_15118_2) == 0); return 0;
}
