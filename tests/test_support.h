#ifndef OPENV2G_TEST_SUPPORT_H
#define OPENV2G_TEST_SUPPORT_H

#include <stddef.h>

#include "openv2g/charging_coordinator/charging_coordinator.h"
#include "openv2g/iso15118_chrg/iso15118_chrg.h"
#include "openv2g/support/charge_hardware_adapter.h"
#include "openv2g/support/plc_link_manager.h"

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
    OpenV2g_PlcLinkManager plc;
    OpenV2g_Iso15118Chrg hlc;
    OpenV2g_ChargeHardwareAdapter hardware;
    OpenV2g_TestOperation operations[32];
    size_t operation_count;
} OpenV2g_TestRig;

void OpenV2g_TestRig_Init(OpenV2g_TestRig *rig);
OpenV2g_CoordinatorPorts OpenV2g_TestRig_Ports(OpenV2g_TestRig *rig);
OpenV2g_CoordinatorConfig OpenV2g_TestRig_Config(void);

#endif
