#include "openv2g/iso15118_chrg/iso15118_2_adapter.h"
#include "openv2g/iso15118_chrg/protocol_data_flow.h"

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

static void fail_adapter(
    OpenV2g_Iso15118_2Adapter *adapter,
    OpenV2g_ProtocolMessageKind kind,
    uint32_t correlation_id,
    OpenV2g_ReasonCode reason)
{
    adapter->status.pending_correlation_id = 0u;
    adapter->status.pending_kind = OPENV2G_PROTOCOL_MESSAGE_NONE;
    adapter->diagnostic.generation = adapter->status.generation;
    adapter->diagnostic.sequence_counter++;
    adapter->diagnostic.protocol = OPENV2G_PROTOCOL_ISO_15118_2;
    adapter->diagnostic.state = adapter->status.state;
    adapter->diagnostic.message_kind = kind;
    adapter->diagnostic.correlation_id = correlation_id;
    adapter->diagnostic.reason = reason;
    set_state(adapter, OPENV2G_PROTOCOL_STATE_FAILED, reason);
}

static OpenV2g_Result send_request(
    OpenV2g_Iso15118_2Adapter *adapter,
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
        OPENV2G_PROTOCOL_ISO_15118_2,
        OPENV2G_SCHEMA_ISO_15118_2,
        adapter->status.mode,
        kind,
        adapter->status.generation,
        correlation_id,
        now,
        &adapter->energy_required_port,
        &adapter->permission_required_port,
        &adapter->metering_evidence,
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

static OpenV2g_Result iso2_start(
    void *context,
    const OpenV2g_ProtocolStartRequest *request)
{
    OpenV2g_Iso15118_2Adapter *adapter =
        (OpenV2g_Iso15118_2Adapter *)context;
    bool mode_enabled;

    if ((adapter == NULL) || (request == NULL) ||
        (adapter->message_required_port.send == NULL) ||
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
    adapter->status.pending_correlation_id = 0u;
    adapter->status.pending_kind = OPENV2G_PROTOCOL_MESSAGE_NONE;
    adapter->next_correlation_id = 0u;
    (void)memset(&adapter->metering_evidence, 0, sizeof(adapter->metering_evidence));
    (void)memset(&adapter->diagnostic, 0, sizeof(adapter->diagnostic));
    set_state(
        adapter, OPENV2G_PROTOCOL_STATE_SESSION_SETUP, OPENV2G_REASON_NONE);
    return send_request(
        adapter, OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP, request->now);
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
    adapter->status.pending_correlation_id = 0u;
    adapter->status.pending_kind = OPENV2G_PROTOCOL_MESSAGE_NONE;
    (void)memset(&adapter->metering_evidence, 0, sizeof(adapter->metering_evidence));
    if (adapter->status.state != OPENV2G_PROTOCOL_STATE_TERMINATED) {
        set_state(adapter, OPENV2G_PROTOCOL_STATE_TERMINATED, reason);
    }
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result advance_common(
    OpenV2g_Iso15118_2Adapter *adapter,
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
        if (adapter->status.mode == OPENV2G_MODE_DC) {
            next_state = OPENV2G_PROTOCOL_STATE_CABLE_CHECK;
            next_kind = OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK;
        } else {
            next_state = OPENV2G_PROTOCOL_STATE_POWER_DELIVERY;
            next_kind = OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START;
        }
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
        next_kind = (adapter->status.mode == OPENV2G_MODE_DC)
                        ? OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND
                        : OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS;
        break;
    case OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP:
        if (adapter->status.mode == OPENV2G_MODE_DC) {
            next_state = OPENV2G_PROTOCOL_STATE_WELDING_DETECTION;
            next_kind = OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION;
        } else {
            next_state = OPENV2G_PROTOCOL_STATE_SESSION_STOP;
            next_kind = OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP;
        }
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

static OpenV2g_Result advance_charge_loop(
    OpenV2g_Iso15118_2Adapter *adapter,
    const OpenV2g_ProtocolMessage *message)
{
    const OpenV2g_ProtocolMessageKind loop_kind =
        (adapter->status.mode == OPENV2G_MODE_DC)
            ? OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND
            : OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS;

    if ((message->kind != loop_kind) &&
        (message->kind != OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT)) {
        fail_adapter(
            adapter,
            message->kind,
            message->correlation_id,
            OPENV2G_REASON_SEQUENCE);
        return OPENV2G_RESULT_REJECTED;
    }
    if (message->metering_receipt_required &&
        (message->kind == loop_kind)) {
        if ((message->metering_evidence.generation !=
             adapter->status.generation) ||
            (message->metering_evidence.evidence_id == 0u) ||
            (message->metering_evidence.source_correlation_id !=
             message->correlation_id) ||
            message->metering_evidence.signature_required) {
            fail_adapter(
                adapter,
                message->kind,
                message->correlation_id,
                message->metering_evidence.signature_required
                    ? OPENV2G_REASON_SECURITY
                    : OPENV2G_REASON_PRECONDITION);
            return OPENV2G_RESULT_REJECTED;
        }
        adapter->metering_evidence = message->metering_evidence;
        set_state(
            adapter,
            OPENV2G_PROTOCOL_STATE_METERING_RECEIPT,
            OPENV2G_REASON_NONE);
        return send_request(
            adapter,
            OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT,
            message->now);
    }
    if (message->ongoing) {
        if (message->kind == OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT) {
            (void)memset(
                &adapter->metering_evidence,
                0,
                sizeof(adapter->metering_evidence));
        }
        set_state(
            adapter, OPENV2G_PROTOCOL_STATE_CHARGE_LOOP, OPENV2G_REASON_NONE);
        return send_request(adapter, loop_kind, message->now);
    }
    set_state(
        adapter,
        OPENV2G_PROTOCOL_STATE_POWER_DELIVERY_STOP,
        OPENV2G_REASON_NONE);
    if (message->kind == OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT) {
        (void)memset(
            &adapter->metering_evidence,
            0,
            sizeof(adapter->metering_evidence));
    }
    return send_request(
        adapter,
        OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP,
        message->now);
}

static OpenV2g_Result iso2_message(
    void *context,
    const OpenV2g_ProtocolMessage *message)
{
    OpenV2g_Iso15118_2Adapter *adapter =
        (OpenV2g_Iso15118_2Adapter *)context;
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
    if ((completed_kind == OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND) ||
        (completed_kind == OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS) ||
        (completed_kind == OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT)) {
        return advance_charge_loop(adapter, message);
    }
    if (message->ongoing) {
        return send_request(adapter, completed_kind, message->now);
    }
    return advance_common(adapter, completed_kind, message->now);
}

static OpenV2g_Result iso2_timer(
    void *context,
    uint32_t generation,
    uint16_t timer_id)
{
    OpenV2g_Iso15118_2Adapter *adapter =
        (OpenV2g_Iso15118_2Adapter *)context;
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

static OpenV2g_Result iso2_read_status(
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

static OpenV2g_Result iso2_read_diagnostic(
    void *context,
    OpenV2g_ProtocolDiagnostic *diagnostic)
{
    const OpenV2g_Iso15118_2Adapter *adapter =
        (const OpenV2g_Iso15118_2Adapter *)context;
    if ((adapter == NULL) || (diagnostic == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (adapter->diagnostic.sequence_counter == 0u) {
        return OPENV2G_RESULT_NOT_IMPLEMENTED;
    }
    *diagnostic = adapter->diagnostic;
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

OpenV2g_Result OpenV2g_Iso15118_2Adapter_BindRequiredPorts(
    OpenV2g_Iso15118_2Adapter *adapter,
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
    port->read = iso2_read_status;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_Iso15118_2Adapter_CreateDiagnosticSenderReceiverPort(
    OpenV2g_Iso15118_2Adapter *adapter,
    OpenV2g_ProtocolDiagnosticSrPort *port)
{
    if ((adapter == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = adapter;
    port->read = iso2_read_diagnostic;
    return OPENV2G_RESULT_OK;
}
