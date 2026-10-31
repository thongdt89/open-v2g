#ifndef OPENV2G_SIMULATION_HOST_SIMULATION_H
#define OPENV2G_SIMULATION_HOST_SIMULATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "openv2g/charging_coordinator/charging_coordinator.h"
#include "openv2g/iso15118_chrg/iso15118_chrg.h"
#include "openv2g/support/charge_hardware_adapter.h"
#include "openv2g/support/cp_pp_manager.h"
#include "openv2g/support/energy_data_provider.h"
#include "openv2g/support/plc_link_manager.h"
#include "openv2g/simulation/slac_peer.h"

#define OPENV2G_HOST_SIM_V2GTP_STORAGE_CAPACITY 1024u
#define OPENV2G_HOST_SIM_TRACE_CAPACITY 64u

/* Host-only trace identifiers. They are not AUTOSAR operation or DEM IDs. */
typedef enum {
    OPENV2G_HOST_SIM_OP_NONE = 0,
    OPENV2G_HOST_SIM_OP_CP_PP_PUBLISHED = 1,
    OPENV2G_HOST_SIM_OP_ENERGY_PUBLISHED = 2,
    OPENV2G_HOST_SIM_OP_LINK_START = 3,
    OPENV2G_HOST_SIM_OP_LINK_READY = 4,
    OPENV2G_HOST_SIM_OP_HLC_START = 5,
    OPENV2G_HOST_SIM_OP_HLC_READY = 6,
    OPENV2G_HOST_SIM_OP_PROTOCOL_RESPONSE = 7,
    OPENV2G_HOST_SIM_OP_HW_PREPARE = 8,
    OPENV2G_HOST_SIM_OP_HW_ENABLE = 9,
    OPENV2G_HOST_SIM_OP_HW_SAFE = 10,
    OPENV2G_HOST_SIM_OP_HLC_STOP = 11,
    OPENV2G_HOST_SIM_OP_LINK_STOP = 12
} OpenV2g_HostSimulationOperation;

typedef struct {
    OpenV2g_HostSimulationOperation operation;
    OpenV2g_TimeUs timestamp;
    uint32_t generation;
    OpenV2g_Result result;
} OpenV2g_HostSimulationTraceEntry;

typedef struct {
    OpenV2g_CoordinatorConfig coordinator;
    OpenV2g_HlcFeatureConfig features;
    OpenV2g_ProtocolId peer_protocol;
    OpenV2g_SchemaId peer_schema;
    OpenV2g_Endpoint peer_endpoint;
    bool simulate_slac;
} OpenV2g_HostSimulationConfig;

/*
 * Host/SIL composition fixture. It deliberately owns project-side logical
 * components only; no supplier AUTOSAR ABI, BSW driver or physical actuator is
 * represented here.
 */
typedef struct {
    OpenV2g_HostSimulationConfig config;
    OpenV2g_ChargingCoordinator coordinator;
    OpenV2g_CpPpManager cp_pp;
    OpenV2g_EnergyDataProvider energy;
    OpenV2g_PlcLinkManager plc;
    OpenV2g_HostSlacPeer slac_peer;
    OpenV2g_Iso15118Chrg hlc;
    OpenV2g_ChargeHardwareAdapter hardware;

    OpenV2g_PlcLinkControlCsPort plc_control_port;
    OpenV2g_HlcControlCsPort hlc_control_port;
    OpenV2g_ChargeHardwareControlCsPort hardware_control_port;
    OpenV2g_CpPpStatusSrPort cp_pp_status_port;
    OpenV2g_EnergySnapshotSrPort energy_status_port;
    OpenV2g_PlcLinkStatusSrPort plc_status_port;
    OpenV2g_HlcStatusSrPort hlc_status_port;
    OpenV2g_ChargeHardwareStatusSrPort hardware_status_port;
    OpenV2g_ChargePermissionSrPort permission_status_port;

    uint8_t v2gtp_storage[OPENV2G_HOST_SIM_V2GTP_STORAGE_CAPACITY];
    OpenV2g_HostSimulationTraceEntry trace[OPENV2G_HOST_SIM_TRACE_CAPACITY];
    size_t trace_count;
    bool trace_overflow;
} OpenV2g_HostSimulation;

OpenV2g_HostSimulationConfig OpenV2g_HostSimulation_DefaultConfig(void);

OpenV2g_Result OpenV2g_HostSimulation_Init(
    OpenV2g_HostSimulation *simulation,
    const OpenV2g_HostSimulationConfig *config);

OpenV2g_QualifiedCpPp OpenV2g_HostSimulation_DefaultCpPp(
    const OpenV2g_HostSimulation *simulation,
    OpenV2g_TimeUs now);

OpenV2g_EvEnergySnapshot OpenV2g_HostSimulation_DefaultEnergy(
    const OpenV2g_HostSimulation *simulation,
    OpenV2g_TimeUs now);

/* External environment injection points. */
OpenV2g_Result OpenV2g_HostSimulation_InjectCpPp(
    OpenV2g_HostSimulation *simulation,
    const OpenV2g_QualifiedCpPp *value,
    OpenV2g_TimeUs now);

OpenV2g_Result OpenV2g_HostSimulation_InjectEnergy(
    OpenV2g_HostSimulation *simulation,
    const OpenV2g_EvEnergySnapshot *value);

/* Surrounding-component drivers for the nominal host flow. */
OpenV2g_Result OpenV2g_HostSimulation_AdvancePlcToReady(
    OpenV2g_HostSimulation *simulation,
    OpenV2g_TimeUs now);

OpenV2g_Result OpenV2g_HostSimulation_InjectLinkLoss(
    OpenV2g_HostSimulation *simulation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason);

OpenV2g_Result OpenV2g_HostSimulation_EstablishHlc(
    OpenV2g_HostSimulation *simulation,
    OpenV2g_TimeUs now);

OpenV2g_Result OpenV2g_HostSimulation_RespondToNextProtocolRequest(
    OpenV2g_HostSimulation *simulation,
    OpenV2g_TimeUs now,
    bool accepted,
    OpenV2g_ProtocolMessageKind *completed_kind);

OpenV2g_Result OpenV2g_HostSimulation_InitializeHardwareSafe(
    OpenV2g_HostSimulation *simulation,
    OpenV2g_TimeUs now);

OpenV2g_Result OpenV2g_HostSimulation_InjectHardwareFeedback(
    OpenV2g_HostSimulation *simulation,
    const OpenV2g_ChargeHardwareFeedback *feedback);

OpenV2g_Result OpenV2g_HostSimulation_PrepareEnergyTransfer(
    OpenV2g_HostSimulation *simulation,
    OpenV2g_TimeUs now);

OpenV2g_Result OpenV2g_HostSimulation_StartEnergyTransfer(
    OpenV2g_HostSimulation *simulation,
    OpenV2g_TimeUs now);

OpenV2g_Result OpenV2g_HostSimulation_Stop(
    OpenV2g_HostSimulation *simulation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason);

const char *OpenV2g_HostSimulation_OperationName(
    OpenV2g_HostSimulationOperation operation);

#endif
