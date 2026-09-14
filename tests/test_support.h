#ifndef OPENV2G_TEST_SUPPORT_H
#define OPENV2G_TEST_SUPPORT_H

#include <stddef.h>

#include "openv2g/charging_coordinator/charging_coordinator.h"
#include "openv2g/iso15118_chrg/iso15118_chrg.h"
#include "openv2g/support/charge_hardware_adapter.h"
#include "openv2g/support/cp_pp_manager.h"
#include "openv2g/support/energy_data_provider.h"
#include "openv2g/support/plc_link_manager.h"

#define OPENV2G_TEST_V2GTP_STORAGE_CAPACITY 1024u

typedef enum {
    TEST_OP_LINK_START = 1,
    TEST_OP_HLC_START = 2,
    TEST_OP_HW_PREPARE = 3,
    TEST_OP_HW_ENABLE = 4,
    TEST_OP_HW_SAFE = 5,
    TEST_OP_HLC_STOP = 6,
    TEST_OP_LINK_STOP = 7
} OpenV2g_TestOperation;

typedef struct {
    OpenV2g_TimeUs now;
    OpenV2g_CpPpManager cp_pp;
    OpenV2g_EnergyDataProvider energy;
    OpenV2g_PlcLinkManager plc;
    OpenV2g_Iso15118Chrg hlc;
    OpenV2g_ChargeHardwareAdapter hardware;
    OpenV2g_PlcLinkControlCsPort plc_control_port;
    OpenV2g_HlcControlCsPort hlc_control_port;
    OpenV2g_ChargeHardwareControlCsPort hardware_control_port;
    OpenV2g_CpPpStatusSrPort cp_pp_status_port;
    OpenV2g_EnergySnapshotSrPort energy_status_port;
    OpenV2g_PlcLinkStatusSrPort plc_status_port;
    OpenV2g_HlcStatusSrPort hlc_status_port;
    OpenV2g_HlcEventQueuedSrPort hlc_event_port;
    OpenV2g_ChargeHardwareStatusSrPort hardware_status_port;
    uint8_t v2gtp_storage[OPENV2G_TEST_V2GTP_STORAGE_CAPACITY];
    OpenV2g_TestOperation operations[32];
    size_t operation_count;
} OpenV2g_TestRig;

void OpenV2g_TestRig_Init(OpenV2g_TestRig *rig);
OpenV2g_CoordinatorPorts OpenV2g_TestRig_Ports(OpenV2g_TestRig *rig);
OpenV2g_CoordinatorConfig OpenV2g_TestRig_Config(void);
OpenV2g_Endpoint OpenV2g_TestEndpoint(OpenV2g_SecurityMode security);
OpenV2g_Result OpenV2g_TestRig_ReadCpPp(
    OpenV2g_TestRig *rig,
    OpenV2g_QualifiedCpPp *value);
OpenV2g_Result OpenV2g_TestRig_ReadEnergy(
    OpenV2g_TestRig *rig,
    OpenV2g_EvEnergySnapshot *value);
OpenV2g_Result OpenV2g_TestRig_ReadPlc(
    OpenV2g_TestRig *rig,
    OpenV2g_PlcLinkStatus *value);
OpenV2g_Result OpenV2g_TestRig_ReadHlc(
    OpenV2g_TestRig *rig,
    OpenV2g_HlcSessionStatus *value);
OpenV2g_Result OpenV2g_TestRig_ReadHardware(
    OpenV2g_TestRig *rig,
    OpenV2g_ChargeHardwareStatus *value);

#endif
