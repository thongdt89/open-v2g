#ifndef OPENV2G_ISO15118_CHRG_PROTOCOL_ADAPTER_H
#define OPENV2G_ISO15118_CHRG_PROTOCOL_ADAPTER_H

#include <stdbool.h>
#include <stdint.h>

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"

/*
 * Project-side semantic message IDs. A generated DIN/ISO EXI mapper converts
 * its schema-specific types to these IDs; these values are not wire IDs.
 */
typedef enum {
    OPENV2G_PROTOCOL_MESSAGE_NONE = 0,
    OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP = 1,
    OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY = 2,
    OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION = 3,
    OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION = 4,
    OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER = 5,
    OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK = 6,
    OPENV2G_PROTOCOL_MESSAGE_PRECHARGE = 7,
    OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START = 8,
    OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND = 9,
    OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS = 10,
    OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT = 11,
    OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP = 12,
    OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION = 13,
    OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP = 14
} OpenV2g_ProtocolMessageKind;

typedef enum {
    OPENV2G_PROTOCOL_STATE_DISABLED = 0,
    OPENV2G_PROTOCOL_STATE_IDLE = 1,
    OPENV2G_PROTOCOL_STATE_SESSION_SETUP = 2,
    OPENV2G_PROTOCOL_STATE_SERVICE_DISCOVERY = 3,
    OPENV2G_PROTOCOL_STATE_SERVICE_SELECTION = 4,
    OPENV2G_PROTOCOL_STATE_AUTHORIZATION = 5,
    OPENV2G_PROTOCOL_STATE_CHARGE_PARAMETER = 6,
    OPENV2G_PROTOCOL_STATE_CABLE_CHECK = 7,
    OPENV2G_PROTOCOL_STATE_PRECHARGE = 8,
    OPENV2G_PROTOCOL_STATE_POWER_DELIVERY = 9,
    OPENV2G_PROTOCOL_STATE_CHARGE_LOOP = 10,
    OPENV2G_PROTOCOL_STATE_METERING_RECEIPT = 11,
    OPENV2G_PROTOCOL_STATE_WELDING_DETECTION = 12,
    OPENV2G_PROTOCOL_STATE_SESSION_STOP = 13,
    OPENV2G_PROTOCOL_STATE_TERMINATED = 14,
    OPENV2G_PROTOCOL_STATE_FAILED = 15
} OpenV2g_ProtocolState;

typedef struct {
    uint32_t generation;
    uint32_t correlation_id;
    OpenV2g_ProtocolMessageKind kind;
    bool accepted;
    bool ongoing;
    OpenV2g_AllowedAction requested_action;
    int32_t requested_power_w;
} OpenV2g_ProtocolMessage;

typedef struct {
    uint32_t generation;
    uint32_t sequence_counter;
    OpenV2g_ProtocolId protocol;
    OpenV2g_SchemaId schema;
    OpenV2g_ChargeMode mode;
    OpenV2g_ProtocolState state;
    OpenV2g_ReasonCode reason;
    uint32_t last_correlation_id;
    bool active;
} OpenV2g_ProtocolAdapterStatus;

typedef struct {
    uint32_t generation;
    OpenV2g_ProtocolId protocol;
    OpenV2g_SchemaId schema;
    OpenV2g_ChargeMode mode;
    OpenV2g_SecurityMode security;
} OpenV2g_ProtocolStartRequest;

/* PROVISIONAL_ASR_STYLE common PAD-IF-001..005 C/S contract. */
typedef struct {
    void *context;
    OpenV2g_Result (*start)(
        void *context,
        const OpenV2g_ProtocolStartRequest *request);
    OpenV2g_Result (*stop)(
        void *context,
        uint32_t generation,
        OpenV2g_ReasonCode reason);
    OpenV2g_Result (*on_message)(
        void *context,
        const OpenV2g_ProtocolMessage *message);
    OpenV2g_Result (*on_timer_expired)(
        void *context,
        uint32_t generation,
        uint16_t timer_id);
} OpenV2g_ProtocolAdapterCsPort;

/* PROVISIONAL_ASR_STYLE authoritative PAD-IF-011 S/R status. */
typedef struct {
    void *context;
    OpenV2g_Result (*read)(
        void *context,
        OpenV2g_ProtocolAdapterStatus *status);
} OpenV2g_ProtocolAdapterStatusSrPort;

#endif
