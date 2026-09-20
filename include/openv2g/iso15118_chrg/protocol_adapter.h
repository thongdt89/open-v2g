#ifndef OPENV2G_ISO15118_CHRG_PROTOCOL_ADAPTER_H
#define OPENV2G_ISO15118_CHRG_PROTOCOL_ADAPTER_H

#include <stdbool.h>
#include <stdint.h>

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"
#include "openv2g/charging_coordinator/permission_manager.h"
#include "openv2g/support/energy_data_provider.h"

/*
 * Project-side semantic message IDs. A generated DIN/ISO EXI mapper converts
 * its schema-specific types to these IDs; these values are not wire IDs.
 */
typedef enum {
    OPENV2G_PROTOCOL_MESSAGE_NONE = 0,
    OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP = 1,
    OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY = 2,
    OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION = 3,
    OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION = 4,
    OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER = 5,
    OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK = 6,
    OPENV2G_PROTOCOL_MESSAGE_PRECHARGE = 7,
    OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START = 8,
    OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND = 9,
    OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS = 10,
    OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT = 11,
    OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP = 12,
    OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION = 13,
    OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP = 14
} OpenV2g_ProtocolMessageKind;

typedef enum {
    OPENV2G_PROTOCOL_STATE_DISABLED = 0,
    OPENV2G_PROTOCOL_STATE_IDLE = 1,
    OPENV2G_PROTOCOL_STATE_SESSION_SETUP = 2,
    OPENV2G_PROTOCOL_STATE_SERVICE_DISCOVERY = 3,
    OPENV2G_PROTOCOL_STATE_SERVICE_SELECTION = 4,
    OPENV2G_PROTOCOL_STATE_AUTHORIZATION = 5,
    OPENV2G_PROTOCOL_STATE_CHARGE_PARAMETER = 6,
    OPENV2G_PROTOCOL_STATE_CABLE_CHECK = 7,
    OPENV2G_PROTOCOL_STATE_PRECHARGE = 8,
    OPENV2G_PROTOCOL_STATE_POWER_DELIVERY = 9,
    OPENV2G_PROTOCOL_STATE_CHARGE_LOOP = 10,
    OPENV2G_PROTOCOL_STATE_METERING_RECEIPT = 11,
    OPENV2G_PROTOCOL_STATE_POWER_DELIVERY_STOP = 12,
    OPENV2G_PROTOCOL_STATE_WELDING_DETECTION = 13,
    OPENV2G_PROTOCOL_STATE_SESSION_STOP = 14,
    OPENV2G_PROTOCOL_STATE_TERMINATED = 15,
    OPENV2G_PROTOCOL_STATE_FAILED = 16
} OpenV2g_ProtocolState;

typedef enum {
    OPENV2G_PROTOCOL_FLOW_NONE = 0,
    OPENV2G_PROTOCOL_FLOW_SESSION = 1,
    OPENV2G_PROTOCOL_FLOW_SERVICE = 2,
    OPENV2G_PROTOCOL_FLOW_AUTHORIZATION = 3,
    OPENV2G_PROTOCOL_FLOW_CHARGE_PARAMETER = 4,
    OPENV2G_PROTOCOL_FLOW_DC_SAFETY = 5,
    OPENV2G_PROTOCOL_FLOW_DC_PRECHARGE = 6,
    OPENV2G_PROTOCOL_FLOW_POWER_START = 7,
    OPENV2G_PROTOCOL_FLOW_ENERGY_LOOP = 8,
    OPENV2G_PROTOCOL_FLOW_METERING = 9,
    OPENV2G_PROTOCOL_FLOW_POWER_STOP = 10,
    OPENV2G_PROTOCOL_FLOW_POST_CHARGE = 11,
    OPENV2G_PROTOCOL_FLOW_SESSION_STOP = 12
} OpenV2g_ProtocolFlowCase;

/*
 * Token for mapper-owned, bounded metering data received in a correlated
 * response. It does not copy or redefine the generated ISO meter structure.
 */
typedef struct {
    uint32_t generation;
    uint32_t evidence_id;
    uint32_t source_correlation_id;
    bool signature_required;
} OpenV2g_ProtocolMeteringEvidence;

typedef struct {
    uint32_t generation;
    uint32_t correlation_id;
    OpenV2g_TimeUs now;
    OpenV2g_ProtocolMessageKind kind;
    bool accepted;
    bool ongoing;
    bool metering_receipt_required;
    OpenV2g_ProtocolMeteringEvidence metering_evidence;
    OpenV2g_AllowedAction requested_action;
    int32_t requested_power_w;
} OpenV2g_ProtocolMessage;

typedef struct {
    uint32_t generation;
    uint32_t sequence_counter;
    OpenV2g_ProtocolId protocol;
    OpenV2g_SchemaId schema;
    OpenV2g_ChargeMode mode;
    OpenV2g_ProtocolState state;
    OpenV2g_ReasonCode reason;
    uint32_t last_correlation_id;
    uint32_t pending_correlation_id;
    OpenV2g_ProtocolMessageKind pending_kind;
    bool active;
} OpenV2g_ProtocolAdapterStatus;

typedef struct {
    uint32_t generation;
    OpenV2g_ProtocolId protocol;
    OpenV2g_SchemaId schema;
    OpenV2g_ChargeMode mode;
    OpenV2g_SecurityMode security;
    OpenV2g_TimeUs now;
} OpenV2g_ProtocolStartRequest;

/*
 * Semantic EV request passed to a generated schema mapper. Whole snapshots
 * are copied to make lifetime and generation ownership explicit.
 */
typedef struct {
    uint32_t generation;
    uint32_t correlation_id;
    OpenV2g_TimeUs timestamp;
    OpenV2g_ProtocolId protocol;
    OpenV2g_SchemaId schema;
    OpenV2g_ChargeMode mode;
    OpenV2g_ProtocolMessageKind kind;
    OpenV2g_ProtocolFlowCase flow_case;
    uint32_t relevant_energy_fields;
    OpenV2g_AllowedAction requested_action;
    int32_t requested_power_w;
    bool has_energy;
    bool has_permission;
    bool has_metering_evidence;
    OpenV2g_EvEnergySnapshot energy;
    OpenV2g_ChargePermission permission;
    OpenV2g_ProtocolMeteringEvidence metering_evidence;
} OpenV2g_ProtocolTxRequest;

/* Required PAD-IF-009 C/S message channel. */
typedef struct {
    void *context;
    OpenV2g_Result (*send)(
        void *context,
        const OpenV2g_ProtocolTxRequest *request);
} OpenV2g_ProtocolMessageChannelCsPort;

typedef struct {
    uint32_t generation;
    uint32_t sequence_counter;
    OpenV2g_ProtocolId protocol;
    OpenV2g_ProtocolState state;
    OpenV2g_ProtocolMessageKind message_kind;
    uint32_t correlation_id;
    OpenV2g_ReasonCode reason;
} OpenV2g_ProtocolDiagnostic;

/* PROVISIONAL_ASR_STYLE common PAD-IF-001..005 C/S contract. */
typedef struct {
    void *context;
    OpenV2g_Result (*start)(
        void *context,
        const OpenV2g_ProtocolStartRequest *request);
    OpenV2g_Result (*stop)(
        void *context,
        uint32_t generation,
        OpenV2g_ReasonCode reason);
    OpenV2g_Result (*on_message)(
        void *context,
        const OpenV2g_ProtocolMessage *message);
    OpenV2g_Result (*on_timer_expired)(
        void *context,
        uint32_t generation,
        uint16_t timer_id);
} OpenV2g_ProtocolAdapterCsPort;

/* PROVISIONAL_ASR_STYLE authoritative PAD-IF-011 S/R status. */
typedef struct {
    void *context;
    OpenV2g_Result (*read)(
        void *context,
        OpenV2g_ProtocolAdapterStatus *status);
} OpenV2g_ProtocolAdapterStatusSrPort;

/* PROVISIONAL_ASR_STYLE retained PAD-IF-012 diagnostic S/R sample. */
typedef struct {
    void *context;
    OpenV2g_Result (*read)(
        void *context,
        OpenV2g_ProtocolDiagnostic *diagnostic);
} OpenV2g_ProtocolDiagnosticSrPort;

#endif
