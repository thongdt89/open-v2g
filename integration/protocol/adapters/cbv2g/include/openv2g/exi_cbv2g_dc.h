#ifndef OPENV2G_EXI_CBV2G_DC_H
#define OPENV2G_EXI_CBV2G_DC_H
#include "openv2g/iso15118_chrg/exi_codec.h"
#include "openv2g/iso15118_chrg/protocol_adapter.h"
#define OPENV2G_EXI_DC_PAYLOAD_MAX 4096u
#define OPENV2G_EXI_DC_WORKSPACE_SIZE (OPENV2G_EXI_DC_PAYLOAD_MAX + 1u)
/* Project DTO IDs: request kind, or kind + response offset. Not wire tags. */
#define OPENV2G_EXI_DC_RESPONSE_OFFSET 100u
#define OPENV2G_EXI_DC_ID_MAX 32u
/* Bounded DC EIM host profile: one charge service and one flat schedule.
 * No imported/generated type is exported. Physical fields use project SI units.
 */
typedef struct {
    OpenV2g_ProtocolMessageKind kind;
    bool response;
    bool accepted;
    bool new_session;
    bool ongoing;
    bool ready;
    bool isolation_valid;
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
OpenV2g_Result OpenV2g_Cbv2gDin_Bind(OpenV2g_ExiCodec *codec);
OpenV2g_Result OpenV2g_Cbv2gIso2_Bind(OpenV2g_ExiCodec *codec);
/* Configuration defaults for a closed host test fixture, never a normative profile. */
OpenV2g_ExiDcDocument OpenV2g_ExiDc_Default(OpenV2g_ProtocolMessageKind kind, bool response);
#endif
