#ifndef OPENV2G_EXI_CBV2G_SAP_H
#define OPENV2G_EXI_CBV2G_SAP_H

#include "openv2g/iso15118_chrg/exi_codec.h"

/* Project DTO IDs/capacities for the pinned host PoC; not imported wire enums. */
#define OPENV2G_EXI_SAP_REQUEST 1u
#define OPENV2G_EXI_SAP_RESPONSE 2u
#define OPENV2G_EXI_SAP_OFFERS_MAX 5u
#define OPENV2G_EXI_SAP_NAMESPACE_MAX 99u
#define OPENV2G_EXI_SAP_PAYLOAD_MAX 1024u
#define OPENV2G_EXI_SAP_WORKSPACE_SIZE (OPENV2G_EXI_SAP_PAYLOAD_MAX + 1u)

typedef struct {
    uint16_t namespace_length;
    char namespace_text[OPENV2G_EXI_SAP_NAMESPACE_MAX]; /* explicit length, no NUL required */
    uint32_t version_major;
    uint32_t version_minor;
    uint8_t wire_schema_id; /* distinct from OpenV2g_SchemaId */
    uint8_t priority;
} OpenV2g_ExiSapOffer;

typedef enum {
    OPENV2G_EXI_SAP_SUCCESS = 0,
    OPENV2G_EXI_SAP_MINOR_DEVIATION = 1,
    OPENV2G_EXI_SAP_NO_NEGOTIATION = 2
} OpenV2g_ExiSapResponseCode;

typedef struct {
    uint16_t message_type_id;
    union {
        struct {
            uint16_t count;
            OpenV2g_ExiSapOffer offers[OPENV2G_EXI_SAP_OFFERS_MAX];
        } request;
        struct {
            OpenV2g_ExiSapResponseCode code;
            bool has_schema_id;
            uint8_t wire_schema_id;
        } response;
    } body;
} OpenV2g_ExiSapDocument;

/* Initialization only, before codec Start. SAP only; other schemas stay unavailable.
 * No generated types escape this adapter. Calls must be owner-serialized.
 * Generated document and encode scratch have bounded automatic storage;
 * decode uses caller workspace as mutable guarded input storage.
 */
OpenV2g_Result OpenV2g_Cbv2gSap_Bind(OpenV2g_ExiCodec *codec);
#endif
