#ifndef OPENV2G_COMMON_TYPES_H
#define OPENV2G_COMMON_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Logical host/SIL types derived from Atomic Interface Specification Rev.3.
 * They deliberately are not supplier ABI or generated AUTOSAR types.
 */

typedef uint64_t OpenV2g_TimeUs;

typedef enum {
    OPENV2G_QUALITY_INVALID = 0,
    OPENV2G_QUALITY_INIT = 1,
    OPENV2G_QUALITY_VALID = 2,
    OPENV2G_QUALITY_DEGRADED = 3,
    OPENV2G_QUALITY_STALE = 4
} OpenV2g_DataQuality;

typedef enum {
    OPENV2G_REASON_NONE = 0,
    OPENV2G_REASON_PRECONDITION = 1,
    OPENV2G_REASON_STALE_DATA = 2,
    OPENV2G_REASON_LINK_LOSS = 3,
    OPENV2G_REASON_TIMEOUT = 4,
    OPENV2G_REASON_SEQUENCE = 5,
    OPENV2G_REASON_FORMAT = 6,
    OPENV2G_REASON_SECURITY = 7,
    OPENV2G_REASON_RESOURCE = 8,
    OPENV2G_REASON_PEER_REJECT = 9,
    OPENV2G_REASON_CANCELLED = 10,
    OPENV2G_REASON_INTERNAL = 11,
    OPENV2G_REASON_HW_MISMATCH = 12,
    OPENV2G_REASON_ISOLATION = 13,
    OPENV2G_REASON_INTERLOCK = 14,
    OPENV2G_REASON_PERMISSION_EXPIRED = 15,
    OPENV2G_REASON_CONFIGURATION = 16
} OpenV2g_ReasonCode;

typedef enum {
    OPENV2G_PLUG_UNKNOWN = 0,
    OPENV2G_PLUG_UNPLUGGED = 1,
    OPENV2G_PLUG_CONNECTED = 2,
    OPENV2G_PLUG_LOCKED = 3
} OpenV2g_PlugState;

typedef enum {
    OPENV2G_CP_INVALID = 0,
    OPENV2G_CP_A = 1,
    OPENV2G_CP_B = 2,
    OPENV2G_CP_C = 3,
    OPENV2G_CP_D = 4,
    OPENV2G_CP_E = 5,
    OPENV2G_CP_F = 6
} OpenV2g_CpState;

typedef enum {
    OPENV2G_COORDINATOR_UNPLUGGED = 0,
    OPENV2G_COORDINATOR_CONNECTED_IDLE = 1,
    OPENV2G_COORDINATOR_LINK_SETUP = 2,
    OPENV2G_COORDINATOR_HLC_NEGOTIATION = 3,
    OPENV2G_COORDINATOR_READY_FOR_ENERGY = 4,
    OPENV2G_COORDINATOR_ENERGY_TRANSFER = 5,
    OPENV2G_COORDINATOR_STOPPING = 6,
    OPENV2G_COORDINATOR_FAULT_LATCHED = 7
} OpenV2g_CoordinatorState;

typedef enum {
    OPENV2G_PERMISSION_DENY = 0,
    OPENV2G_PERMISSION_ALLOW = 1
} OpenV2g_PermissionDecision;

typedef enum {
    OPENV2G_ACTION_NONE = 0,
    OPENV2G_ACTION_PREPARE = 1,
    OPENV2G_ACTION_ENERGIZE = 2,
    OPENV2G_ACTION_IMPORT_POWER = 3,
    OPENV2G_ACTION_EXPORT_POWER = 4,
    OPENV2G_ACTION_RAMP_TO_ZERO = 5,
    OPENV2G_ACTION_DISCHARGE = 6
} OpenV2g_AllowedAction;

typedef enum {
    OPENV2G_MODE_NONE = 0,
    OPENV2G_MODE_AC = 1,
    OPENV2G_MODE_DC = 2,
    OPENV2G_MODE_AC_BPT = 3,
    OPENV2G_MODE_DC_BPT = 4
} OpenV2g_ChargeMode;

typedef enum {
    OPENV2G_DIRECTION_NONE = 0,
    OPENV2G_DIRECTION_IMPORT_TO_EV = 1,
    OPENV2G_DIRECTION_EXPORT_FROM_EV = 2,
    OPENV2G_DIRECTION_ZERO_TRANSITION = 3
} OpenV2g_PowerDirection;

typedef enum {
    OPENV2G_HLC_IDLE = 0,
    OPENV2G_HLC_WAIT_LINK = 1,
    OPENV2G_HLC_SDP = 2,
    OPENV2G_HLC_TRANSPORT = 3,
    OPENV2G_HLC_SAP = 4,
    OPENV2G_HLC_PROTOCOL_SESSION = 5,
    OPENV2G_HLC_CLOSING = 6,
    OPENV2G_HLC_ERROR = 7
} OpenV2g_HlcState;

typedef enum {
    OPENV2G_HLC_EVENT_STATE_CHANGED = 0,
    OPENV2G_HLC_EVENT_PARAMETERS_ACCEPTED = 1,
    OPENV2G_HLC_EVENT_PEER_REQUEST = 2,
    OPENV2G_HLC_EVENT_AUTH_RESULT = 3,
    OPENV2G_HLC_EVENT_SESSION_COMPLETE = 4,
    OPENV2G_HLC_EVENT_WARNING = 5,
    OPENV2G_HLC_EVENT_FAILURE = 6
} OpenV2g_HlcEventCategory;

typedef enum {
    OPENV2G_PROTOCOL_NONE = 0,
    OPENV2G_PROTOCOL_DIN_70121 = 1,
    OPENV2G_PROTOCOL_ISO_15118_2 = 2,
    OPENV2G_PROTOCOL_ISO_15118_20 = 3
} OpenV2g_ProtocolId;

typedef enum {
    OPENV2G_SCHEMA_NONE = 0,
    OPENV2G_SCHEMA_SAP = 1,
    OPENV2G_SCHEMA_DIN_70121 = 2,
    OPENV2G_SCHEMA_ISO_15118_2 = 3,
    OPENV2G_SCHEMA_ISO_20_COMMON = 4,
    OPENV2G_SCHEMA_ISO_20_AC = 5,
    OPENV2G_SCHEMA_ISO_20_DC = 6
} OpenV2g_SchemaId;

typedef enum {
    OPENV2G_TRANSPORT_TCP = 0,
    OPENV2G_TRANSPORT_UDP = 1
} OpenV2g_TransportProtocol;

typedef enum {
    OPENV2G_SECURITY_NONE = 0,
    OPENV2G_SECURITY_TLS12 = 1,
    OPENV2G_SECURITY_TLS13 = 2
} OpenV2g_SecurityMode;

typedef enum {
    OPENV2G_SECURITY_NOT_REQUESTED = 0,
    OPENV2G_SECURITY_NEGOTIATING = 1,
    OPENV2G_SECURITY_ESTABLISHED = 2,
    OPENV2G_SECURITY_FAILED = 3
} OpenV2g_SecurityState;

typedef enum {
    OPENV2G_TRANSPORT_CLOSED = 0,
    OPENV2G_TRANSPORT_CONNECTING = 1,
    OPENV2G_TRANSPORT_PLAIN_ACTIVE = 2,
    OPENV2G_TRANSPORT_TLS_HANDSHAKE = 3,
    OPENV2G_TRANSPORT_SECURE_ACTIVE = 4,
    OPENV2G_TRANSPORT_CLOSING = 5,
    OPENV2G_TRANSPORT_ERROR = 6
} OpenV2g_TransportState;

typedef enum {
    OPENV2G_PLC_DOWN = 0,
    OPENV2G_PLC_MODEM_INIT = 1,
    OPENV2G_PLC_SLAC_MATCHING = 2,
    OPENV2G_PLC_ASSOCIATING = 3,
    OPENV2G_PLC_IP_CONFIGURING = 4,
    OPENV2G_PLC_LINK_READY = 5,
    OPENV2G_PLC_STOPPING = 6,
    OPENV2G_PLC_FAULT = 7
} OpenV2g_PlcLinkState;

typedef enum {
    OPENV2G_HW_SAFE = 0,
    OPENV2G_HW_PREPARING = 1,
    OPENV2G_HW_READY = 2,
    OPENV2G_HW_ENERGIZED = 3,
    OPENV2G_HW_STOPPING = 4,
    OPENV2G_HW_FAULT = 5
} OpenV2g_HardwareState;

typedef enum {
    OPENV2G_FAULT_INFO = 0,
    OPENV2G_FAULT_WARNING = 1,
    OPENV2G_FAULT_CONTROLLED_STOP = 2,
    OPENV2G_FAULT_EMERGENCY_STOP = 3
} OpenV2g_FaultSeverity;

typedef struct {
    uint32_t session_generation;
    uint32_t sequence_counter;
    OpenV2g_TimeUs timestamp;
    OpenV2g_DataQuality quality;
    OpenV2g_ReasonCode reason;
} OpenV2g_CommEnvelope;

typedef struct {
    int32_t max_import_power_w;
    int32_t max_export_power_w;
    int32_t max_charge_current_ma;
    int32_t max_discharge_current_ma;
    uint32_t max_voltage_mv;
    uint32_t ramp_limit_w_per_s;
} OpenV2g_PowerLimits;

typedef struct {
    OpenV2g_CommEnvelope env;
    OpenV2g_PlugState plug_state;
    OpenV2g_CpState cp_state;
    bool hlc_trigger;
} OpenV2g_QualifiedCpPp;

typedef struct {
    OpenV2g_CommEnvelope env;
    uint32_t snapshot_id;
    uint16_t soc_permille;
    uint32_t dc_voltage_mv;
    int32_t dc_current_ma;
    int16_t battery_temperature_deci_c;
    uint32_t isolation_kohm;
    bool interlock_closed;
    OpenV2g_PowerLimits limits;
    uint32_t availability_mask;
} OpenV2g_EvEnergySnapshot;

typedef struct {
    OpenV2g_CommEnvelope env;
    uint32_t link_generation;
    OpenV2g_PlcLinkState state;
    bool modem_ready;
    bool slac_matched;
    bool associated;
    bool ipv6_ready;
} OpenV2g_PlcLinkStatus;

typedef struct {
    OpenV2g_CommEnvelope env;
    OpenV2g_HlcState state;
    OpenV2g_ProtocolId protocol;
    OpenV2g_SchemaId schema;
    OpenV2g_ChargeMode selected_mode;
    uint32_t permission_used;
    uint16_t protocol_step;
} OpenV2g_HlcSessionStatus;

typedef struct {
    OpenV2g_CommEnvelope env;
    uint32_t event_id;
    uint32_t evidence_id;
    OpenV2g_HlcEventCategory category;
    OpenV2g_ProtocolId protocol;
    uint16_t protocol_step;
    OpenV2g_AllowedAction requested_action;
    int32_t requested_power_w;
} OpenV2g_HlcEvent;

typedef struct {
    OpenV2g_CommEnvelope env;
    OpenV2g_HardwareState state;
    bool connector_locked;
    bool precharge_active;
    bool positive_contactor_closed;
    bool negative_contactor_closed;
    bool discharge_active;
    uint32_t measured_voltage_mv;
    int32_t measured_current_ma;
} OpenV2g_ChargeHardwareStatus;

typedef struct {
    OpenV2g_CommEnvelope env;
    uint32_t permission_id;
    OpenV2g_PermissionDecision decision;
    OpenV2g_AllowedAction action;
    OpenV2g_ChargeMode mode;
    OpenV2g_PowerDirection direction;
    OpenV2g_PowerLimits limits;
    OpenV2g_TimeUs valid_until;
} OpenV2g_ChargePermission;

typedef struct {
    OpenV2g_CommEnvelope env;
    uint32_t fault_id;
    OpenV2g_FaultSeverity severity;
    OpenV2g_ReasonCode cause;
    bool latched;
} OpenV2g_BlockingFaultStatus;

typedef struct {
    OpenV2g_CommEnvelope env;
    bool enabled;
    OpenV2g_PowerDirection direction;
    bool measured_zero_confirmed;
    bool has_new_permission;
    OpenV2g_PowerLimits limits;
} OpenV2g_BptPermissionRequest;

typedef struct {
    uint8_t address[16];
    uint16_t port;
    OpenV2g_TransportProtocol transport;
    OpenV2g_SecurityMode security;
    uint16_t interface_id;
} OpenV2g_Endpoint;

typedef struct {
    OpenV2g_CommEnvelope env;
    uint32_t connection_id;
    OpenV2g_TransportState state;
    OpenV2g_SecurityState security;
    OpenV2g_Endpoint endpoint;
    uint16_t tx_queue_used;
    uint16_t rx_queue_used;
} OpenV2g_TransportStatus;

#endif
