#ifndef OPENV2G_SIM_SLAC_PEER_H
#define OPENV2G_SIM_SLAC_PEER_H
#include "openv2g/support/plc_link_manager.h"
/* Deterministic typed peer and key-reference model, not a raw HomePlug modem. */
typedef struct {
    OpenV2g_PlcLinkManager *owner; OpenV2g_SlacConfig config;
    uint32_t generation; uint32_t key_version; uint8_t run_id[8]; uint8_t pev[6]; uint8_t mac[6];
    uint8_t starts; uint8_t sounds; uint8_t attenuation; uint8_t drop_param_requests;
    bool active; bool attenuation_acked; bool key_installed; bool reject_match;
    unsigned parameter_requests; unsigned match_requests; unsigned cancels;
} OpenV2g_HostSlacPeer;
OpenV2g_SlacConfig OpenV2g_HostSlacConfig(void);
void OpenV2g_HostSlacPeer_Init(OpenV2g_HostSlacPeer *, OpenV2g_PlcLinkManager *, const OpenV2g_SlacConfig *);
OpenV2g_SlacPorts OpenV2g_HostSlacPeer_Ports(OpenV2g_HostSlacPeer *);
#endif
