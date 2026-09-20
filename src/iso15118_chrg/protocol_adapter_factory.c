#include "openv2g/iso15118_chrg/protocol_adapter_factory.h"

#include <string.h>

static void publish_status(
    OpenV2g_ProtocolAdapterFactory *factory,
    OpenV2g_AdapterFactoryState state,
    OpenV2g_ReasonCode reason)
{
    factory->status.sequence_counter++;
    factory->status.state = state;
    factory->status.reason = reason;
}

void OpenV2g_ProtocolAdapterFactory_Init(
    OpenV2g_ProtocolAdapterFactory *factory,
    OpenV2g_Din70121Adapter *din,
    OpenV2g_Iso15118_2Adapter *iso2)
{
    if (factory == NULL) {
        return;
    }
    (void)memset(factory, 0, sizeof(*factory));
    factory->din = din;
    factory->iso2 = iso2;
    factory->status.state = OPENV2G_ADAPTER_FACTORY_READY;
}

OpenV2g_Result OpenV2g_ProtocolAdapterFactory_Create(
    OpenV2g_ProtocolAdapterFactory *factory,
    const OpenV2g_ProtocolStartRequest *request,
    OpenV2g_ProtocolAdapterCsPort *adapter_port,
    OpenV2g_ProtocolAdapterStatusSrPort *status_port,
    OpenV2g_ProtocolDiagnosticSrPort *diagnostic_port)
{
    OpenV2g_Result result;

    if ((factory == NULL) || (request == NULL) || (adapter_port == NULL) ||
        (status_port == NULL) || (diagnostic_port == NULL) ||
        (request->generation == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (factory->status.state == OPENV2G_ADAPTER_FACTORY_ACTIVE) {
        return OPENV2G_RESULT_BUSY;
    }
    if (factory->status.state != OPENV2G_ADAPTER_FACTORY_READY) {
        return OPENV2G_RESULT_REJECTED;
    }

    publish_status(
        factory, OPENV2G_ADAPTER_FACTORY_CREATING, OPENV2G_REASON_NONE);
    (void)memset(&factory->active_port, 0, sizeof(factory->active_port));
    (void)memset(
        &factory->active_status_port, 0, sizeof(factory->active_status_port));
    (void)memset(
        &factory->active_diagnostic_port,
        0,
        sizeof(factory->active_diagnostic_port));
    if ((request->protocol == OPENV2G_PROTOCOL_DIN_70121) &&
        (request->schema == OPENV2G_SCHEMA_DIN_70121) &&
        (factory->din != NULL)) {
        result = OpenV2g_Din70121Adapter_CreateClientServerPort(
            factory->din, &factory->active_port);
        if (result == OPENV2G_RESULT_OK) {
            result = OpenV2g_Din70121Adapter_CreateStatusSenderReceiverPort(
                factory->din, &factory->active_status_port);
        }
        if (result == OPENV2G_RESULT_OK) {
            result = OpenV2g_Din70121Adapter_CreateDiagnosticSenderReceiverPort(
                factory->din, &factory->active_diagnostic_port);
        }
    } else if ((request->protocol == OPENV2G_PROTOCOL_ISO_15118_2) &&
               (request->schema == OPENV2G_SCHEMA_ISO_15118_2) &&
               (factory->iso2 != NULL)) {
        result = OpenV2g_Iso15118_2Adapter_CreateClientServerPort(
            factory->iso2, &factory->active_port);
        if (result == OPENV2G_RESULT_OK) {
            result =
                OpenV2g_Iso15118_2Adapter_CreateStatusSenderReceiverPort(
                    factory->iso2, &factory->active_status_port);
        }
        if (result == OPENV2G_RESULT_OK) {
            result =
                OpenV2g_Iso15118_2Adapter_CreateDiagnosticSenderReceiverPort(
                    factory->iso2, &factory->active_diagnostic_port);
        }
    } else {
        result = OPENV2G_RESULT_NOT_IMPLEMENTED;
    }

    if (result == OPENV2G_RESULT_OK) {
        result = factory->active_port.start(
            factory->active_port.context, request);
    }
    if ((result != OPENV2G_RESULT_OK) &&
        (result != OPENV2G_RESULT_ACCEPTED)) {
        (void)memset(&factory->active_port, 0, sizeof(factory->active_port));
        (void)memset(
            &factory->active_status_port,
            0,
            sizeof(factory->active_status_port));
        (void)memset(
            &factory->active_diagnostic_port,
            0,
            sizeof(factory->active_diagnostic_port));
        factory->status.generation = request->generation;
        publish_status(
            factory,
            OPENV2G_ADAPTER_FACTORY_FAILED,
            (result == OPENV2G_RESULT_NOT_IMPLEMENTED)
                ? OPENV2G_REASON_CONFIGURATION
                : OPENV2G_REASON_PRECONDITION);
        return result;
    }

    factory->status.generation = request->generation;
    factory->status.active_protocol = request->protocol;
    factory->status.active_schema = request->schema;
    publish_status(
        factory, OPENV2G_ADAPTER_FACTORY_ACTIVE, OPENV2G_REASON_NONE);
    *adapter_port = factory->active_port;
    *status_port = factory->active_status_port;
    *diagnostic_port = factory->active_diagnostic_port;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ProtocolAdapterFactory_Destroy(
    OpenV2g_ProtocolAdapterFactory *factory,
    uint32_t generation,
    OpenV2g_ReasonCode reason)
{
    OpenV2g_Result result;
    if (factory == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (factory->status.state == OPENV2G_ADAPTER_FACTORY_READY) {
        return OPENV2G_RESULT_OK;
    }
    if ((factory->status.state != OPENV2G_ADAPTER_FACTORY_ACTIVE) ||
        (generation != factory->status.generation)) {
        return OPENV2G_RESULT_REJECTED;
    }
    publish_status(
        factory, OPENV2G_ADAPTER_FACTORY_DESTROYING, reason);
    result = factory->active_port.stop(
        factory->active_port.context, generation, reason);
    if (result != OPENV2G_RESULT_OK) {
        publish_status(
            factory, OPENV2G_ADAPTER_FACTORY_FAILED, OPENV2G_REASON_INTERNAL);
        return result;
    }
    (void)memset(&factory->active_port, 0, sizeof(factory->active_port));
    (void)memset(
        &factory->active_status_port, 0, sizeof(factory->active_status_port));
    (void)memset(
        &factory->active_diagnostic_port,
        0,
        sizeof(factory->active_diagnostic_port));
    factory->status.active_protocol = OPENV2G_PROTOCOL_NONE;
    factory->status.active_schema = OPENV2G_SCHEMA_NONE;
    publish_status(factory, OPENV2G_ADAPTER_FACTORY_READY, reason);
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ProtocolAdapterFactory_Reset(
    OpenV2g_ProtocolAdapterFactory *factory,
    uint32_t generation)
{
    if (factory == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (factory->status.state == OPENV2G_ADAPTER_FACTORY_ACTIVE) {
        return OpenV2g_ProtocolAdapterFactory_Destroy(
            factory, generation, OPENV2G_REASON_CANCELLED);
    }
    if ((factory->status.state == OPENV2G_ADAPTER_FACTORY_FAILED) &&
        (factory->status.generation != generation)) {
        return OPENV2G_RESULT_REJECTED;
    }
    (void)memset(&factory->active_port, 0, sizeof(factory->active_port));
    (void)memset(
        &factory->active_status_port, 0, sizeof(factory->active_status_port));
    (void)memset(
        &factory->active_diagnostic_port,
        0,
        sizeof(factory->active_diagnostic_port));
    factory->status.active_protocol = OPENV2G_PROTOCOL_NONE;
    factory->status.active_schema = OPENV2G_SCHEMA_NONE;
    publish_status(
        factory, OPENV2G_ADAPTER_FACTORY_READY, OPENV2G_REASON_NONE);
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result cs_create(
    void *context,
    const OpenV2g_ProtocolStartRequest *request,
    OpenV2g_ProtocolAdapterCsPort *adapter_port,
    OpenV2g_ProtocolAdapterStatusSrPort *status_port,
    OpenV2g_ProtocolDiagnosticSrPort *diagnostic_port)
{
    return OpenV2g_ProtocolAdapterFactory_Create(
        (OpenV2g_ProtocolAdapterFactory *)context,
        request,
        adapter_port,
        status_port,
        diagnostic_port);
}

static OpenV2g_Result cs_destroy(
    void *context,
    uint32_t generation,
    OpenV2g_ReasonCode reason)
{
    return OpenV2g_ProtocolAdapterFactory_Destroy(
        (OpenV2g_ProtocolAdapterFactory *)context, generation, reason);
}

static OpenV2g_Result cs_reset(void *context, uint32_t generation)
{
    return OpenV2g_ProtocolAdapterFactory_Reset(
        (OpenV2g_ProtocolAdapterFactory *)context, generation);
}

static OpenV2g_Result sr_read(
    void *context,
    OpenV2g_AdapterFactoryStatus *status)
{
    const OpenV2g_ProtocolAdapterFactory *factory =
        (const OpenV2g_ProtocolAdapterFactory *)context;
    if ((factory == NULL) || (status == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    *status = factory->status;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ProtocolAdapterFactory_CreateClientServerPort(
    OpenV2g_ProtocolAdapterFactory *factory,
    OpenV2g_ProtocolAdapterFactoryCsPort *port)
{
    if ((factory == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = factory;
    port->create = cs_create;
    port->destroy = cs_destroy;
    port->reset = cs_reset;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ProtocolAdapterFactory_CreateStatusSenderReceiverPort(
    OpenV2g_ProtocolAdapterFactory *factory,
    OpenV2g_AdapterFactoryStatusSrPort *port)
{
    if ((factory == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = factory;
    port->read = sr_read;
    return OPENV2G_RESULT_OK;
}
