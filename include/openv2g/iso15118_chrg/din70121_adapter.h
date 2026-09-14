#ifndef OPENV2G_ISO15118_CHRG_DIN70121_ADAPTER_H
#define OPENV2G_ISO15118_CHRG_DIN70121_ADAPTER_H

#include <stdbool.h>

#include "openv2g/iso15118_chrg/protocol_adapter.h"

typedef struct {
    bool enabled;
    OpenV2g_ProtocolAdapterStatus status;
} OpenV2g_Din70121Adapter;

void OpenV2g_Din70121Adapter_Init(
    OpenV2g_Din70121Adapter *adapter,
    bool enabled);
OpenV2g_Result OpenV2g_Din70121Adapter_CreateClientServerPort(
    OpenV2g_Din70121Adapter *adapter,
    OpenV2g_ProtocolAdapterCsPort *port);
OpenV2g_Result OpenV2g_Din70121Adapter_CreateStatusSenderReceiverPort(
    OpenV2g_Din70121Adapter *adapter,
    OpenV2g_ProtocolAdapterStatusSrPort *port);

#endif
