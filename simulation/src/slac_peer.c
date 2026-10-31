#include "openv2g/simulation/slac_peer.h"
#include <string.h>
OpenV2g_SlacConfig OpenV2g_HostSlacConfig(void)
{
    /* Provisional test settings; not normative timing or vehicle thresholds. */
    const OpenV2g_SlacConfig config = {3u,10u,58u,30u,3u,250000u,1000000u,50000u}; return config;
}
void OpenV2g_HostSlacPeer_Init(OpenV2g_HostSlacPeer *peer, OpenV2g_PlcLinkManager *owner, const OpenV2g_SlacConfig *config)
{
    if (peer == NULL || config == NULL) { return; }
    (void)memset(peer,0,sizeof(*peer)); peer->owner = owner; peer->config = *config;
    peer->mac[0] = 2u; peer->mac[5] = 2u; peer->attenuation = 7u;
}
static bool key_current(void *context, uint32_t generation, const uint8_t nid[7], uint32_t handle, uint32_t version)
{
    const OpenV2g_HostSlacPeer *peer = context;
    static const uint8_t network_id[7] = {0x42u,1u,2u,3u,4u,5u,6u};
    return peer != NULL && peer->active && peer->key_installed && peer->generation == generation &&
        handle == 7u && version == peer->key_version && version != 0u && memcmp(nid,network_id,7u) == 0;
}
static void cancel(void *context, uint32_t generation)
{
    OpenV2g_HostSlacPeer *peer = context;
    if (peer != NULL && peer->generation == generation && peer->active) {
        peer->active = peer->key_installed = false; ++peer->cancels;
    }
}
static OpenV2g_Result transmit(void *context, const OpenV2g_SlacFrame *request)
{
    OpenV2g_HostSlacPeer *peer = context; OpenV2g_SlacFrame response; unsigned i;
    if (peer == NULL || peer->owner == NULL || request == NULL || request->env.session_generation == 0u ||
        request->env.session_generation != peer->owner->generation) { return OPENV2G_RESULT_REJECTED; }
    response = *request; (void)memcpy(response.source,peer->mac,6u); (void)memcpy(response.destination,request->source,6u); response.accepted = true;
    if (request->kind == OPENV2G_SLAC_PARAM_REQ) {
        static const uint8_t broadcast[6] = {255u,255u,255u,255u,255u,255u};
        if (memcmp(request->destination,broadcast,6u) != 0 || request->sound_count != peer->config.sound_count || request->group_count != peer->config.group_count) { return OPENV2G_RESULT_REJECTED; }
        if (request->env.session_generation != peer->generation) {
            peer->starts = peer->sounds = 0u; peer->attenuation_acked = peer->key_installed = false; peer->key_version = 0u;
            peer->generation = request->env.session_generation; (void)memcpy(peer->run_id,request->run_id,8u); (void)memcpy(peer->pev,request->source,6u);
        }
        if (memcmp(peer->run_id,request->run_id,8u) != 0 || memcmp(peer->pev,request->source,6u) != 0) { return OPENV2G_RESULT_REJECTED; }
        peer->active = true; ++peer->parameter_requests;
        if (peer->drop_param_requests != 0u) { --peer->drop_param_requests; return OPENV2G_RESULT_ACCEPTED; }
        response.kind = OPENV2G_SLAC_PARAM_CNF;
        return OpenV2g_PlcLinkManager_EnqueueSlac(peer->owner,&response);
    }
    if (!peer->active || peer->generation != request->env.session_generation || memcmp(request->run_id,peer->run_id,8u) != 0 ||
        memcmp(request->source,peer->pev,6u) != 0 || memcmp(request->destination,peer->mac,6u) != 0) { return OPENV2G_RESULT_REJECTED; }
    switch (request->kind) {
    case OPENV2G_SLAC_START_ATTEN:
        if (peer->starts >= peer->config.start_indications || peer->sounds != 0u) { return OPENV2G_RESULT_REJECTED; }
        ++peer->starts; return OPENV2G_RESULT_ACCEPTED;
    case OPENV2G_SLAC_SOUND:
        if (peer->starts != peer->config.start_indications || peer->sounds >= peer->config.sound_count ||
            request->sounds_remaining != peer->config.sound_count-peer->sounds-1u) { return OPENV2G_RESULT_REJECTED; }
        if (++peer->sounds != peer->config.sound_count) { return OPENV2G_RESULT_ACCEPTED; }
        response.kind = OPENV2G_SLAC_ATTEN_IND; response.sounds_remaining = 0u;
        for (i = 0u; i < peer->config.group_count; ++i) { response.attenuation[i] = peer->attenuation; }
        return OpenV2g_PlcLinkManager_EnqueueSlac(peer->owner,&response);
    case OPENV2G_SLAC_ATTEN_RSP:
        if (peer->sounds != peer->config.sound_count || !request->accepted) { return OPENV2G_RESULT_REJECTED; }
        peer->attenuation_acked = true; return OPENV2G_RESULT_ACCEPTED;
    case OPENV2G_SLAC_MATCH_REQ:
        if (!peer->attenuation_acked) { return OPENV2G_RESULT_REJECTED; }
        if (!peer->key_installed && peer->key_version != 0u) { return OPENV2G_RESULT_REJECTED; } /* No same-run key revival. */
        ++peer->match_requests; response.kind = OPENV2G_SLAC_MATCH_CNF; response.accepted = !peer->reject_match;
        if (!peer->reject_match && peer->key_version == 0u) { peer->key_version = 1u; }
        response.key_handle = 7u; response.key_version = peer->key_version; response.network_id[0] = 0x42u;
        for (i = 1u; i < 7u; ++i) { response.network_id[i] = (uint8_t)i; }
        peer->key_installed = !peer->reject_match;
        return OpenV2g_PlcLinkManager_EnqueueSlac(peer->owner,&response);
    default: return OPENV2G_RESULT_REJECTED;
    }
}
OpenV2g_SlacPorts OpenV2g_HostSlacPeer_Ports(OpenV2g_HostSlacPeer *peer)
{
    OpenV2g_SlacPorts ports = {peer,transmit,key_current,cancel}; return ports;
}
