#include "openv2g/iso15118_chrg/iso15118_2_adapter.h"

#include <string.h>

static void set_state(
    OpenV2g_Iso15118_2Adapter *adapter,
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

static OpenV2g_Result iso2_start(
    void *context,
    const OpenV2g_ProtocolStartRequest *request)
{
    OpenV2g_Iso15118_2Adapter *adapter =
        (OpenV2g_Iso15118_2Adapter *)context;
    bool mode_enabled;

    if ((adapter == NULL) || (request == NULL) ||
        (request->generation == 0u) ||
        (request->protocol != OPENV2G_PROTOCOL_ISO_15118_2) ||
        (request->schema != OPENV2G_SCHEMA_ISO_15118_2) ||
        ((adapter->status.state != OPENV2G_PROTOCOL_STATE_IDLE) &&
         (adapter->status.state != OPENV2G_PROTOCOL_STATE_TERMINATED))) {
        return OPENV2G_RESULT_REJECTED;
    }
    if ((request->security != OPENV2G_SECURITY_NONE) &&
        (request->security != OPENV2G_SECURITY_TLS12) &&
        (request->security != OPENV2G_SECURITY_TLS13)) {
        return OPENV2G_RESULT_REJECTED;
    }
    mode_enabled = ((request->mode == OPENV2G_MODE_DC) &&
                    adapter->dc_eim_enabled) ||
                   ((request->mode == OPENV2G_MODE_AC) &&
                    adapter->ac_eim_enabled);
    if (!mode_enabled) {
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

static OpenV2g_Result iso2_stop(
    void *context,
    uint32_t generation,
    OpenV2g_ReasonCode reason)
{
    OpenV2g_Iso15118_2Adapter *adapter =
        (OpenV2g_Iso15118_2Adapter *)context;
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

static OpenV2g_Result handle_charge_loop(
    OpenV2g_Iso15118_2Adapter *adapter,
    const OpenV2g_ProtocolMessage *message)
{
    const bool is_dc = adapter->status.mode == OPENV2G_MODE_DC;
    const OpenV2g_ProtocolMessageKind loop_kind =
        is_dc ? OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND
              : OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS;

    if (message->kind == loop_kind) {
        adapter->status.last_correlation_id = message->correlation_id;
        set_state(
            adapter,
            OPENV2G_PROTOCOL_STATE_CHARGE_LOOP,
            OPENV2G_REASON_NONE);
        return OPENV2G_RESULT_OK;
    }
    if (message->kind == OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT) {
        adapter->status.last_correlation_id = message->correlation_id;
        set_state(
            adapter,
            OPENV2G_PROTOCOL_STATE_METERING_RECEIPT,
            OPENV2G_REASON_NONE);
        return OPENV2G_RESULT_OK;
    }
    if (message->kind == OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP) {
        adapter->status.last_correlation_id = message->correlation_id;
        set_state(
            adapter,
            is_dc ? OPENV2G_PROTOCOL_STATE_WELDING_DETECTION
                  : OPENV2G_PROTOCOL_STATE_SESSION_STOP,
            OPENV2G_REASON_NONE);
        return OPENV2G_RESULT_OK;
    }
    set_state(adapter, OPENV2G_PROTOCOL_STATE_FAILED, OPENV2G_REASON_SEQUENCE);
    return OPENV2G_RESULT_REJECTED;
}

static OpenV2g_Result iso2_message(
    void *context,
    const OpenV2g_ProtocolMessage *message)
{
    OpenV2g_Iso15118_2Adapter *adapter =
        (OpenV2g_Iso15118_2Adapter *)context;
    OpenV2g_ProtocolState next;

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
        return handle_charge_loop(adapter, message);
    }
    if (adapter->status.state == OPENV2G_PROTOCOL_STATE_METERING_RECEIPT) {
        const OpenV2g_ProtocolMessageKind expected =
            (adapter->status.mode == OPENV2G_MODE_DC)
                ? OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND
                : OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS;
        if (message->kind != expected) {
            set_state(
                adapter,
                OPENV2G_PROTOCOL_STATE_FAILED,
                OPENV2G_REASON_SEQUENCE);
            return OPENV2G_RESULT_REJECTED;
        }
        adapter->status.last_correlation_id = message->correlation_id;
        set_state(
            adapter,
            OPENV2G_PROTOCOL_STATE_CHARGE_LOOP,
            OPENV2G_REASON_NONE);
        return OPENV2G_RESULT_OK;
    }
    if (message->kind != expected_kind(adapter->status.state)) {
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
        next = (adapter->status.mode == OPENV2G_MODE_DC)
                   ? OPENV2G_PROTOCOL_STATE_CABLE_CHECK
                   : OPENV2G_PROTOCOL_STATE_POWER_DELIVERY;
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

static OpenV2g_Result iso2_timer(
    void *context,
    uint32_t generation,
    uint16_t timer_id)
{
    OpenV2g_Iso15118_2Adapter *adapter =
        (OpenV2g_Iso15118_2Adapter *)context;
    if ((adapter == NULL) || !adapter->status.active || (timer_id == 0u) ||
        (generation != adapter->status.generation)) {
        return OPENV2G_RESULT_REJECTED;
    }
    set_state(adapter, OPENV2G_PROTOCOL_STATE_FAILED, OPENV2G_REASON_TIMEOUT);
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result iso2_read(
    void *context,
    OpenV2g_ProtocolAdapterStatus *status)
{
    const OpenV2g_Iso15118_2Adapter *adapter =
        (const OpenV2g_Iso15118_2Adapter *)context;
    if ((adapter == NULL) || (status == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    *status = adapter->status;
    return OPENV2G_RESULT_OK;
}

void OpenV2g_Iso15118_2Adapter_Init(
    OpenV2g_Iso15118_2Adapter *adapter,
    bool dc_eim_enabled,
    bool ac_eim_enabled,
    bool pnc_enabled)
{
    if (adapter == NULL) {
        return;
    }
    (void)memset(adapter, 0, sizeof(*adapter));
    adapter->dc_eim_enabled = dc_eim_enabled;
    adapter->ac_eim_enabled = ac_eim_enabled;
    adapter->pnc_enabled = pnc_enabled;
    adapter->status.protocol = OPENV2G_PROTOCOL_ISO_15118_2;
    adapter->status.schema = OPENV2G_SCHEMA_ISO_15118_2;
    adapter->status.state = (dc_eim_enabled || ac_eim_enabled || pnc_enabled)
                                ? OPENV2G_PROTOCOL_STATE_IDLE
                                : OPENV2G_PROTOCOL_STATE_DISABLED;
}

OpenV2g_Result OpenV2g_Iso15118_2Adapter_CreateClientServerPort(
    OpenV2g_Iso15118_2Adapter *adapter,
    OpenV2g_ProtocolAdapterCsPort *port)
{
    if ((adapter == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = adapter;
    port->start = iso2_start;
    port->stop = iso2_stop;
    port->on_message = iso2_message;
    port->on_timer_expired = iso2_timer;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_Iso15118_2Adapter_CreateStatusSenderReceiverPort(
    OpenV2g_Iso15118_2Adapter *adapter,
    OpenV2g_ProtocolAdapterStatusSrPort *port)
{
    if ((adapter == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = adapter;
    port->read = iso2_read;
    return OPENV2G_RESULT_OK;
}
