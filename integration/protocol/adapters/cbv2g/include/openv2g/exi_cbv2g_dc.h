#ifndef OPENV2G_EXI_CBV2G_DC_H
#define OPENV2G_EXI_CBV2G_DC_H
#include "openv2g/iso15118_chrg/exi_codec.h"
#include "openv2g/iso15118_chrg/protocol_adapter.h"
#define OPENV2G_EXI_DC_PAYLOAD_MAX 4096u
#define OPENV2G_EXI_DC_WORKSPACE_SIZE (OPENV2G_EXI_DC_PAYLOAD_MAX + 1u)
/* Project DTO IDs: request kind, or kind + response offset. Not wire tags. */
#define OPENV2G_EXI_DC_RESPONSE_OFFSET 100u
#define OPENV2G_EXI_DC_ID_MAX 32u
#define OPENV2G_EXI_PARAMETER_MAX 5u
typedef struct { int16_t set_id; char name[32]; uint16_t name_length; bool is_string; int32_t integer_value; char text[32]; uint16_t text_length; } OpenV2g_ExiParameter;
#define OPENV2G_EXI_SCHEDULE_MAX 3u
#define OPENV2G_EXI_SCHEDULE_POINTS_MAX 5u
typedef struct { uint32_t start_s; uint32_t duration_s; int32_t power_w; } OpenV2g_ExiSchedulePoint;
typedef struct { uint8_t id; uint8_t count; OpenV2g_ExiSchedulePoint points[OPENV2G_EXI_SCHEDULE_POINTS_MAX]; } OpenV2g_ExiSchedule;
#define OPENV2G_EXI_METER_ID_MAX 32u
#define OPENV2G_EXI_METER_SIGNATURE_MAX 64u
typedef struct {
    uint16_t id_length;
    char id[OPENV2G_EXI_METER_ID_MAX];
    bool has_reading;
    uint64_t reading_wh;
    bool has_status;
    int16_t status;
    bool has_time;
    int64_t time_s;
    uint16_t signature_length;
    uint8_t signature[OPENV2G_EXI_METER_SIGNATURE_MAX];
} OpenV2g_ExiMeterInfo;
#define OPENV2G_EXI_SIGNATURE_ID_MAX 32u
typedef struct {
    char body_id[OPENV2G_EXI_SIGNATURE_ID_MAX];
    uint16_t body_id_length;
    uint8_t digest[32];
    uint8_t value[64]; /* Raw P-256 r || s; not DER. */
} OpenV2g_ExiSignature;
/* Bounded host data model; capacities are explicit, not imported ABI.
 * No imported/generated type is exported. Physical fields use project SI units.
 */
typedef struct {
    OpenV2g_ProtocolMessageKind kind;
    bool response;
    bool has_signature;
    OpenV2g_ExiSignature signature;
    bool accepted;
    bool new_session;
    bool ongoing;
    bool ready;
    bool isolation_valid;
    bool resumed_session;
    bool pause_session;
    bool ac_mode; /* Bounded single-phase AC EIM host profile. */
    bool rcd_fault;
    uint32_t ac_energy_wh;
    bool charging_complete;
    bool has_meter_info;
    bool receipt_required;
    OpenV2g_ExiMeterInfo meter_info;
    bool waiting_for_customer;
    bool omit_pending_parameters;
    bool omit_pending_schedules;
    uint32_t min_voltage_mv;
    int32_t min_current_ma;
    uint16_t raw_response_code;
    bool has_max_voltage;
    bool has_max_current;
    bool has_max_power;
    bool voltage_limit_achieved;
    bool current_limit_achieved;
    bool power_limit_achieved;
    uint8_t parameter_count;
    OpenV2g_ExiParameter parameters[OPENV2G_EXI_PARAMETER_MAX];
    uint8_t schedule_count;
    OpenV2g_ExiSchedule schedules[OPENV2G_EXI_SCHEDULE_MAX];
    uint8_t session_id[8];
    uint8_t evcc_id[6];
    uint8_t evse_id[OPENV2G_EXI_DC_ID_MAX];
    uint16_t evse_id_length;
    uint16_t service_id;
    uint8_t schedule_id;
    uint16_t soc_permille;
    uint32_t voltage_mv;
    int32_t current_ma;
    uint32_t max_voltage_mv;
    int32_t max_current_ma;
    int32_t max_power_w;
    int32_t schedule_power_w;
    uint32_t schedule_duration_s;
} OpenV2g_ExiDcDocument;
/* Strict one-local-reference ISO-2 receipt profile. Serialization is not trust verification. */
OpenV2g_Result OpenV2g_Cbv2gIso2_ReceiptFragment(const OpenV2g_ExiDcDocument *document,
    uint8_t *output, size_t capacity, size_t *length);
OpenV2g_Result OpenV2g_Cbv2gIso2_SignedInfoFragment(const OpenV2g_ExiSignature *signature,
    uint8_t *output, size_t capacity, size_t *length);
OpenV2g_Result OpenV2g_Cbv2gDin_Bind(OpenV2g_ExiCodec *codec);
OpenV2g_Result OpenV2g_Cbv2gIso2_Bind(OpenV2g_ExiCodec *codec);
/* Configuration defaults for a closed host test fixture, never a normative profile. */
OpenV2g_ExiDcDocument OpenV2g_ExiDc_Default(OpenV2g_ProtocolMessageKind kind, bool response);
#endif
