#ifndef OPENV2G_ISO15118_CHRG_ISO15118_2_ADAPTER_H
#define OPENV2G_ISO15118_CHRG_ISO15118_2_ADAPTER_H

#include <stdbool.h>

#include "openv2g/iso15118_chrg/protocol_adapter.h"

typedef struct {
    bool dc_eim_enabled;
    bool ac_eim_enabled;
    bool pnc_enabled;
    OpenV2g_ProtocolAdapterStatus status;
} OpenV2g_Iso15118_2Adapter;

void OpenV2g_Iso15118_2Adapter_Init(
    OpenV2g_Iso15118_2Adapter *adapter,
    bool dc_eim_enabled,
    bool ac_eim_enabled,
    bool pnc_enabled);
OpenV2g_Result OpenV2g_Iso15118_2Adapter_CreateClientServerPort(
    OpenV2g_Iso15118_2Adapter *adapter,
    OpenV2g_ProtocolAdapterCsPort *port);
OpenV2g_Result OpenV2g_Iso15118_2Adapter_CreateStatusSenderReceiverPort(
    OpenV2g_Iso15118_2Adapter *adapter,
    OpenV2g_ProtocolAdapterStatusSrPort *port);

#endif
