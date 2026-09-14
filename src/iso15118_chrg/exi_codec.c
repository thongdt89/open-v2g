#include "openv2g/iso15118_chrg/exi_codec.h"

#include <string.h>

static bool schema_is_valid(OpenV2g_SchemaId schema)
{
    return (schema > OPENV2G_SCHEMA_NONE) &&
           ((uint32_t)schema < OPENV2G_SCHEMA_REGISTRY_SLOT_COUNT);
}

static void clear_result(OpenV2g_ExiResult *result)
{
    if (result != NULL) {
        (void)memset(result, 0, sizeof(*result));
    }
}

static void set_failure(
    OpenV2g_ExiCodec *codec,
    OpenV2g_SchemaId schema,
    uint16_t message_type_id,
    OpenV2g_ReasonCode reason)
{
    (void)memset(&codec->last_diagnostic, 0, sizeof(codec->last_diagnostic));
    codec->last_diagnostic.schema = schema;
    codec->last_diagnostic.message_type_id = message_type_id;
    codec->last_diagnostic.reason = reason;
    codec->diagnostic_generation = codec->generation;
    codec->diagnostic_sequence_counter++;
    if (codec->diagnostic_sequence_counter == 0u) {
        codec->diagnostic_sequence_counter = 1u;
    }
    codec->diagnostic_valid = true;
}

static void publish_backend_failure(OpenV2g_ExiCodec *codec)
{
    codec->diagnostic_generation = codec->generation;
    codec->diagnostic_sequence_counter++;
    if (codec->diagnostic_sequence_counter == 0u) {
        codec->diagnostic_sequence_counter = 1u;
    }
    codec->diagnostic_valid = true;
}

static OpenV2g_Result get_binding(
    OpenV2g_ExiCodec *codec,
    uint32_t generation,
    OpenV2g_SchemaId schema,
    OpenV2g_SchemaDescriptor *descriptor,
    OpenV2g_ExiBinding **binding)
{
    OpenV2g_Result result;

    if ((codec == NULL) || (descriptor == NULL) || (binding == NULL) ||
        !codec->active || (generation == 0u) ||
        (generation != codec->generation) || !schema_is_valid(schema)) {
        return OPENV2G_RESULT_REJECTED;
    }
    if ((codec->schema_required_port.context == NULL) ||
        (codec->schema_required_port.get_schema_descriptor == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    result = codec->schema_required_port.get_schema_descriptor(
        codec->schema_required_port.context,
        schema,
        descriptor);
    if (result != OPENV2G_RESULT_OK) {
        return result;
    }
    *binding = &codec->bindings[(uint32_t)schema];
    if (!(*binding)->bound) {
        return OPENV2G_RESULT_NOT_IMPLEMENTED;
    }
    return OPENV2G_RESULT_OK;
}

void OpenV2g_ExiCodec_Init(
    OpenV2g_ExiCodec *codec,
    OpenV2g_SchemaRegistry *registry)
{
    if (codec != NULL) {
        (void)memset(codec, 0, sizeof(*codec));
        codec->registry = registry;
        if (registry != NULL) {
            (void)OpenV2g_SchemaRegistry_CreateClientServerPort(
                registry,
                &codec->schema_required_port);
            (void)OpenV2g_SchemaRegistry_CreateStatusSenderReceiverPort(
                registry,
                &codec->schema_status_required_port);
        }
    }
}

OpenV2g_Result OpenV2g_ExiCodec_Bind(
    OpenV2g_ExiCodec *codec,
    const OpenV2g_SchemaDescriptor *descriptor,
    const OpenV2g_ExiBackend *backend)
{
    OpenV2g_Result result;

    if ((codec == NULL) || (codec->registry == NULL) || codec->active ||
        (descriptor == NULL) || !schema_is_valid(descriptor->schema) ||
        (backend == NULL) || (backend->encode == NULL) ||
        (backend->decode == NULL) || (backend->reset == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    result = OpenV2g_SchemaRegistry_Register(codec->registry, descriptor);
    if (result != OPENV2G_RESULT_OK) {
        return result;
    }
    codec->bindings[(uint32_t)descriptor->schema].backend = *backend;
    codec->bindings[(uint32_t)descriptor->schema].bound = true;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ExiCodec_Start(
    OpenV2g_ExiCodec *codec,
    uint32_t generation)
{
    size_t index;
    bool has_bound_backend = false;
    OpenV2g_SchemaRegistryStatus schema_status;

    if ((codec == NULL) || (codec->registry == NULL) || (generation == 0u) ||
        codec->active || (codec->schema_status_required_port.read == NULL) ||
        (codec->schema_status_required_port.read(
             codec->schema_status_required_port.context,
             &schema_status) != OPENV2G_RESULT_OK) ||
        !schema_status.ready) {
        return OPENV2G_RESULT_REJECTED;
    }
    for (index = 1u; index < OPENV2G_SCHEMA_REGISTRY_SLOT_COUNT; ++index) {
        if (codec->bindings[index].bound &&
            ((schema_status.available_mask & ((uint32_t)1u << index)) != 0u)) {
            has_bound_backend = true;
        }
    }
    if (!has_bound_backend) {
        return OPENV2G_RESULT_REJECTED;
    }
    codec->generation = generation;
    codec->active = true;
    (void)memset(&codec->last_diagnostic, 0, sizeof(codec->last_diagnostic));
    codec->diagnostic_generation = 0u;
    codec->diagnostic_sequence_counter = 0u;
    codec->diagnostic_valid = false;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ExiCodec_Encode(
    OpenV2g_ExiCodec *codec,
    uint32_t generation,
    const OpenV2g_ExiEncodeRequest *request,
    OpenV2g_ExiResult *result)
{
    OpenV2g_SchemaDescriptor descriptor;
    OpenV2g_ExiBinding *binding;
    OpenV2g_Result call_result;
    size_t encoded_length = 0u;

    clear_result(result);
    if ((codec == NULL) || (request == NULL) || (result == NULL) ||
        (request->typed_object == NULL) || (request->typed_object_size == 0u) ||
        (request->output == NULL) || (request->output_capacity == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }
    call_result = get_binding(
        codec, generation, request->schema, &descriptor, &binding);
    if (call_result != OPENV2G_RESULT_OK) {
        set_failure(codec, request->schema, request->message_type_id,
                    OPENV2G_REASON_CONFIGURATION);
        return call_result;
    }
    if (request->typed_object_size > descriptor.max_typed_object) {
        set_failure(codec, request->schema, request->message_type_id,
                    OPENV2G_REASON_RESOURCE);
        return OPENV2G_RESULT_OVERFLOW;
    }

    (void)memset(request->output, 0, request->output_capacity);
    (void)memset(&codec->last_diagnostic, 0, sizeof(codec->last_diagnostic));
    codec->last_diagnostic.schema = request->schema;
    codec->last_diagnostic.message_type_id = request->message_type_id;
    call_result = binding->backend.encode(
        binding->backend.context,
        request->message_type_id,
        request->typed_object,
        request->typed_object_size,
        request->output,
        request->output_capacity,
        &encoded_length,
        &codec->last_diagnostic);
    if ((call_result != OPENV2G_RESULT_OK) ||
        (encoded_length > request->output_capacity) ||
        (encoded_length > descriptor.max_payload)) {
        (void)memset(request->output, 0, request->output_capacity);
        if (call_result == OPENV2G_RESULT_OK) {
            set_failure(codec, request->schema, request->message_type_id,
                        OPENV2G_REASON_RESOURCE);
            return OPENV2G_RESULT_OVERFLOW;
        }
        if (codec->last_diagnostic.reason == OPENV2G_REASON_NONE) {
            codec->last_diagnostic.reason = OPENV2G_REASON_FORMAT;
        }
        publish_backend_failure(codec);
        return call_result;
    }

    result->schema = request->schema;
    result->message_type_id = request->message_type_id;
    result->encoded_length = encoded_length;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ExiCodec_Decode(
    OpenV2g_ExiCodec *codec,
    uint32_t generation,
    const OpenV2g_ExiDecodeRequest *request,
    OpenV2g_ExiResult *result)
{
    OpenV2g_SchemaDescriptor descriptor;
    OpenV2g_ExiBinding *binding;
    OpenV2g_Result call_result;
    uint16_t decoded_message_type_id = 0u;

    clear_result(result);
    if ((codec == NULL) || (request == NULL) || (result == NULL) ||
        (request->input == NULL) || (request->input_length == 0u) ||
        (request->candidate_object == NULL) || (request->output_object == NULL) ||
        (request->candidate_object == request->output_object) ||
        (request->object_size == 0u) || (request->workspace == NULL) ||
        (request->workspace_capacity == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }
    call_result = get_binding(
        codec, generation, request->schema, &descriptor, &binding);
    if (call_result != OPENV2G_RESULT_OK) {
        set_failure(codec, request->schema, request->expected_message_type_id,
                    OPENV2G_REASON_CONFIGURATION);
        return call_result;
    }
    if ((request->input_length > descriptor.max_payload) ||
        (request->object_size > descriptor.max_typed_object) ||
        (request->workspace_capacity < descriptor.max_workspace)) {
        set_failure(codec, request->schema, request->expected_message_type_id,
                    OPENV2G_REASON_RESOURCE);
        return OPENV2G_RESULT_OVERFLOW;
    }

    (void)memset(request->candidate_object, 0, request->object_size);
    (void)memset(&codec->last_diagnostic, 0, sizeof(codec->last_diagnostic));
    codec->last_diagnostic.schema = request->schema;
    codec->last_diagnostic.message_type_id = request->expected_message_type_id;
    call_result = binding->backend.decode(
        binding->backend.context,
        request->input,
        request->input_length,
        request->candidate_object,
        request->object_size,
        request->workspace,
        request->workspace_capacity,
        &decoded_message_type_id,
        &codec->last_diagnostic);
    if ((call_result != OPENV2G_RESULT_OK) ||
        (decoded_message_type_id != request->expected_message_type_id)) {
        binding->backend.reset(
            binding->backend.context,
            request->workspace,
            request->workspace_capacity);
        (void)memset(request->workspace, 0, request->workspace_capacity);
        (void)memset(request->candidate_object, 0, request->object_size);
        if (call_result == OPENV2G_RESULT_OK) {
            set_failure(codec, request->schema, decoded_message_type_id,
                        OPENV2G_REASON_FORMAT);
            return OPENV2G_RESULT_REJECTED;
        }
        if (codec->last_diagnostic.reason == OPENV2G_REASON_NONE) {
            codec->last_diagnostic.reason = OPENV2G_REASON_FORMAT;
        }
        publish_backend_failure(codec);
        return call_result;
    }

    (void)memcpy(
        request->output_object,
        request->candidate_object,
        request->object_size);
    result->schema = request->schema;
    result->message_type_id = decoded_message_type_id;
    result->encoded_length = request->input_length;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ExiCodec_ResetWorkspace(
    OpenV2g_ExiCodec *codec,
    uint32_t generation,
    OpenV2g_SchemaId schema,
    uint8_t *workspace,
    size_t workspace_capacity)
{
    OpenV2g_SchemaDescriptor descriptor;
    OpenV2g_ExiBinding *binding;
    OpenV2g_Result result;

    if ((workspace == NULL) || (workspace_capacity == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }
    result = get_binding(codec, generation, schema, &descriptor, &binding);
    if (result != OPENV2G_RESULT_OK) {
        return result;
    }
    binding->backend.reset(
        binding->backend.context,
        workspace,
        workspace_capacity);
    (void)memset(workspace, 0, workspace_capacity);
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ExiCodec_Stop(
    OpenV2g_ExiCodec *codec,
    uint32_t generation)
{
    if (codec == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (!codec->active) {
        return OPENV2G_RESULT_OK;
    }
    if (generation != codec->generation) {
        return OPENV2G_RESULT_REJECTED;
    }
    codec->active = false;
    codec->generation = 0u;
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result cs_encode(
    void *context,
    uint32_t generation,
    const OpenV2g_ExiEncodeRequest *request,
    OpenV2g_ExiResult *result)
{
    return OpenV2g_ExiCodec_Encode(
        (OpenV2g_ExiCodec *)context,
        generation,
        request,
        result);
}

static OpenV2g_Result cs_decode(
    void *context,
    uint32_t generation,
    const OpenV2g_ExiDecodeRequest *request,
    OpenV2g_ExiResult *result)
{
    return OpenV2g_ExiCodec_Decode(
        (OpenV2g_ExiCodec *)context,
        generation,
        request,
        result);
}

static OpenV2g_Result cs_reset_workspace(
    void *context,
    uint32_t generation,
    OpenV2g_SchemaId schema,
    uint8_t *workspace,
    size_t workspace_capacity)
{
    return OpenV2g_ExiCodec_ResetWorkspace(
        (OpenV2g_ExiCodec *)context,
        generation,
        schema,
        workspace,
        workspace_capacity);
}

static OpenV2g_Result sr_read_diagnostic(
    void *context,
    OpenV2g_ExiDiagnosticSample *sample)
{
    const OpenV2g_ExiCodec *codec = (const OpenV2g_ExiCodec *)context;

    if ((codec == NULL) || (sample == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    (void)memset(sample, 0, sizeof(*sample));
    if (!codec->diagnostic_valid) {
        return OPENV2G_RESULT_NOT_IMPLEMENTED;
    }
    sample->session_generation = codec->diagnostic_generation;
    sample->sequence_counter = codec->diagnostic_sequence_counter;
    sample->value = codec->last_diagnostic;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ExiCodec_CreateClientServerPort(
    OpenV2g_ExiCodec *codec,
    OpenV2g_ExiCodecCsPort *port)
{
    if ((codec == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = codec;
    port->encode = cs_encode;
    port->decode = cs_decode;
    port->reset_workspace = cs_reset_workspace;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ExiCodec_CreateDiagnosticSenderReceiverPort(
    OpenV2g_ExiCodec *codec,
    OpenV2g_ExiDiagnosticSrPort *port)
{
    if ((codec == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = codec;
    port->read = sr_read_diagnostic;
    return OPENV2G_RESULT_OK;
}
