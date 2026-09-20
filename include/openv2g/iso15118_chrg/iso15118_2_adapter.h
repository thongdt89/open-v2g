#ifndef OPENV2G_ISO15118_CHRG_ISO15118_2_ADAPTER_H
#define OPENV2G_ISO15118_CHRG_ISO15118_2_ADAPTER_H

#include <stdbool.h>

#include "openv2g/iso15118_chrg/protocol_adapter.h"

typedef struct {
    bool dc_eim_enabled;
    bool ac_eim_enabled;
    bool pnc_enabled;
    OpenV2g_ProtocolAdapterStatus status;
    OpenV2g_ProtocolMessageChannelCsPort message_required_port;
    OpenV2g_EnergySnapshotSrPort energy_required_port;
    OpenV2g_ChargePermissionSrPort permission_required_port;
    OpenV2g_ProtocolMeteringEvidence metering_evidence;
    OpenV2g_ProtocolDiagnostic diagnostic;
    uint32_t next_correlation_id;
} OpenV2g_Iso15118_2Adapter;

void OpenV2g_Iso15118_2Adapter_Init(
    OpenV2g_Iso15118_2Adapter *adapter,
    bool dc_eim_enabled,
    bool ac_eim_enabled,
    bool pnc_enabled);
OpenV2g_Result OpenV2g_Iso15118_2Adapter_BindRequiredPorts(
    OpenV2g_Iso15118_2Adapter *adapter,
    const OpenV2g_ProtocolMessageChannelCsPort *message_port,
    const OpenV2g_EnergySnapshotSrPort *energy_port,
    const OpenV2g_ChargePermissionSrPort *permission_port);
OpenV2g_Result OpenV2g_Iso15118_2Adapter_CreateClientServerPort(
    OpenV2g_Iso15118_2Adapter *adapter,
    OpenV2g_ProtocolAdapterCsPort *port);
OpenV2g_Result OpenV2g_Iso15118_2Adapter_CreateStatusSenderReceiverPort(
    OpenV2g_Iso15118_2Adapter *adapter,
    OpenV2g_ProtocolAdapterStatusSrPort *port);
OpenV2g_Result OpenV2g_Iso15118_2Adapter_CreateDiagnosticSenderReceiverPort(
    OpenV2g_Iso15118_2Adapter *adapter,
    OpenV2g_ProtocolDiagnosticSrPort *port);

#endif
