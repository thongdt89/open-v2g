#ifndef OPENV2G_ISO15118_CHRG_PROTOCOL_ADAPTER_FACTORY_H
#define OPENV2G_ISO15118_CHRG_PROTOCOL_ADAPTER_FACTORY_H

#include <stdbool.h>

#include "openv2g/iso15118_chrg/din70121_adapter.h"
#include "openv2g/iso15118_chrg/iso15118_2_adapter.h"

typedef enum {
    OPENV2G_ADAPTER_FACTORY_READY = 0,
    OPENV2G_ADAPTER_FACTORY_CREATING = 1,
    OPENV2G_ADAPTER_FACTORY_ACTIVE = 2,
    OPENV2G_ADAPTER_FACTORY_DESTROYING = 3,
    OPENV2G_ADAPTER_FACTORY_FAILED = 4
} OpenV2g_AdapterFactoryState;

typedef struct {
    uint32_t sequence_counter;
    uint32_t generation;
    OpenV2g_AdapterFactoryState state;
    OpenV2g_ProtocolId active_protocol;
    OpenV2g_SchemaId active_schema;
    OpenV2g_ReasonCode reason;
} OpenV2g_AdapterFactoryStatus;

typedef struct {
    OpenV2g_Din70121Adapter *din;
    OpenV2g_Iso15118_2Adapter *iso2;
    OpenV2g_ProtocolAdapterCsPort active_port;
    OpenV2g_ProtocolAdapterStatusSrPort active_status_port;
    OpenV2g_ProtocolDiagnosticSrPort active_diagnostic_port;
    OpenV2g_AdapterFactoryStatus status;
} OpenV2g_ProtocolAdapterFactory;

typedef struct {
    void *context;
    OpenV2g_Result (*create)(
        void *context,
        const OpenV2g_ProtocolStartRequest *request,
        OpenV2g_ProtocolAdapterCsPort *adapter_port,
        OpenV2g_ProtocolAdapterStatusSrPort *status_port,
        OpenV2g_ProtocolDiagnosticSrPort *diagnostic_port);
    OpenV2g_Result (*destroy)(
        void *context,
        uint32_t generation,
        OpenV2g_ReasonCode reason);
    OpenV2g_Result (*reset)(void *context, uint32_t generation);
} OpenV2g_ProtocolAdapterFactoryCsPort;

typedef struct {
    void *context;
    OpenV2g_Result (*read)(
        void *context,
        OpenV2g_AdapterFactoryStatus *status);
} OpenV2g_AdapterFactoryStatusSrPort;

void OpenV2g_ProtocolAdapterFactory_Init(
    OpenV2g_ProtocolAdapterFactory *factory,
    OpenV2g_Din70121Adapter *din,
    OpenV2g_Iso15118_2Adapter *iso2);
OpenV2g_Result OpenV2g_ProtocolAdapterFactory_Create(
    OpenV2g_ProtocolAdapterFactory *factory,
    const OpenV2g_ProtocolStartRequest *request,
    OpenV2g_ProtocolAdapterCsPort *adapter_port,
    OpenV2g_ProtocolAdapterStatusSrPort *status_port,
    OpenV2g_ProtocolDiagnosticSrPort *diagnostic_port);
OpenV2g_Result OpenV2g_ProtocolAdapterFactory_Destroy(
    OpenV2g_ProtocolAdapterFactory *factory,
    uint32_t generation,
    OpenV2g_ReasonCode reason);
OpenV2g_Result OpenV2g_ProtocolAdapterFactory_Reset(
    OpenV2g_ProtocolAdapterFactory *factory,
    uint32_t generation);
OpenV2g_Result OpenV2g_ProtocolAdapterFactory_CreateClientServerPort(
    OpenV2g_ProtocolAdapterFactory *factory,
    OpenV2g_ProtocolAdapterFactoryCsPort *port);
OpenV2g_Result OpenV2g_ProtocolAdapterFactory_CreateStatusSenderReceiverPort(
    OpenV2g_ProtocolAdapterFactory *factory,
    OpenV2g_AdapterFactoryStatusSrPort *port);

#endif
