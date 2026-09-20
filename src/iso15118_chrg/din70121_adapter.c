#include "openv2g/iso15118_chrg/din70121_adapter.h"
#include "openv2g/iso15118_chrg/protocol_data_flow.h"

#include <string.h>

static void set_state(
    OpenV2g_Din70121Adapter *adapter,
    OpenV2g_ProtocolState state,
    OpenV2g_ReasonCode reason)
{
    adapter->status.sequence_counter++;
    adapter->status.state = state;
    adapter->status.reason = reason;
    adapter->status.active =
        (state >= OPENV2G_PROTOCOL_STATE_SESSION_SETUP) &&
        (state < OPENV2G_PROTOCOL_STATE_TERMINATED);
}

static void fail_adapter(
    OpenV2g_Din70121Adapter *adapter,
    OpenV2g_ProtocolMessageKind kind,
    uint32_t correlation_id,
    OpenV2g_ReasonCode reason)
{
    adapter->status.pending_correlation_id = 0u;
    adapter->status.pending_kind = OPENV2G_PROTOCOL_MESSAGE_NONE;
    adapter->diagnostic.generation = adapter->status.generation;
    adapter->diagnostic.sequence_counter++;
    adapter->diagnostic.protocol = OPENV2G_PROTOCOL_DIN_70121;
    adapter->diagnostic.state = adapter->status.state;
    adapter->diagnostic.message_kind = kind;
    adapter->diagnostic.correlation_id = correlation_id;
    adapter->diagnostic.reason = reason;
    set_state(adapter, OPENV2G_PROTOCOL_STATE_FAILED, reason);
}

static OpenV2g_Result send_request(
    OpenV2g_Din70121Adapter *adapter,
    OpenV2g_ProtocolMessageKind kind,
    OpenV2g_TimeUs now)
{
    OpenV2g_ProtocolTxRequest request;
    OpenV2g_ReasonCode reason;
    OpenV2g_Result result;
    uint32_t correlation_id = adapter->next_correlation_id + 1u;

    if (correlation_id == 0u) {
        correlation_id = 1u;
    }
    result = OpenV2g_ProtocolDataFlow_PrepareRequest(
        OPENV2G_PROTOCOL_DIN_70121,
        OPENV2G_SCHEMA_DIN_70121,
        OPENV2G_MODE_DC,
        kind,
        adapter->status.generation,
        correlation_id,
        now,
        &adapter->energy_required_port,
        &adapter->permission_required_port,
        NULL,
        &request,
        &reason);
    if (result != OPENV2G_RESULT_OK) {
        fail_adapter(adapter, kind, correlation_id, reason);
        return result;
    }
    result = adapter->message_required_port.send(
        adapter->message_required_port.context, &request);
    if ((result != OPENV2G_RESULT_OK) &&
        (result != OPENV2G_RESULT_ACCEPTED)) {
        fail_adapter(
            adapter, kind, request.correlation_id, OPENV2G_REASON_RESOURCE);
        return result;
    }
    adapter->next_correlation_id = correlation_id;
    adapter->status.pending_correlation_id = request.correlation_id;
    adapter->status.pending_kind = kind;
    return OPENV2G_RESULT_ACCEPTED;
}

static OpenV2g_Result din_start(
    void *context,
    const OpenV2g_ProtocolStartRequest *request)
{
    OpenV2g_Din70121Adapter *adapter =
        (OpenV2g_Din70121Adapter *)context;
    if ((adapter == NULL) || (request == NULL) || !adapter->enabled ||
        (adapter->message_required_port.send == NULL) ||
        (request->generation == 0u) ||
        (request->protocol != OPENV2G_PROTOCOL_DIN_70121) ||
        (request->schema != OPENV2G_SCHEMA_DIN_70121) ||
        (request->mode != OPENV2G_MODE_DC) ||
        (request->security != OPENV2G_SECURITY_NONE) ||
        ((adapter->status.state != OPENV2G_PROTOCOL_STATE_IDLE) &&
         (adapter->status.state != OPENV2G_PROTOCOL_STATE_TERMINATED))) {
        return OPENV2G_RESULT_REJECTED;
    }
    adapter->status.generation = request->generation;
    adapter->status.protocol = request->protocol;
    adapter->status.schema = request->schema;
    adapter->status.mode = request->mode;
    adapter->status.last_correlation_id = 0u;
    adapter->status.pending_correlation_id = 0u;
    adapter->status.pending_kind = OPENV2G_PROTOCOL_MESSAGE_NONE;
    adapter->next_correlation_id = 0u;
    (void)memset(&adapter->diagnostic, 0, sizeof(adapter->diagnostic));
    set_state(
        adapter, OPENV2G_PROTOCOL_STATE_SESSION_SETUP, OPENV2G_REASON_NONE);
    return send_request(
        adapter, OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP, request->now);
}

static OpenV2g_Result din_stop(
    void *context,
    uint32_t generation,
    OpenV2g_ReasonCode reason)
{
    OpenV2g_Din70121Adapter *adapter =
        (OpenV2g_Din70121Adapter *)context;
    if (adapter == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    if ((adapter->status.state == OPENV2G_PROTOCOL_STATE_DISABLED) ||
        (adapter->status.state == OPENV2G_PROTOCOL_STATE_IDLE)) {
        return OPENV2G_RESULT_OK;
    }
    if (generation != adapter->status.generation) {
        return OPENV2G_RESULT_REJECTED;
    }
    adapter->status.pending_correlation_id = 0u;
    adapter->status.pending_kind = OPENV2G_PROTOCOL_MESSAGE_NONE;
    if (adapter->status.state != OPENV2G_PROTOCOL_STATE_TERMINATED) {
        set_state(adapter, OPENV2G_PROTOCOL_STATE_TERMINATED, reason);
    }
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result advance_after_response(
    OpenV2g_Din70121Adapter *adapter,
    OpenV2g_ProtocolMessageKind kind,
    OpenV2g_TimeUs now)
{
    OpenV2g_ProtocolState next_state;
    OpenV2g_ProtocolMessageKind next_kind;

    switch (kind) {
    case OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP:
        next_state = OPENV2G_PROTOCOL_STATE_SERVICE_DISCOVERY;
        next_kind = OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY;
        break;
    case OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY:
        next_state = OPENV2G_PROTOCOL_STATE_SERVICE_SELECTION;
        next_kind = OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION;
        break;
    case OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION:
        next_state = OPENV2G_PROTOCOL_STATE_AUTHORIZATION;
        next_kind = OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION;
        break;
    case OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION:
        next_state = OPENV2G_PROTOCOL_STATE_CHARGE_PARAMETER;
        next_kind = OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER;
        break;
    case OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER:
        next_state = OPENV2G_PROTOCOL_STATE_CABLE_CHECK;
        next_kind = OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK;
        break;
    case OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK:
        next_state = OPENV2G_PROTOCOL_STATE_PRECHARGE;
        next_kind = OPENV2G_PROTOCOL_MESSAGE_PRECHARGE;
        break;
    case OPENV2G_PROTOCOL_MESSAGE_PRECHARGE:
        next_state = OPENV2G_PROTOCOL_STATE_POWER_DELIVERY;
        next_kind = OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START;
        break;
    case OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START:
        next_state = OPENV2G_PROTOCOL_STATE_CHARGE_LOOP;
        next_kind = OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND;
        break;
    case OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND:
        next_state = OPENV2G_PROTOCOL_STATE_POWER_DELIVERY_STOP;
        next_kind = OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP;
        break;
    case OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP:
        next_state = OPENV2G_PROTOCOL_STATE_WELDING_DETECTION;
        next_kind = OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION;
        break;
    case OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION:
        next_state = OPENV2G_PROTOCOL_STATE_SESSION_STOP;
        next_kind = OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP;
        break;
    case OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP:
        set_state(
            adapter, OPENV2G_PROTOCOL_STATE_TERMINATED, OPENV2G_REASON_NONE);
        return OPENV2G_RESULT_OK;
    default:
        fail_adapter(adapter, kind, 0u, OPENV2G_REASON_SEQUENCE);
        return OPENV2G_RESULT_REJECTED;
    }
    set_state(adapter, next_state, OPENV2G_REASON_NONE);
    return send_request(adapter, next_kind, now);
}

static OpenV2g_Result din_message(
    void *context,
    const OpenV2g_ProtocolMessage *message)
{
    OpenV2g_Din70121Adapter *adapter =
        (OpenV2g_Din70121Adapter *)context;
    OpenV2g_ProtocolMessageKind completed_kind;

    if ((adapter == NULL) || (message == NULL) ||
        (message->generation != adapter->status.generation) ||
        !adapter->status.active) {
        return OPENV2G_RESULT_REJECTED;
    }
    if ((message->correlation_id != adapter->status.pending_correlation_id) ||
        (message->kind != adapter->status.pending_kind)) {
        fail_adapter(
            adapter,
            message->kind,
            message->correlation_id,
            OPENV2G_REASON_SEQUENCE);
        return OPENV2G_RESULT_REJECTED;
    }
    completed_kind = adapter->status.pending_kind;
    adapter->status.last_correlation_id = message->correlation_id;
    adapter->status.pending_correlation_id = 0u;
    adapter->status.pending_kind = OPENV2G_PROTOCOL_MESSAGE_NONE;
    if (!message->accepted) {
        fail_adapter(
            adapter,
            completed_kind,
            message->correlation_id,
            OPENV2G_REASON_PEER_REJECT);
        return OPENV2G_RESULT_REJECTED;
    }
    if (message->ongoing) {
        return send_request(adapter, completed_kind, message->now);
    }
    return advance_after_response(adapter, completed_kind, message->now);
}

static OpenV2g_Result din_timer(
    void *context,
    uint32_t generation,
    uint16_t timer_id)
{
    OpenV2g_Din70121Adapter *adapter =
        (OpenV2g_Din70121Adapter *)context;
    if ((adapter == NULL) || !adapter->status.active || (timer_id == 0u) ||
        (generation != adapter->status.generation) ||
        (adapter->status.pending_correlation_id == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }
    fail_adapter(
        adapter,
        adapter->status.pending_kind,
        adapter->status.pending_correlation_id,
        OPENV2G_REASON_TIMEOUT);
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result din_read_status(
    void *context,
    OpenV2g_ProtocolAdapterStatus *status)
{
    const OpenV2g_Din70121Adapter *adapter =
        (const OpenV2g_Din70121Adapter *)context;
    if ((adapter == NULL) || (status == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    *status = adapter->status;
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result din_read_diagnostic(
    void *context,
    OpenV2g_ProtocolDiagnostic *diagnostic)
{
    const OpenV2g_Din70121Adapter *adapter =
        (const OpenV2g_Din70121Adapter *)context;
    if ((adapter == NULL) || (diagnostic == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (adapter->diagnostic.sequence_counter == 0u) {
        return OPENV2G_RESULT_NOT_IMPLEMENTED;
    }
    *diagnostic = adapter->diagnostic;
    return OPENV2G_RESULT_OK;
}

void OpenV2g_Din70121Adapter_Init(
    OpenV2g_Din70121Adapter *adapter,
    bool enabled)
{
    if (adapter == NULL) {
        return;
    }
    (void)memset(adapter, 0, sizeof(*adapter));
    adapter->enabled = enabled;
    adapter->status.protocol = OPENV2G_PROTOCOL_DIN_70121;
    adapter->status.schema = OPENV2G_SCHEMA_DIN_70121;
    adapter->status.mode = OPENV2G_MODE_DC;
    adapter->status.state = enabled ? OPENV2G_PROTOCOL_STATE_IDLE
                                    : OPENV2G_PROTOCOL_STATE_DISABLED;
}

OpenV2g_Result OpenV2g_Din70121Adapter_BindRequiredPorts(
    OpenV2g_Din70121Adapter *adapter,
    const OpenV2g_ProtocolMessageChannelCsPort *message_port,
    const OpenV2g_EnergySnapshotSrPort *energy_port,
    const OpenV2g_ChargePermissionSrPort *permission_port)
{
    if ((adapter == NULL) || (message_port == NULL) ||
        (message_port->send == NULL) ||
        ((adapter->status.state != OPENV2G_PROTOCOL_STATE_IDLE) &&
         (adapter->status.state != OPENV2G_PROTOCOL_STATE_TERMINATED))) {
        return OPENV2G_RESULT_REJECTED;
    }
    adapter->message_required_port = *message_port;
    if (energy_port != NULL) {
        adapter->energy_required_port = *energy_port;
    }
    if (permission_port != NULL) {
        adapter->permission_required_port = *permission_port;
    }
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_Din70121Adapter_CreateClientServerPort(
    OpenV2g_Din70121Adapter *adapter,
    OpenV2g_ProtocolAdapterCsPort *port)
{
    if ((adapter == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = adapter;
    port->start = din_start;
    port->stop = din_stop;
    port->on_message = din_message;
    port->on_timer_expired = din_timer;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_Din70121Adapter_CreateStatusSenderReceiverPort(
    OpenV2g_Din70121Adapter *adapter,
    OpenV2g_ProtocolAdapterStatusSrPort *port)
{
    if ((adapter == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = adapter;
    port->read = din_read_status;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_Din70121Adapter_CreateDiagnosticSenderReceiverPort(
    OpenV2g_Din70121Adapter *adapter,
    OpenV2g_ProtocolDiagnosticSrPort *port)
{
    if ((adapter == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = adapter;
    port->read = din_read_diagnostic;
    return OPENV2G_RESULT_OK;
}
