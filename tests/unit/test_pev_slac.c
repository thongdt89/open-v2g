#include "openv2g/simulation/slac_peer.h"
#include "test.h"
#include "openv2g/simulation/host_simulation.h"
#include <string.h>
static const uint8_t run[8] = {1u,2u,3u,4u,5u,6u,7u,8u};
static const uint8_t pev[6] = {2u,0u,0u,0u,0u,1u};
static int init(OpenV2g_PlcLinkManager *manager, OpenV2g_HostSlacPeer *peer)
{
    OpenV2g_SlacConfig config = OpenV2g_HostSlacConfig(); OpenV2g_SlacPorts ports;
    OpenV2g_PlcLinkManager_Init(manager); OpenV2g_HostSlacPeer_Init(peer,manager,&config); ports = OpenV2g_HostSlacPeer_Ports(peer);
    TEST_ASSERT(OpenV2g_PlcLinkManager_BindSlac(manager,&config,&ports) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_PlcLinkManager_Start(manager,1u,100u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_PlcLinkManager_OnModemReady(manager,1u,100u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_PlcLinkManager_OnSlacMatched(manager,1u,100u) == OPENV2G_RESULT_REJECTED); return 0;
}
static OpenV2g_Result drive(OpenV2g_PlcLinkManager *manager, uint32_t generation, OpenV2g_TimeUs now)
{
    unsigned i; OpenV2g_Result result = OPENV2G_RESULT_BUSY;
    for (i = 0u; i < 64u && result == OPENV2G_RESULT_BUSY; ++i) { result = OpenV2g_PlcLinkManager_PumpSlac(manager,generation,now); }
    return result;
}
int main(void)
{
    OpenV2g_PlcLinkManager manager; OpenV2g_HostSlacPeer peer;
    OpenV2g_SlacFrame frame; OpenV2g_PlcLinkStatus before;
    uint8_t next_run[8]; unsigned i;
    {
        OpenV2g_HostSimulation simulation;
        TEST_ASSERT(OpenV2g_HostSimulation_Init(&simulation,NULL) == OPENV2G_RESULT_OK && !simulation.config.simulate_slac);
    }
    TEST_ASSERT(init(&manager,&peer) == 0);
    TEST_ASSERT(OpenV2g_PlcLinkManager_BeginSlac(&manager,1u,100u,run,pev) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(manager.pev_slac.status.state == OPENV2G_SLAC_WAIT_PARAM && manager.pev_slac.rx_count == 1u && !manager.status.slac_matched);
    TEST_ASSERT(drive(&manager,1u,100u) == OPENV2G_RESULT_OK && manager.status.state == OPENV2G_PLC_ASSOCIATING);
    TEST_ASSERT(peer.starts == 3u && peer.sounds == 10u && peer.match_requests == 1u && peer.key_installed);
    TEST_ASSERT(OpenV2g_PlcLinkManager_OnAssociated(&manager,1u,101u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_PlcLinkManager_OnIpv6Ready(&manager,1u,102u) == OPENV2G_RESULT_OK && manager.status.ipv6_ready);
    peer.key_installed = false;
    TEST_ASSERT(OpenV2g_PlcLinkManager_PumpSlac(&manager,1u,103u) == OPENV2G_RESULT_REJECTED && manager.status.state == OPENV2G_PLC_FAULT && !manager.status.slac_matched);
    {
        OpenV2g_SlacPorts ports = OpenV2g_HostSlacPeer_Ports(&peer);
        (void)memset(&frame,0,sizeof(frame)); frame.kind = OPENV2G_SLAC_MATCH_REQ; frame.env.session_generation = 1u;
        (void)memcpy(frame.source,pev,6u); (void)memcpy(frame.destination,peer.mac,6u); (void)memcpy(frame.run_id,run,8u);
        TEST_ASSERT(ports.send(ports.context,&frame) == OPENV2G_RESULT_REJECTED && !peer.key_installed);
    }
    TEST_ASSERT(peer.active && peer.cancels == 0u); /* Established-link cleanup waits for coordinated Stop. */
    TEST_ASSERT(OpenV2g_PlcLinkManager_Stop(&manager,1u,104u,OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_OK && peer.cancels == 1u);
    TEST_ASSERT(OpenV2g_PlcLinkManager_Stop(&manager,1u,104u,OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_OK && peer.cancels == 1u);
    TEST_ASSERT(OpenV2g_PlcLinkManager_Start(&manager,2u,200u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_PlcLinkManager_OnModemReady(&manager,2u,200u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_PlcLinkManager_BeginSlac(&manager,2u,200u,run,pev) == OPENV2G_RESULT_REJECTED);
    (void)memcpy(next_run,run,8u); next_run[0] = 2u;
    TEST_ASSERT(OpenV2g_PlcLinkManager_BeginSlac(&manager,2u,200u,next_run,pev) == OPENV2G_RESULT_ACCEPTED);
    before = manager.status;
    TEST_ASSERT(OpenV2g_PevSlac_Cancel(&manager.pev_slac,1u,200u) == OPENV2G_RESULT_REJECTED && memcmp(&before,&manager.status,sizeof(before)) == 0);
    frame = manager.pev_slac.rx[manager.pev_slac.rx_head]; frame.env.session_generation = 1u;
    TEST_ASSERT(OpenV2g_PlcLinkManager_EnqueueSlac(&manager,&frame) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(drive(&manager,2u,200u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_PevSlac_Cancel(&manager.pev_slac,2u,200u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_PevSlac_Cancel(&manager.pev_slac,2u,1u) == OPENV2G_RESULT_OK && peer.cancels == 2u);
    TEST_ASSERT(init(&manager,&peer) == 0); peer.drop_param_requests = 1u;
    TEST_ASSERT(OpenV2g_PlcLinkManager_BeginSlac(&manager,1u,100u,run,pev) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_PlcLinkManager_PumpSlac(&manager,1u,50100u) == OPENV2G_RESULT_BUSY);
    TEST_ASSERT(drive(&manager,1u,50100u) == OPENV2G_RESULT_OK && peer.parameter_requests == 2u);
    TEST_ASSERT(OpenV2g_PlcLinkManager_Stop(&manager,1u,50100u,OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_OK);
    TEST_ASSERT(init(&manager,&peer) == 0);
    TEST_ASSERT(OpenV2g_PlcLinkManager_BeginSlac(&manager,1u,100u,run,pev) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_PlcLinkManager_PumpSlac(&manager,1u,200100u) == OPENV2G_RESULT_BUSY);
    for (i = 0u; i < 3u; ++i) { TEST_ASSERT(OpenV2g_PlcLinkManager_PumpSlac(&manager,1u,400100u) == OPENV2G_RESULT_BUSY); }
    for (i = 0u; i < 10u; ++i) { TEST_ASSERT(OpenV2g_PlcLinkManager_PumpSlac(&manager,1u,600100u) == OPENV2G_RESULT_BUSY); }
    TEST_ASSERT(OpenV2g_PlcLinkManager_PumpSlac(&manager,1u,800100u) == OPENV2G_RESULT_BUSY);
    TEST_ASSERT(OpenV2g_PlcLinkManager_PumpSlac(&manager,1u,1000100u) == OPENV2G_RESULT_REJECTED && manager.status.env.reason == OPENV2G_REASON_TIMEOUT);
    TEST_ASSERT(init(&manager,&peer) == 0); peer.attenuation = 31u;
    TEST_ASSERT(OpenV2g_PlcLinkManager_BeginSlac(&manager,1u,100u,run,pev) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(drive(&manager,1u,100u) == OPENV2G_RESULT_REJECTED && !manager.status.slac_matched && peer.match_requests == 0u);
    TEST_ASSERT(init(&manager,&peer) == 0); peer.reject_match = true;
    TEST_ASSERT(OpenV2g_PlcLinkManager_BeginSlac(&manager,1u,100u,run,pev) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(drive(&manager,1u,100u) == OPENV2G_RESULT_REJECTED && !peer.key_installed);
    TEST_ASSERT(init(&manager,&peer) == 0); peer.drop_param_requests = 3u;
    TEST_ASSERT(OpenV2g_PlcLinkManager_BeginSlac(&manager,1u,100u,run,pev) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_PlcLinkManager_PumpSlac(&manager,1u,50100u) == OPENV2G_RESULT_BUSY);
    TEST_ASSERT(OpenV2g_PlcLinkManager_PumpSlac(&manager,1u,100100u) == OPENV2G_RESULT_BUSY && peer.parameter_requests == 3u);
    TEST_ASSERT(OpenV2g_PlcLinkManager_PumpSlac(&manager,1u,150100u) == OPENV2G_RESULT_BUSY && peer.parameter_requests == 3u);
    TEST_ASSERT(OpenV2g_PlcLinkManager_PumpSlac(&manager,1u,250100u) == OPENV2G_RESULT_REJECTED && manager.status.env.reason == OPENV2G_REASON_TIMEOUT);
    TEST_ASSERT(init(&manager,&peer) == 0);
    TEST_ASSERT(OpenV2g_PlcLinkManager_BeginSlac(&manager,1u,100u,run,pev) == OPENV2G_RESULT_ACCEPTED);
    frame = manager.pev_slac.rx[0];
    for (i = 1u; i < OPENV2G_SLAC_RX_CAPACITY; ++i) { TEST_ASSERT(OpenV2g_PlcLinkManager_EnqueueSlac(&manager,&frame) == OPENV2G_RESULT_ACCEPTED); }
    TEST_ASSERT(OpenV2g_PlcLinkManager_EnqueueSlac(&manager,&frame) == OPENV2G_RESULT_OVERFLOW && manager.pev_slac.status.state == OPENV2G_SLAC_WAIT_PARAM);
    TEST_ASSERT(OpenV2g_PlcLinkManager_PumpSlac(&manager,1u,100u) == OPENV2G_RESULT_REJECTED && manager.status.state == OPENV2G_PLC_FAULT);
    TEST_ASSERT(init(&manager,&peer) == 0); peer.drop_param_requests = 1u;
    TEST_ASSERT(OpenV2g_PlcLinkManager_BeginSlac(&manager,1u,100u,run,pev) == OPENV2G_RESULT_ACCEPTED);
    (void)memset(&frame,0,sizeof(frame)); frame.env.session_generation = 1u; frame.env.timestamp = 100u; frame.env.quality = OPENV2G_QUALITY_VALID;
    frame.kind = OPENV2G_SLAC_PARAM_CNF; frame.accepted = true; frame.sound_count = 10u; frame.group_count = 58u;
    (void)memcpy(frame.source,peer.mac,6u); (void)memcpy(frame.destination,pev,6u); (void)memcpy(frame.run_id,run,8u); frame.run_id[0] ^= 1u;
    TEST_ASSERT(OpenV2g_PlcLinkManager_EnqueueSlac(&manager,&frame) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_PlcLinkManager_PumpSlac(&manager,1u,100u) == OPENV2G_RESULT_BUSY && manager.pev_slac.status.state == OPENV2G_SLAC_WAIT_PARAM);
    frame.run_id[0] ^= 1u;
    TEST_ASSERT(OpenV2g_PlcLinkManager_EnqueueSlac(&manager,&frame) == OPENV2G_RESULT_ACCEPTED); frame.source[5] = 99u;
    TEST_ASSERT(OpenV2g_PlcLinkManager_PumpSlac(&manager,1u,100u) == OPENV2G_RESULT_BUSY && manager.pev_slac.status.peer[5] == 2u);
    TEST_ASSERT(drive(&manager,1u,100u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_PlcLinkManager_PumpSlac(&manager,1u,99u) == OPENV2G_RESULT_REJECTED && manager.status.state == OPENV2G_PLC_FAULT);
    TEST_ASSERT(OpenV2g_PlcLinkManager_Stop(&manager,1u,100u,OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_OK && !peer.active);
    return 0;
}
