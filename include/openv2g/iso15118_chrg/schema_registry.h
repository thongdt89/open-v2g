#ifndef OPENV2G_ISO15118_CHRG_SCHEMA_REGISTRY_H
#define OPENV2G_ISO15118_CHRG_SCHEMA_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"

#define OPENV2G_SCHEMA_REGISTRY_SLOT_COUNT 7u

typedef struct {
    OpenV2g_SchemaId schema;
    OpenV2g_ProtocolId protocol;
    OpenV2g_ChargeMode mode;
    size_t max_payload;
    size_t max_workspace;
    size_t max_typed_object;
    uint16_t max_collection;
    const char *implementation_id;
    const char *schema_revision;
    const char *generated_checksum;
} OpenV2g_SchemaDescriptor;

typedef struct {
    OpenV2g_SchemaDescriptor entries[OPENV2G_SCHEMA_REGISTRY_SLOT_COUNT];
    uint32_t available_mask;
    uint32_t integrity_error_mask;
    uint32_t status_sequence_counter;
} OpenV2g_SchemaRegistry;

typedef struct {
    OpenV2g_SchemaId schema;
    size_t max_payload;
    size_t max_workspace;
    size_t max_typed_object;
    uint16_t max_collection;
} OpenV2g_SchemaLimits;

typedef struct {
    uint32_t sequence_counter;
    bool ready;
    uint32_t available_mask;
    uint32_t integrity_error_mask;
} OpenV2g_SchemaRegistryStatus;

/*
 * PROVISIONAL_ASR_STYLE C/S interface. A server exposes this port and a
 * client stores the same handle as its required port. Vendor RTE generation
 * may later replace these function pointers without changing client logic.
 */
typedef struct {
    void *context;
    OpenV2g_Result (*get_schema_descriptor)(
        void *context,
        OpenV2g_SchemaId schema,
        OpenV2g_SchemaDescriptor *descriptor);
    OpenV2g_Result (*get_schema_limits)(
        void *context,
        OpenV2g_SchemaId schema,
        OpenV2g_SchemaLimits *limits);
    OpenV2g_Result (*check_protocol_compatibility)(
        void *context,
        OpenV2g_ProtocolId protocol,
        OpenV2g_SchemaId schema,
        OpenV2g_ChargeMode mode);
} OpenV2g_SchemaRegistryCsPort;

/* PROVISIONAL_ASR_STYLE S/R receiver view of authoritative registry status. */
typedef struct {
    void *context;
    OpenV2g_Result (*read)(
        void *context,
        OpenV2g_SchemaRegistryStatus *status);
} OpenV2g_SchemaRegistryStatusSrPort;

void OpenV2g_SchemaRegistry_Init(OpenV2g_SchemaRegistry *registry);

OpenV2g_Result OpenV2g_SchemaRegistry_Register(
    OpenV2g_SchemaRegistry *registry,
    const OpenV2g_SchemaDescriptor *descriptor);

OpenV2g_Result OpenV2g_SchemaRegistry_MarkIntegrityError(
    OpenV2g_SchemaRegistry *registry,
    OpenV2g_SchemaId schema);

OpenV2g_Result OpenV2g_SchemaRegistry_Get(
    const OpenV2g_SchemaRegistry *registry,
    OpenV2g_SchemaId schema,
    const OpenV2g_SchemaDescriptor **descriptor);

OpenV2g_Result OpenV2g_SchemaRegistry_GetLimits(
    const OpenV2g_SchemaRegistry *registry,
    OpenV2g_SchemaId schema,
    OpenV2g_SchemaLimits *limits);

OpenV2g_Result OpenV2g_SchemaRegistry_CheckCompatibility(
    const OpenV2g_SchemaRegistry *registry,
    OpenV2g_ProtocolId protocol,
    OpenV2g_SchemaId schema,
    OpenV2g_ChargeMode mode);

OpenV2g_Result OpenV2g_SchemaRegistry_CreateClientServerPort(
    OpenV2g_SchemaRegistry *registry,
    OpenV2g_SchemaRegistryCsPort *port);

OpenV2g_Result OpenV2g_SchemaRegistry_CreateStatusSenderReceiverPort(
    OpenV2g_SchemaRegistry *registry,
    OpenV2g_SchemaRegistryStatusSrPort *port);

#endif
