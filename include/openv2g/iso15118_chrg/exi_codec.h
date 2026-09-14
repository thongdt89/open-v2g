#ifndef OPENV2G_ISO15118_CHRG_EXI_CODEC_H
#define OPENV2G_ISO15118_CHRG_EXI_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"
#include "openv2g/iso15118_chrg/schema_registry.h"

typedef struct {
    OpenV2g_SchemaId schema;
    uint16_t message_type_id;
    uint32_t bit_offset;
    uint16_t rejected_constraint;
    OpenV2g_ReasonCode reason;
} OpenV2g_ExiDiagnostic;

typedef OpenV2g_Result (*OpenV2g_ExiBackendEncode)(
    void *context,
    uint16_t message_type_id,
    const void *typed_object,
    size_t typed_object_size,
    uint8_t *output,
    size_t output_capacity,
    size_t *encoded_length,
    OpenV2g_ExiDiagnostic *diagnostic);

typedef OpenV2g_Result (*OpenV2g_ExiBackendDecode)(
    void *context,
    const uint8_t *input,
    size_t input_length,
    void *candidate_object,
    size_t object_capacity,
    uint8_t *workspace,
    size_t workspace_capacity,
    uint16_t *decoded_message_type_id,
    OpenV2g_ExiDiagnostic *diagnostic);

typedef void (*OpenV2g_ExiBackendReset)(
    void *context,
    uint8_t *workspace,
    size_t workspace_capacity);

typedef struct {
    void *context;
    OpenV2g_ExiBackendEncode encode;
    OpenV2g_ExiBackendDecode decode;
    OpenV2g_ExiBackendReset reset;
} OpenV2g_ExiBackend;

typedef struct {
    OpenV2g_SchemaId schema;
    uint16_t message_type_id;
    const void *typed_object;
    size_t typed_object_size;
    uint8_t *output;
    size_t output_capacity;
} OpenV2g_ExiEncodeRequest;

typedef struct {
    OpenV2g_SchemaId schema;
    uint16_t expected_message_type_id;
    const uint8_t *input;
    size_t input_length;
    void *candidate_object;
    void *output_object;
    size_t object_size;
    uint8_t *workspace;
    size_t workspace_capacity;
} OpenV2g_ExiDecodeRequest;

typedef struct {
    OpenV2g_SchemaId schema;
    uint16_t message_type_id;
    size_t encoded_length;
} OpenV2g_ExiResult;

typedef struct {
    OpenV2g_ExiBackend backend;
    bool bound;
} OpenV2g_ExiBinding;

typedef struct {
    uint32_t session_generation;
    uint32_t sequence_counter;
    OpenV2g_ExiDiagnostic value;
} OpenV2g_ExiDiagnosticSample;

/*
 * PROVISIONAL_ASR_STYLE C/S interface corresponding to EXI-IF-001..003.
 * The server implementation is ExiCodec; SAP/protocol adapters use this
 * handle as their required port.
 */
typedef struct {
    void *context;
    OpenV2g_Result (*encode)(
        void *context,
        uint32_t generation,
        const OpenV2g_ExiEncodeRequest *request,
        OpenV2g_ExiResult *result);
    OpenV2g_Result (*decode)(
        void *context,
        uint32_t generation,
        const OpenV2g_ExiDecodeRequest *request,
        OpenV2g_ExiResult *result);
    OpenV2g_Result (*reset_workspace)(
        void *context,
        uint32_t generation,
        OpenV2g_SchemaId schema,
        uint8_t *workspace,
        size_t workspace_capacity);
} OpenV2g_ExiCodecCsPort;

/* PROVISIONAL_ASR_STYLE S/R receiver view for EXI diagnostic publication. */
typedef struct {
    void *context;
    OpenV2g_Result (*read)(
        void *context,
        OpenV2g_ExiDiagnosticSample *sample);
} OpenV2g_ExiDiagnosticSrPort;

typedef struct {
    OpenV2g_SchemaRegistry *registry;
    OpenV2g_SchemaRegistryCsPort schema_required_port;
    OpenV2g_SchemaRegistryStatusSrPort schema_status_required_port;
    OpenV2g_ExiBinding bindings[OPENV2G_SCHEMA_REGISTRY_SLOT_COUNT];
    uint32_t generation;
    bool active;
    OpenV2g_ExiDiagnostic last_diagnostic;
    uint32_t diagnostic_generation;
    uint32_t diagnostic_sequence_counter;
    bool diagnostic_valid;
} OpenV2g_ExiCodec;

void OpenV2g_ExiCodec_Init(
    OpenV2g_ExiCodec *codec,
    OpenV2g_SchemaRegistry *registry);

OpenV2g_Result OpenV2g_ExiCodec_Bind(
    OpenV2g_ExiCodec *codec,
    const OpenV2g_SchemaDescriptor *descriptor,
    const OpenV2g_ExiBackend *backend);

OpenV2g_Result OpenV2g_ExiCodec_Start(
    OpenV2g_ExiCodec *codec,
    uint32_t generation);

OpenV2g_Result OpenV2g_ExiCodec_Encode(
    OpenV2g_ExiCodec *codec,
    uint32_t generation,
    const OpenV2g_ExiEncodeRequest *request,
    OpenV2g_ExiResult *result);

OpenV2g_Result OpenV2g_ExiCodec_Decode(
    OpenV2g_ExiCodec *codec,
    uint32_t generation,
    const OpenV2g_ExiDecodeRequest *request,
    OpenV2g_ExiResult *result);

OpenV2g_Result OpenV2g_ExiCodec_ResetWorkspace(
    OpenV2g_ExiCodec *codec,
    uint32_t generation,
    OpenV2g_SchemaId schema,
    uint8_t *workspace,
    size_t workspace_capacity);

OpenV2g_Result OpenV2g_ExiCodec_Stop(
    OpenV2g_ExiCodec *codec,
    uint32_t generation);

OpenV2g_Result OpenV2g_ExiCodec_CreateClientServerPort(
    OpenV2g_ExiCodec *codec,
    OpenV2g_ExiCodecCsPort *port);

OpenV2g_Result OpenV2g_ExiCodec_CreateDiagnosticSenderReceiverPort(
    OpenV2g_ExiCodec *codec,
    OpenV2g_ExiDiagnosticSrPort *port);

#endif
