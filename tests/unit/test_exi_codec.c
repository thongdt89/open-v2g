#include <string.h>

#include "openv2g/iso15118_chrg/exi_codec.h"
#include "test.h"

typedef struct {
    uint32_t value;
} TestTypedMessage;

typedef struct {
    unsigned int reset_count;
} TestBackendContext;

static OpenV2g_Result fake_encode(
    void *context,
    uint16_t message_type_id,
    const void *typed_object,
    size_t typed_object_size,
    uint8_t *output,
    size_t output_capacity,
    size_t *encoded_length,
    OpenV2g_ExiDiagnostic *diagnostic)
{
    const TestTypedMessage *message = (const TestTypedMessage *)typed_object;
    (void)context;
    (void)diagnostic;
    if ((typed_object_size != sizeof(*message)) || (output_capacity < 2u)) {
        return OPENV2G_RESULT_OVERFLOW;
    }
    output[0] = (uint8_t)message_type_id;
    output[1] = (uint8_t)message->value;
    *encoded_length = 2u;
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result fake_decode(
    void *context,
    const uint8_t *input,
    size_t input_length,
    void *candidate_object,
    size_t object_capacity,
    uint8_t *workspace,
    size_t workspace_capacity,
    uint16_t *decoded_message_type_id,
    OpenV2g_ExiDiagnostic *diagnostic)
{
    TestTypedMessage *message = (TestTypedMessage *)candidate_object;
    (void)context;
    (void)diagnostic;
    if ((input_length < 2u) || (object_capacity != sizeof(*message)) ||
        (workspace_capacity < 1u)) {
        return OPENV2G_RESULT_REJECTED;
    }
    workspace[0] = 0xA5u;
    message->value = input[1];
    *decoded_message_type_id = input[0];
    if (input[0] == 0xFFu) {
        return OPENV2G_RESULT_REJECTED;
    }
    return OPENV2G_RESULT_OK;
}

static void fake_reset(
    void *context,
    uint8_t *workspace,
    size_t workspace_capacity)
{
    TestBackendContext *backend_context = (TestBackendContext *)context;
    backend_context->reset_count++;
    (void)memset(workspace, 0xCC, workspace_capacity);
}

static OpenV2g_SchemaDescriptor sap_descriptor(void)
{
    OpenV2g_SchemaDescriptor descriptor;
    (void)memset(&descriptor, 0, sizeof(descriptor));
    descriptor.schema = OPENV2G_SCHEMA_SAP;
    descriptor.protocol = OPENV2G_PROTOCOL_NONE;
    descriptor.mode = OPENV2G_MODE_NONE;
    descriptor.max_payload = 8u;
    descriptor.max_workspace = 16u;
    descriptor.max_typed_object = sizeof(TestTypedMessage);
    descriptor.max_collection = 4u;
    descriptor.implementation_id = "test-backend";
    descriptor.schema_revision = "test-only";
    descriptor.generated_checksum = "not-production";
    return descriptor;
}

static OpenV2g_ExiBackend fake_backend(TestBackendContext *context)
{
    OpenV2g_ExiBackend backend;
    backend.context = context;
    backend.encode = fake_encode;
    backend.decode = fake_decode;
    backend.reset = fake_reset;
    return backend;
}

static int TestRegistryIsExplicitAndFailClosed(void)
{
    OpenV2g_SchemaRegistry registry;
    OpenV2g_SchemaDescriptor descriptor = sap_descriptor();
    OpenV2g_SchemaDescriptor copied_descriptor;
    OpenV2g_SchemaLimits limits;
    OpenV2g_SchemaRegistryCsPort cs_port;
    OpenV2g_SchemaRegistryStatusSrPort status_port;
    OpenV2g_SchemaRegistryStatus status;
    const OpenV2g_SchemaDescriptor *found = NULL;

    OpenV2g_SchemaRegistry_Init(&registry);
    TEST_ASSERT(OpenV2g_SchemaRegistry_CreateClientServerPort(
                    &registry, &cs_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_SchemaRegistry_CreateStatusSenderReceiverPort(
                    &registry, &status_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(status_port.read(status_port.context, &status) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(!status.ready);
    TEST_ASSERT(status.sequence_counter == 0u);
    TEST_ASSERT(OpenV2g_SchemaRegistry_Get(
                    &registry, OPENV2G_SCHEMA_SAP, &found) ==
                OPENV2G_RESULT_NOT_IMPLEMENTED);
    TEST_ASSERT(found == NULL);
    TEST_ASSERT(OpenV2g_SchemaRegistry_Register(&registry, &descriptor) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_SchemaRegistry_Register(&registry, &descriptor) ==
                OPENV2G_RESULT_BUSY);
    TEST_ASSERT(cs_port.get_schema_descriptor(
                    cs_port.context,
                    OPENV2G_SCHEMA_SAP,
                    &copied_descriptor) == OPENV2G_RESULT_OK);
    TEST_ASSERT(copied_descriptor.schema == OPENV2G_SCHEMA_SAP);
    TEST_ASSERT(cs_port.get_schema_limits(
                    cs_port.context,
                    OPENV2G_SCHEMA_SAP,
                    &limits) == OPENV2G_RESULT_OK);
    TEST_ASSERT(limits.max_workspace == 16u);
    TEST_ASSERT(cs_port.check_protocol_compatibility(
                    cs_port.context,
                    OPENV2G_PROTOCOL_NONE,
                    OPENV2G_SCHEMA_SAP,
                    OPENV2G_MODE_NONE) == OPENV2G_RESULT_OK);
    TEST_ASSERT(cs_port.check_protocol_compatibility(
                    cs_port.context,
                    OPENV2G_PROTOCOL_ISO_15118_2,
                    OPENV2G_SCHEMA_SAP,
                    OPENV2G_MODE_DC) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(status_port.read(status_port.context, &status) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(status.ready);
    TEST_ASSERT(status.sequence_counter == 1u);
    TEST_ASSERT(OpenV2g_SchemaRegistry_MarkIntegrityError(
                    &registry, OPENV2G_SCHEMA_SAP) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_SchemaRegistry_Get(
                    &registry, OPENV2G_SCHEMA_SAP, &found) ==
                OPENV2G_RESULT_NOT_IMPLEMENTED);
    TEST_ASSERT(status_port.read(status_port.context, &status) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(!status.ready);
    TEST_ASSERT(status.sequence_counter == 2u);
    TEST_ASSERT(status.integrity_error_mask != 0u);
    return 0;
}

static int TestEncodeDecodeRoundTrip(void)
{
    OpenV2g_SchemaRegistry registry;
    OpenV2g_ExiCodec codec;
    OpenV2g_SchemaDescriptor descriptor = sap_descriptor();
    TestBackendContext context = {0u};
    OpenV2g_ExiBackend backend = fake_backend(&context);
    OpenV2g_ExiCodecCsPort cs_port;
    OpenV2g_ExiDiagnosticSrPort diagnostic_port;
    OpenV2g_ExiDiagnosticSample diagnostic_sample;
    TestTypedMessage input = {42u};
    TestTypedMessage candidate = {0u};
    TestTypedMessage output = {0u};
    uint8_t encoded[8];
    uint8_t workspace[16];
    OpenV2g_ExiEncodeRequest encode_request;
    OpenV2g_ExiDecodeRequest decode_request;
    OpenV2g_ExiResult result;

    OpenV2g_SchemaRegistry_Init(&registry);
    OpenV2g_ExiCodec_Init(&codec, &registry);
    TEST_ASSERT(OpenV2g_ExiCodec_Bind(&codec, &descriptor, &backend) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ExiCodec_Start(&codec, 17u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ExiCodec_CreateClientServerPort(&codec, &cs_port) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ExiCodec_CreateDiagnosticSenderReceiverPort(
                    &codec, &diagnostic_port) == OPENV2G_RESULT_OK);

    (void)memset(&encode_request, 0, sizeof(encode_request));
    encode_request.schema = OPENV2G_SCHEMA_SAP;
    encode_request.message_type_id = 3u;
    encode_request.typed_object = &input;
    encode_request.typed_object_size = sizeof(input);
    encode_request.output = encoded;
    encode_request.output_capacity = sizeof(encoded);
    TEST_ASSERT(cs_port.encode(
                    cs_port.context, 17u, &encode_request, &result) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(result.encoded_length == 2u);

    (void)memset(workspace, 0x55, sizeof(workspace));
    (void)memset(&decode_request, 0, sizeof(decode_request));
    decode_request.schema = OPENV2G_SCHEMA_SAP;
    decode_request.expected_message_type_id = 3u;
    decode_request.input = encoded;
    decode_request.input_length = result.encoded_length;
    decode_request.candidate_object = &candidate;
    decode_request.output_object = &output;
    decode_request.object_size = sizeof(output);
    decode_request.workspace = workspace;
    decode_request.workspace_capacity = sizeof(workspace);
    TEST_ASSERT(cs_port.decode(
                    cs_port.context, 17u, &decode_request, &result) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(output.value == input.value);
    TEST_ASSERT(result.message_type_id == 3u);
    TEST_ASSERT(diagnostic_port.read(
                    diagnostic_port.context, &diagnostic_sample) ==
                OPENV2G_RESULT_NOT_IMPLEMENTED);
    TEST_ASSERT(cs_port.reset_workspace(
                    cs_port.context,
                    17u,
                    OPENV2G_SCHEMA_SAP,
                    workspace,
                    sizeof(workspace)) == OPENV2G_RESULT_OK);
    TEST_ASSERT(context.reset_count == 1u);
    TEST_ASSERT(workspace[0] == 0u);
    TEST_ASSERT(OpenV2g_ExiCodec_Stop(&codec, 17u) == OPENV2G_RESULT_OK);
    return 0;
}

static int TestFailureDoesNotPublishPartialObject(void)
{
    OpenV2g_SchemaRegistry registry;
    OpenV2g_ExiCodec codec;
    OpenV2g_SchemaDescriptor descriptor = sap_descriptor();
    TestBackendContext context = {0u};
    OpenV2g_ExiBackend backend = fake_backend(&context);
    OpenV2g_ExiCodecCsPort cs_port;
    OpenV2g_ExiDiagnosticSrPort diagnostic_port;
    OpenV2g_ExiDiagnosticSample diagnostic_sample;
    TestTypedMessage candidate = {0u};
    TestTypedMessage output = {0x12345678u};
    uint8_t malformed[] = {0xFFu, 0x99u};
    uint8_t workspace[16];
    OpenV2g_ExiDecodeRequest request;
    OpenV2g_ExiResult result;
    size_t index;

    OpenV2g_SchemaRegistry_Init(&registry);
    OpenV2g_ExiCodec_Init(&codec, &registry);
    TEST_ASSERT(OpenV2g_ExiCodec_Bind(&codec, &descriptor, &backend) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ExiCodec_Start(&codec, 18u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ExiCodec_CreateClientServerPort(&codec, &cs_port) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ExiCodec_CreateDiagnosticSenderReceiverPort(
                    &codec, &diagnostic_port) == OPENV2G_RESULT_OK);
    (void)memset(workspace, 0x55, sizeof(workspace));
    (void)memset(&request, 0, sizeof(request));
    request.schema = OPENV2G_SCHEMA_SAP;
    request.expected_message_type_id = 3u;
    request.input = malformed;
    request.input_length = sizeof(malformed);
    request.candidate_object = &candidate;
    request.output_object = &output;
    request.object_size = sizeof(output);
    request.workspace = workspace;
    request.workspace_capacity = sizeof(workspace);

    TEST_ASSERT(cs_port.decode(
                    cs_port.context, 18u, &request, &result) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(output.value == 0x12345678u);
    TEST_ASSERT(candidate.value == 0u);
    TEST_ASSERT(context.reset_count == 1u);
    TEST_ASSERT(diagnostic_port.read(
                    diagnostic_port.context, &diagnostic_sample) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(diagnostic_sample.session_generation == 18u);
    TEST_ASSERT(diagnostic_sample.sequence_counter == 1u);
    TEST_ASSERT(diagnostic_sample.value.schema == OPENV2G_SCHEMA_SAP);
    TEST_ASSERT(diagnostic_sample.value.reason == OPENV2G_REASON_FORMAT);
    for (index = 0u; index < sizeof(workspace); ++index) {
        TEST_ASSERT(workspace[index] == 0u);
    }
    return 0;
}

static int TestGenerationAndBounds(void)
{
    OpenV2g_SchemaRegistry registry;
    OpenV2g_ExiCodec codec;
    OpenV2g_SchemaDescriptor descriptor = sap_descriptor();
    TestBackendContext context = {0u};
    OpenV2g_ExiBackend backend = fake_backend(&context);
    OpenV2g_ExiCodecCsPort cs_port;
    TestTypedMessage input = {1u};
    uint8_t output[1] = {0xAAu};
    OpenV2g_ExiEncodeRequest request;
    OpenV2g_ExiResult result;

    OpenV2g_SchemaRegistry_Init(&registry);
    OpenV2g_ExiCodec_Init(&codec, &registry);
    TEST_ASSERT(OpenV2g_ExiCodec_Bind(&codec, &descriptor, &backend) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ExiCodec_Start(&codec, 19u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ExiCodec_CreateClientServerPort(&codec, &cs_port) ==
                OPENV2G_RESULT_OK);
    (void)memset(&request, 0, sizeof(request));
    request.schema = OPENV2G_SCHEMA_SAP;
    request.message_type_id = 1u;
    request.typed_object = &input;
    request.typed_object_size = sizeof(input);
    request.output = output;
    request.output_capacity = sizeof(output);

    TEST_ASSERT(cs_port.encode(
                    cs_port.context, 20u, &request, &result) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(cs_port.encode(
                    cs_port.context, 19u, &request, &result) ==
                OPENV2G_RESULT_OVERFLOW);
    TEST_ASSERT(output[0] == 0u);
    return 0;
}

int main(void)
{
    TEST_ASSERT(TestRegistryIsExplicitAndFailClosed() == 0);
    TEST_ASSERT(TestEncodeDecodeRoundTrip() == 0);
    TEST_ASSERT(TestFailureDoesNotPublishPartialObject() == 0);
    TEST_ASSERT(TestGenerationAndBounds() == 0);
    return 0;
}
