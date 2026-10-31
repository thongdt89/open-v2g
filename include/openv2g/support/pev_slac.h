#ifndef OPENV2G_SUPPORT_PEV_SLAC_H
#define OPENV2G_SUPPORT_PEV_SLAC_H
#include "openv2g/common/types.h"
#include "openv2g/common/result.h"
#define OPENV2G_SLAC_GROUPS_MAX 58u
#define OPENV2G_SLAC_RX_CAPACITY 8u
/* PROJECT typed host/modem seam, not an Ethernet/HomePlug imported ABI. */
typedef enum { OPENV2G_SLAC_PARAM_REQ = 1, OPENV2G_SLAC_PARAM_CNF,
    OPENV2G_SLAC_START_ATTEN, OPENV2G_SLAC_SOUND, OPENV2G_SLAC_ATTEN_IND,
    OPENV2G_SLAC_ATTEN_RSP, OPENV2G_SLAC_MATCH_REQ, OPENV2G_SLAC_MATCH_CNF } OpenV2g_SlacMessageKind;
typedef struct {
    OpenV2g_CommEnvelope env;
    OpenV2g_SlacMessageKind kind;
    uint8_t run_id[8]; uint8_t source[6]; uint8_t destination[6];
    uint8_t sound_count; uint8_t sounds_remaining; uint8_t group_count;
    uint8_t attenuation[OPENV2G_SLAC_GROUPS_MAX];
    bool accepted; uint8_t network_id[7]; uint32_t key_handle; uint32_t key_version;
} OpenV2g_SlacFrame;
typedef struct {
    uint8_t start_indications; uint8_t sound_count; uint8_t group_count;
    uint8_t max_attenuation; uint8_t max_transmissions;
    OpenV2g_TimeUs stage_budget; OpenV2g_TimeUs total_budget; OpenV2g_TimeUs retry_interval;
} OpenV2g_SlacConfig;
typedef struct {
    void *context;
    /* Provider copies frame before return. Callbacks only enqueue copied RX data. */
    OpenV2g_Result (*send)(void *, const OpenV2g_SlacFrame *);
    bool (*key_is_current)(void *, uint32_t generation, const uint8_t network_id[7], uint32_t handle, uint32_t version);
    void (*cancel)(void *, uint32_t generation);
} OpenV2g_SlacPorts;
typedef enum { OPENV2G_SLAC_IDLE = 0, OPENV2G_SLAC_WAIT_PARAM, OPENV2G_SLAC_SEND_START,
    OPENV2G_SLAC_SEND_SOUND, OPENV2G_SLAC_WAIT_ATTEN, OPENV2G_SLAC_SEND_ATTEN_RSP,
    OPENV2G_SLAC_SEND_MATCH, OPENV2G_SLAC_WAIT_MATCH, OPENV2G_SLAC_MATCHED,
    OPENV2G_SLAC_FAILED, OPENV2G_SLAC_CANCELLED } OpenV2g_SlacState;
typedef struct {
    OpenV2g_CommEnvelope env; OpenV2g_SlacState state;
    uint8_t run_id[8]; uint8_t peer[6]; uint8_t network_id[7];
    uint32_t key_handle; uint32_t key_version;
} OpenV2g_SlacStatus;
typedef struct {
    OpenV2g_SlacConfig config; OpenV2g_SlacPorts ports; OpenV2g_SlacStatus status;
    uint8_t pev[6]; uint8_t last_run_id[8]; uint8_t starts_sent; uint8_t sounds_sent; uint8_t transmissions;
    OpenV2g_TimeUs stage_started; OpenV2g_TimeUs stage_deadline; OpenV2g_TimeUs total_deadline;
    OpenV2g_TimeUs retry_at; OpenV2g_TimeUs owner_time;
    OpenV2g_SlacFrame rx[OPENV2G_SLAC_RX_CAPACITY]; uint8_t rx_head; uint8_t rx_count;
    bool overflow; bool bound; bool cancel_notified;
} OpenV2g_PevSlac;
OpenV2g_Result OpenV2g_PevSlac_Init(OpenV2g_PevSlac *, const OpenV2g_SlacConfig *, const OpenV2g_SlacPorts *);
OpenV2g_Result OpenV2g_PevSlac_Start(OpenV2g_PevSlac *, uint32_t generation, OpenV2g_TimeUs now, const uint8_t run_id[8], const uint8_t pev[6]);
OpenV2g_Result OpenV2g_PevSlac_Enqueue(OpenV2g_PevSlac *, const OpenV2g_SlacFrame *);
OpenV2g_Result OpenV2g_PevSlac_Pump(OpenV2g_PevSlac *, uint32_t generation, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_PevSlac_Cancel(OpenV2g_PevSlac *, uint32_t generation, OpenV2g_TimeUs now);
#endif
