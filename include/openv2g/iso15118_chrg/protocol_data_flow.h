#ifndef OPENV2G_ISO15118_CHRG_PROTOCOL_DATA_FLOW_H
#define OPENV2G_ISO15118_CHRG_PROTOCOL_DATA_FLOW_H

#include "openv2g/iso15118_chrg/protocol_adapter.h"

/*
 * Protocol-neutral projection rules for the main DIN 70121 and ISO 15118-2
 * EVCC flows. Generated schema mappers remain responsible for exact wire
 * types, optional-field encoding and edition-specific response codes.
 */
typedef struct {
    OpenV2g_ProtocolFlowCase flow_case;
    uint32_t relevant_energy_fields;
    uint32_t required_energy_fields;
    bool requires_energy;
    bool uses_permission;
    bool requires_permission;
    bool requires_metering_evidence;
    OpenV2g_AllowedAction requested_action;
    OpenV2g_HlcEventCategory event_category;
} OpenV2g_ProtocolDataFlowRule;

OpenV2g_Result OpenV2g_ProtocolDataFlow_GetRule(
    OpenV2g_ProtocolId protocol,
    OpenV2g_ChargeMode mode,
    OpenV2g_ProtocolMessageKind kind,
    OpenV2g_ProtocolDataFlowRule *rule);

OpenV2g_Result OpenV2g_ProtocolDataFlow_PrepareRequest(
    OpenV2g_ProtocolId protocol,
    OpenV2g_SchemaId schema,
    OpenV2g_ChargeMode mode,
    OpenV2g_ProtocolMessageKind kind,
    uint32_t generation,
    uint32_t correlation_id,
    OpenV2g_TimeUs now,
    const OpenV2g_EnergySnapshotSrPort *energy_port,
    const OpenV2g_ChargePermissionSrPort *permission_port,
    const OpenV2g_ProtocolMeteringEvidence *metering_evidence,
    OpenV2g_ProtocolTxRequest *request,
    OpenV2g_ReasonCode *reason);

#endif
