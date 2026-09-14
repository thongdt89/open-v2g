#include "openv2g/iso15118_chrg/din70121_adapter.h"

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

static bool correlation_is_new(
    const OpenV2g_ProtocolAdapterStatus *status,
    uint32_t correlation_id)
{
    uint32_t delta;
    if (correlation_id == 0u) {
        return false;
    }
    if (status->last_correlation_id == 0u) {
        return true;
    }
    delta = correlation_id - status->last_correlation_id;
    return (delta != 0u) && (delta < 0x80000000u);
}

static OpenV2g_ProtocolMessageKind expected_kind(
    OpenV2g_ProtocolState state)
{
    switch (state) {
    case OPENV2G_PROTOCOL_STATE_SESSION_SETUP:
        return OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP;
    case OPENV2G_PROTOCOL_STATE_SERVICE_DISCOVERY:
        return OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY;
    case OPENV2G_PROTOCOL_STATE_SERVICE_SELECTION:
        return OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION;
    case OPENV2G_PROTOCOL_STATE_AUTHORIZATION:
        return OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION;
    case OPENV2G_PROTOCOL_STATE_CHARGE_PARAMETER:
        return OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER;
    case OPENV2G_PROTOCOL_STATE_CABLE_CHECK:
        return OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK;
    case OPENV2G_PROTOCOL_STATE_PRECHARGE:
        return OPENV2G_PROTOCOL_MESSAGE_PRECHARGE;
    case OPENV2G_PROTOCOL_STATE_POWER_DELIVERY:
        return OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START;
    case OPENV2G_PROTOCOL_STATE_WELDING_DETECTION:
        return OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION;
    case OPENV2G_PROTOCOL_STATE_SESSION_STOP:
        return OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP;
    default:
        return OPENV2G_PROTOCOL_MESSAGE_NONE;
    }
}

static OpenV2g_Result din_start(
    void *context,
    const OpenV2g_ProtocolStartRequest *request)
{
    OpenV2g_Din70121Adapter *adapter =
        (OpenV2g_Din70121Adapter *)context;
    if ((adapter == NULL) || (request == NULL) || !adapter->enabled ||
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
    set_state(
        adapter, OPENV2G_PROTOCOL_STATE_SESSION_SETUP, OPENV2G_REASON_NONE);
    return OPENV2G_RESULT_ACCEPTED;
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
    if (adapter->status.state != OPENV2G_PROTOCOL_STATE_TERMINATED) {
        set_state(adapter, OPENV2G_PROTOCOL_STATE_TERMINATED, reason);
    }
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result din_message(
    void *context,
    const OpenV2g_ProtocolMessage *message)
{
    OpenV2g_Din70121Adapter *adapter =
        (OpenV2g_Din70121Adapter *)context;
    OpenV2g_ProtocolState next;
    OpenV2g_ProtocolMessageKind expected;

    if ((adapter == NULL) || (message == NULL) ||
        (message->generation != adapter->status.generation) ||
        !adapter->status.active ||
        !correlation_is_new(&adapter->status, message->correlation_id)) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (!message->accepted) {
        adapter->status.last_correlation_id = message->correlation_id;
        set_state(
            adapter, OPENV2G_PROTOCOL_STATE_FAILED, OPENV2G_REASON_PEER_REJECT);
        return OPENV2G_RESULT_REJECTED;
    }

    if (adapter->status.state == OPENV2G_PROTOCOL_STATE_CHARGE_LOOP) {
        if (message->kind == OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND) {
            adapter->status.last_correlation_id = message->correlation_id;
            set_state(
                adapter,
                OPENV2G_PROTOCOL_STATE_CHARGE_LOOP,
                OPENV2G_REASON_NONE);
            return OPENV2G_RESULT_OK;
        }
        if (message->kind == OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP) {
            adapter->status.last_correlation_id = message->correlation_id;
            set_state(
                adapter,
                OPENV2G_PROTOCOL_STATE_WELDING_DETECTION,
                OPENV2G_REASON_NONE);
            return OPENV2G_RESULT_OK;
        }
    }

    expected = expected_kind(adapter->status.state);
    if (message->kind != expected) {
        set_state(
            adapter, OPENV2G_PROTOCOL_STATE_FAILED, OPENV2G_REASON_SEQUENCE);
        return OPENV2G_RESULT_REJECTED;
    }
    switch (adapter->status.state) {
    case OPENV2G_PROTOCOL_STATE_SESSION_SETUP:
        next = OPENV2G_PROTOCOL_STATE_SERVICE_DISCOVERY;
        break;
    case OPENV2G_PROTOCOL_STATE_SERVICE_DISCOVERY:
        next = OPENV2G_PROTOCOL_STATE_SERVICE_SELECTION;
        break;
    case OPENV2G_PROTOCOL_STATE_SERVICE_SELECTION:
        next = OPENV2G_PROTOCOL_STATE_AUTHORIZATION;
        break;
    case OPENV2G_PROTOCOL_STATE_AUTHORIZATION:
        next = OPENV2G_PROTOCOL_STATE_CHARGE_PARAMETER;
        break;
    case OPENV2G_PROTOCOL_STATE_CHARGE_PARAMETER:
        next = OPENV2G_PROTOCOL_STATE_CABLE_CHECK;
        break;
    case OPENV2G_PROTOCOL_STATE_CABLE_CHECK:
        next = OPENV2G_PROTOCOL_STATE_PRECHARGE;
        break;
    case OPENV2G_PROTOCOL_STATE_PRECHARGE:
        next = OPENV2G_PROTOCOL_STATE_POWER_DELIVERY;
        break;
    case OPENV2G_PROTOCOL_STATE_POWER_DELIVERY:
        next = OPENV2G_PROTOCOL_STATE_CHARGE_LOOP;
        break;
    case OPENV2G_PROTOCOL_STATE_WELDING_DETECTION:
        next = OPENV2G_PROTOCOL_STATE_SESSION_STOP;
        break;
    case OPENV2G_PROTOCOL_STATE_SESSION_STOP:
        next = OPENV2G_PROTOCOL_STATE_TERMINATED;
        break;
    default:
        set_state(
            adapter, OPENV2G_PROTOCOL_STATE_FAILED, OPENV2G_REASON_SEQUENCE);
        return OPENV2G_RESULT_REJECTED;
    }
    adapter->status.last_correlation_id = message->correlation_id;
    set_state(adapter, next, OPENV2G_REASON_NONE);
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result din_timer(
    void *context,
    uint32_t generation,
    uint16_t timer_id)
{
    OpenV2g_Din70121Adapter *adapter =
        (OpenV2g_Din70121Adapter *)context;
    if ((adapter == NULL) || !adapter->status.active || (timer_id == 0u) ||
        (generation != adapter->status.generation)) {
        return OPENV2G_RESULT_REJECTED;
    }
    set_state(adapter, OPENV2G_PROTOCOL_STATE_FAILED, OPENV2G_REASON_TIMEOUT);
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result din_read(
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
    port->read = din_read;
    return OPENV2G_RESULT_OK;
}
