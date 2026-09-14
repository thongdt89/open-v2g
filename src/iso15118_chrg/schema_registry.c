#include "openv2g/iso15118_chrg/schema_registry.h"

#include <string.h>

#include "openv2g/config.h"

static bool schema_is_valid(OpenV2g_SchemaId schema)
{
    return (schema > OPENV2G_SCHEMA_NONE) &&
           ((uint32_t)schema < OPENV2G_SCHEMA_REGISTRY_SLOT_COUNT);
}

static uint32_t schema_mask(OpenV2g_SchemaId schema)
{
    return ((uint32_t)1u << (uint32_t)schema);
}

static void advance_status_sequence(OpenV2g_SchemaRegistry *registry)
{
    registry->status_sequence_counter++;
    if (registry->status_sequence_counter == 0u) {
        registry->status_sequence_counter = 1u;
    }
}

static bool descriptor_is_valid(const OpenV2g_SchemaDescriptor *descriptor)
{
    if ((descriptor == NULL) || !schema_is_valid(descriptor->schema) ||
        (descriptor->max_payload == 0u) ||
        (descriptor->max_payload > OPENV2G_CFG_V2GTP_PAYLOAD_MAX) ||
        (descriptor->max_workspace == 0u) ||
        (descriptor->max_workspace > OPENV2G_CFG_EXI_WORKSPACE_MAX) ||
        (descriptor->max_typed_object == 0u) ||
        (descriptor->max_collection == 0u) ||
        (descriptor->max_collection > OPENV2G_CFG_SCHEDULE_ENTRY_MAX) ||
        (descriptor->implementation_id == NULL) ||
        (descriptor->schema_revision == NULL) ||
        (descriptor->generated_checksum == NULL) ||
        (descriptor->implementation_id[0] == '\0') ||
        (descriptor->schema_revision[0] == '\0') ||
        (descriptor->generated_checksum[0] == '\0')) {
        return false;
    }
    if (descriptor->schema == OPENV2G_SCHEMA_SAP) {
        return (descriptor->protocol == OPENV2G_PROTOCOL_NONE) &&
               (descriptor->mode == OPENV2G_MODE_NONE);
    }
    if (descriptor->schema == OPENV2G_SCHEMA_DIN_70121) {
        return (descriptor->protocol == OPENV2G_PROTOCOL_DIN_70121) &&
               (descriptor->mode == OPENV2G_MODE_DC);
    }
    if (descriptor->schema == OPENV2G_SCHEMA_ISO_15118_2) {
        return (descriptor->protocol == OPENV2G_PROTOCOL_ISO_15118_2) &&
               ((descriptor->mode == OPENV2G_MODE_DC) ||
                (descriptor->mode == OPENV2G_MODE_AC));
    }
    if (descriptor->schema == OPENV2G_SCHEMA_ISO_20_COMMON) {
        return (descriptor->protocol == OPENV2G_PROTOCOL_ISO_15118_20) &&
               (descriptor->mode == OPENV2G_MODE_NONE);
    }
    if (descriptor->schema == OPENV2G_SCHEMA_ISO_20_AC) {
        return (descriptor->protocol == OPENV2G_PROTOCOL_ISO_15118_20) &&
               ((descriptor->mode == OPENV2G_MODE_AC) ||
                (descriptor->mode == OPENV2G_MODE_AC_BPT));
    }
    return (descriptor->schema == OPENV2G_SCHEMA_ISO_20_DC) &&
           (descriptor->protocol == OPENV2G_PROTOCOL_ISO_15118_20) &&
           ((descriptor->mode == OPENV2G_MODE_DC) ||
            (descriptor->mode == OPENV2G_MODE_DC_BPT));
}

void OpenV2g_SchemaRegistry_Init(OpenV2g_SchemaRegistry *registry)
{
    if (registry != NULL) {
        (void)memset(registry, 0, sizeof(*registry));
    }
}

OpenV2g_Result OpenV2g_SchemaRegistry_Register(
    OpenV2g_SchemaRegistry *registry,
    const OpenV2g_SchemaDescriptor *descriptor)
{
    uint32_t mask;

    if ((registry == NULL) || !descriptor_is_valid(descriptor)) {
        return OPENV2G_RESULT_REJECTED;
    }

    mask = schema_mask(descriptor->schema);
    if (((registry->available_mask & mask) != 0u) ||
        ((registry->integrity_error_mask & mask) != 0u) ||
        (registry->entries[(uint32_t)descriptor->schema].schema !=
         OPENV2G_SCHEMA_NONE)) {
        return OPENV2G_RESULT_BUSY;
    }
    registry->entries[(uint32_t)descriptor->schema] = *descriptor;
    registry->available_mask |= mask;
    advance_status_sequence(registry);
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_SchemaRegistry_MarkIntegrityError(
    OpenV2g_SchemaRegistry *registry,
    OpenV2g_SchemaId schema)
{
    uint32_t mask;

    if ((registry == NULL) || !schema_is_valid(schema)) {
        return OPENV2G_RESULT_REJECTED;
    }
    mask = schema_mask(schema);
    registry->available_mask &= ~mask;
    registry->integrity_error_mask |= mask;
    advance_status_sequence(registry);
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_SchemaRegistry_Get(
    const OpenV2g_SchemaRegistry *registry,
    OpenV2g_SchemaId schema,
    const OpenV2g_SchemaDescriptor **descriptor)
{
    uint32_t mask;

    if (descriptor != NULL) {
        *descriptor = NULL;
    }
    if ((registry == NULL) || (descriptor == NULL) || !schema_is_valid(schema)) {
        return OPENV2G_RESULT_REJECTED;
    }
    mask = schema_mask(schema);
    if (((registry->available_mask & mask) == 0u) ||
        ((registry->integrity_error_mask & mask) != 0u)) {
        return OPENV2G_RESULT_NOT_IMPLEMENTED;
    }
    *descriptor = &registry->entries[(uint32_t)schema];
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_SchemaRegistry_GetLimits(
    const OpenV2g_SchemaRegistry *registry,
    OpenV2g_SchemaId schema,
    OpenV2g_SchemaLimits *limits)
{
    const OpenV2g_SchemaDescriptor *descriptor;
    OpenV2g_Result result;

    if (limits == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    (void)memset(limits, 0, sizeof(*limits));
    result = OpenV2g_SchemaRegistry_Get(registry, schema, &descriptor);
    if (result != OPENV2G_RESULT_OK) {
        return result;
    }
    limits->schema = descriptor->schema;
    limits->max_payload = descriptor->max_payload;
    limits->max_workspace = descriptor->max_workspace;
    limits->max_typed_object = descriptor->max_typed_object;
    limits->max_collection = descriptor->max_collection;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_SchemaRegistry_CheckCompatibility(
    const OpenV2g_SchemaRegistry *registry,
    OpenV2g_ProtocolId protocol,
    OpenV2g_SchemaId schema,
    OpenV2g_ChargeMode mode)
{
    const OpenV2g_SchemaDescriptor *descriptor;
    OpenV2g_Result result;

    result = OpenV2g_SchemaRegistry_Get(registry, schema, &descriptor);
    if (result != OPENV2G_RESULT_OK) {
        return result;
    }
    if ((descriptor->protocol != protocol) || (descriptor->mode != mode)) {
        return OPENV2G_RESULT_REJECTED;
    }
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result cs_get_descriptor(
    void *context,
    OpenV2g_SchemaId schema,
    OpenV2g_SchemaDescriptor *descriptor)
{
    const OpenV2g_SchemaDescriptor *stored_descriptor;
    OpenV2g_Result result;

    if (descriptor == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    (void)memset(descriptor, 0, sizeof(*descriptor));
    result = OpenV2g_SchemaRegistry_Get(
        (const OpenV2g_SchemaRegistry *)context,
        schema,
        &stored_descriptor);
    if (result == OPENV2G_RESULT_OK) {
        *descriptor = *stored_descriptor;
    }
    return result;
}

static OpenV2g_Result cs_get_limits(
    void *context,
    OpenV2g_SchemaId schema,
    OpenV2g_SchemaLimits *limits)
{
    return OpenV2g_SchemaRegistry_GetLimits(
        (const OpenV2g_SchemaRegistry *)context,
        schema,
        limits);
}

static OpenV2g_Result cs_check_compatibility(
    void *context,
    OpenV2g_ProtocolId protocol,
    OpenV2g_SchemaId schema,
    OpenV2g_ChargeMode mode)
{
    return OpenV2g_SchemaRegistry_CheckCompatibility(
        (const OpenV2g_SchemaRegistry *)context,
        protocol,
        schema,
        mode);
}

static OpenV2g_Result sr_read_status(
    void *context,
    OpenV2g_SchemaRegistryStatus *status)
{
    const OpenV2g_SchemaRegistry *registry =
        (const OpenV2g_SchemaRegistry *)context;

    if ((registry == NULL) || (status == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    status->sequence_counter = registry->status_sequence_counter;
    status->ready = registry->available_mask != 0u;
    status->available_mask = registry->available_mask;
    status->integrity_error_mask = registry->integrity_error_mask;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_SchemaRegistry_CreateClientServerPort(
    OpenV2g_SchemaRegistry *registry,
    OpenV2g_SchemaRegistryCsPort *port)
{
    if ((registry == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = registry;
    port->get_schema_descriptor = cs_get_descriptor;
    port->get_schema_limits = cs_get_limits;
    port->check_protocol_compatibility = cs_check_compatibility;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_SchemaRegistry_CreateStatusSenderReceiverPort(
    OpenV2g_SchemaRegistry *registry,
    OpenV2g_SchemaRegistryStatusSrPort *port)
{
    if ((registry == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = registry;
    port->read = sr_read_status;
    return OPENV2G_RESULT_OK;
}
